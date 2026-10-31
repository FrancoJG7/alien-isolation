/* usar.dll (en mods_vr): usar con la palma.
 *
 * Estirás el brazo hacia adelante, rápido y con la mano por delante de la
 * cara, como empujando una puerta o apretando un botón, y Ripley "usa"
 * (acción 12 del mapa de juego, la E; la 17 resultó ser la R). El juego solo hace algo si te está
 * mostrando la indicación de usar, así que empujar al aire no hace nada.
 *
 * La tecla se aprieta con SendInput (como pausa.dll con Esc), con el código
 * de teclado que tenga la acción en el mapa del juego. No usamos la tecla
 * virtual porque mapa.dll ya engancha esos mismos dispositivos y dos features
 * no pueden enganchar lo mismo. Por eso solo aprieta con la ventana del
 * juego al frente.
 *
 * Buscador (Buscar=1, diagnóstico): para que más adelante haga falta tocar
 * el objeto de verdad, hay que saber dónde guarda el juego "el objeto que
 * tenés delante". Mira cada 0.1 s los punteros de Ripley (pj+0..0x4000) y de
 * su control ([pj+0x64]+0..0x400) y anota los que pasan de nada a algo o de
 * algo a nada, con la distancia del objeto a Ripley si tiene una posición
 * cerca. Los que cambian solos todo el tiempo se ignoran. Cuando apretás
 * usar (con el botón o con el gesto) anota cuáles cambiaron hace poco.
 *
 * Vibrar=1 (v0.1.4): vibra solo si el empujón usó algo. Al usar, Ripley
 * guarda la interacción en curso en pj+0x3570 o +0x3574 (lo encontró el
 * buscador: aparece 0.1 a 0.2 s después y se va al terminar). Si en
 * CONFIRMAR segundos aparece un objeto nuevo entre pj+0x3560 y +0x3590,
 * vibra; si no, fue al aire. Vibrar=2 vibra siempre, como antes.
 *
 * EnLocker=0 (v0.1.8): escondida en el locker (o en un ducto: pj+0x2b4,
 * forzada a agacharse, la misma bandera que usan agachar y palanca) el
 * empujón no aprieta usar. Ahí usar te saca del locker, y al inclinarte
 * hacia adelante las manos van hacia adelante con vos y se disparaba solo. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"
#include "tecla_virtual.h"
#include "usar_gesto.h"

#ifndef VERSION
#define VERSION "usar v0.1.9"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define OFS_CONTROL         0x64
#define OFS_POSICION        0x70
#define OFS_FORZADO         0x2b4         /* forzada a agacharse: locker, ducto */
#define OFS_ESCONDITE       0x3718        /* el locker en el que estás (0 = afuera) */
#define SLOT_POSES          11
#define MAX_DISPOSITIVOS    16

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
static float g_mantener = 0.12f;   /* s con la tecla abajo */
static int g_mano = 2;              /* 0 izquierda, 1 derecha, 2 cualquiera */
static GestoUsar g_gesto = { 1.0f, 0.12f, 50.0f, 0.6f };
static int g_vibrar = 1;
static int g_en_locker = 0;         /* 1 = el empujon tambien aprieta en el locker */
static int g_log = 1, g_log_gesto = 1, g_buscar = 1;

/* Estado */
static EstadoUsar g_est[2];
static unsigned g_tecla;            /* código DIK que apretamos */
static float g_t_tecla = 10.0f, g_t_mandos = 10.0f;
static float g_toque;               /* > 0: tecla abajo */
static struct { float vel, delante, angulo; int activo, empujo; } g_pico[2];
static int g_usar_antes;
/* Cabeza al empezar cada empujón (para ver cuánto se movió hasta la E). */
static struct { int activo; float pos[3], frente[3]; } g_cab[2];

#define CONF_INI    0x3560
#define CONF_N      12
#define CONFIRMAR   0.8f
static float g_t_confirmar;
static int g_conf_mano;
static unsigned g_conf_antes[CONF_N];

#define logf_(...) do { if (g_log && g_host) g_host->log(g_host, __VA_ARGS__); } while (0)

static void *leer_ptr(unsigned dir)
{
    void **p = (void **)(UINT_PTR)dir;
    if (!dir || IsBadReadPtr(p, 4))
        return NULL;
    return *p;
}

static unsigned char *ripley(void)
{
    unsigned m = (unsigned)(UINT_PTR)leer_ptr(DIR_MANAGER_PJ + g_delta);
    unsigned lista;
    unsigned char *pj;
    if (!m)
        return NULL;
    m = (unsigned)(UINT_PTR)leer_ptr(m + 8);
    if (!m || IsBadReadPtr((void *)(UINT_PTR)m, 0x88) || *(unsigned *)(UINT_PTR)(m + 0x84) == 0)
        return NULL;
    lista = (unsigned)(UINT_PTR)leer_ptr(m + 0x80);
    pj = lista ? (unsigned char *)leer_ptr(lista) : NULL;
    return pj && !IsBadReadPtr(pj, 0x400) ? pj : NULL;
}

/* 1 si Ripley está en control libre (no en menús ni cinemáticas). */
static int ripley_libre(void)
{
    unsigned char *pj = ripley(), *ctl;
    if (!pj)
        return 0;
    ctl = (unsigned char *)leer_ptr((unsigned)(UINT_PTR)(pj + OFS_CONTROL));
    return ctl && !IsBadReadPtr(ctl + 0x1f, 1) && ctl[0x1f];
}

/* 1 si Ripley está escondida en un locker o en un ducto. */
static int en_locker(void)
{
    unsigned char *pj = ripley();
    if (!pj || IsBadReadPtr(pj + OFS_FORZADO, 1) || IsBadReadPtr(pj + OFS_ESCONDITE, 4))
        return 0;
    /* Los lockers de pie no ponen +0x2b4 (solo ductos y agachada a la
     * fuerza); +0x3718 tiene el locker desde que entrás hasta que salís. */
    return pj[OFS_FORZADO] || *(unsigned *)(pj + OFS_ESCONDITE) != 0;
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

/* La tecla de teclado (tipo 1) de la acción en el mapa del juego. 1 = lista,
 * 0 = todavía no hay mapa, -1 = no tiene. */
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
        logf_("accion %d, tecla %u: codigo %04x tipo %u banderas %02x", g_accion, k, c, tipo, fl);
        if (!g_tecla && tipo == 1 && c != 0xffff && c < 0x100 && !(fl & 0x40))
            g_tecla = c;
    }
    if (!g_tecla)
        return -1;
    logf_("aprieto la tecla %02x (la de la accion %d en el juego)", g_tecla, g_accion);
    return 1;
}

/* ---- Buscador del objeto que tenés delante ---- */

#define PJ_DW       (0x4000 / 4)
#define CTL_DW      (0x400 / 4)
#define N_DW        (PJ_DW + CTL_DW)
#define CALENTAR    5.0f            /* s mirando sin anotar, para saber qué cambia solo */
#define VENTANA     20.0f
#define MAX_CAMBIOS 4               /* más cambios por ventana = ruido */

static unsigned g_antes[N_DW];
static float g_t_cambio[N_DW];      /* s desde el último cambio */
static unsigned char g_cambios[N_DW], g_mudo[N_DW];
static int g_hay_antes;
static float g_t_buscar, g_t_ventana, g_t_calentar;
static unsigned char *g_pj_antes;

/* Un objeto de C++ del juego: se puede leer y su primer campo es una vtable
 * dentro de AI.exe. Sin esto, los floats (0x3f...) pasan por punteros. */
static int parece_objeto(unsigned v)
{
    static unsigned ini, fin;
    unsigned vt;
    if (!ini) {
        unsigned char *b = (unsigned char *)GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
        ini = (unsigned)(UINT_PTR)b;
        fin = ini + nt->OptionalHeader.SizeOfImage;
    }
    if (v < 0x10000 || (v & 3) || IsBadReadPtr((void *)(UINT_PTR)v, 0x200))
        return 0;
    vt = *(unsigned *)(UINT_PTR)v;
    return vt >= ini && vt < fin && !(vt & 3);
}

static const char *nombre_slot(int i, char *buf)
{
    if (i < PJ_DW)
        sprintf(buf, "pj+%x", i * 4);
    else
        sprintf(buf, "ctl+%x", (i - PJ_DW) * 4);
    return buf;
}

/* Busca en el objeto tres floats que parezcan una posición a menos de 5 m de
 * Ripley (y no la de Ripley misma). Un nivel de punteros más adentro también. */
static int posicion_cerca(const unsigned char *o, const float *rp, int nivel, char *out, int tam)
{
    int k, len = 0, hallados = 0;
    for (k = 0; k + 12 <= 0x200 && hallados < 2; k += 4) {
        const float *f = (const float *)(o + k);
        float dx = f[0] - rp[0], dy = f[1] - rp[1], dz = f[2] - rp[2], d;
        if (!(f[0] == f[0]) || !(f[1] == f[1]) || !(f[2] == f[2]))
            continue;
        if (fabsf(f[0]) < 0.5f && fabsf(f[1]) < 0.5f && fabsf(f[2]) < 0.5f)
            continue;               /* ceros y vectores unitarios */
        d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d > 0.05f && d < 5.0f) {
            len += snprintf(out + len, tam - len, " pos +%x a %.2f m", k, d);
            hallados++;
            k += 8;
        }
    }
    if (!hallados && nivel == 0)
        for (k = 0; k < 0x100 && !hallados; k += 4) {
            unsigned v = *(const unsigned *)(o + k);
            char sub[80];
            if (parece_objeto(v) && posicion_cerca((const unsigned char *)(UINT_PTR)v, rp, 1, sub, sizeof sub)) {
                len += snprintf(out + len, tam - len, " [+%x]%s", k, sub);
                hallados++;
            }
        }
    return hallados;
}

static void leer_slots(unsigned char *pj, unsigned *v)
{
    unsigned char *ctl = (unsigned char *)leer_ptr((unsigned)(UINT_PTR)(pj + OFS_CONTROL));
    memset(v, 0, N_DW * 4);
    if (!IsBadReadPtr(pj, 0x4000))
        memcpy(v, pj, 0x4000);
    if (ctl && !IsBadReadPtr(ctl, 0x400))
        memcpy(v + PJ_DW, ctl, 0x400);
}

static void buscar(float dt)
{
    static unsigned ahora[N_DW];
    unsigned char *pj;
    float rp[3];
    int i;
    for (i = 0; i < N_DW; i++)
        g_t_cambio[i] += dt;
    g_t_buscar += dt;
    if (g_t_buscar < 0.1f)
        return;
    dt = g_t_buscar;
    g_t_buscar = 0.0f;
    if (!(pj = ripley()))
        return;
    if (pj != g_pj_antes) {         /* otra Ripley (carga de nivel): empezar de nuevo */
        g_pj_antes = pj;
        g_hay_antes = 0;
        g_t_calentar = 0.0f;
        memset(g_mudo, 0, sizeof g_mudo);
        memset(g_cambios, 0, sizeof g_cambios);
        logf_("buscador: Ripley en %p, miro %d punteros", pj, N_DW);
    }
    memcpy(rp, pj + OFS_POSICION, sizeof rp);
    leer_slots(pj, ahora);
    if (!g_hay_antes) {
        memcpy(g_antes, ahora, sizeof g_antes);
        g_hay_antes = 1;
        return;
    }
    g_t_ventana += dt;
    if (g_t_ventana > VENTANA) {
        g_t_ventana = 0.0f;
        memset(g_cambios, 0, sizeof g_cambios);
    }
    if (g_t_calentar < CALENTAR)
        g_t_calentar += dt;
    for (i = 0; i < N_DW; i++) {
        unsigned a = g_antes[i], b = ahora[i];
        char nom[16], pos[200];
        if (a == b)
            continue;
        g_antes[i] = b;
        if (!(a == 0 ? parece_objeto(b) : b == 0 ? parece_objeto(a) : parece_objeto(a) && parece_objeto(b)))
            continue;               /* solo punteros a objetos que aparecen, se van o cambian */
        g_t_cambio[i] = 0.0f;
        if (g_mudo[i])
            continue;
        if (g_cambios[i] < 255)
            g_cambios[i]++;
        if (g_t_calentar < CALENTAR || g_cambios[i] > MAX_CAMBIOS) {
            if (g_t_calentar >= CALENTAR)
                logf_("buscador: %s cambia mucho, lo ignoro", nombre_slot(i, nom));
            g_mudo[i] = 1;
            continue;
        }
        pos[0] = 0;
        if (b)
            posicion_cerca((const unsigned char *)(UINT_PTR)b, rp, 0, pos, sizeof pos);
        logf_("buscador: %s %08x -> %08x%s", nombre_slot(i, nom), a, b, pos);
    }
}

/* Al apretar usar: los punteros que cambiaron en los últimos 3 s. */
static void log_al_usar(const char *por)
{
    char linea[600], nom[16];
    int i, len;
    if (!g_buscar || !g_hay_antes)
        return;
    len = snprintf(linea, sizeof linea, "usar (%s); cambiaron hace poco:", por);
    for (i = 0; i < N_DW && len < (int)sizeof linea - 40; i++)
        if (g_t_cambio[i] < 3.0f && !g_mudo[i])
            len += snprintf(linea + len, sizeof linea - len, " %s=%08x (%.1f s)", nombre_slot(i, nom), g_antes[i], g_t_cambio[i]);
    logf_("%s", linea);
}

/* ---- Gesto ---- */

static void log_cabeza(int mano, const float v[3][4])
{
    float d = 0.0f, c = 0.0f;
    int j;
    if (!g_cab[mano].activo)
        return;
    for (j = 0; j < 3; j++) {
        float x = v[j][3] - g_cab[mano].pos[j];
        d += x * x;
        c += -v[j][2] * g_cab[mano].frente[j];
    }
    if (c > 1.0f)
        c = 1.0f;
    logf_("  la cabeza se movio %.1f cm y giro %.1f grados desde que empezo el empujon", sqrtf(d) * 100.0f, acosf(c) * 57.29578f);
}

static void empujar(int mano, const float visor[3][4])
{
    int libre = ripley_libre(), foco = juego_con_foco();
    if (!libre || !foco) {
        logf_("empujon con la mano %s, pero %s: no aprieto", mano ? "derecha" : "izquierda",
              !foco ? "el juego no tiene el foco" : "Ripley no esta en control libre");
        return;
    }
    if (!g_en_locker && en_locker()) {
        logf_("empujon con la mano %s, pero Ripley esta en el locker o en un ducto: no aprieto (EnLocker=0)",
              mano ? "derecha" : "izquierda");
        return;
    }
    tecla(1);
    g_toque = g_mantener;
    if (g_vibrar == 2) {
        vibrar_pedir(mano, 1, 20.0f, 0.0f, 0.5f);
    } else if (g_vibrar == 1) {
        unsigned char *pj = ripley();
        if (pj && !IsBadReadPtr(pj + CONF_INI, CONF_N * 4)) {
            memcpy(g_conf_antes, pj + CONF_INI, sizeof g_conf_antes);
            g_conf_mano = mano;
            g_t_confirmar = CONFIRMAR;
        }
    }
    logf_("empujon con la mano %s: aprieto usar", mano ? "derecha" : "izquierda");
    log_cabeza(mano, visor);
    log_al_usar("gesto");
}

/* Vibra si el último empujón empezó una interacción. */
static void confirmar(float dt)
{
    unsigned char *pj;
    int k;
    if (g_t_confirmar <= 0.0f)
        return;
    g_t_confirmar -= dt;
    pj = ripley();
    if (pj && !IsBadReadPtr(pj + CONF_INI, CONF_N * 4))
        for (k = 0; k < CONF_N; k++) {
            unsigned v = ((unsigned *)(pj + CONF_INI))[k];
            if (v != g_conf_antes[k] && parece_objeto(v)) {
                vibrar_pedir(g_conf_mano, 1, 20.0f, 0.0f, 0.5f);
                logf_("el empujon uso algo (pj+%x = %08x)", CONF_INI + k * 4, v);
                g_t_confirmar = 0.0f;
                return;
            }
        }
    if (g_t_confirmar <= 0.0f)
        logf_("el empujon no uso nada (al aire)");
}

/* LogGesto: un renglón por cada movimiento rápido hacia adelante, con lo
 * máximo que llegó, para ajustar Velocidad, Alcance y Angulo. */
static void log_pico(int mano, float f, float d, float a, int empujo)
{
    if (!g_log_gesto)
        return;
    if (f > g_gesto.velocidad * 0.6f) {
        if (!g_pico[mano].activo)
            memset(&g_pico[mano], 0, sizeof g_pico[mano]);
        g_pico[mano].activo = 1;
        if (f > g_pico[mano].vel) {
            g_pico[mano].vel = f;
            g_pico[mano].angulo = a;
        }
        if (d > g_pico[mano].delante)
            g_pico[mano].delante = d;
        g_pico[mano].empujo |= empujo;
    } else if (g_pico[mano].activo) {
        g_pico[mano].activo = 0;
        logf_("movimiento de la mano %s: %.2f m/s hacia adelante, %.2f m delante de los ojos, %.0f grados -> %s",
              mano ? "derecha" : "izquierda", g_pico[mano].vel, g_pico[mano].delante, g_pico[mano].angulo,
              g_pico[mano].empujo ? "empujon" : "no alcanzo");
    }
}

static void f_tick(float dt)
{
    void *sys;
    PoseVR p[MAX_DISPOSITIVOS];
    int mano, st;

    if (g_toque > 0.0f) {
        g_toque -= dt;
        if (g_toque <= 0.0f)
            tecla(0);
    }
    if (g_pausado)
        return;
    if (g_buscar)
        buscar(dt);
    confirmar(dt);
    /* Usar con el botón de verdad (o con nuestra tecla): estado de la acción. */
    st = tv_estado(g_delta, g_accion);
    if (st >= 0) {
        if ((st & 1) && !(g_usar_antes & 1) && g_toque <= 0.0f)
            log_al_usar("boton");
        g_usar_antes = st;
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
            g_tecla = 0xffffffffu;  /* no volver a buscar hasta que cambie el .ini */
        }
        return;
    }
    if (!g_activado || g_tecla == 0xffffffffu)
        return;

    memset(p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[SLOT_POSES])(sys, 2, 0.0f, p, MAX_DISPOSITIVOS);
    if (!p[0].valida)
        return;
    for (mano = 0; mano < 2; mano++) {
        int idx = vibrar_indice(mano), empujo;
        float f, d, a;
        if (g_mano != 2 && g_mano != mano)
            continue;
        /* Mando sin seguimiento: sin gesto. */
        if (idx <= 0 || idx >= MAX_DISPOSITIVOS || !p[idx].valida || p[idx].resultado != 200) {
            memset(&g_est[mano], 0, sizeof g_est[mano]);
            continue;
        }
        usar_medir(p[0].m, p[idx].m, p[idx].vel, &f, &d, &a);
        empujo = usar_paso(&g_est[mano], &g_gesto, f, d, a, dt);
        log_pico(mano, f, d, a, empujo);
        if (f > 0.4f && !g_cab[mano].activo) {
            int j;
            g_cab[mano].activo = 1;
            for (j = 0; j < 3; j++) {
                g_cab[mano].pos[j] = p[0].m[j][3];
                g_cab[mano].frente[j] = -p[0].m[j][2];
            }
        } else if (f < 0.2f) {
            g_cab[mano].activo = 0;
        }
        if (empujo && g_toque <= 0.0f)
            empujar(mano, p[0].m);
    }
}

static float leer_float(const char *clave, const char *def, float min, float max)
{
    char buf[32];
    float v;
    GetPrivateProfileStringA("Usar", clave, def, buf, sizeof buf, g_host->ini);
    v = (float)atof(buf);
    return v >= min && v <= max ? v : (float)atof(def);
}

static void f_config(void)
{
    const char *ruta = g_host->ini;
    char buf[32];
    int accion_antes = g_accion;
    unsigned tecla_antes = g_tecla_ini;
    g_activado = GetPrivateProfileIntA("Usar", "Activado", 1, ruta);
    g_accion = GetPrivateProfileIntA("Usar", "Accion", 12, ruta);
    g_mano = GetPrivateProfileIntA("Usar", "Mano", 2, ruta);
    if (g_mano < 0 || g_mano > 2)
        g_mano = 2;
    g_mantener = leer_float("Mantener", "0.12", 0.03f, 3.0f);
    GetPrivateProfileStringA("Usar", "Tecla", "0", buf, sizeof buf, ruta);
    g_tecla_ini = (unsigned)strtoul(buf, NULL, 16) & 0xff;
    g_gesto.velocidad = leer_float("Velocidad", "1.0", 0.2f, 5.0f);
    g_gesto.alcance = leer_float("Alcance", "0.12", 0.0f, 0.8f);
    g_gesto.angulo = leer_float("Angulo", "50", 5.0f, 90.0f);
    g_gesto.espera = leer_float("Espera", "0.6", 0.1f, 5.0f);
    g_vibrar = GetPrivateProfileIntA("Usar", "Vibrar", 1, ruta);
    g_en_locker = GetPrivateProfileIntA("Usar", "EnLocker", 0, ruta);
    g_log = GetPrivateProfileIntA("Usar", "Log", 1, ruta);
    g_log_gesto = GetPrivateProfileIntA("Usar", "LogGesto", 1, ruta);
    g_buscar = GetPrivateProfileIntA("Usar", "Buscar", 1, ruta);
    if (g_accion != accion_antes || g_tecla_ini != tecla_antes) {
        if (g_toque > 0.0f) {
            tecla(0);
            g_toque = 0.0f;
        }
        g_tecla = 0;                /* volver a buscar la tecla */
        g_t_tecla = 10.0f;
    }
    logf_("ini: Activado=%d Accion=%d Tecla=%02x Mano=%d Velocidad=%.2f Alcance=%.2f Angulo=%.0f Espera=%.2f Mantener=%.2f EnLocker=%d Buscar=%d LogGesto=%d",
          g_activado, g_accion, g_tecla_ini, g_mano, g_gesto.velocidad, g_gesto.alcance, g_gesto.angulo,
          g_gesto.espera, g_mantener, g_en_locker, g_buscar, g_log_gesto);
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
    f->prioridad = 65;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
