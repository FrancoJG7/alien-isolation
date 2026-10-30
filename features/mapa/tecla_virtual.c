#include <windows.h>
#include "tecla_virtual.h"

#define DIR_TABLA           0x01759b44u   /* [ ] + 0x48 = mapa de acciones de juego */
#define OFS_MAPA_JUEGO      0x48
#define OFS_ACCIONES        0x04          /* acción i en mapa + 4 + i*0x40 */
#define TAM_ACCION          0x40
#define OFS_DISPOSITIVOS    0x103c        /* + tipo*4 */
#define OFS_CANT_ACCIONES   0x1054
#define MAX_TIPOS           6
#define MAX_TECLAS          4
#define MAX_CLASES          3

typedef char (__attribute__((thiscall)) *FnAbajo)(void *disp, unsigned codigo);

static volatile LONG g_abajo;
static int g_n;
static void *g_disp[MAX_TECLAS];
static unsigned g_cod[MAX_TECLAS];

/* Teclas sueltas: dispositivo y hasta 8 códigos, cada uno apretado o no. */
#define MAX_SUELTAS 8
static void *volatile g_disp_sueltas;
static volatile LONG g_suelta_cod[MAX_SUELTAS] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static volatile LONG g_hay_sueltas;

/* Un enganche por clase de dispositivo (cada una tiene su vtable). */
static void *g_vtabla[MAX_CLASES];
static FnAbajo g_real[MAX_CLASES];

static int forzada(void *disp, unsigned codigo)
{
    int i;
    if (g_hay_sueltas && disp == g_disp_sueltas)
        for (i = 0; i < MAX_SUELTAS; i++)
            if (g_suelta_cod[i] == (LONG)(codigo & 0xffff))
                return 1;
    if (!g_abajo)
        return 0;
    for (i = 0; i < g_n; i++)
        if (g_disp[i] == disp && g_cod[i] == (codigo & 0xffff))
            return 1;
    return 0;
}

#define ENVOLTORIO(k) \
    static char __attribute__((thiscall)) abajo_##k(void *disp, unsigned codigo) \
    { return forzada(disp, codigo) ? 1 : g_real[k](disp, codigo); }
ENVOLTORIO(0)
ENVOLTORIO(1)
ENVOLTORIO(2)
static void *const g_envoltorios[MAX_CLASES] = { (void *)abajo_0, (void *)abajo_1, (void *)abajo_2 };

static void *leer_ptr(unsigned dir)
{
    void **p = (void **)(UINT_PTR)dir;
    if (!dir || IsBadReadPtr(p, 4))
        return NULL;
    return *p;
}

unsigned char *tv_mapa(unsigned delta)
{
    unsigned char *t = (unsigned char *)leer_ptr(DIR_TABLA + delta);
    if (!t || IsBadReadPtr(t + OFS_MAPA_JUEGO, OFS_CANT_ACCIONES + 4))
        return NULL;
    return t + OFS_MAPA_JUEGO;
}

int tv_estado(unsigned delta, int accion)
{
    unsigned char *m = tv_mapa(delta);
    if (!m || accion < 0 || accion >= 64)
        return -1;
    return m[OFS_ACCIONES + accion * TAM_ACCION + 0x38];
}

static int enganchar_clase(const Anfitrion *a, void **vt)
{
    int k;
    for (k = 0; k < MAX_CLASES; k++)
        if (g_vtabla[k] == vt)
            return 1;
    for (k = 0; k < MAX_CLASES; k++)
        if (!g_vtabla[k]) {
            g_real[k] = (FnAbajo)a->enganchar_puntero(a, &vt[1], g_envoltorios[k]);
            if (!g_real[k])
                return 0;
            g_vtabla[k] = vt;
            return 1;
        }
    return 0;
}

int tv_preparar(const Anfitrion *a, int accion)
{
    unsigned char *m = tv_mapa(a->delta), *ac, *comb;
    unsigned n, k;
    int cuantas = 0;
    if (!m)
        return 0;
    if (accion < 0 || (unsigned)accion >= *(unsigned *)(m + OFS_CANT_ACCIONES) || accion >= 64)
        return -1;
    ac = m + OFS_ACCIONES + accion * TAM_ACCION;
    comb = *(unsigned char **)(ac + 0x30);
    n = *(unsigned *)(ac + 0x34);
    if (!comb || n > 16 || IsBadReadPtr(comb, n * 8))
        return -1;
    g_abajo = 0;
    g_n = 0;
    for (k = 0; k < n && cuantas < MAX_TECLAS; k++) {
        unsigned c = *(unsigned short *)(comb + k * 8 + 4);
        unsigned tipo = comb[k * 8 + 6], fl = comb[k * 8 + 7];
        unsigned char *d;
        if (c == 0xffff || (fl & 0x40) || tipo >= MAX_TIPOS)
            continue;
        d = *(unsigned char **)(m + OFS_DISPOSITIVOS + tipo * 4);
        if (!d || IsBadReadPtr(d, 0x14) || !d[0x10])
            continue;
        if (!enganchar_clase(a, *(void ***)d)) {
            a->log(a, "tecla virtual: no pude enganchar el dispositivo tipo %u", tipo);
            continue;
        }
        g_disp[cuantas] = d;
        g_cod[cuantas] = c;
        a->log(a, "tecla virtual: accion %d -> codigo %04x del dispositivo tipo %u", accion, c, tipo);
        cuantas++;
    }
    g_n = cuantas;
    return cuantas ? 1 : -1;
}

void tv_apretar(int abajo)
{
    InterlockedExchange(&g_abajo, abajo ? 1 : 0);
}

int tv_apretada(void)
{
    return g_abajo != 0;
}

int tv_tecla_preparar(const Anfitrion *a, int tipo)
{
    unsigned char *m = tv_mapa(a->delta), *d;
    if (!m || tipo < 0 || tipo >= MAX_TIPOS)
        return 0;
    d = *(unsigned char **)(m + OFS_DISPOSITIVOS + tipo * 4);
    if (!d || IsBadReadPtr(d, 0x14) || !enganchar_clase(a, *(void ***)d))
        return 0;
    g_disp_sueltas = d;
    return 1;
}

void tv_tecla(unsigned codigo, int abajo)
{
    int i, libre = -1;
    for (i = 0; i < MAX_SUELTAS; i++) {
        if (g_suelta_cod[i] == (LONG)codigo) {
            if (!abajo)
                InterlockedExchange(&g_suelta_cod[i], -1);
            return;
        }
        if (g_suelta_cod[i] < 0 && libre < 0)
            libre = i;
    }
    if (abajo && libre >= 0) {
        InterlockedExchange(&g_suelta_cod[libre], (LONG)codigo);
        g_hay_sueltas = 1;
    }
}

void tv_teclas_soltar(void)
{
    int i;
    for (i = 0; i < MAX_SUELTAS; i++)
        InterlockedExchange(&g_suelta_cod[i], -1);
    g_hay_sueltas = 0;
}
