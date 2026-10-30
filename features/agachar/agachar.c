/* agachar.dll (en mods_vr): agacharse en la vida real para
 * Alien: Isolation VR. Es una feature: la carga xinput_ag.dll (el cargador),
 * que tiene los enganches al juego y la puede cambiar en vivo (ver
 * anfitrion.h).
 *
 * Historia: hasta la v0.1.10 todo esto estaba en xinput_ag.dll; en la v0.1.11
 * era agachar_logica.dll. Desde la v0.2.0 el cargador busca OpenVR, tiene la
 * tecla de pánico y avisa cuando cambia agachar.ini.
 *
 * En cada cuadro mira la altura del visor y agacha o para a Ripley con las
 * mismas funciones que usan los scripts del juego. Además corrige la pose
 * que MotherVR le da al juego, para que no incline la vista al agacharte.
 *
 * Desde la v0.2.1 se calibra con un gesto: los dos gatillos apretados 2 s
 * toman tu altura de ese momento como "Ripley parada", estés parado o
 * sentado. Desde la v0.2.3, además, la vista queda siempre a AlturaVista en
 * esa postura, así calibrar parado o sentado se ve igual. Confirma vibrando (1 toque parado, 2 sentado) y con un beep.
 * Desde la v0.2.4 solo cuenta con los dos brazos extendidos hacia adelante
 * (BrazosAdelante), para que cerrar las manos por gusto no recalibre.
 *
 * Direcciones para AI.exe de Steam (crc32 0x17c2184b), sin rebasar. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "logica.h"
#include "anfitrion.h"
#include "vibrar.h"

#ifndef VERSION
#define VERSION "agachar v0.2.4"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u   /* global: manager de personajes */
#define DIR_AGACHAR         0x007cae20u   /* Character::Crouch */
#define DIR_PARAR           0x007cae90u   /* Character::Uncrouch */
#define DIR_GET_JUGADOR     0x0078baf0u   /* lee el jugador desde el manager (solo se verifica la versión) */
#define DIR_COND_AGACHADO   0x006eaf0du   /* condición del alien "está agachado" (ídem) */

#define OFS_AGACHADO        0x2af         /* bandera que mira el alien */
#define OFS_CONTROL         0x64

typedef struct { float m[3][4]; } Mat34;
typedef struct {
    Mat34 pose;
    float vel[3];
    float vel_ang[3];
    int resultado;
    unsigned char valida;
    unsigned char conectado;
} PoseVR;

typedef void (__attribute__((thiscall)) *FnPersonaje)(void *pj);
typedef void (__attribute__((thiscall)) *FnPoses)(void *sys, int universo, float prediccion,
                                                  PoseVR *poses, unsigned cantidad);

static Config g_cfg;
static Estado g_est;
static Config g_cfg_m;           /* para la altura que ve MotherVR */
static Estado g_est_m;
static int g_activado = 1;
static int g_universo = 2;      /* 2 = crudo: no lo mueve el recentrado */
static int g_log = 1;
static int g_iniciado;          /* 0 sin probar, 1 ok, -1 desactivado */
static unsigned g_delta;        /* rebase de AI.exe */
static LARGE_INTEGER g_freq, g_ultimo;
static Accion g_pendiente;
static float g_tiempo_log;
static const Anfitrion *g_host;
static volatile int g_pausado;  /* tecla de pánico del cargador */
static int g_estaba_pausado;
static int g_fijar = 1;         /* altura fija para MotherVR */

/* Calibrar con gesto */
static int g_cal_activado = 1;
static float g_cal_segundos = 2.0f;
static float g_cal_limite_sentado = 1.45f;
static int g_cal_boton;         /* 0 gatillos, 1 agarres */
static int g_cal_vibrar = 1;
static float g_ref;             /* altura de parada que tomó el gesto (0 = no hay) */
static float g_ref_m;           /* lo mismo, en la escala de MotherVR */
static int g_ref_sentado;
static float g_vista = 1.30f;   /* altura que ve MotherVR en la postura calibrada (0 = la tuya) */
static int g_cal_brazos = 1;    /* 1 = solo con los brazos extendidos hacia adelante */
static float g_cal_alcance = 0.40f;  /* cuánto delante de los ojos tiene que estar cada mano (m) */
static volatile float g_y_m;    /* última altura de MotherVR (la deja el hilo que dibuja) */
static volatile int g_y_m_ok;

#define logf_(...) do { if (g_log && g_host) g_host->log(g_host, __VA_ARGS__); } while (0)

static float leer_float(const char *ruta, const char *clave, float defecto)
{
    char buf[64], def[64];
    snprintf(def, sizeof def, "%g", defecto);
    GetPrivateProfileStringA("Agachar", clave, def, buf, sizeof buf, ruta);
    return (float)atof(buf);
}

/* Si hay una altura tomada con el gesto, manda sobre AlturaParado del .ini
 * y sobre la que se aprende sola. */
static void aplicar_ref(void)
{
    if (g_ref > 0.0f)
        g_cfg.altura_fija = g_ref;
    if (g_ref_m != 0.0f)
        g_cfg_m.altura_fija = g_ref_m;
}

static void leer_ini(void)
{
    const char *ruta = g_host->ini;
    logica_config_defecto(&g_cfg);
    g_activado = GetPrivateProfileIntA("Agachar", "Activado", 1, ruta);
    g_log = GetPrivateProfileIntA("Agachar", "Log", 1, ruta);
    g_universo = GetPrivateProfileIntA("Agachar", "Universo", 2, ruta);
    g_cfg.parar_solo = GetPrivateProfileIntA("Agachar", "PararSolo", 1, ruta);
    g_cfg.bajada_agachar = leer_float(ruta, "BajadaAgachar", g_cfg.bajada_agachar);
    g_cfg.bajada_parar = leer_float(ruta, "BajadaParar", g_cfg.bajada_parar);
    g_cfg.demora = leer_float(ruta, "DemoraMs", g_cfg.demora * 1000.0f) / 1000.0f;
    g_cfg.altura_fija = leer_float(ruta, "AlturaParado", 0.0f);
    g_fijar = GetPrivateProfileIntA("Agachar", "FijarAltura", 1, ruta);
    g_cfg.velocidad = leer_float(ruta, "Velocidad", g_cfg.velocidad);
    g_cfg.bajada_rapida = leer_float(ruta, "BajadaRapida", g_cfg.bajada_rapida);
    g_cfg.subida_rapida = leer_float(ruta, "SubidaRapida", g_cfg.subida_rapida);
    if (g_cfg.bajada_parar >= g_cfg.bajada_agachar)
        g_cfg.bajada_parar = g_cfg.bajada_agachar * 0.6f;
    g_cfg_m = g_cfg;
    g_cfg_m.altura_fija = 0.0f;     /* la altura de MotherVR va en otra escala que la del .ini */

    g_cal_activado = GetPrivateProfileIntA("Calibrar", "Activado", 1, ruta);
    g_cal_boton = GetPrivateProfileIntA("Calibrar", "Boton", 0, ruta);
    g_cal_vibrar = GetPrivateProfileIntA("Calibrar", "Vibrar", 1, ruta);
    {
        char buf[64];
        GetPrivateProfileStringA("Calibrar", "Segundos", "2", buf, sizeof buf, ruta);
        g_cal_segundos = (float)atof(buf);
        if (g_cal_segundos < 0.5f || g_cal_segundos > 10.0f)
            g_cal_segundos = 2.0f;
        GetPrivateProfileStringA("Calibrar", "LimiteSentado", "1.45", buf, sizeof buf, ruta);
        g_cal_limite_sentado = (float)atof(buf);
        GetPrivateProfileStringA("Calibrar", "AlturaVista", "1.30", buf, sizeof buf, ruta);
        g_vista = (float)atof(buf);
        if (g_vista < 0.0f || g_vista > 2.5f)
            g_vista = 1.30f;
        g_cal_brazos = GetPrivateProfileIntA("Calibrar", "BrazosAdelante", 1, ruta);
        GetPrivateProfileStringA("Calibrar", "Alcance", "0.40", buf, sizeof buf, ruta);
        g_cal_alcance = (float)atof(buf);
        if (g_cal_alcance < 0.10f || g_cal_alcance > 1.0f)
            g_cal_alcance = 0.40f;
    }
    aplicar_ref();
}

static int coincide(unsigned dir, const char *hex)
{
    const unsigned char *p = (const unsigned char *)(UINT_PTR)(dir + g_delta);
    size_t i, n = strlen(hex) / 2;
    if (IsBadReadPtr(p, n))
        return 0;
    for (i = 0; i < n; i++) {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        if (p[i] != v)
            return 0;
    }
    return 1;
}

static void log_config(void)
{
    logf_("activado %d, umbrales: agachar %.2f m, parar %.2f m, demora %.0f ms, altura fija %.2f, universo %d",
          g_activado, g_cfg.bajada_agachar, g_cfg.bajada_parar, g_cfg.demora * 1000.0f,
          g_cfg.altura_fija, g_universo);
    if (g_cfg.velocidad > 0.0f)
        logf_("rapido: a mas de %.2f m/s agacha tras bajar %.2f m y para tras subir %.2f m", g_cfg.velocidad, g_cfg.bajada_rapida, g_cfg.subida_rapida);
    else
        logf_("rapido: apagado (Velocidad=0)");
    logf_("altura fija para MotherVR: %s", g_fijar ? "si" : "no (FijarAltura=0)");
    logf_("calibrar con gesto: %s, %s %.1f s, sentado si la cabeza esta debajo de %.2f m",
          g_cal_activado ? "si" : "no", g_cal_boton ? "los dos agarres" : "los dos gatillos",
          g_cal_segundos, g_cal_limite_sentado);
    logf_("calibrar: %s", g_cal_brazos ? "solo con los dos brazos extendidos hacia adelante" : "con los brazos en cualquier lado (BrazosAdelante=0)");
    if (g_cal_brazos)
        logf_("calibrar: cada mano a %.2f m o mas delante de los ojos", g_cal_alcance);
    if (g_ref > 0.0f)
        logf_("uso la altura del gesto: %.2f m (%s)", g_ref, g_ref_sentado ? "sentado" : "parado");
    if (g_vista > 0.0f)
        logf_("despues del gesto la vista queda a %.2f m (AlturaVista)", g_vista);
    if (GetPrivateProfileIntA("Agachar", "Metodo", 0, g_host->ini) != 0)
        logf_("Metodo 1 y 2 (tocar el boton) ya no existen: uso el directo");
}

/* --- Baúl ---
 * Lo que se aprendió mirándote (sobre todo tu altura de parado) sobrevive a
 * las recargas. Si cambia la estructura Estado, subir BAUL_VERSION. */
#define BAUL_VERSION 2
typedef struct {
    Estado est;
    Estado est_m;
    int universo;
    float ref, ref_m;           /* desde la versión 2: la altura del gesto */
    int ref_sentado;
} Guardado;

static void recuperar_baul(void)
{
    Guardado g;
    memset(&g, 0, sizeof g);
    /* La versión 1 (agachar v0.2.0) es el mismo bloque sin la altura del gesto. */
    if (!g_host->baul_leer(g_host, BAUL_VERSION, &g, sizeof g) &&
        !g_host->baul_leer(g_host, 1, &g, (int)((char *)&g.ref - (char *)&g)))
        return;
    if (g.universo != g_universo) {
        logf_("cambiaste Universo: recalibro la altura");
        return;
    }
    g_est = g.est;
    g_est_m = g.est_m;
    g_ref = g.ref;
    g_ref_m = g.ref_m;
    g_ref_sentado = g.ref_sentado;
    aplicar_ref();
    if (g_ref > 0.0f)
        logf_("recupero la altura del gesto: %.2f m (%s)", g_ref, g_ref_sentado ? "sentado" : "parado");
    if (g_est.calibrado || g_est_m.calibrado)
        logf_("recupero la calibracion: referencia parado %.2f m (la de MotherVR %.2f m)",
              g_est.altura_parado, g_est_m.altura_parado);
}

static void guardar_baul(void)
{
    Guardado g;
    memset(&g, 0, sizeof g);
    g.est = g_est;
    g.est_m = g_est_m;
    g.universo = g_universo;
    g.ref = g_ref;
    g.ref_m = g_ref_m;
    g.ref_sentado = g_ref_sentado;
    g_host->baul_guardar(g_host, BAUL_VERSION, &g, sizeof g);
}

static void iniciar(void)
{
    g_delta = g_host->delta;
    leer_ini();
    logica_reset(&g_est);
    logica_reset(&g_est_m);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_ultimo);
    if (g_host->recarga)
        logf_(VERSION " (recarga %d)", g_host->recarga);
    else
        logf_(VERSION);
    log_config();
    recuperar_baul();
    /* Bytes que no pisa GRAND ni el rebase: si no coinciden, el exe es otro. */
    if (!coincide(DIR_GET_JUGADOR + 5, "8b400883b88400000000") ||
        !coincide(DIR_AGACHAR + 10, "b1017408888e3a3e0000eb07") ||
        !coincide(DIR_PARAR + 9, "c6863a3e00000084c07406") ||
        !coincide(DIR_COND_AGACHADO, "8a98af020000")) {
        logf_("AI.exe no es la version esperada: desactivado");
        g_iniciado = -1;
        return;
    }
    g_iniciado = 1;
}

static void *leer_ptr(unsigned dir)
{
    void **p = (void **)(UINT_PTR)dir;
    if (!dir || IsBadReadPtr(p, 4))
        return NULL;
    return *p;
}

static unsigned char *jugador(void)
{
    unsigned m = (unsigned)(UINT_PTR)leer_ptr(DIR_MANAGER_PJ + g_delta);
    unsigned lista;
    if (!m)
        return NULL;
    m = (unsigned)(UINT_PTR)leer_ptr(m + 8);
    if (!m || IsBadReadPtr((void *)(UINT_PTR)m, 0x88))
        return NULL;
    if (*(unsigned *)(UINT_PTR)(m + 0x84) == 0)
        return NULL;
    lista = (unsigned)(UINT_PTR)leer_ptr(m + 0x80);
    if (!lista)
        return NULL;
    return (unsigned char *)leer_ptr(lista);
}

/* 1 si el juego tiene al jugador en control libre. */
static int jugador_libre(unsigned char *pj)
{
    unsigned char *ctl;
    if (IsBadReadPtr(pj, 0x400))
        return 0;
    ctl = (unsigned char *)leer_ptr((unsigned)(UINT_PTR)(pj + OFS_CONTROL));
    return ctl && !IsBadReadPtr(ctl + 0x1f, 1) && ctl[0x1f];
}

static int altura_visor(float *y)
{
    PoseVR pose;
    void *sys = g_host->openvr();
    if (!sys)
        return 0;
    memset(&pose, 0, sizeof pose);
    ((FnPoses)(*(void ***)sys)[11])(sys, g_universo, 0.0f, &pose, 1);
    if (!pose.valida)
        return 0;
    *y = pose.pose.m[1][3];
    return isfinite(*y);
}

/* --- Agacharse ---
 * Character::Crouch (0x7cae20) pone el pedido de agacharse en +0x3e3a (si
 * +0x3e2e está prendido; +0x3e34 lo fuerza) y 0x7d2ad0 lo copia a +0x2af en
 * cada cuadro, con la animación. */
#define OFS_PERMITE         0x3e2e
#define OFS_FUERZA          0x3e34
#define OFS_PEDIDO          0x3e3a
#define OFS_PEDIDO_ANTES    0x3e46
#define OFS_FORZADO_A       0x2b0
#define OFS_FORZADO_B       0x2b4

static float g_t_pend;
static int g_lineas_vigilar;

static void campos(unsigned char *pj, char *buf, int n)
{
    unsigned char *o = (unsigned char *)leer_ptr((unsigned)(UINT_PTR)(pj + 0x31c));
    int o48 = o && !IsBadReadPtr(o + 0x48, 1) ? o[0x48] : -1;
    snprintf(buf, n, "agachado %d, pedido %d (antes %d), permite %d, fuerza %d, forzado %d/%d, 31c.48 %d",
             pj[OFS_AGACHADO], pj[OFS_PEDIDO], pj[OFS_PEDIDO_ANTES], pj[OFS_PERMITE],
             pj[OFS_FUERZA], pj[OFS_FORZADO_A], pj[OFS_FORZADO_B], o48);
}

/* Anota cada cambio de esos campos (hasta 400 líneas), haya hecho algo el
 * mod o no: así se ve también qué hace el botón de verdad. */
static void vigilar(void)
{
    static char antes[200];
    char ahora[200];
    unsigned char *pj = jugador();
    if (!pj || IsBadReadPtr(pj, OFS_PEDIDO_ANTES + 1) || g_lineas_vigilar >= 400)
        return;
    campos(pj, ahora, sizeof ahora);
    if (strcmp(ahora, antes) != 0) {
        logf_("  [%s]", ahora);
        strcpy(antes, ahora);
        g_lineas_vigilar++;
    }
}

/* Después de cada cambio de postura se vigila al personaje 2,5 s: a veces el
 * juego deshace el pedido. Si pasa, se vuelve a pedir, con 0,2 s entre
 * intentos y hasta 5 intentos. */
#define VIGILAR_SEG      2.5f
#define ENTRE_INTENTOS   0.2f
#define MAX_REINTENTOS   5

static void directo_paso(float dt)
{
    static float t_llamada;
    static int intentos;
    unsigned char *pj;
    int listo;
    if (g_pendiente == NADA)
        return;
    if (g_t_pend == 0.0f) {         /* cambio de postura nuevo */
        t_llamada = 1e9f;
        intentos = 0;
    }
    g_t_pend += dt;
    t_llamada += dt;
    pj = jugador();
    if (!pj || IsBadReadPtr(pj, OFS_PEDIDO_ANTES + 1)) {
        if (g_t_pend > VIGILAR_SEG)
            g_pendiente = NADA;
        return;
    }
    if (g_pendiente == AGACHAR)
        listo = pj[OFS_AGACHADO] || pj[OFS_PEDIDO];
    else
        listo = !pj[OFS_AGACHADO] && !pj[OFS_PEDIDO];
    if (g_t_pend > VIGILAR_SEG) {
        if (!listo)
            logf_("dejo de insistir: el juego no la %s", g_pendiente == AGACHAR ? "agacho" : "paro");
        g_pendiente = NADA;
        return;
    }
    if (listo || t_llamada < ENTRE_INTENTOS || intentos > MAX_REINTENTOS)
        return;
    if (!jugador_libre(pj))
        return;
    if (g_pendiente == AGACHAR) {
        ((FnPersonaje)(UINT_PTR)(DIR_AGACHAR + g_delta))(pj);
        if (!pj[OFS_PEDIDO]) {
            /* Crouch no dejó el pedido (permite = 0): se pone igual,
             * como lo haría Crouch con permite = 1. */
            pj[OFS_PEDIDO] = 1;
            logf_("%s Crouch; no quedo pedido, lo pongo yo", intentos ? "reintento" : "llamo");
        } else {
            logf_("%s Crouch", intentos ? "reintento" : "llamo");
        }
    } else {
        if (pj[OFS_FORZADO_B])
            return;             /* el juego la tiene agachada a la fuerza: esperar */
        ((FnPersonaje)(UINT_PTR)(DIR_PARAR + g_delta))(pj);
        logf_("%s Uncrouch", intentos ? "reintento" : "llamo");
    }
    t_llamada = 0.0f;
    intentos++;
}

/* --- Altura fija para MotherVR ---
 * MotherVR inclina la vista hacia el piso cuanto más baja el visor. La pose
 * que devuelve tiene la altura en out[7] = -escala * altura, y la pose cruda
 * queda en self+0x10 (altura en self+0x2c). Se le suma a out[7] lo que el
 * visor bajó respecto de la altura parado, así el juego siempre ve la cabeza
 * a la altura de parado. GRAND sigue viendo la altura real. */
static LARGE_INTEGER g_ultimo_m;
static float g_bajada_m;            /* última compensación (m), para el log */

static int pose_mvr(void *self, float *y)
{
    const unsigned char *p = (const unsigned char *)self + 0x10;
    if (IsBadReadPtr(p, 0x50) || !p[0x4c])
        return 0;
    *y = *(const float *)(p + 0x1c);
    return isfinite(*y);
}

static void compensar(void *self, float *out, float escala, int actualizar)
{
    float y, bajada;
    if (!pose_mvr(self, &y))
        return;
    if (actualizar) {
        g_y_m = y;
        g_y_m_ok = 1;
    }
    if (!g_fijar || g_pausado || IsBadWritePtr(out, 12 * sizeof(float)))
        return;
    if (actualizar) {
        LARGE_INTEGER ahora;
        float dt;
        QueryPerformanceCounter(&ahora);
        dt = g_ultimo_m.QuadPart ? (float)(ahora.QuadPart - g_ultimo_m.QuadPart) / (float)g_freq.QuadPart : 0.0f;
        g_ultimo_m = ahora;
        if (dt < 0.0f || dt > 0.1f)
            dt = 0.1f;
        logica_paso(&g_cfg_m, &g_est_m, y, dt);
    }
    if (!g_est_m.calibrado)
        return;
    if (g_ref_m != 0.0f && g_vista > 0.0f) {
        /* Calibrado con el gesto: en la postura calibrada MotherVR ve
         * AlturaVista, estés sentado o parado. Más abajo sigue fija (como
         * siempre); más arriba sube lo mismo que vos. */
        float sobre = y - g_est_m.altura_parado;
        bajada = (sobre > 0.0f ? sobre : 0.0f) + g_vista - y;
    } else {
        bajada = g_est_m.altura_parado - y;
        if (bajada <= 0.0f)
            return;
    }
    g_bajada_m = bajada;
    out[7] -= escala * bajada;
}

/* Activado=0 o tecla de pánico: soltar todo y, al volver, recalibrar desde
 * cero para no agachar ni parar de golpe. */
static void en_pausa(void)
{
    if (g_estaba_pausado)
        return;
    g_estaba_pausado = 1;
    g_pendiente = NADA;
}

static void salir_de_pausa(void)
{
    if (!g_estaba_pausado)
        return;
    g_estaba_pausado = 0;
    logica_reset(&g_est);
    QueryPerformanceCounter(&g_ultimo);
}

/* --- Calibrar con gesto ---
 * Los dos gatillos (o agarres) apretados Segundos: tu altura de ese momento
 * pasa a ser la de Ripley parada. Hay que soltar para volver a calibrar.
 * Con BrazosAdelante, además, las dos manos tienen que estar extendidas
 * hacia adelante (como cuando calibrás todo el cuerpo). */

#define MAX_POSES           16

/* Cuánto está cada mano delante de los ojos (en la dirección a la que mira
 * el visor, sin contar la altura), a los costados y por debajo.
 * Devuelve 0 si no hay tracking del visor o de alguna mano. */
static int brazos_medir(void *sys, float delante[2], float costado[2], float abajo[2])
{
    PoseVR p[MAX_POSES];
    float fx, fz, n, rx, rz;
    int i;
    memset(p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[11])(sys, g_universo, 0.0f, p, MAX_POSES);
    if (!p[0].valida)
        return 0;
    /* El visor mira hacia -z de su matriz. */
    fx = -p[0].pose.m[0][2];
    fz = -p[0].pose.m[2][2];
    n = sqrtf(fx * fx + fz * fz);
    if (n < 0.1f)
        return 0;                   /* mirando derecho arriba o abajo */
    fx /= n;
    fz /= n;
    rx = -fz;
    rz = fx;
    for (i = 0; i < 2; i++) {
        int k = vibrar_indice(i);
        float dx, dy, dz;
        if (k <= 0 || k >= MAX_POSES || !p[k].valida)
            return 0;
        dx = p[k].pose.m[0][3] - p[0].pose.m[0][3];
        dy = p[k].pose.m[1][3] - p[0].pose.m[1][3];
        dz = p[k].pose.m[2][3] - p[0].pose.m[2][3];
        delante[i] = dx * fx + dz * fz;
        costado[i] = dx * rx + dz * rz;
        abajo[i] = -dy;
    }
    return 1;
}

/* 1 si las dos manos están extendidas hacia adelante: cada una a Alcance o
 * más delante de los ojos, no más de 0.45 m al costado y no más de 0.60 m
 * por debajo (no colgando a los costados del cuerpo). */
static int brazos_adelante(void *sys, char *motivo, int n)
{
    float d[2], c[2], a[2];
    int i;
    if (!brazos_medir(sys, d, c, a)) {
        snprintf(motivo, n, "sin tracking del visor o de un mando");
        return 0;
    }
    for (i = 0; i < 2; i++)
        if (d[i] < g_cal_alcance || fabsf(c[i]) > 0.45f || a[i] > 0.60f) {
            snprintf(motivo, n, "mano %s a %.2f m delante (pide %.2f), %.2f m al costado, %.2f m abajo",
                     i ? "derecha" : "izquierda", d[i], g_cal_alcance, c[i], a[i]);
            return 0;
        }
    snprintf(motivo, n, "izq %.2f m delante, der %.2f m delante", d[0], d[1]);
    return 1;
}
static void calibrar_paso(float dt, float y)
{
    static float t_apretado, t_mandos = 10.0f;
    static int esperar_soltar, fallos_log, cambios_log, antes = -1;
    int ahora;
    void *sys = g_host->openvr();
    unsigned long long b[2] = { 0, 0 };
    float gat[2] = { 0.0f, 0.0f };
    int i, apretado = 1;

    t_mandos += dt;
    if (t_mandos > 2.0f) {
        t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    vibrar_tick(sys, dt);
    if (!g_cal_activado || !sys)
        return;
    ahora = 0;
    for (i = 0; i < 2; i++) {
        unsigned long long boton = g_cal_boton ? BOTON_AGARRE : BOTON_GATILLO;
        if (!vibrar_botones(sys, i, &b[i], &gat[i])) {
            apretado = 0;
            if (vibrar_indice(i) > 0 && fallos_log < 3) {
                fallos_log++;
                logf_("calibrar: no puedo leer los botones del mando %s (indice %d)", i ? "derecho" : "izquierdo", vibrar_indice(i));
            }
        } else if (!(b[i] & boton) && !(g_cal_boton == 0 && gat[i] > 0.9f)) {
            apretado = 0;
        } else {
            ahora |= 1 << i;
        }
    }
    /* Las primeras veces, anotar qué ve de los botones (para saber si llegan). */
    if (ahora != antes && cambios_log < 20) {
        cambios_log++;
        logf_("calibrar: %s apretado(s): izq %s (gatillo %.2f, botones %llx), der %s (gatillo %.2f, botones %llx)",
              g_cal_boton ? "agarres" : "gatillos", (ahora & 1) ? "si" : "no", gat[0], b[0],
              (ahora & 2) ? "si" : "no", gat[1], b[1]);
    }
    antes = ahora;
    if (!apretado) {
        t_apretado = 0.0f;
        esperar_soltar = 0;
        return;
    }
    if (esperar_soltar)
        return;
    if (g_cal_brazos) {
        static float t_log_brazos = 10.0f;
        char motivo[160];
        t_log_brazos += dt;
        if (!brazos_adelante(sys, motivo, sizeof motivo)) {
            /* Botones apretados con los brazos en otro lado: cerrar las
             * manos por gusto. No cuenta, y el tiempo arranca de cero. */
            if (t_log_brazos > 5.0f) {
                logf_("calibrar: botones apretados pero los brazos no estan adelante (%s): no calibro", motivo);
                t_log_brazos = 0.0f;
            }
            t_apretado = 0.0f;
            return;
        }
    }
    t_apretado += dt;
    if (t_apretado < g_cal_segundos)
        return;
    esperar_soltar = 1;

    g_ref = y;
    g_ref_sentado = y < g_cal_limite_sentado;
    g_ref_m = g_y_m_ok ? g_y_m : 0.0f;
    g_cfg.altura_fija = 0.0f;       /* lo que diga el .ini queda debajo del gesto */
    g_cfg_m.altura_fija = 0.0f;
    aplicar_ref();
    logica_paso(&g_cfg, &g_est, y, 0.0f);
    if (g_ref_m != 0.0f)
        logica_paso(&g_cfg_m, &g_est_m, g_ref_m, 0.0f);
    guardar_baul();
    logf_("calibrado con el gesto: %s, cabeza a %.2f m (MotherVR %.2f)%s",
          g_ref_sentado ? "SENTADO" : "PARADO", y, g_ref_m, g_cal_brazos ? ", con los brazos adelante" : "");
    if (g_cal_vibrar) {
        vibrar_pedir(0, g_ref_sentado ? 2 : 1, 150.0f, 120.0f, 1.0f);
        vibrar_pedir(1, g_ref_sentado ? 2 : 1, 150.0f, 120.0f, 1.0f);
    }
    MessageBeep(MB_OK);
}

static void paso(void)
{
    LARGE_INTEGER ahora;
    float dt, y;
    Accion a;

    salir_de_pausa();

    QueryPerformanceCounter(&ahora);
    dt = (float)(ahora.QuadPart - g_ultimo.QuadPart) / (float)g_freq.QuadPart;
    if (dt < 0.004f)
        return;
    g_ultimo = ahora;
    if (dt > 0.1f)
        dt = 0.1f;

    if (!altura_visor(&y))
        return;
    calibrar_paso(dt, y);
    a = logica_paso(&g_cfg, &g_est, y, dt);
    if (a != NADA) {
        logf_("postura: %s (cabeza %.2f m, referencia parado %.2f m, velocidad %.2f m/s)",
              a == AGACHAR ? "abajo" : "arriba", y, g_est.altura_parado, g_est.velocidad);
        g_pendiente = a;
        g_t_pend = 0.0f;
    }
    g_tiempo_log += dt;
    if (g_tiempo_log > 5.0f) {
        g_tiempo_log = 0.0f;
        logf_("cabeza %.2f m, referencia parado %.2f m%s", y, g_est.altura_parado,
              g_host->estado_mvr() ? "" : " (pose de MotherVR sin enganchar)");
        if (g_host->estado_mvr() && g_fijar)
            logf_("  altura fija: MotherVR ve la cabeza %.2f m mas %s", fabsf(g_bajada_m), g_bajada_m < 0.0f ? "abajo" : "arriba");
    }
    vigilar();
    directo_paso(dt);
}

/* --- Lo que llama el cargador --- */

static void f_tick(float dt)
{
    (void)dt;                   /* paso() mide su propio tiempo, como antes */
    if (g_iniciado == 1 && g_activado && !g_pausado)
        paso();
    else
        en_pausa();
}

/* Desde el hilo que dibuja (MotherVR pide la pose del visor). */
static void f_pose(int cabeza, void *self, float *out, float escala)
{
    if (cabeza)
        g_bajada_m = 0.0f;
    compensar(self, out, escala, cabeza);
}

static void f_config(void)
{
    int universo_antes = g_universo;
    leer_ini();
    if (g_universo != universo_antes) {
        logica_reset(&g_est);       /* otra escala de altura: recalibrar */
        logica_reset(&g_est_m);
        g_ref = g_ref_m = 0.0f;
        leer_ini();
    }
    logf_("agachar.ini recargado");
    log_config();
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
    if (pausado) {
        en_pausa();
        vibrar_cortar();
    }
}

static void f_terminar(void)
{
    guardar_baul();
    g_host = NULL;
}

__declspec(dllexport) int mod_iniciar(const Anfitrion *a, Feature *f)
{
    if (!a || a->version != ANFITRION_VERSION || a->tam < (int)sizeof(Anfitrion))
        return ANFITRION_VERSION;   /* el cargador ve que no coincide y avisa */
    g_host = a;
    iniciar();
    f->tam = sizeof *f;
    f->prioridad = 10;          /* la altura fija va antes que el resto de lo que toque la pose */
    f->tick = f_tick;
    f->pose = f_pose;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
