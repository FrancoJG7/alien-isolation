/* bengala.dll (en mods_vr): prender la bengala raspándola con la otra mano.
 *
 * Con la bengala en la mano de Ripley, pasás la otra mano rápido por arriba
 * del mando que la tiene (más cerca que Radio y más rápido que Velocidad,
 * contando la velocidad de una mano respecto de la otra) y el mod aprieta
 * la acción que la prende (la del botón B del mando). Despacio no prende.
 *
 * Cómo aprieta: igual que usar.dll, con la tecla de teclado que tenga esa
 * acción en el mapa del juego (SendInput), o la Tecla del .ini.
 *
 * Qué falta saber (Descubrir=1 lo anota en ai_mods.log):
 * - el número de la acción que prende la bengala: con Descubrir=1, cada vez
 *   que el juego ve una acción recién apretada la anota; apretás B con la
 *   bengala en la mano y ahí está;
 * - qué valor da "objeto en la mano" con la bengala ([[pj+0x31c]+0x3c]+0x3bc8,
 *   el mismo que lee dedos.dll), para no apretar nada con otra cosa. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"
#include "tecla_virtual.h"

#ifndef VERSION
#define VERSION "bengala v0.1.0"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define OFS_CONTROL         0x64
#define OFS_ESTADO_31C      0x31c
#define SLOT_POSES          11
#define MAX_DISPOSITIVOS    16
#define MAX_ACCIONES        64

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
static int g_accion = 0;            /* 0 = todavía no se sabe: no aprieta */
static unsigned g_tecla_ini;        /* 0 = la del mapa del juego */
static int g_mano = 1;              /* la que tiene la bengala: 0 izquierda, 1 derecha */
static float g_radio = 0.15f;       /* m entre los dos mandos */
static float g_velocidad = 0.8f;    /* m/s de una mano respecto de la otra */
static float g_espera = 1.0f;
static float g_mantener = 0.12f;
static unsigned g_objeto;           /* "objeto en la mano" de la bengala; 0 = cualquiera */
static int g_descubrir = 1;
static int g_vibrar = 1;
static int g_log = 1;

/* Estado */
static unsigned g_tecla;
static float g_t_tecla = 10.0f, g_t_mandos = 10.0f, g_t_espera, g_toque;
static unsigned g_equipado_antes = 0xffffffffu;
static unsigned char g_estado_antes[MAX_ACCIONES];
static float g_pico_vel, g_pico_dist = 9.0f;
static int g_pico_activo;

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
static void descubrir(unsigned eq)
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
            logf_("descubrir: accion %u recien apretada (objeto en la mano %08x)", k, eq);
        g_estado_antes[k] = (unsigned char)st;
    }
}

static void prender(int libre, float vel, float dist)
{
    if (!libre || !juego_con_foco()) {
        logf_("raspon (%.2f m/s a %.2f m), pero %s: no aprieto", vel, dist,
              !libre ? "Ripley no esta en control libre" : "el juego no tiene el foco");
        return;
    }
    tecla(1);
    g_toque = g_mantener;
    g_t_espera = g_espera;
    if (g_vibrar)
        vibrar_pedir(g_mano, 2, 15.0f, 25.0f, 0.6f);
    logf_("raspon (%.2f m/s a %.2f m): prendo la bengala (accion %d)", vel, dist, g_accion);
}

static void f_tick(float dt)
{
    void *sys;
    PoseVR p[MAX_DISPOSITIVOS];
    unsigned char *pj;
    unsigned eq;
    int ih, il, j, libre, con_bengala;
    float d = 0.0f, v = 0.0f;

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
        logf_("objeto en la mano: %08x%s", eq, eq == 0 ? " (nada)" : eq >= 0xfffffffeu ? " (no se pudo leer)" : "");
        g_equipado_antes = eq;
    }
    if (g_descubrir)
        descubrir(eq);

    if (!(sys = g_host->openvr()))
        return;
    vibrar_tick(sys, dt);
    g_t_mandos += dt;
    if (g_t_mandos > 2.0f) {
        g_t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    if (g_accion <= 0)
        return;                     /* todavía no sabemos qué apretar */
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

    con_bengala = eq != 0 && eq < 0xfffffffeu && (!g_objeto || eq == g_objeto);
    ih = vibrar_indice(g_mano);
    il = vibrar_indice(1 - g_mano);
    if (!con_bengala || ih <= 0 || il <= 0 || ih >= MAX_DISPOSITIVOS || il >= MAX_DISPOSITIVOS) {
        g_pico_activo = 0;
        return;
    }
    memset(p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[SLOT_POSES])(sys, 2, 0.0f, p, MAX_DISPOSITIVOS);
    if (!p[ih].valida || !p[il].valida || p[ih].resultado != 200 || p[il].resultado != 200)
        return;
    for (j = 0; j < 3; j++) {
        float x = p[il].m[j][3] - p[ih].m[j][3], w = p[il].vel[j] - p[ih].vel[j];
        d += x * x;
        v += w * w;
    }
    d = sqrtf(d);
    v = sqrtf(v);
    libre = ripley_libre(pj);

    /* Log de cada pasada cerca, para ajustar Radio y Velocidad. */
    if (d < g_radio * 1.5f && v > g_velocidad * 0.5f) {
        g_pico_activo = 1;
        if (v > g_pico_vel)
            g_pico_vel = v;
        if (d < g_pico_dist)
            g_pico_dist = d;
    } else if (g_pico_activo) {
        g_pico_activo = 0;
        logf_("pasada de la mano: %.2f m/s, %.2f m del otro mando", g_pico_vel, g_pico_dist);
        g_pico_vel = 0.0f;
        g_pico_dist = 9.0f;
    }
    if (g_t_espera <= 0.0f && g_toque <= 0.0f && d < g_radio && v > g_velocidad)
        prender(libre, v, d);
}

/* --- .ini --- */
static float leer_float(const char *clave, float def, float min, float max)
{
    char buf[32], dd[32];
    float v;
    snprintf(dd, sizeof dd, "%g", def);
    GetPrivateProfileStringA("Bengala", clave, dd, buf, sizeof buf, g_host->ini);
    v = (float)atof(buf);
    return v >= min && v <= max ? v : def;
}

static void f_config(void)
{
    const char *ruta = g_host->ini;
    char buf[32];
    int accion_antes = g_accion;
    unsigned tecla_antes = g_tecla_ini;
    g_activado = GetPrivateProfileIntA("Bengala", "Activado", 1, ruta);
    g_accion = GetPrivateProfileIntA("Bengala", "Accion", 0, ruta);
    GetPrivateProfileStringA("Bengala", "Tecla", "0", buf, sizeof buf, ruta);
    g_tecla_ini = (unsigned)strtoul(buf, NULL, 16) & 0xff;
    g_mano = GetPrivateProfileIntA("Bengala", "Mano", 1, ruta) ? 1 : 0;
    g_radio = leer_float("Radio", 0.15f, 0.03f, 0.6f);
    g_velocidad = leer_float("Velocidad", 0.8f, 0.1f, 5.0f);
    g_espera = leer_float("Espera", 1.0f, 0.1f, 10.0f);
    g_mantener = leer_float("Mantener", 0.12f, 0.03f, 3.0f);
    GetPrivateProfileStringA("Bengala", "Objeto", "0", buf, sizeof buf, ruta);
    g_objeto = (unsigned)strtoul(buf, NULL, 16);
    g_descubrir = GetPrivateProfileIntA("Bengala", "Descubrir", 1, ruta);
    g_vibrar = GetPrivateProfileIntA("Bengala", "Vibrar", 1, ruta);
    g_log = GetPrivateProfileIntA("Bengala", "Log", 1, ruta);
    if (g_accion != accion_antes || g_tecla_ini != tecla_antes) {
        if (g_toque > 0.0f) {
            tecla(0);
            g_toque = 0.0f;
        }
        g_tecla = 0;
        g_t_tecla = 10.0f;
    }
    logf_("ini: Activado=%d Accion=%d Tecla=%02x Mano=%s Radio=%.2f Velocidad=%.2f Espera=%.1f Mantener=%.2f Objeto=%08x Descubrir=%d",
          g_activado, g_accion, g_tecla_ini, g_mano ? "derecha" : "izquierda", g_radio, g_velocidad, g_espera,
          g_mantener, g_objeto, g_descubrir);
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
    f->prioridad = 66;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
