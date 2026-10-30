/* pausa.dll (en mods_vr): te sacás el visor y el juego se pausa solo.
 *
 * Cómo sabe que te lo sacaste (cualquiera de los dos, configurable):
 * - OpenVR: el nivel de actividad del visor (el sensor de cara) deja de ser
 *   "en uso" (GetTrackedDeviceActivityLevel, IVRSystem_017 slot 16);
 * - quieto: el visor no se mueve nada (ni un milímetro ni una décima de
 *   grado) durante Quieto segundos. Con el visor puesto la cabeza siempre se
 *   mueve un poco; apoyado en la mesa, no.
 *
 * Cuando pasan Segundos así, aprieta Esc una vez (SendInput con el código de
 * DirectInput, que es lo que lee el juego), solo si la ventana del juego
 * tiene el foco y Ripley está en control libre con su cámara (no en
 * cinemáticas). Al ponértelo no hace nada: sacás la pausa vos.
 *
 * Ojo: no sabemos todavía si el juego ya está en pausa. Si te sacás el visor
 * con el menú de pausa abierto, Esc lo cerraría; por eso no aprieta si
 * Ripley no está en control libre y anota en el log lo que ve, para buscar
 * la bandera de pausa. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"

#ifndef VERSION
#define VERSION "pausa v0.1"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define OFS_CONTROL         0x64
#define OFS_CAMARAS         0x58
#define OFS_CAM_ACTIVA      0x1d8
#define OFS_CAM_JUGADOR     0x1dc

#define SLOT_POSES          11      /* GetDeviceToAbsoluteTrackingPose */
#define SLOT_ACTIVIDAD      16      /* GetTrackedDeviceActivityLevel */
#define DIK_ESCAPE          0x01

typedef struct {
    float m[3][4];
    float vel[3], vel_ang[3];
    int resultado;
    unsigned char valida, conectado, relleno[2];
} PoseVR;

typedef void (__attribute__((thiscall)) *FnPoses)(void *sys, int universo, float prediccion, PoseVR *poses, unsigned n);
typedef int (__attribute__((thiscall)) *FnActividad)(void *sys, unsigned indice);

static const Anfitrion *g_host;
static unsigned g_delta;
static int g_pausado;               /* tecla de pánico */

/* Configuración */
static int g_activado = 1;
static float g_segundos = 1.0f;
static int g_usar_sensor = 1;
static float g_quieto = 2.0f;       /* 0 = no mirar si está quieto */
static int g_log = 1;

/* Estado */
static int g_sin_cara;              /* 1 = decidimos que no lo tenés puesto */
static float g_t_sin;               /* segundos seguidos que parece sacado */
static int g_ya_pauso;              /* ya apretó Esc en esta sacada */
static float g_t_quieto;
static float g_ref[3][4];
static int g_hay_ref;
static int g_actividad_antes = -99;
static float g_t_tecla = -1.0f;     /* >= 0: Esc está abajo desde hace tanto */
static float g_t_log;

#define logf_(...) do { if (g_log && g_host) g_host->log(g_host, __VA_ARGS__); } while (0)

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

/* Lo que vemos del juego, para saber si conviene apretar Esc (y, en el log,
 * para encontrar la bandera de pausa). */
typedef struct {
    int hay_pj, libre, camara_jugador;
    unsigned char ctl[8];           /* control + 0x1c..0x23 */
} Juego;

static void mirar_juego(Juego *j)
{
    unsigned char *pj = jugador(), *ctl;
    unsigned m;
    memset(j, 0, sizeof *j);
    if (!pj || IsBadReadPtr(pj, 0x400))
        return;
    j->hay_pj = 1;
    ctl = (unsigned char *)leer_ptr((unsigned)(UINT_PTR)(pj + OFS_CONTROL));
    if (ctl && !IsBadReadPtr(ctl + 0x1c, 8)) {
        memcpy(j->ctl, ctl + 0x1c, 8);
        j->libre = ctl[0x1f];
    }
    m = (unsigned)(UINT_PTR)leer_ptr(DIR_MANAGER_PJ + g_delta);
    if (m) {
        unsigned char *cm = (unsigned char *)leer_ptr(m + OFS_CAMARAS);
        if (cm && !IsBadReadPtr(cm, 0x1e0))
            j->camara_jugador = *(void **)(cm + OFS_CAM_ACTIVA) &&
                                *(void **)(cm + OFS_CAM_ACTIVA) == *(void **)(cm + OFS_CAM_JUGADOR);
    }
}

static void log_juego(const char *cuando)
{
    Juego j;
    mirar_juego(&j);
    logf_("%s: ripley %s, control libre %d, camara del jugador %d, control+1c %02x %02x %02x %02x %02x %02x %02x %02x",
          cuando, j.hay_pj ? "si" : "no", j.libre, j.camara_jugador,
          j.ctl[0], j.ctl[1], j.ctl[2], j.ctl[3], j.ctl[4], j.ctl[5], j.ctl[6], j.ctl[7]);
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

static void tecla_esc(int abajo)
{
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = DIK_ESCAPE;
    in.ki.dwFlags = KEYEVENTF_SCANCODE | (abajo ? 0 : KEYEVENTF_KEYUP);
    SendInput(1, &in, sizeof in);
}

/* 1 si el visor parece sacado según el sensor. -1 si no se sabe. */
static int sensor_sin_cara(void *sys)
{
    int a = ((FnActividad)(*(void ***)sys)[SLOT_ACTIVIDAD])(sys, 0);
    if (a != g_actividad_antes) {
        static const char *nombres[] = { "quieto 10 s", "en uso", "en uso (hace 0.5 s que no)", "en reposo" };
        logf_("sensor del visor: %d (%s)", a, a >= 0 && a <= 3 ? nombres[a] : "no se sabe");
        g_actividad_antes = a;
    }
    if (a == 1 || a == 2)
        return 0;
    if (a == 0 || a == 3)
        return 1;
    return -1;
}

/* Cuántos segundos lleva el visor sin moverse nada. */
static void mirar_quieto(void *sys, float dt)
{
    PoseVR p;
    int i, j, igual = 1;
    memset(&p, 0, sizeof p);
    ((FnPoses)(*(void ***)sys)[SLOT_POSES])(sys, 2, 0.0f, &p, 1);
    if (!p.valida) {
        g_t_quieto += dt;           /* sin tracking cuenta como quieto */
        return;
    }
    if (g_hay_ref)
        for (i = 0; i < 3; i++)
            for (j = 0; j < 4; j++) {
                /* 1 mm en la posición, ~0.1 grado en la rotación */
                float tol = j == 3 ? 0.001f : 0.002f;
                if (fabsf(p.m[i][j] - g_ref[i][j]) > tol)
                    igual = 0;
            }
    if (!g_hay_ref || !igual) {
        memcpy(g_ref, p.m, sizeof g_ref);
        g_hay_ref = 1;
        g_t_quieto = 0.0f;
        return;
    }
    g_t_quieto += dt;
}

static void f_tick(float dt)
{
    void *sys;
    int sin = 0, s;
    Juego j;

    /* Esc abajo unos 150 ms, para que el juego lo vea en algún cuadro. */
    if (g_t_tecla >= 0.0f) {
        g_t_tecla += dt;
        if (g_t_tecla > 0.15f) {
            tecla_esc(0);
            g_t_tecla = -1.0f;
        }
    }
    if (!g_activado || g_pausado || !(sys = g_host->openvr()))
        return;
    if (dt > 0.25f)
        dt = 0.25f;

    if (g_usar_sensor && (s = sensor_sin_cara(sys)) > 0)
        sin = 1;
    mirar_quieto(sys, dt);
    if (g_quieto > 0.0f && g_t_quieto >= g_quieto)
        sin = 1;

    g_t_sin = sin ? g_t_sin + dt : 0.0f;
    if (!g_sin_cara && g_t_sin >= g_segundos) {
        g_sin_cara = 1;
        g_ya_pauso = 0;
        logf_("te sacaste el visor (%s)", g_t_quieto >= g_quieto && g_quieto > 0.0f ? "no se mueve" : "sensor");
        log_juego("al sacarlo");
        mirar_juego(&j);
        if (!juego_con_foco())
            logf_("no pauso: el juego no tiene el foco");
        else if (!j.hay_pj || !j.libre || !j.camara_jugador)
            logf_("no pauso: Ripley no esta en control libre (menu, cinematica o ya en pausa)");
        else {
            tecla_esc(1);
            g_t_tecla = 0.0f;
            g_ya_pauso = 1;
            logf_("pauso (Esc)");
        }
        g_t_log = 0.0f;
    } else if (g_sin_cara && !sin) {
        g_sin_cara = 0;
        logf_("te pusiste el visor%s", g_ya_pauso ? " (la pausa la sacas vos)" : "");
        log_juego("al ponerlo");
    } else if (g_sin_cara) {
        /* Mientras está sacado, cada 3 s, lo que se ve del juego (ya en pausa). */
        g_t_log += dt;
        if (g_t_log > 3.0f) {
            g_t_log = 0.0f;
            log_juego("en pausa");
        }
    }
}

static void leer_ini(void)
{
    const char *ruta = g_host->ini;
    char buf[32];
    g_activado = GetPrivateProfileIntA("Pausa", "Activado", 1, ruta);
    g_usar_sensor = GetPrivateProfileIntA("Pausa", "Sensor", 1, ruta);
    g_log = GetPrivateProfileIntA("Pausa", "Log", 1, ruta);
    GetPrivateProfileStringA("Pausa", "Segundos", "1", buf, sizeof buf, ruta);
    g_segundos = (float)atof(buf);
    if (g_segundos < 0.5f || g_segundos > 60.0f)
        g_segundos = 1.0f;
    GetPrivateProfileStringA("Pausa", "Quieto", "2", buf, sizeof buf, ruta);
    g_quieto = (float)atof(buf);
    if (g_quieto < 0.0f || g_quieto > 60.0f)
        g_quieto = 2.0f;
}

static void f_config(void)
{
    leer_ini();
    logf_("ini: Activado=%d Segundos=%.1f Sensor=%d Quieto=%.1f Log=%d",
          g_activado, g_segundos, g_usar_sensor, g_quieto, g_log);
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
    g_t_sin = 0.0f;
}

static void f_terminar(void)
{
    if (g_t_tecla >= 0.0f)
        tecla_esc(0);
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
    f->prioridad = 50;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
