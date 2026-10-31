/* palanca.dll (en mods_vr): bajar palancas con la mano.
 *
 * Interactuás con la palanca como con todo (el empujón de usar.dll, o la E),
 * y en lugar de apretar el botón que pide el juego para bajarla (la A),
 * bajás la mano como si tiraras de la palanca de verdad.
 *
 * Por defecto (Boton=-1) no hace falta botón: después de cada "usar"
 * (acción 12) queda armado Ventana segundos; pasado Gracia (la mano que
 * vuelve del empujón no cuenta), por cada Paso metros que baja una mano
 * desde lo más alto que estuvo (bajando a más de Velocidad), aprieta una vez
 * la acción (Accion, la tecla que tenga en el juego, por SendInput como
 * usar.dll). La palanca del generador pide machacar la A unas 6 veces para
 * llenar una barra: así la barra se llena mientras bajás la mano. Cada toque
 * vuelve a empezar la Ventana, hasta MaxToques. Con Boton 0/1/2 se baja
 * agarrando (agarre, A/X, gatillo) y anda siempre, sin armar.
 *
 * No sabemos todavía qué acción pide el juego para la palanca: por defecto
 * la 12 (usar, la E), que es lo que suele hacer la A. Descubrir=1 anota cada
 * acción que se aprieta, así que si no anda, apretás la A de verdad y en
 * ai_mods.log queda cuál es. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"
#include "tecla_virtual.h"

#ifndef VERSION
#define VERSION "palanca v0.2.1"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define OFS_CONTROL         0x64
#define SLOT_POSES          11
#define MAX_DISPOSITIVOS    16
#define MAX_ACCIONES        64
#define BOTON_A             (1ull << 7)   /* A en el mando derecho, X en el izquierdo */

typedef struct {
    float m[3][4];
    float vel[3], vel_ang[3];
    int resultado;
    unsigned char valida, conectado, relleno[2];
} PoseVR;

typedef void (__attribute__((thiscall)) *FnPoses)(void *sys, int universo, float prediccion, PoseVR *poses, unsigned n);

static const Anfitrion *g_host;
static unsigned g_delta;
static int g_pausado;

/* Configuración */
static int g_activado = 1;
static int g_accion = 12;
static unsigned g_tecla_ini;        /* 0 = la del mapa del juego */
static int g_mano = 2;              /* 0 izquierda, 1 derecha, 2 cualquiera */
static int g_boton = -1;            /* -1 sin botón (armado por usar), 0 agarre, 1 A/X, 2 gatillo */
static float g_paso = 0.03f;        /* m hacia abajo por cada toque */
static float g_velocidad = 0.25f;   /* m/s hacia abajo para que cuente */
static int g_max_toques = 8;        /* por vez que se arma (o por agarre) */
static float g_entre = 0.06f;       /* s suelta entre un toque y el siguiente */
static float g_ventana = 8.0f;      /* s armado después de usar */
static float g_gracia = 0.8f;       /* s del principio que no cuentan */
#define ACCION_USAR 12
#define PROPIO_SEG  0.35f
static float g_mantener = 0.06f;
static int g_descubrir = 1;
static int g_vibrar = 1;
static int g_log = 1;

/* Estado */
static unsigned g_tecla;
static float g_t_tecla = 10.0f, g_t_mandos = 10.0f, g_toque, g_suelta = 10.0f;
static int g_pendientes, g_hechos, g_mano_activa = -1;
static unsigned char g_estado_antes[MAX_ACCIONES];
static float g_armado = -1.0f;      /* s desde el último usar; < 0 = no */
static int g_usar_antes;
static struct {
    int agarrado;
    float y0, bajo, vel_max;
} g_m[2];

#define logf_(...) do { if (g_log && g_host) g_host->log(g_host, __VA_ARGS__); } while (0)

static void *leer_ptr(const void *dir)
{
    if (!dir || IsBadReadPtr(dir, 4))
        return NULL;
    return *(void *const *)dir;
}

static unsigned char *jugador(void)
{
    unsigned char *m = leer_ptr((void *)(UINT_PTR)(DIR_MANAGER_PJ + g_delta)), *lista;
    if (!m)
        return NULL;
    m = leer_ptr(m + 8);
    if (!m || IsBadReadPtr(m, 0x88) || *(unsigned *)(m + 0x84) == 0)
        return NULL;
    lista = leer_ptr(m + 0x80);
    return lista ? leer_ptr(lista) : NULL;
}

static int ripley_libre(unsigned char *pj)
{
    unsigned char *ctl = pj ? leer_ptr(pj + OFS_CONTROL) : NULL;
    return ctl && !IsBadReadPtr(ctl + 0x1f, 1) && ctl[0x1f];
}

static int juego_con_foco(void)
{
    DWORD pid = 0;
    HWND w = GetForegroundWindow();
    if (!w)
        return 0;
    GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static void tecla(int abajo)
{
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)(g_tecla & 0x7f);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (abajo ? 0 : KEYEVENTF_KEYUP) | (g_tecla & 0x80 ? KEYEVENTF_EXTENDEDKEY : 0);
    SendInput(1, &in, sizeof in);
}

/* La tecla de teclado de la acción (como usar.dll). 1 = lista, 0 = todavía
 * no hay mapa, -1 = no tiene. Anota todas las teclas de la acción. */
static int buscar_tecla(void)
{
    unsigned char *m = tv_mapa(g_delta), *comb;
    unsigned n, k;
    if (g_tecla_ini) {
        g_tecla = g_tecla_ini;
        logf_("aprieto la tecla %02x (del .ini)", g_tecla);
        return 1;
    }
    if (!m)
        return 0;
    if ((unsigned)g_accion >= *(unsigned *)(m + 0x1054) || g_accion >= MAX_ACCIONES)
        return -1;
    comb = *(unsigned char **)(m + 4 + g_accion * 0x40 + 0x30);
    n = *(unsigned *)(m + 4 + g_accion * 0x40 + 0x34);
    if (!comb || n > 16 || IsBadReadPtr(comb, n * 8))
        return -1;
    g_tecla = 0;
    for (k = 0; k < n; k++) {
        unsigned c = *(unsigned short *)(comb + k * 8 + 4), tipo = comb[k * 8 + 6], fl = comb[k * 8 + 7];
        logf_("accion %d, tecla %u: codigo %04x tipo %u banderas %02x", g_accion, k, c, tipo, fl);
        if (!g_tecla && tipo == 1 && c != 0xffff && c < 0x100 && !(fl & 0x40))
            g_tecla = c;
    }
    if (!g_tecla)
        return -1;
    logf_("aprieto la tecla %02x (la de la accion %d en el juego)", g_tecla, g_accion);
    return 1;
}

/* Descubrir: anota cada acción del mapa de juego que se aprieta. */
static void descubrir(void)
{
    unsigned char *m = tv_mapa(g_delta);
    unsigned n, k;
    if (!m)
        return;
    n = *(unsigned *)(m + 0x1054);
    if (n > MAX_ACCIONES)
        n = MAX_ACCIONES;
    for (k = 0; k < n; k++) {
        int st = tv_estado(g_delta, (int)k);
        if (st < 0)
            break;
        if ((st & 1) && !(g_estado_antes[k] & 1) && (g_toque <= 0.0f))
            logf_("descubrir: accion %u recien apretada", k);
        g_estado_antes[k] = (unsigned char)st;
    }
}

/* Un toque de la acción, si toca (lo llama tick cuando hay pendientes). */
static void toque(void)
{
    if (!ripley_libre(jugador()) || !juego_con_foco()) {
        logf_("palanca: %d toques pendientes, pero %s: no aprieto", g_pendientes,
              !juego_con_foco() ? "el juego no tiene el foco" : "Ripley no esta en control libre");
        g_pendientes = 0;
        return;
    }
    tecla(1);
    g_toque = g_mantener;
    g_pendientes--;
    g_hechos++;
    if (g_vibrar && g_mano_activa >= 0)
        vibrar_pedir(g_mano_activa, 1, 12.0f, 0.0f, 0.5f);
}

static void reiniciar(void)
{
    g_m[0].agarrado = g_m[1].agarrado = 0;
    g_pendientes = 0;
    g_hechos = 0;
    g_mano_activa = -1;
}

static int agarrando(void *sys, int mano)
{
    unsigned long long b;
    float gat;
    if (!vibrar_botones(sys, mano, &b, &gat))
        return 0;
    if (g_boton == 1)
        return (b & BOTON_A) != 0;
    if (g_boton == 2)
        return gat > 0.6f;
    return (b & BOTON_AGARRE) != 0;
}

static void f_tick(float dt)
{
    void *sys;
    PoseVR p[MAX_DISPOSITIVOS];
    int mano;

    if (dt > 0.1f)
        dt = 0.1f;
    if (g_toque > 0.0f) {
        g_toque -= dt;
        if (g_toque <= 0.0f) {
            tecla(0);
            g_suelta = 0.0f;
        }
    } else {
        g_suelta += dt;
    }
    if (g_pausado || !g_activado)
        return;
    if (g_toque <= 0.0f && g_suelta >= g_entre && g_pendientes > 0)
        toque();
    if (g_descubrir)
        descubrir();
    if (g_boton < 0) {
        int st = tv_estado(g_delta, ACCION_USAR);
        if (st >= 0) {
            /* Nuestra propia tecla llega al juego con atraso: un usar hasta
             * PROPIO_SEG después del último toque es nuestro, no arma. */
            if ((st & 1) && !(g_usar_antes & 1) && g_toque <= 0.0f && g_suelta > PROPIO_SEG && g_pendientes == 0) {
                if (g_hechos)
                    logf_("palanca: %d toques con la mano %s", g_hechos, g_mano_activa ? "derecha" : "izquierda");
                g_armado = 0.0f;
                reiniciar();        /* empezar a medir de nuevo */
                logf_("usar: palanca armada %.0f s", g_ventana);
            }
            g_usar_antes = st;
        }
        if (g_armado >= 0.0f) {
            g_armado += dt;
            if (g_armado > g_ventana) {
                g_armado = -1.0f;
                logf_("palanca desarmada (pasaron %.0f s, %d toques)", g_ventana, g_hechos);
            }
        }
    }
    if (!(sys = g_host->openvr()))
        return;
    vibrar_tick(sys, dt);
    g_t_mandos += dt;
    if (g_t_mandos > 2.0f) {
        g_t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    if (!g_tecla) {
        g_t_tecla += dt;
        if (g_t_tecla < 2.0f)
            return;
        g_t_tecla = 0.0f;
        if (buscar_tecla() < 0) {
            logf_("la accion %d no tiene tecla de teclado: pone Tecla= en el .ini", g_accion);
            g_tecla = 0xffffffffu;
        }
        return;
    }
    if (g_tecla == 0xffffffffu)
        return;

    memset(p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[SLOT_POSES])(sys, 2, 0.0f, p, MAX_DISPOSITIVOS);
    for (mano = 0; mano < 2; mano++) {
        int k = vibrar_indice(mano), ag;
        float y, vy;
        if (g_mano != 2 && g_mano != mano)
            continue;
        if (k <= 0 || k >= MAX_DISPOSITIVOS || !p[k].valida || p[k].resultado != 200) {
            g_m[mano].agarrado = 0;
            continue;
        }
        y = p[k].m[1][3];
        vy = p[k].vel[1];
        if (g_boton < 0)
            ag = g_armado >= g_gracia;
        else
            ag = agarrando(sys, mano);
        if (ag && !g_m[mano].agarrado) {
            g_m[mano].agarrado = 1;
            g_m[mano].y0 = y;
            g_m[mano].bajo = 0.0f;
            g_m[mano].vel_max = 0.0f;
        } else if (!ag && g_m[mano].agarrado) {
            g_m[mano].agarrado = 0;
            if (g_m[mano].bajo > g_paso)
                logf_("%s con la mano %s: bajo %.2f m, %.2f m/s como mucho, %d toques", g_boton < 0 ? "ventana" : "agarre",
                      mano ? "derecha" : "izquierda", g_m[mano].bajo, g_m[mano].vel_max, mano == g_mano_activa ? g_hechos : 0);
            if (g_boton >= 0 && mano == g_mano_activa)
                reiniciar();
        }
        if (!g_m[mano].agarrado || (g_mano_activa >= 0 && g_mano_activa != mano))
            continue;
        if (g_m[mano].y0 < y)
            g_m[mano].y0 = y;       /* desde lo más alto */
        if (-vy > g_m[mano].vel_max)
            g_m[mano].vel_max = -vy;
        if (-vy < g_velocidad) {
            /* Quieta o subiendo: lo que bajó despacio no cuenta. */
            if (g_mano_activa < 0 && g_m[mano].y0 - y < g_paso)
                g_m[mano].y0 = y > g_m[mano].y0 - g_paso ? g_m[mano].y0 : y;
            continue;
        }
        g_m[mano].bajo = g_m[mano].y0 - y;
        {
            int debidos = (int)(g_m[mano].bajo / g_paso);
            if (debidos > g_max_toques)
                debidos = g_max_toques;
            if (debidos > g_hechos + g_pendientes) {
                if (g_mano_activa < 0)
                    logf_("palanca: bajando con la mano %s", mano ? "derecha" : "izquierda");
                g_mano_activa = mano;
                g_pendientes = debidos - g_hechos;
                if (g_boton < 0)
                    g_armado = g_gracia;    /* sigue armada mientras bajes */
            }
        }
    }
}

/* --- .ini --- */
static float leer_float(const char *clave, float def, float min, float max)
{
    char buf[32], dd[32];
    float v;
    snprintf(dd, sizeof dd, "%g", def);
    GetPrivateProfileStringA("Palanca", clave, dd, buf, sizeof buf, g_host->ini);
    v = (float)atof(buf);
    return v >= min && v <= max ? v : def;
}

static void f_config(void)
{
    const char *ruta = g_host->ini;
    char buf[32];
    int accion_antes = g_accion;
    unsigned tecla_antes = g_tecla_ini;
    g_activado = GetPrivateProfileIntA("Palanca", "Activado", 1, ruta);
    g_accion = GetPrivateProfileIntA("Palanca", "Accion", 12, ruta);
    GetPrivateProfileStringA("Palanca", "Tecla", "0", buf, sizeof buf, ruta);
    g_tecla_ini = (unsigned)strtoul(buf, NULL, 16) & 0xff;
    g_mano = GetPrivateProfileIntA("Palanca", "Mano", 2, ruta);
    if (g_mano < 0 || g_mano > 2)
        g_mano = 2;
    g_boton = GetPrivateProfileIntA("Palanca", "Boton", -1, ruta);
    g_paso = leer_float("Paso", 0.03f, 0.01f, 0.5f);
    g_velocidad = leer_float("Velocidad", 0.25f, 0.0f, 5.0f);
    g_max_toques = GetPrivateProfileIntA("Palanca", "MaxToques", 8, ruta);
    g_entre = leer_float("Entre", 0.06f, 0.02f, 1.0f);
    g_ventana = leer_float("Ventana", 8.0f, 0.5f, 60.0f);
    g_gracia = leer_float("Gracia", 0.8f, 0.0f, 5.0f);
    g_mantener = leer_float("Mantener", 0.06f, 0.02f, 3.0f);
    g_descubrir = GetPrivateProfileIntA("Palanca", "Descubrir", 1, ruta);
    g_vibrar = GetPrivateProfileIntA("Palanca", "Vibrar", 1, ruta);
    g_log = GetPrivateProfileIntA("Palanca", "Log", 1, ruta);
    if (g_accion != accion_antes || g_tecla_ini != tecla_antes) {
        if (g_toque > 0.0f) {
            tecla(0);
            g_toque = 0.0f;
        }
        g_tecla = 0;
        g_t_tecla = 10.0f;
    }
    logf_("ini: Activado=%d Accion=%d Tecla=%02x Mano=%d Boton=%d Paso=%.2f Velocidad=%.2f MaxToques=%d Ventana=%.1f Gracia=%.1f Mantener=%.2f Entre=%.2f Descubrir=%d",
          g_activado, g_accion, g_tecla_ini, g_mano, g_boton, g_paso, g_velocidad, g_max_toques, g_ventana, g_gracia,
          g_mantener, g_entre, g_descubrir);
}

static void soltar(void)
{
    if (g_toque > 0.0f)
        tecla(0);
    g_toque = 0.0f;
    g_pendientes = 0;
    vibrar_cortar();
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
    if (pausado)
        soltar();
}

static void f_terminar(void)
{
    soltar();
    g_host = NULL;
}

__declspec(dllexport) int mod_iniciar(const Anfitrion *a, Feature *f)
{
    if (!a || a->version != ANFITRION_VERSION || a->tam < (int)sizeof(Anfitrion))
        return ANFITRION_VERSION;
    g_host = a;
    g_delta = a->delta;
    logf_(VERSION " (recarga %d)", a->recarga);
    f_config();
    f->tam = sizeof *f;
    f->prioridad = 67;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
