#include <windows.h>
#include <string.h>
#include "vibrar.h"

#define SLOT_INDICE_ROL     18      /* GetTrackedDeviceIndexForControllerRole */
#define SLOT_ESTADO_MANDO   33      /* GetControllerState */
#define SLOT_PULSO          35      /* TriggerHapticPulse */
#define MAX_DISPOSITIVOS    16
#define PULSO_MAX_US        3999

typedef unsigned (__attribute__((thiscall)) *FnIndiceRol)(void *sys, int rol);
typedef void (__attribute__((thiscall)) *FnPulso)(void *sys, unsigned indice, unsigned eje, unsigned short us);
typedef char (__attribute__((thiscall)) *FnEstadoMando)(void *sys, unsigned indice, void *estado, unsigned tam);

/* En Windows openvr.h lo empaqueta a 8 (el pack 4 es solo de Linux y Mac):
 * hay 4 bytes de relleno después de paquete y mide 64. Si el tamaño que se
 * pasa no coincide, SteamVR no contesta (fue el error de la v0.2.1). */
typedef struct {
    unsigned paquete;
    unsigned relleno;
    unsigned long long apretados;
    unsigned long long tocados;
    struct { float x, y; } eje[5];
} EstadoMando;                      /* VRControllerState_t, 64 bytes */
typedef char comprobar_tam_estado[sizeof(EstadoMando) == 64 ? 1 : -1];

static volatile LONG g_idx[2] = { -1, -1 };
static struct {
    int veces;                      /* toques que faltan, contando el actual */
    float ms, pausa_ms, fuerza;
    float t;                        /* ms dentro del toque actual (o de la pausa, si es negativo) */
} g_cola[2];

void vibrar_buscar_mandos(void *sys)
{
    int i;
    if (!sys)
        return;
    for (i = 0; i < 2; i++) {
        unsigned k = ((FnIndiceRol)(*(void ***)sys)[SLOT_INDICE_ROL])(sys, i + 1);
        g_idx[i] = k < MAX_DISPOSITIVOS ? (LONG)k : -1;
    }
}

int vibrar_indice(int mano)
{
    return mano == 0 || mano == 1 ? (int)g_idx[mano] : -1;
}

void vibrar_pedir(int mano, int veces, float ms, float pausa_ms, float fuerza)
{
    if ((mano != 0 && mano != 1) || veces <= 0 || !(ms > 0.0f) || !(fuerza > 0.0f))
        return;
    if (fuerza > 1.0f)
        fuerza = 1.0f;
    g_cola[mano].veces = veces;
    g_cola[mano].ms = ms;
    g_cola[mano].pausa_ms = pausa_ms > 0.0f ? pausa_ms : 0.0f;
    g_cola[mano].fuerza = fuerza;
    g_cola[mano].t = 0.0f;
}

int vibrar_tick(void *sys, float dt)
{
    int i, mando = 0;
    for (i = 0; i < 2; i++) {
        LONG k = g_idx[i];
        if (g_cola[i].veces <= 0)
            continue;
        if (g_cola[i].t >= 0.0f && sys && k > 0) {
            unsigned short us = (unsigned short)(g_cola[i].fuerza * PULSO_MAX_US);
            ((FnPulso)(*(void ***)sys)[SLOT_PULSO])(sys, (unsigned)k, 0, us);
            mando = 1;
        }
        g_cola[i].t += dt * 1000.0f;
        if (g_cola[i].t >= g_cola[i].ms) {
            g_cola[i].veces--;
            g_cola[i].t = -g_cola[i].pausa_ms;
        }
    }
    return mando;
}

void vibrar_cortar(void)
{
    memset(g_cola, 0, sizeof g_cola);
}

int vibrar_botones(void *sys, int mano, unsigned long long *apretados, float *gatillo)
{
    EstadoMando e;
    LONG k = vibrar_indice(mano);
    *apretados = 0;
    *gatillo = 0.0f;
    if (!sys || k <= 0)
        return 0;
    memset(&e, 0, sizeof e);
    if (!((FnEstadoMando)(*(void ***)sys)[SLOT_ESTADO_MANDO])(sys, (unsigned)k, &e, sizeof e))
        return 0;
    *apretados = e.apretados;
    *gatillo = e.eje[1].x;          /* eje 1 = gatillo */
    return 1;
}
