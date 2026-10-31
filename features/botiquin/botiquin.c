/* botiquin.dll (en mods_vr): curarte llevándote el botiquín al cuello.
 *
 * Con el botiquín en la mano de Ripley, llevás cualquiera de los dos mandos
 * al cuello (debajo del visor, entre Abajo y AbajoMax metros, y a menos de
 * Radio metros de él de costado) y lo dejás ahí Quieto segundos: el mod
 * aprieta la acción que usa el botiquín (la del botón B del mando, 16, la
 * misma que prende la bengala), como si fuera una inyección.
 *
 * Cómo aprieta: igual que bengala.dll, con la tecla de teclado que tenga esa
 * acción en el mapa del juego (SendInput), o la Tecla del .ini.
 *
 * Cómo sabe que lo que tenés en la mano es el botiquín: "objeto en la mano"
 * ([[pj+0x31c]+0x3c]+0x3bc8) tiene que ser Objeto. Ese valor puede cambiar
 * de una sesión del juego a otra (parece un puntero), así que además lo
 * aprende solo (Aprender=1): si apretás B con algo en la mano y en los
 * segundos siguientes la vida de Ripley sube, eso era el botiquín. Cuando
 * cambia el objeto en la mano, anota sus primeros bytes (Volcar=1) para
 * encontrar más adelante un número que no cambie.
 *
 * Con la vida llena no aprieta (SoloSiFalta=1), para no gastar el botiquín. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"
#include "tecla_virtual.h"

#ifndef VERSION
#define VERSION "botiquin v0.1.0"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define OFS_CONTROL         0x64
#define OFS_ESTADO_31C      0x31c
#define OFS_VIDA            0x33c   /* ushort actual, +2 ushort máximo, +4 float multiplicador */
#define SLOT_POSES          11
#define MAX_DISPOSITIVOS    16
#define VISOR               0       /* índice de OpenVR del visor */

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
static int g_accion = 16;           /* recargar: la del botón B, que usa el botiquín */
static unsigned g_tecla_ini;        /* 0 = la del mapa del juego */
static int g_mano = -1;             /* -1 cualquiera, 0 izquierda, 1 derecha */
static float g_abajo = 0.05f;       /* m debajo del visor, mínimo */
static float g_abajo_max = 0.35f;   /* m debajo del visor, máximo */
static float g_radio = 0.20f;       /* m de costado (horizontal) desde el visor */
static float g_quieto = 0.20f;      /* s con la mano en el cuello */
static float g_espera = 3.0f;
static float g_mantener = 0.15f;
static unsigned g_objeto_ini;       /* 0 = todavía no se sabe */
static int g_aprender = 1;
static float g_ventana = 8.0f;      /* s entre la B y la subida de la vida para aprender */
static int g_solo_si_falta = 1;
static int g_volcar = 1;
static int g_vibrar = 1;
static int g_log = 1;

/* Estado */
static unsigned g_tecla;
static float g_t_tecla = 10.0f, g_t_mandos = 10.0f, g_t_espera, g_toque, g_t_cuello;
static unsigned g_equipado_antes = 0xffffffffu, g_objeto_aprendido;
static int g_estado_b_antes, g_en_cuello, g_salio = 1, g_avise_sin_objeto;
static float g_t_desde_b = 99.0f;
static unsigned g_objeto_b;         /* objeto en la mano cuando se apretó B */
static int g_vida_antes = -1;
static float g_min_abajo, g_min_radio = 9.0f;  /* lo más cerca del cuello en esta pasada */

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

static unsigned equipado(unsigned char *pj)
{
    unsigned char *o = pj ? leer_ptr(pj + OFS_ESTADO_31C) : NULL, *g;
    if (!o || IsBadReadPtr(o + 0x3c, 4))
        return 0xfffffffeu;
    g = leer_ptr(o + 0x3c);         /* gestor de equipo: [[pj+0x31c]+0x3c] */
    if (!g || IsBadReadPtr(g + 0x3bc8, 4))
        return 0xfffffffeu;
    return *(unsigned *)(g + 0x3bc8);
}

/* Vida actual y si está llena (-1 si no se pudo leer). */
static int vida(unsigned char *pj, int *llena)
{
    unsigned short act, max;
    float mult;
    if (!pj || IsBadReadPtr(pj + OFS_VIDA, 8))
        return -1;
    act = *(unsigned short *)(pj + OFS_VIDA);
    max = *(unsigned short *)(pj + OFS_VIDA + 2);
    mult = *(float *)(pj + OFS_VIDA + 4);
    if (mult <= 0.0f || mult > 100.0f)
        mult = 1.0f;
    *llena = max > 0 && act + 0.5f >= max * mult;
    return act;
}

/* Los primeros bytes del objeto en la mano, si es un puntero que se lee. */
static void volcar(unsigned eq)
{
    const unsigned *o = (const unsigned *)(UINT_PTR)eq;
    if (eq < 0x10000u || eq >= 0xfffffffeu || IsBadReadPtr(o, 0x40))
        return;
    logf_("objeto %08x por dentro: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
          eq, o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], o[8], o[9], o[10], o[11], o[12], o[13], o[14], o[15]);
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

/* La tecla de teclado de la acción (como bengala.dll). 1 = lista, 0 =
 * todavía no hay mapa, -1 = no tiene. */
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
    if ((unsigned)g_accion >= *(unsigned *)(m + 0x1054) || g_accion >= 64)
        return -1;
    comb = *(unsigned char **)(m + 4 + g_accion * 0x40 + 0x30);
    n = *(unsigned *)(m + 4 + g_accion * 0x40 + 0x34);
    if (!comb || n > 16 || IsBadReadPtr(comb, n * 8))
        return -1;
    g_tecla = 0;
    for (k = 0; k < n; k++) {
        unsigned c = *(unsigned short *)(comb + k * 8 + 4), tipo = comb[k * 8 + 6], fl = comb[k * 8 + 7];
        if (!g_tecla && tipo == 1 && c != 0xffff && c < 0x100 && !(fl & 0x40))
            g_tecla = c;
    }
    if (!g_tecla)
        return -1;
    logf_("aprieto la tecla %02x (la de la accion %d en el juego)", g_tecla, g_accion);
    return 1;
}

static unsigned objeto(void)
{
    return g_objeto_aprendido ? g_objeto_aprendido : g_objeto_ini;
}

/* Aprender: B con algo en la mano y después la vida sube = era el botiquín. */
static void aprender(unsigned char *pj, unsigned eq, float dt)
{
    int st = tv_estado(g_delta, g_accion), llena, v;
    if (st >= 0) {
        if ((st & 1) && !(g_estado_b_antes & 1)) {
            g_t_desde_b = 0.0f;
            g_objeto_b = eq;
        }
        g_estado_b_antes = st;
    }
    g_t_desde_b += dt;
    v = vida(pj, &llena);
    if (v < 0)
        return;
    if (g_vida_antes >= 0 && v != g_vida_antes)
        logf_("vida: %d -> %d", g_vida_antes, v);
    if (g_vida_antes >= 0 && v > g_vida_antes && g_t_desde_b < g_ventana) {
        if (g_aprender && g_objeto_b != 0 && g_objeto_b < 0xfffffffeu && g_objeto_b != objeto()) {
            g_objeto_aprendido = g_objeto_b;
            logf_("aprendido: el botiquin es el objeto %08x (B %.1f s antes de curarte). Para el .ini: Objeto=%08x",
                  g_objeto_b, g_t_desde_b, g_objeto_b);
        }
        g_t_desde_b = 99.0f;
    }
    g_vida_antes = v;
}

static void inyectar(unsigned char *pj, int mano)
{
    int llena = 0;
    if (!ripley_libre(pj) || !juego_con_foco()) {
        logf_("mano %s en el cuello, pero %s: no aprieto", mano ? "derecha" : "izquierda",
              !ripley_libre(pj) ? "Ripley no esta en control libre" : "el juego no tiene el foco");
        return;
    }
    if (g_solo_si_falta && vida(pj, &llena) >= 0 && llena) {
        logf_("mano %s en el cuello, pero la vida esta llena: no aprieto", mano ? "derecha" : "izquierda");
        return;
    }
    tecla(1);
    g_toque = g_mantener;
    g_t_espera = g_espera;
    if (g_vibrar)
        vibrar_pedir(mano, 3, 20.0f, 60.0f, 0.7f);
    logf_("mano %s en el cuello (%.2f m abajo, %.2f m de costado): uso el botiquin (accion %d)",
          mano ? "derecha" : "izquierda", g_min_abajo, g_min_radio, g_accion);
}

static void f_tick(float dt)
{
    void *sys;
    PoseVR p[MAX_DISPOSITIVOS];
    unsigned char *pj;
    unsigned eq;
    int mano, cerca = 0, cerca_suelto = 0, mano_cerca = -1, con_botiquin;

    if (dt > 0.1f)
        dt = 0.1f;
    if (g_toque > 0.0f) {
        g_toque -= dt;
        if (g_toque <= 0.0f)
            tecla(0);
    }
    if (g_t_espera > 0.0f)
        g_t_espera -= dt;
    if (g_pausado || !g_activado)
        return;

    pj = jugador();
    eq = equipado(pj);
    if (eq != g_equipado_antes) {
        logf_("objeto en la mano: %08x%s%s", eq, eq == 0 ? " (nada)" : eq >= 0xfffffffeu ? " (no se pudo leer)" : "",
              eq && eq == objeto() ? " (el botiquin)" : "");
        if (g_volcar)
            volcar(eq);
        g_equipado_antes = eq;
        g_avise_sin_objeto = 0;
    }
    aprender(pj, eq, dt);

    if (!(sys = g_host->openvr()))
        return;
    vibrar_tick(sys, dt);
    g_t_mandos += dt;
    if (g_t_mandos > 2.0f) {
        g_t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    if (g_accion <= 0)
        return;
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

    con_botiquin = eq != 0 && eq < 0xfffffffeu && objeto() && eq == objeto();
    if (!objeto() && eq != 0 && eq < 0xfffffffeu && !g_avise_sin_objeto) {
        g_avise_sin_objeto = 1;
        logf_("todavia no se que objeto es el botiquin: usalo una vez con B (si la vida sube lo aprendo) o pone Objeto= en el .ini");
    }
    if (!con_botiquin) {
        g_en_cuello = 0;
        g_min_radio = 9.0f;
        g_t_cuello = 0.0f;
        return;
    }

    memset(p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[SLOT_POSES])(sys, 2, 0.0f, p, MAX_DISPOSITIVOS);
    if (!p[VISOR].valida || p[VISOR].resultado != 200)
        return;
    for (mano = 0; mano < 2; mano++) {
        int i = vibrar_indice(mano);
        float dx, dz, abajo, costado;
        if ((g_mano >= 0 && mano != g_mano) || i <= 0 || i >= MAX_DISPOSITIVOS || !p[i].valida || p[i].resultado != 200)
            continue;
        dx = p[i].m[0][3] - p[VISOR].m[0][3];
        dz = p[i].m[2][3] - p[VISOR].m[2][3];
        abajo = p[VISOR].m[1][3] - p[i].m[1][3];
        costado = sqrtf(dx * dx + dz * dz);
        if (costado < g_radio * 1.5f && abajo > g_abajo - 0.1f && abajo < g_abajo_max * 1.5f) {
            cerca_suelto = 1;
            if (costado < g_min_radio) {
                g_min_radio = costado;
                g_min_abajo = abajo;
            }
        }
        if (costado < g_radio && abajo > g_abajo && abajo < g_abajo_max) {
            cerca = 1;
            mano_cerca = mano;
        }
    }
    /* Log de cada pasada cerca del cuello, para ajustar la zona. */
    if (cerca_suelto) {
        g_en_cuello = 1;
    } else if (g_en_cuello) {
        logf_("pasada cerca del cuello: %.2f m abajo del visor, %.2f m de costado", g_min_abajo, g_min_radio);
        g_en_cuello = 0;
        g_min_radio = 9.0f;
    }
    if (!cerca) {
        g_t_cuello = 0.0f;
        g_salio = 1;
        return;
    }
    g_t_cuello += dt;
    if (g_t_cuello >= g_quieto && g_salio && g_t_espera <= 0.0f && g_toque <= 0.0f) {
        g_salio = 0;                /* hasta que saques la mano del cuello */
        inyectar(pj, mano_cerca);
    }
}

/* --- .ini --- */
static float leer_float(const char *clave, float def, float min, float max)
{
    char buf[32], dd[32];
    float v;
    snprintf(dd, sizeof dd, "%g", def);
    GetPrivateProfileStringA("Botiquin", clave, dd, buf, sizeof buf, g_host->ini);
    v = (float)atof(buf);
    return v >= min && v <= max ? v : def;
}

static void f_config(void)
{
    const char *ruta = g_host->ini;
    char buf[32];
    int accion_antes = g_accion;
    unsigned tecla_antes = g_tecla_ini;
    g_activado = GetPrivateProfileIntA("Botiquin", "Activado", 1, ruta);
    g_accion = GetPrivateProfileIntA("Botiquin", "Accion", 16, ruta);
    GetPrivateProfileStringA("Botiquin", "Tecla", "0", buf, sizeof buf, ruta);
    g_tecla_ini = (unsigned)strtoul(buf, NULL, 16) & 0xff;
    g_mano = GetPrivateProfileIntA("Botiquin", "Mano", -1, ruta);
    if (g_mano > 1 || g_mano < -1)
        g_mano = -1;
    g_abajo = leer_float("Abajo", 0.05f, -0.2f, 0.5f);
    g_abajo_max = leer_float("AbajoMax", 0.35f, 0.05f, 0.8f);
    g_radio = leer_float("Radio", 0.20f, 0.03f, 0.6f);
    g_quieto = leer_float("Quieto", 0.20f, 0.0f, 3.0f);
    g_espera = leer_float("Espera", 3.0f, 0.1f, 30.0f);
    g_mantener = leer_float("Mantener", 0.15f, 0.03f, 5.0f);
    GetPrivateProfileStringA("Botiquin", "Objeto", "0", buf, sizeof buf, ruta);
    g_objeto_ini = (unsigned)strtoul(buf, NULL, 16);
    g_aprender = GetPrivateProfileIntA("Botiquin", "Aprender", 1, ruta);
    g_ventana = leer_float("Ventana", 8.0f, 1.0f, 30.0f);
    g_solo_si_falta = GetPrivateProfileIntA("Botiquin", "SoloSiFalta", 1, ruta);
    g_volcar = GetPrivateProfileIntA("Botiquin", "Volcar", 1, ruta);
    g_vibrar = GetPrivateProfileIntA("Botiquin", "Vibrar", 1, ruta);
    g_log = GetPrivateProfileIntA("Botiquin", "Log", 1, ruta);
    if (g_accion != accion_antes || g_tecla_ini != tecla_antes) {
        if (g_toque > 0.0f) {
            tecla(0);
            g_toque = 0.0f;
        }
        g_tecla = 0;
        g_t_tecla = 10.0f;
    }
    logf_("ini: Activado=%d Accion=%d Tecla=%02x Mano=%d Abajo=%.2f AbajoMax=%.2f Radio=%.2f Quieto=%.2f Espera=%.1f Mantener=%.2f Objeto=%08x Aprender=%d SoloSiFalta=%d (objeto que uso: %08x)",
          g_activado, g_accion, g_tecla_ini, g_mano, g_abajo, g_abajo_max, g_radio, g_quieto, g_espera, g_mantener,
          g_objeto_ini, g_aprender, g_solo_si_falta, objeto());
}

static void soltar(void)
{
    if (g_toque > 0.0f)
        tecla(0);
    g_toque = 0.0f;
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
