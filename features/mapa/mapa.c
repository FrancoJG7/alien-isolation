/* mapa.dll (en mods_vr): mapa en la muñeca.
 *
 * Girás la muñeca izquierda hacia la cara, como para mirar la hora, y se
 * abre el mapa; la bajás y se cierra. El mapa se abre apretando su acción
 * (Tab, la 38 del mapa de juego) con la tecla virtual.
 *
 * El gesto: el mando izquierdo a menos de Distancia del visor y su Eje (por
 * defecto -x: el dorso de la mano izquierda, donde va el reloj) apuntando a
 * los ojos con menos de Angulo grados de diferencia, sostenido Demora
 * segundos. Para soltar hay un margen chico (MargenAngulo grados y 5 cm
 * más), así no parpadea en el borde. Desde la v0.2 se cierra apenas la
 * muñeca empieza a girar rápido para afuera (más de 120 grados por
 * segundo) o pasa el margen durante DemoraCerrar segundos, sin esperar a
 * que termines de bajarla.
 *
 * Modo=0: un toque abre y otro toque cierra (el mapa queda abierto solo).
 *   Para no reabrirlo si lo cerraste a mano, el toque de cerrar se da solo si
 *   Ripley no está en control libre (el menú abierto), si CerrarSoloSiMenu=1.
 * Modo=1: mantiene la tecla mientras mirás (si el juego lo muestra solo
 *   mientras se mantiene).
 *
 * Con el mapa abierto, el stick de la otra mano también mueve el mapa
 * (StickOtraMano):
 * - StickModo=1 (v0.4, analógico): el juego lee los sticks del mando que
 *   arma GRAND con dispositivo->vtable[3](código, invertir) (0x976820 ->
 *   0x976630; códigos 0x12-0x15 = +0x250..+0x25c del objeto stick en +0x18).
 *   Con el mapa abierto, cuando pregunta por el stick izquierdo le
 *   contestamos el derecho si está más inclinado. Es el mismo camino que el
 *   stick izquierdo, así que se mueve igual de suave.
 * - StickModo=2 (v0.3): aprieta las teclas TeclasMover del teclado (todo o
 *   nada, más tosco).
 *
 * Desde la v0.7 la mano tiene que estar casi quieta (más lenta que
 * VelocidadMax) durante la Demora, y después de un empujón (la mano yendo
 * hacia adelante a más de 1 m/s) no abre por Calma segundos: al empujar con
 * la palma (usar.dll) el dorso de la mano izquierda queda mirando a la cara
 * y abría el mapa. Subir la muñeca hacia la cara no cuenta como empujón.
 *
 * Desde la v0.7.2 el reloj además tiene que estar cerca del centro de la
 * vista (MiradaMax grados entre el frente del visor y la dirección
 * visor->mando): hay que mirarlo de verdad. Al mirarse los dedos, el dorso
 * de la mano izquierda queda hacia la cara pero en el borde de la vista, y
 * abría el mapa sin querer. "abro el mapa" anota el ángulo del eje, la
 * distancia, la velocidad de la mano y esos grados.
 *
 * LogEjes=1 anota, con el mando cerca de la cara, los grados de cada eje del
 * mando hacia los ojos: si el gesto no se dispara, ahí se ve cuál eje usar. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"
#include "tecla_virtual.h"
#include "mapa_gesto.h"

#ifndef VERSION
#define VERSION "mapa v0.7.2"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define OFS_CONTROL         0x64
#define SLOT_POSES          11
#define MAX_DISPOSITIVOS    16
#define TOQUE_SEG           0.12f

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
static int g_accion = 38;
static int g_modo;
static int g_cerrar_solo_menu = 1;
static int g_mano;                  /* 0 izquierda, 1 derecha */
static int g_eje, g_signo = -1;
static float g_angulo = 35.0f;
static float g_distancia = 0.45f;
static float g_demora = 0.25f;
static float g_demora_cerrar = 0.03f;
static float g_margen_ang = 8.0f;
static float g_vel_max = 0.6f;      /* m/s: más rápido no es mirar la hora */
static float g_calma = 1.0f;        /* s quieta después de un movimiento rápido */
static float g_t_calma;
static float g_mirada_max = 30.0f;  /* grados del reloj al centro de la vista; 0 = sin límite */
static float g_ult_ang, g_ult_dist, g_ult_rapidez, g_ult_mirada;    /* para el log de abrir */
static int g_aviso_mirada;          /* ya anotamos que este gesto no abre por la mirada */
static int g_vibrar = 1;
static int g_log = 1, g_log_ejes = 1;
static int g_stick = 1;
static float g_zona_muerta = 0.45f;
static unsigned g_teclas_mover[4] = { 0x11, 0x1e, 0x1f, 0x20 };   /* W A S D */
static int g_stick_modo = 1;
/* Ejes del mapa de menús (+0x2998), confirmados con el log de la v0.5:
 * 0x215/0x216 = stick izquierdo x/y, 0x217/0x218 = derecho x/y. */
static unsigned g_ejes[4] = { 0x215, 0x216, 0x217, 0x218 };       /* izq x, izq y, der x, der y */

/* Stick analógico: enganche a vtable[3] del mando de GRAND (tipo 0). */
typedef float (__attribute__((thiscall)) *FnEje)(void *disp, unsigned codigo, unsigned invertir);
static FnEje g_eje_real;
static void *volatile g_pad;
static volatile int g_sumar;        /* 1 = mapa abierto: el derecho cuenta como izquierdo */
static int g_pad_listo;             /* 1 enganchado, -1 no se pudo */
static float g_t_log_stick;

static float __attribute__((thiscall)) mi_eje(void *disp, unsigned codigo, unsigned invertir)
{
    float v = g_eje_real(disp, codigo, invertir), w;
    if (!g_sumar)
        return v;
    if (codigo == g_ejes[0])
        w = g_eje_real(disp, g_ejes[2], invertir);
    else if (codigo == g_ejes[1])
        w = g_eje_real(disp, g_ejes[3], invertir);
    else
        return v;
    return fabsf(w) > fabsf(v) ? w : v;
}

/* Estado */
static int g_tv;                    /* 1 tecla virtual lista, -1 no se pudo */
static float g_t_buscar = 10.0f, g_t_mandos = 10.0f;
static int g_abierto;               /* lo abrimos nosotros */
static float g_t_mira, g_t_deja;
static int g_teclado;               /* 1 = teclado enganchado para las teclas de mover */
static int g_mover_antes;           /* bits de las teclas de mover apretadas */
static float g_ang_antes = -1.0f;   /* para la velocidad de giro de la muñeca */
static float g_toque;               /* > 0: tecla apretada por un toque */
static float g_t_log_ejes;
static float g_t_abierto;           /* segundos desde que lo abrimos */
static int g_libre_no_sirve;        /* el control libre no cambia con el mapa abierto */

#define logf_(...) do { if (g_log && g_host) g_host->log(g_host, __VA_ARGS__); } while (0)

static void *leer_ptr(unsigned dir)
{
    void **p = (void **)(UINT_PTR)dir;
    if (!dir || IsBadReadPtr(p, 4))
        return NULL;
    return *p;
}

/* 1 si Ripley está en control libre (no en menús ni cinemáticas). */
static int ripley_libre(void)
{
    unsigned m = (unsigned)(UINT_PTR)leer_ptr(DIR_MANAGER_PJ + g_delta);
    unsigned lista;
    unsigned char *pj, *ctl;
    if (!m)
        return 0;
    m = (unsigned)(UINT_PTR)leer_ptr(m + 8);
    if (!m || IsBadReadPtr((void *)(UINT_PTR)m, 0x88) || *(unsigned *)(UINT_PTR)(m + 0x84) == 0)
        return 0;
    lista = (unsigned)(UINT_PTR)leer_ptr(m + 0x80);
    pj = lista ? (unsigned char *)leer_ptr(lista) : NULL;
    if (!pj || IsBadReadPtr(pj, 0x400))
        return 0;
    ctl = (unsigned char *)leer_ptr((unsigned)(UINT_PTR)(pj + OFS_CONTROL));
    return ctl && !IsBadReadPtr(ctl + 0x1f, 1) && ctl[0x1f];
}

static void tocar(void)
{
    tv_apretar(1);
    g_toque = TOQUE_SEG;
}

static void abrir(int libre)
{
    g_abierto = 1;
    g_t_abierto = 0.0f;
    if (g_modo == 1)
        tv_apretar(1);
    else
        tocar();
    if (g_vibrar)
        vibrar_pedir(g_mano, 1, 30.0f, 0.0f, 0.6f);
    logf_("abro el mapa (control libre %d): eje a %.0f grados, %.2f m, mano a %.2f m/s, reloj a %.0f grados del centro de la vista",
          libre, g_ult_ang, g_ult_dist, g_ult_rapidez, g_ult_mirada);
}

static void cerrar(int libre, const char *por)
{
    g_abierto = 0;
    if (g_modo == 1) {
        tv_apretar(0);
        logf_("suelto el mapa (%s)", por);
    } else if (!g_cerrar_solo_menu || g_libre_no_sirve || !libre) {
        tocar();
        logf_("cierro el mapa (%s, control libre %d)", por, libre);
    } else {
        logf_("no cierro el mapa (%s): Ripley esta en control libre, parece que ya estaba cerrado", por);
    }
}

/* Con el mapa abierto, el stick de la otra mano aprieta las teclas de mover. */
static void mover_mapa(void *sys)
{
    float x = 0.0f, y = 0.0f;
    int bits = 0, k;
    if (g_stick_modo == 1) {
        g_sumar = g_stick && g_abierto && g_pad_listo == 1;
        return;
    }
    g_sumar = 0;
    if (g_stick && g_abierto && g_teclado && vibrar_stick(sys, 1 - g_mano, &x, &y)) {
        if (y > g_zona_muerta)
            bits |= 1;
        if (x < -g_zona_muerta)
            bits |= 2;
        if (y < -g_zona_muerta)
            bits |= 4;
        if (x > g_zona_muerta)
            bits |= 8;
    }
    if (bits == g_mover_antes)
        return;
    for (k = 0; k < 4; k++)
        tv_tecla(g_teclas_mover[k], (bits >> k) & 1);
    if (g_log && bits && !g_mover_antes)
        logf_("muevo el mapa con el stick (%.2f, %.2f)", x, y);
    g_mover_antes = bits;
}

static unsigned char *mapa_n(int n);

/* El mando de GRAND (tipo 0) tal como lo ve el mapa de menús, que es el que
 * mueve el mapa. */
static void preparar_pad(void)
{
    unsigned char *m = mapa_n(1), *d;
    void **vt;
    if (!m)
        return;
    d = *(unsigned char **)(m + 0x103c);         /* tipo 0: el mando de GRAND */
    if (!d || IsBadReadPtr(d, 0x14)) {
        g_pad_listo = -1;
        logf_("no hay mando de GRAND (tipo 0): el stick derecho no mueve el mapa");
        return;
    }
    vt = *(void ***)d;
    g_eje_real = (FnEje)g_host->enganchar_puntero(g_host, &vt[3], (void *)mi_eje);
    if (!g_eje_real) {
        g_pad_listo = -1;
        logf_("no pude enganchar los sticks: el stick derecho no mueve el mapa");
        return;
    }
    g_pad = d;
    g_pad_listo = 1;
    logf_("stick derecho para mover el mapa listo (mando %p, ejes %x %x %x %x)", d, g_ejes[0], g_ejes[1], g_ejes[2], g_ejes[3]);
}

/* Diagnóstico (v0.5): en el mando de GRAND los sticks dan 0, así que el
 * stick izquierdo llega por otro lado. Los tres mapas de acciones (juego en
 * tabla+0x48, y +0x2998, +0x3a0c) tienen una lista de ejes analógicos
 * (mapa+0xa74 combinaciones, +0xa78 cantidad, +0xa80 valor, +0xa84
 * banderas; paso 0x44; cantidad de ejes en mapa+0x1058). Al abrir el mapa
 * anotamos esas listas una vez, y mientras está abierto, cada medio segundo,
 * los ejes que no están en 0 junto con los sticks de OpenVR. */
static const unsigned g_ofs_mapas[3] = { 0x48, 0x2998, 0x3a0c };

static unsigned char *mapa_n(int n)
{
    unsigned char *m = tv_mapa(g_delta);
    if (!m)
        return NULL;
    m = m - 0x48 + g_ofs_mapas[n];
    return IsBadReadPtr(m, 0x1060) ? NULL : m;
}

static void log_listas(void)
{
    static int hecho;
    int n;
    unsigned i, k, cant;
    char linea[300];
    int len;
    if (hecho)
        return;
    hecho = 1;
    for (n = 0; n < 3; n++) {
        unsigned char *m = mapa_n(n);
        if (!m)
            continue;
        cant = *(unsigned *)(m + 0x1058);
        logf_("ejes del mapa %d (+%x): %u", n, g_ofs_mapas[n], cant);
        for (i = 0; i < cant && i < 48; i++) {
            unsigned char *e = m + 0xa78 + i * 0x44;
            unsigned char *comb = *(unsigned char **)(e - 4);
            unsigned nc = *(unsigned *)e;
            len = snprintf(linea, sizeof linea, "  eje %2u: fl %02x valor %.2f, %u:", i, e[0xc], *(float *)(e + 8), nc);
            if (comb && nc <= 8 && !IsBadReadPtr(comb, nc * 8))
                for (k = 0; k < nc && len < (int)sizeof linea - 40; k++)
                    len += snprintf(linea + len, sizeof linea - len, " [cod %04x tipo %u fl %02x]",
                                    *(unsigned short *)(comb + k * 8 + 4), comb[k * 8 + 6], comb[k * 8 + 7]);
            logf_("%s", linea);
        }
    }
}

static void log_stick(float dt)
{
    void *sys = g_host->openvr();
    char linea[400];
    int len, n;
    unsigned i, cant;
    float lx, ly, rx, ry;
    if (!g_log || !g_log_ejes || !g_abierto)
        return;
    log_listas();
    g_t_log_stick += dt;
    if (g_t_log_stick < 0.5f)
        return;
    g_t_log_stick = 0.0f;
    vibrar_stick(sys, 0, &lx, &ly);
    vibrar_stick(sys, 1, &rx, &ry);
    len = snprintf(linea, sizeof linea, "sticks VR izq %.2f %.2f der %.2f %.2f; ejes no cero:", lx, ly, rx, ry);
    for (n = 0; n < 3; n++) {
        unsigned char *m = mapa_n(n);
        if (!m)
            continue;
        cant = *(unsigned *)(m + 0x1058);
        for (i = 0; i < cant && i < 48 && len < (int)sizeof linea - 24; i++) {
            float v = *(float *)(m + 0xa80 + i * 0x44);
            if (v > 0.01f || v < -0.01f)
                len += snprintf(linea + len, sizeof linea - len, " m%d.%u=%.2f", n, i, v);
        }
    }
    logf_("%s", linea);
}

static void log_ejes(const float visor[3][4], const float mando[3][4], float dist)
{
    static const char *nombres[6] = { "+x", "-x", "+y", "-y", "+z", "-z" };
    char linea[200];
    int k, len;
    float a;
    len = snprintf(linea, sizeof linea, "mando a %.2f m de los ojos; grados hacia los ojos:", dist);
    for (k = 0; k < 6; k++) {
        gesto_medir(visor, mando, k / 2, (k & 1) ? -1 : 1, &a);
        len += snprintf(linea + len, sizeof linea - len, " %s %.0f", nombres[k], a);
    }
    logf_("%s", linea);
}

static void f_tick(float dt)
{
    void *sys;
    PoseVR p[MAX_DISPOSITIVOS];
    int idx, mira, deja, libre;
    float ang, dist;

    if (g_toque > 0.0f) {
        g_toque -= dt;
        if (g_toque <= 0.0f)
            tv_apretar(0);
    }
    if (!(sys = g_host->openvr()))
        return;
    vibrar_tick(sys, dt);
    g_t_mandos += dt;
    if (g_t_mandos > 2.0f) {
        g_t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    if (g_tv == 0) {
        g_t_buscar += dt;
        if (g_t_buscar < 2.0f)
            return;
        g_t_buscar = 0.0f;
        g_tv = tv_preparar(g_host, g_accion);
        if (g_tv < 0)
            logf_("no encuentro las teclas de la accion %d: apagado", g_accion);
        return;
    }
    if (g_tv < 0 || !g_activado || g_pausado) {
        if (g_mover_antes) {
            tv_teclas_soltar();
            g_mover_antes = 0;
        }
        return;
    }
    if (g_stick && g_stick_modo == 2 && !g_teclado) {
        g_teclado = tv_tecla_preparar(g_host, 1);
        logf_(g_teclado ? "stick (teclas) para mover el mapa listo" : "no pude enganchar el teclado: el stick no mueve el mapa");
        if (!g_teclado)
            g_stick = 0;
    }
    if (g_stick && g_stick_modo == 1 && !g_pad_listo)
        preparar_pad();
    mover_mapa(sys);
    log_stick(dt);

    idx = vibrar_indice(g_mano);
    memset(p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[SLOT_POSES])(sys, 2, 0.0f, p, MAX_DISPOSITIVOS);
    /* Mando sin seguimiento: sin gesto (y si estaba abierto, queda como está). */
    if (idx <= 0 || idx >= MAX_DISPOSITIVOS || !p[0].valida || !p[idx].valida || p[idx].resultado != 200) {
        g_t_mira = 0.0f;
        g_ang_antes = -1.0f;
        return;
    }
    dist = gesto_medir(p[0].m, p[idx].m, g_eje, g_signo, &ang);
    mira = dist < g_distancia && ang < g_angulo;
    {
        /* Grados entre el frente del visor (-z) y la dirección visor->mando. */
        float d[3], dn, c;
        int j;
        for (j = 0; j < 3; j++)
            d[j] = p[idx].m[j][3] - p[0].m[j][3];
        dn = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        c = dn > 1e-3f ? -(d[0] * p[0].m[0][2] + d[1] * p[0].m[1][2] + d[2] * p[0].m[2][2]) / dn : 1.0f;
        g_ult_mirada = acosf(fmaxf(-1.0f, fminf(1.0f, c))) * 57.2958f;
    }
    if (mira && g_mirada_max > 0.0f && g_ult_mirada > g_mirada_max) {
        if (g_log && !g_aviso_mirada && !g_abierto && g_t_calma <= 0.0f) {
            g_aviso_mirada = 1;
            logf_("dorso hacia la cara (eje a %.0f grados, %.2f m) pero el reloj esta a %.0f grados del centro de la vista: no abro",
                  ang, dist, g_ult_mirada);
        }
        mira = 0;
    } else if (!mira) {
        g_aviso_mirada = 0;
    }
    g_ult_ang = ang;
    g_ult_dist = dist;
    {
        const float *v = p[idx].vel;
        float rapidez = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        g_ult_rapidez = rapidez;
        float fx = -p[0].m[0][2], fz = -p[0].m[2][2], fn = sqrtf(fx * fx + fz * fz);
        float adelante = fn > 1e-3f ? (v[0] * fx + v[2] * fz) / fn : 0.0f;
        if (adelante > 1.0f) {
            if (g_log && g_t_calma <= 0.0f && g_calma > 0.0f)
                logf_("empujon de la mano (%.2f m/s hacia adelante): no abro por %.1f s", adelante, g_calma);
            g_t_calma = g_calma;
        } else if (g_t_calma > 0.0f) {
            g_t_calma -= dt;
        }
        if (g_vel_max > 0.0f && rapidez > g_vel_max)
            mira = 0;               /* empieza a contar la Demora de nuevo */
    }
    deja = dist > g_distancia + 0.05f || ang > g_angulo + g_margen_ang;
    /* Girando rápido para afuera y ya fuera del ángulo: cerrar ya. */
    if (g_ang_antes >= 0.0f && dt > 0.0f && ang > g_angulo && (ang - g_ang_antes) / dt > 120.0f)
        deja = 2;
    g_ang_antes = ang;

    if (g_log_ejes && dist < g_distancia + 0.15f) {
        g_t_log_ejes += dt;
        if (g_t_log_ejes > 0.5f) {
            g_t_log_ejes = 0.0f;
            log_ejes(p[0].m, p[idx].m, dist);
        }
    }

    if (!g_abierto) {
        g_t_mira = mira ? g_t_mira + dt : 0.0f;
        if (g_t_mira >= g_demora && g_t_calma <= 0.0f) {
            libre = ripley_libre();
            g_t_mira = 0.0f;
            if (libre)
                abrir(libre);
            else if (g_log)
                logf_("mirada al reloj, pero Ripley no esta en control libre: no abro");
        }
    } else {
        /* Medio segundo después de abrir: si Ripley sigue "en control libre",
         * esa bandera no dice si el mapa está abierto y no se puede usar para
         * cerrar. */
        if (g_t_abierto < 0.6f) {
            g_t_abierto += dt;
            if (g_t_abierto >= 0.6f && !g_libre_no_sirve && ripley_libre()) {
                g_libre_no_sirve = 1;
                logf_("con el mapa abierto Ripley sigue en control libre: cierro siempre con un toque");
            }
        }
        g_t_deja = deja ? g_t_deja + dt : 0.0f;
        if (deja == 2 || g_t_deja >= g_demora_cerrar) {
            g_t_deja = 0.0f;
            cerrar(ripley_libre(), "bajaste la muneca");
        }
    }
}

static float leer_float(const char *clave, const char *def)
{
    char buf[32];
    GetPrivateProfileStringA("Mapa", clave, def, buf, sizeof buf, g_host->ini);
    return (float)atof(buf);
}

static void f_config(void)
{
    const char *ruta = g_host->ini;
    char buf[16];
    int accion_antes = g_accion;
    g_activado = GetPrivateProfileIntA("Mapa", "Activado", 1, ruta);
    g_accion = GetPrivateProfileIntA("Mapa", "Accion", 38, ruta);
    g_modo = GetPrivateProfileIntA("Mapa", "Modo", 0, ruta);
    g_cerrar_solo_menu = GetPrivateProfileIntA("Mapa", "CerrarSoloSiMenu", 1, ruta);
    g_mano = GetPrivateProfileIntA("Mapa", "Mano", 0, ruta) ? 1 : 0;
    g_vibrar = GetPrivateProfileIntA("Mapa", "Vibrar", 1, ruta);
    g_log = GetPrivateProfileIntA("Mapa", "Log", 1, ruta);
    g_log_ejes = GetPrivateProfileIntA("Mapa", "LogEjes", 1, ruta);
    g_stick = GetPrivateProfileIntA("Mapa", "StickOtraMano", 1, ruta);
    g_stick_modo = GetPrivateProfileIntA("Mapa", "StickModo", 1, ruta) == 2 ? 2 : 1;
    {
        char t[64];
        unsigned c[4];
        GetPrivateProfileStringA("Mapa", "StickEjes", "215 216 217 218", t, sizeof t, ruta);
        if (sscanf(t, "%x %x %x %x", &c[0], &c[1], &c[2], &c[3]) == 4)
            memcpy(g_ejes, c, sizeof c);
    }
    g_zona_muerta = leer_float("ZonaMuerta", "0.45");
    if (g_zona_muerta < 0.05f || g_zona_muerta > 0.95f)
        g_zona_muerta = 0.45f;
    {
        char t[64];
        unsigned c[4];
        GetPrivateProfileStringA("Mapa", "TeclasMover", "11 1e 1f 20", t, sizeof t, ruta);
        if (sscanf(t, "%x %x %x %x", &c[0], &c[1], &c[2], &c[3]) == 4) {
            tv_teclas_soltar();
            g_mover_antes = 0;
            memcpy(g_teclas_mover, c, sizeof c);
        }
    }
    GetPrivateProfileStringA("Mapa", "Eje", "-x", buf, sizeof buf, ruta);
    if (!gesto_leer_eje(buf, &g_eje, &g_signo)) {
        g_eje = 0;
        g_signo = -1;
    }
    g_angulo = leer_float("Angulo", "35");
    if (g_angulo < 5.0f || g_angulo > 90.0f)
        g_angulo = 35.0f;
    g_distancia = leer_float("Distancia", "0.45");
    if (g_distancia < 0.1f || g_distancia > 1.0f)
        g_distancia = 0.45f;
    g_demora = leer_float("Demora", "0.25");
    if (g_demora < 0.0f || g_demora > 3.0f)
        g_demora = 0.25f;
    g_demora_cerrar = leer_float("DemoraCerrar", "0.03");
    if (g_demora_cerrar < 0.0f || g_demora_cerrar > 3.0f)
        g_demora_cerrar = 0.03f;
    g_vel_max = leer_float("VelocidadMax", "0.6");
    if (g_vel_max < 0.0f || g_vel_max > 5.0f)
        g_vel_max = 0.6f;
    g_calma = leer_float("Calma", "1.0");
    if (g_calma < 0.0f || g_calma > 3.0f)
        g_calma = 1.0f;
    g_margen_ang = leer_float("MargenAngulo", "8");
    if (g_margen_ang < 0.0f || g_margen_ang > 45.0f)
        g_margen_ang = 8.0f;
    g_mirada_max = leer_float("MiradaMax", "30");
    if (g_mirada_max < 0.0f || g_mirada_max > 90.0f)
        g_mirada_max = 30.0f;
    if (g_accion != accion_antes && g_tv == 1)
        g_tv = 0;                   /* otra acción: volver a buscar sus teclas */
    if (!g_activado && g_abierto && g_modo == 1) {
        tv_apretar(0);
        g_abierto = 0;
    }
    logf_("ini: Activado=%d Accion=%d Modo=%d Mano=%s Eje=%s%c Angulo=%.0f Distancia=%.2f Demora=%.2f CerrarSoloSiMenu=%d DemoraCerrar=%.2f MargenAngulo=%.0f VelocidadMax=%.2f Calma=%.2f MiradaMax=%.0f",
          g_activado, g_accion, g_modo, g_mano ? "derecha" : "izquierda", g_signo < 0 ? "-" : "+",
          "xyz"[g_eje], g_angulo, g_distancia, g_demora, g_cerrar_solo_menu, g_demora_cerrar, g_margen_ang, g_vel_max, g_calma,
          g_mirada_max);
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
    if (pausado) {
        g_sumar = 0;
        tv_apretar(0);
        tv_teclas_soltar();
        g_mover_antes = 0;
        g_toque = 0.0f;
        vibrar_cortar();
    }
}

static void f_terminar(void)
{
    g_sumar = 0;
    tv_apretar(0);
    tv_teclas_soltar();
    vibrar_cortar();
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
    f->prioridad = 60;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
