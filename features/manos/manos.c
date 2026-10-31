/* manos.dll (en mods_vr): que las manos de GRAND no atraviesen paredes.
 *
 * - Envuelve IVRCompositor::GetLastPoses (la tabla de SteamVR que usa
 *   MotherVR): ahí MotherVR llena su lista de poses (self+0x10) y GRAND saca
 *   de esa lista las manos. Si hay pared entre tus ojos y la mano, corremos
 *   la mano hacia tus ojos hasta justo antes de la pared. GRAND mueve la mano
 *   y el arma con esa posición.
 * - El rayo se tira en el hilo del juego (la física no se toca desde el hilo
 *   que dibuja): desde la cámara del juego (los ojos de Ripley) hasta la mano.
 *   El resultado (qué parte del camino ojos-mano se permite) se aplica en la
 *   próxima pose.
 *
 * Desde la v0.6 no es un solo rayo al centro de la mano: también a puntos
 * alrededor (Radio) y a uno adelante, hacia donde apunta el mando (Adelante:
 * dedos y arma). Así no se cuela en bordes, como el marco de una puerta.
 *
 * Del espacio del visor al mapa: v = mano - visor se pasa a los ejes del
 * visor (x derecha, y arriba, -z adelante) y de ahí a los de la cámara del
 * juego (x derecha, y arriba, +z adelante; probado contra una pared en la
 * v0.4).
 *
 * Desde la v0.7, cuando una mano empieza a chocar contra una pared, ese mando
 * da un toquecito (Vibrar=1): sentís la pared sin mirarla.
 *
 * Desde la v0.8 (UsarAlTocar=1), ese mismo choque aprieta "usar" (la tecla
 * Tecla, la E, con SendInput como usar.dll): tocás el panel, la puerta o la
 * computadora y Ripley la usa. Si el juego no muestra la indicación de usar,
 * la tecla no hace nada. Una vez cada UsarCada segundos como máximo.
 *
 * Desde la v0.8.1, el toque se suelta antes (SoltarToque, 0.985 en vez de
 * 0.995), también si la mano retrocede un poco aunque siga cerca de algo
 * (SoltarRetroceso) y, si se pide, por tiempo (ToqueMax). Con el brazo
 * arriba la mano queda casi siempre pegada al mueble y antes no volvía a
 * disparar. El log anota cada disparo y cada empujón que no disparó, con el
 * motivo, f y la altura de la mano respecto de los ojos (sin tope, como
 * mucho una línea cada 0.5 s para los que no disparan).
 *
 * Direcciones: AI.exe de Steam sin rebasar, MotherVR 0.8.1 (dxgi.dll) como
 * desplazamiento desde su base. Ver ideas_gameplay/ANALISIS_MANOS.md y
 * agachar/NOTAS.md (manos v0.1 a v0.5). */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"

#ifndef VERSION
#define VERSION "manos v0.8.1"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u   /* global: manager de personajes */
#define DIR_RAYO            0x007ebab0u   /* envoltorio del juego de hkpWorld::castRay (cdecl) */
#define DIR_MUNDO           0x0170791cu   /* hkpWorld* (0 = sin mundo) */
#define OFS_CAMARAS         0x58          /* manager de personajes + 0x58 = manager de cámaras */
#define OFS_CAM_ACTIVA      0x1d8
#define OFS_CAM_JUGADOR     0x1dc
#define OFS_CAM_ROT         0x2c          /* cuaternión x,y,z,w (0x432300) */
#define OFS_CAM_POS         0x3c

#define MVR_COMPOSITOR      0x6af50u      /* IVRCompositor_021* que guarda MotherVR */
#define MVR_LLAMADA         0x7123u       /* "mov eax,[compositor]" en la pose de cabeza (0x100070e0) */
#define SLOT_LAST_POSES     3             /* IVRCompositor::GetLastPoses */
#define SLOT_INDICE_ROL     18            /* IVRSystem::GetTrackedDeviceIndexForControllerRole */
#define MAX_DISPOSITIVOS    16

typedef struct {
    float m[3][4];                  /* posición en m[0][3], m[1][3], m[2][3] */
    float vel[3], vel_ang[3];
    int resultado;
    unsigned char valida, conectado, relleno[2];
} PoseVR;                           /* TrackedDevicePose_t, 0x50 bytes */

typedef int (__attribute__((thiscall)) *FnLastPoses)(void *comp, PoseVR *render, unsigned n, PoseVR *juego, unsigned nj);
typedef unsigned (__attribute__((thiscall)) *FnIndiceRol)(void *sys, int rol);
/* cdecl; desde, hasta y normal alineados a 16 */
typedef char (__cdecl *FnRayo)(const float *desde, const float *hasta, float *fraccion, float *normal, unsigned filtro);

static const Anfitrion *g_host;
static unsigned g_delta;
static FnLastPoses g_poses_real;
static volatile LONG g_idx[2] = { -1, -1 };     /* 0 izquierda, 1 derecha; los busca el hilo del juego */
static volatile float g_subir_der;              /* prueba: metros */

/* Lo que deja el hilo de la pose para el hilo del juego. */
static CRITICAL_SECTION g_cs;
static int g_cs_lista;
static struct {
    float visor[3][4];
    float mano[2][3][4];            /* sin corregir (matriz entera, para hacia dónde apunta) */
    int ok_visor, ok_mano[2];
} g_ult;
/* Lo que deja el hilo del juego para el de la pose: qué parte del camino
 * ojos-mano se deja (1 = todo). */
static volatile float g_permitido[2] = { 1.0f, 1.0f };

/* Configuración */
static int g_activado = 1;
static int g_pausado;
static int g_log = 1;
static float g_intervalo = 2.0f;
static float g_margen = 0.06f;
static float g_escala = 1.0f;
static float g_radio = 0.05f;
static float g_adelante = 0.12f;
static unsigned g_filtro = 0x17;
static int g_vibrar = 1;
static float g_vibrar_ms = 40.0f;
static float g_vibrar_fuerza = 0.8f;
static float g_vibrar_cada = 0.5f;

/* Pared al tacto: la mano está tocando (con histéresis) y cuánto hace que vibró. */
static int g_tocando[2];
static float g_desde_toque[2] = { 10.0f, 10.0f };
static float g_f_min[2];            /* f más bajo desde que empezó el toque */
static float g_tocando_seg[2];      /* cuánto hace que está tocando */
static float g_soltar = 0.985f;
static float g_soltar_retroceso = 0.04f;
static float g_toque_max;           /* segundos; 0 = no se suelta por tiempo */
static int g_toques[2], g_pulsos_log;

static float g_desde[4] __attribute__((aligned(16)));
static float g_hasta[4] __attribute__((aligned(16)));
static float g_normal[4] __attribute__((aligned(16)));

/* Para el log */
static int g_cuadros, g_cuadros_frenada[2];
static float g_min_permitido[2] = { 1.0f, 1.0f };

static void *leer_ptr(unsigned dir)
{
    void **p = (void **)(UINT_PTR)dir;
    if (!dir || IsBadReadPtr(p, 4))
        return NULL;
    return *p;
}

/* --- Hilo que dibuja: la pose de SteamVR --- */

static int __attribute__((thiscall)) mi_poses(void *comp, PoseVR *render, unsigned n, PoseVR *juego, unsigned nj)
{
    int r = g_poses_real(comp, render, n, juego, nj);
    int i;
    if (!render || n == 0 || n > 64 || !render[0].valida)
        return r;
    if (g_cs_lista)
        EnterCriticalSection(&g_cs);
    for (i = 0; i < 2; i++) {
        LONG k = g_idx[i];
        PoseVR *p;
        float f = g_permitido[i];
        int j;
        if (k <= 0 || (unsigned)k >= n) {
            g_ult.ok_mano[i] = 0;
            continue;
        }
        p = &render[k];
        memcpy(g_ult.mano[i], p->m, sizeof g_ult.mano[i]);
        g_ult.ok_mano[i] = p->valida;
        if (p->valida && f < 1.0f)
            for (j = 0; j < 3; j++)
                p->m[j][3] = render[0].m[j][3] + f * (p->m[j][3] - render[0].m[j][3]);
    }
    memcpy(g_ult.visor, render[0].m, sizeof g_ult.visor);
    g_ult.ok_visor = 1;
    if (g_cs_lista)
        LeaveCriticalSection(&g_cs);
    if (g_subir_der != 0.0f && g_idx[1] > 0 && (unsigned)g_idx[1] < n)
        render[g_idx[1]].m[1][3] += g_subir_der;
    return r;
}

/* --- Hilo del juego --- */

static void buscar_mandos(void)
{
    void *sys = g_host->openvr();
    int i;
    if (!sys)
        return;
    for (i = 0; i < 2; i++) {
        unsigned k = ((FnIndiceRol)(*(void ***)sys)[SLOT_INDICE_ROL])(sys, i + 1);
        g_idx[i] = k < MAX_DISPOSITIVOS ? (LONG)k : -1;
    }
    vibrar_buscar_mandos(sys);
}

/* La cámara del jugador, si es la que se está usando (no en cinemáticas). */
static unsigned char *camara_jugador(void)
{
    unsigned m = (unsigned)(UINT_PTR)leer_ptr(DIR_MANAGER_PJ + g_delta);
    unsigned char *cm, *cam;
    if (!m)
        return NULL;
    cm = (unsigned char *)leer_ptr(m + OFS_CAMARAS);
    if (!cm || IsBadReadPtr(cm, 0x230))
        return NULL;
    cam = *(unsigned char **)(cm + OFS_CAM_ACTIVA);
    if (!cam || cam != *(unsigned char **)(cm + OFS_CAM_JUGADOR) || IsBadReadPtr(cam, 0x160))
        return NULL;
    return cam;
}

/* Fracción libre del camino desde..hasta (1 si no choca o no se pudo tirar). */
static float rayo(const float *desde, const float *hasta)
{
    FnRayo f = (FnRayo)(UINT_PTR)(DIR_RAYO + g_delta);
    float fr = 1.0f;
    if (!leer_ptr(DIR_MUNDO + g_delta))
        return 1.0f;
    memcpy(g_desde, desde, 3 * sizeof(float));
    memcpy(g_hasta, hasta, 3 * sizeof(float));
    g_desde[3] = g_hasta[3] = 0.0f;
    if (!f(g_desde, g_hasta, &fr, g_normal, g_filtro))
        return 1.0f;
    if (!(fr >= 0.0f && fr <= 1.0f))
        return 1.0f;
    return fr;
}

/* Del espacio del visor al mapa (una dirección; no posición). */
static void al_mapa(const float visor[3][4], const float ejes[3][3], const float *t, float *dm)
{
    float l[3];
    int j;
    /* a los ejes del visor (las columnas de su matriz) */
    for (j = 0; j < 3; j++)
        l[j] = visor[0][j] * t[0] + visor[1][j] * t[1] + visor[2][j] * t[2];
    /* visor: x derecha, y arriba, -z adelante  ->  cámara: x, y, +z */
    l[2] = -l[2];
    for (j = 0; j < 3; j++)
        dm[j] = g_escala * (l[0] * ejes[0][j] + l[1] * ejes[1][j] + l[2] * ejes[2][j]);
}

static float largo(const float *v)
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Parte del camino ojos-punto que está libre (1 = todo), con el margen. */
static float libre_hasta(const float *pc, const float *v)
{
    float d = largo(v), hasta[3], fr, libre;
    int j;
    if (!(d > 0.05f) || d > 3.5f)
        return 1.0f;
    /* el rayo va un poco más allá, para frenar antes de tocar */
    for (j = 0; j < 3; j++)
        hasta[j] = pc[j] + v[j] * (d + g_margen) / d;
    fr = rayo(pc, hasta);
    libre = fr * (d + g_margen) - g_margen;
    return libre >= d ? 1.0f : (libre <= 0.0f ? 0.0f : libre / d);
}

/* Un toque cuando la mano empieza a frenar contra algo. Se suelta recién
 * cuando la mano queda bien libre, para no vibrar de nuevo por temblar en el
 * borde, y nunca más de un toque cada VibrarCada segundos por mano. */
static int g_usar_al_tocar = 1;
static unsigned g_tecla_usar = 0x12;        /* E */
static float g_usar_cada = 1.0f;
static float g_desde_usar = 10.0f, g_tecla_abajo;

static int juego_con_foco(void)
{
    DWORD pid = 0;
    HWND w = GetForegroundWindow();
    if (!w)
        return 0;
    GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static void tecla_usar(int abajo)
{
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = (WORD)(g_tecla_usar & 0x7f);
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (abajo ? 0 : KEYEVENTF_KEYUP) | (g_tecla_usar & 0x80 ? KEYEVENTF_EXTENDEDKEY : 0);
    SendInput(1, &in, sizeof in);
}

/* Para el log de cada intento: altura de la mano respecto de los ojos (m) y
 * hacia dónde miran la cabeza, la cámara del juego y el mando (grados, + arriba). */
typedef struct {
    float altura, pitch_cabeza, pitch_camara, pitch_mano;
} Intento;
static float g_desde_log_no = 10.0f;

static void log_intento(int i, const char *que, float f, const Intento *in)
{
    g_host->log(g_host, "mano %s: %s (f %.3f, mano %+.2f m de los ojos, cabeza %+.0f, camara %+.0f, mando %+.0f grados)",
                i ? "derecha" : "izquierda", que, f, in->altura, in->pitch_cabeza, in->pitch_camara, in->pitch_mano);
}

/* La mano tocó algo: apretar usar (el juego decide si hay qué usar). */
static void usar_al_tocar(int i, float f, const Intento *in)
{
    const char *motivo = NULL;
    if (!g_usar_al_tocar)
        return;
    if (g_desde_usar < g_usar_cada || g_tecla_abajo > 0.0f)
        motivo = "no aprieto usar (UsarCada)";
    else if (!juego_con_foco())
        motivo = "no aprieto usar (el juego no tiene el foco)";
    if (motivo) {
        if (g_log && g_desde_log_no >= 0.5f) {
            g_desde_log_no = 0.0f;
            log_intento(i, motivo, f, in);
        }
        return;
    }
    tecla_usar(1);
    g_tecla_abajo = 0.12f;
    g_desde_usar = 0.0f;
    if (g_log)
        log_intento(i, "toco algo, aprieto usar", f, in);
}

static void tick_usar(float dt)
{
    g_desde_usar += dt;
    if (g_tecla_abajo > 0.0f) {
        g_tecla_abajo -= dt;
        if (g_tecla_abajo <= 0.0f)
            tecla_usar(0);
    }
}

static void tocar_pared(int i, float f, const Intento *in)
{
    if (!g_tocando[i] && f < 0.97f) {
        g_tocando[i] = 1;
        g_f_min[i] = f;
        g_tocando_seg[i] = 0.0f;
        usar_al_tocar(i, f, in);
        if (g_vibrar && g_desde_toque[i] >= g_vibrar_cada) {
            vibrar_pedir(i, 1, g_vibrar_ms, 0.0f, g_vibrar_fuerza);
            g_desde_toque[i] = 0.0f;
            g_toques[i]++;
        }
    } else if (g_tocando[i]) {
        if (f > g_soltar || f > g_f_min[i] + g_soltar_retroceso ||
            (g_toque_max > 0.0f && g_tocando_seg[i] >= g_toque_max)) {
            g_tocando[i] = 0;
        } else if (f < g_f_min[i] - 0.03f) {
            /* empuja más contra lo mismo sin haberse soltado: no dispara */
            g_f_min[i] = f;
            if (g_usar_al_tocar && g_log && g_desde_log_no >= 0.5f) {
                g_desde_log_no = 0.0f;
                log_intento(i, "empuja pero sigue tocando desde antes, no aprieto usar", f, in);
            }
        }
    }
}

static void calcular(void)
{
    unsigned char *cam = g_activado && !g_pausado ? camara_jugador() : NULL;
    float visor[3][4], mano[2][3][4], ejes[3][3], pc[3];
    int ok_visor, ok_mano[2], i;
    float x, y, z, w;

    if (!cam || !g_cs_lista) {
        g_permitido[0] = g_permitido[1] = 1.0f;
        g_tocando[0] = g_tocando[1] = 0;
        return;
    }
    EnterCriticalSection(&g_cs);
    memcpy(visor, g_ult.visor, sizeof visor);
    memcpy(mano, g_ult.mano, sizeof mano);
    ok_visor = g_ult.ok_visor;
    ok_mano[0] = g_ult.ok_mano[0];
    ok_mano[1] = g_ult.ok_mano[1];
    LeaveCriticalSection(&g_cs);
    if (!ok_visor) {
        g_permitido[0] = g_permitido[1] = 1.0f;
        return;
    }

    memcpy(pc, cam + OFS_CAM_POS, sizeof pc);
    x = ((const float *)(cam + OFS_CAM_ROT))[0];
    y = ((const float *)(cam + OFS_CAM_ROT))[1];
    z = ((const float *)(cam + OFS_CAM_ROT))[2];
    w = ((const float *)(cam + OFS_CAM_ROT))[3];
    /* ejes de la cámara en el mapa: x derecha, y arriba, z adelante */
    ejes[0][0] = 1.0f - 2.0f * (y * y + z * z); ejes[0][1] = 2.0f * (x * y + w * z); ejes[0][2] = 2.0f * (x * z - w * y);
    ejes[1][0] = 2.0f * (x * y - w * z); ejes[1][1] = 1.0f - 2.0f * (x * x + z * z); ejes[1][2] = 2.0f * (y * z + w * x);
    ejes[2][0] = 2.0f * (x * z + w * y); ejes[2][1] = 2.0f * (y * z - w * x); ejes[2][2] = 1.0f - 2.0f * (x * x + y * y);

    g_cuadros++;
    for (i = 0; i < 2; i++) {
        float t[3], v[3], apunta[3], a[3], b[3], pv[3], d, f, fk;
        int j, k;
        if (!ok_mano[i]) {
            g_permitido[i] = 1.0f;
            g_tocando[i] = 0;
            continue;
        }
        for (j = 0; j < 3; j++)
            t[j] = mano[i][j][3] - visor[j][3];
        al_mapa(visor, ejes, t, v);
        d = largo(v);
        if (!(d > 0.05f) || d > 3.0f) {
            g_permitido[i] = 1.0f;
            continue;
        }
        f = libre_hasta(pc, v);

        /* dos direcciones perpendiculares al camino ojos-mano */
        a[0] = -v[2]; a[1] = 0.0f; a[2] = v[0];             /* horizontal */
        if (largo(a) < 0.01f) { a[0] = 1.0f; a[1] = a[2] = 0.0f; }
        b[0] = v[1] * a[2] - v[2] * a[1];
        b[1] = v[2] * a[0] - v[0] * a[2];
        b[2] = v[0] * a[1] - v[1] * a[0];
        {
            float la = largo(a), lb = largo(b);
            for (j = 0; j < 3; j++) {
                a[j] /= la;
                b[j] = lb > 0.0f ? b[j] / lb : 0.0f;
            }
        }
        if (g_radio > 0.0f)
            for (k = 0; k < 4; k++) {
                const float *e = k < 2 ? a : b;
                float sg = (k & 1) ? -g_radio : g_radio;
                for (j = 0; j < 3; j++)
                    pv[j] = v[j] + sg * e[j];
                fk = libre_hasta(pc, pv);
                if (fk < f)
                    f = fk;
            }
        /* hacia donde apunta el mando (su -z) */
        if (g_adelante > 0.0f) {
            for (j = 0; j < 3; j++)
                t[j] = -mano[i][j][2];
            al_mapa(visor, ejes, t, apunta);
            for (j = 0; j < 3; j++)
                pv[j] = v[j] + g_adelante * apunta[j];
            fk = libre_hasta(pc, pv);
            if (fk < f)
                f = fk;
        }

        g_permitido[i] = f;
        {
            Intento in;
            in.altura = v[1];
            in.pitch_cabeza = asinf(fmaxf(-1.0f, fminf(1.0f, -visor[1][2]))) * 57.2958f;
            in.pitch_camara = asinf(fmaxf(-1.0f, fminf(1.0f, ejes[2][1]))) * 57.2958f;
            in.pitch_mano = asinf(fmaxf(-1.0f, fminf(1.0f, -mano[i][1][2]))) * 57.2958f;
            tocar_pared(i, f, &in);
        }
        if (f < 1.0f)
            g_cuadros_frenada[i]++;
        if (f < g_min_permitido[i])
            g_min_permitido[i] = f;
    }
}

static void leer_ini(void)
{
    const char *ruta = g_host->ini;
    char buf[32];
    g_activado = GetPrivateProfileIntA("Manos", "Activado", 1, ruta);
    g_log = GetPrivateProfileIntA("Manos", "Log", 1, ruta);
    GetPrivateProfileStringA("Manos", "Intervalo", "2.0", buf, sizeof buf, ruta);
    g_intervalo = (float)atof(buf);
    if (g_intervalo < 0.5f)
        g_intervalo = 0.5f;
    GetPrivateProfileStringA("Manos", "Margen", "0.06", buf, sizeof buf, ruta);
    g_margen = (float)atof(buf);
    if (g_margen < 0.0f || g_margen > 0.5f)
        g_margen = 0.06f;
    GetPrivateProfileStringA("Manos", "Escala", "1.0", buf, sizeof buf, ruta);
    g_escala = (float)atof(buf);
    if (g_escala < 0.2f || g_escala > 5.0f)
        g_escala = 1.0f;
    g_filtro = GetPrivateProfileIntA("Manos", "Filtro", 0x17, ruta);
    GetPrivateProfileStringA("Manos", "Radio", "0.05", buf, sizeof buf, ruta);
    g_radio = (float)atof(buf);
    if (g_radio < 0.0f || g_radio > 0.3f)
        g_radio = 0.05f;
    GetPrivateProfileStringA("Manos", "Adelante", "0.12", buf, sizeof buf, ruta);
    g_adelante = (float)atof(buf);
    if (g_adelante < 0.0f || g_adelante > 0.6f)
        g_adelante = 0.12f;
    g_vibrar = GetPrivateProfileIntA("Manos", "Vibrar", 1, ruta);
    GetPrivateProfileStringA("Manos", "VibrarMs", "40", buf, sizeof buf, ruta);
    g_vibrar_ms = (float)atof(buf);
    if (g_vibrar_ms < 5.0f || g_vibrar_ms > 500.0f)
        g_vibrar_ms = 40.0f;
    GetPrivateProfileStringA("Manos", "VibrarFuerza", "0.8", buf, sizeof buf, ruta);
    g_vibrar_fuerza = (float)atof(buf);
    if (g_vibrar_fuerza < 0.0f || g_vibrar_fuerza > 1.0f)
        g_vibrar_fuerza = 0.8f;
    GetPrivateProfileStringA("Manos", "VibrarCada", "0.5", buf, sizeof buf, ruta);
    g_vibrar_cada = (float)atof(buf);
    if (g_vibrar_cada < 0.0f || g_vibrar_cada > 10.0f)
        g_vibrar_cada = 0.5f;
    g_usar_al_tocar = GetPrivateProfileIntA("Manos", "UsarAlTocar", 1, ruta);
    GetPrivateProfileStringA("Manos", "Tecla", "12", buf, sizeof buf, ruta);
    g_tecla_usar = (unsigned)strtoul(buf, NULL, 16) & 0xff;
    if (!g_tecla_usar)
        g_tecla_usar = 0x12;
    GetPrivateProfileStringA("Manos", "UsarCada", "1.0", buf, sizeof buf, ruta);
    g_usar_cada = (float)atof(buf);
    if (g_usar_cada < 0.1f || g_usar_cada > 10.0f)
        g_usar_cada = 1.0f;
    GetPrivateProfileStringA("Manos", "SoltarToque", "0.985", buf, sizeof buf, ruta);
    g_soltar = (float)atof(buf);
    if (g_soltar < 0.97f || g_soltar > 1.0f)
        g_soltar = 0.985f;
    GetPrivateProfileStringA("Manos", "SoltarRetroceso", "0.04", buf, sizeof buf, ruta);
    g_soltar_retroceso = (float)atof(buf);
    if (g_soltar_retroceso <= 0.0f || g_soltar_retroceso > 1.0f)
        g_soltar_retroceso = 0.04f;
    GetPrivateProfileStringA("Manos", "ToqueMax", "0", buf, sizeof buf, ruta);
    g_toque_max = (float)atof(buf);
    if (g_toque_max < 0.0f || g_toque_max > 30.0f)
        g_toque_max = 0.0f;
    GetPrivateProfileStringA("Manos", "PruebaSubirDerecha", "0", buf, sizeof buf, ruta);
    g_subir_der = (float)atof(buf);
}

static void f_tick(float dt)
{
    static float acum_mandos = 2.0f, acum_log;
    acum_mandos += dt;
    if (acum_mandos >= 2.0f) {
        acum_mandos = 0.0f;
        buscar_mandos();
    }
    g_desde_toque[0] += dt;
    g_desde_toque[1] += dt;
    g_tocando_seg[0] += dt;
    g_tocando_seg[1] += dt;
    g_desde_log_no += dt;
    tick_usar(dt);
    calcular();
    if (vibrar_tick(g_host->openvr(), dt) && g_log && g_pulsos_log < 3) {
        g_pulsos_log++;
        g_host->log(g_host, "vibro (pared al tacto), mandos %d/%d%s", vibrar_indice(0), vibrar_indice(1),
                    g_pulsos_log == 3 ? " (no lo anoto mas)" : "");
    }
    acum_log += dt;
    if (acum_log < g_intervalo)
        return;
    acum_log = 0.0f;
    if (g_log && (g_cuadros_frenada[0] || g_cuadros_frenada[1]))
        g_host->log(g_host, "frenada: izq %d de %d cuadros (hasta %.0f%% del brazo), der %d (hasta %.0f%%)",
                    g_cuadros_frenada[0], g_cuadros, g_min_permitido[0] * 100.0f,
                    g_cuadros_frenada[1], g_min_permitido[1] * 100.0f);
    if (g_log && (g_toques[0] || g_toques[1]))
        g_host->log(g_host, "pared al tacto: %d toques izq, %d der", g_toques[0], g_toques[1]);
    g_toques[0] = g_toques[1] = 0;
    g_cuadros = g_cuadros_frenada[0] = g_cuadros_frenada[1] = 0;
    g_min_permitido[0] = g_min_permitido[1] = 1.0f;
}

static void f_config(void)
{
    leer_ini();
    if (!g_activado)
        g_permitido[0] = g_permitido[1] = 1.0f;
    g_host->log(g_host, "ini: Activado=%d Margen=%.2f Radio=%.2f Adelante=%.2f Escala=%.2f Filtro=%u Log=%d Intervalo=%.1f PruebaSubirDerecha=%.2f",
                g_activado, g_margen, g_radio, g_adelante, g_escala, g_filtro, g_log, g_intervalo, g_subir_der);
    g_host->log(g_host, "ini: Vibrar=%d VibrarMs=%.0f VibrarFuerza=%.2f VibrarCada=%.1f UsarAlTocar=%d Tecla=%02x UsarCada=%.1f",
                g_vibrar, g_vibrar_ms, g_vibrar_fuerza, g_vibrar_cada, g_usar_al_tocar, g_tecla_usar, g_usar_cada);
    g_host->log(g_host, "ini: SoltarToque=%.3f SoltarRetroceso=%.2f ToqueMax=%.1f",
                g_soltar, g_soltar_retroceso, g_toque_max);
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
    if (pausado) {
        g_permitido[0] = g_permitido[1] = 1.0f;
        if (g_tecla_abajo > 0.0f)
            tecla_usar(0);
        g_tecla_abajo = 0.0f;
        vibrar_cortar();
    }
}

static void f_terminar(void)
{
    g_permitido[0] = g_permitido[1] = 1.0f;
    if (g_tecla_abajo > 0.0f)
        tecla_usar(0);
    g_tecla_abajo = 0.0f;
    vibrar_cortar();
    g_subir_der = 0.0f;
    g_host = NULL;
}

/* La entrada GetLastPoses de la tabla del IVRCompositor que usa MotherVR.
 * Verifica que MotherVR sea la versión conocida. */
static void **entrada_last_poses(const Anfitrion *a)
{
    unsigned char *base = (unsigned char *)GetModuleHandleA("dxgi.dll");
    unsigned char *lee;
    void **comp;
    if (!base) {
        a->log(a, "no encuentro dxgi.dll (MotherVR)");
        return NULL;
    }
    lee = base + MVR_LLAMADA;
    if (IsBadReadPtr(lee, 5) || lee[0] != 0xa1 || *(unsigned char **)(lee + 1) != base + MVR_COMPOSITOR) {
        a->log(a, "MotherVR no es la version conocida (0.8.1): no engancho las poses");
        return NULL;
    }
    comp = *(void ***)(base + MVR_COMPOSITOR);
    if (!comp || IsBadReadPtr(comp, 4) || IsBadReadPtr(*comp, 4 * (SLOT_LAST_POSES + 1))) {
        a->log(a, "MotherVR todavia no tiene el compositor de SteamVR");
        return NULL;
    }
    return &(*(void ***)comp)[SLOT_LAST_POSES];
}

__declspec(dllexport) int mod_iniciar(const Anfitrion *a, Feature *f)
{
    void **slot;
    if (a->version != ANFITRION_VERSION)
        return ANFITRION_VERSION;
    g_host = a;
    g_delta = a->delta;
    if (!g_cs_lista) {
        InitializeCriticalSection(&g_cs);
        g_cs_lista = 1;
    }
    a->log(a, VERSION " (recarga %d)", a->recarga);
    f_config();

    slot = entrada_last_poses(a);
    if (slot) {
        g_poses_real = (FnLastPoses)a->enganchar_puntero(a, slot, (void *)mi_poses);
        if (!g_poses_real)
            a->log(a, "no pude enganchar las poses de SteamVR");
    }

    f->tam = sizeof *f;
    f->prioridad = 30;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
