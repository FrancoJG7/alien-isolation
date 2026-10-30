/* Feature de prueba para prueba_recarga.c (no va al juego).
 * - tick: cuenta cuadros y guarda la cuenta en el baúl;
 * - pose: suma 1 a out[11] (para ver la cadena de poses);
 * - engancha a pedido "objetivo" (código) y "tabla[0]" (puntero) del exe de
 *   prueba. Compilar con -DVERSION='"prueba A"' o B. */
#include <windows.h>
#include <stdio.h>
#include "../src/anfitrion.h"

typedef int (__cdecl *FnObjetivo)(int);

static const Anfitrion *g_host;
static int g_cuenta;
static FnObjetivo g_objetivo_real, g_tabla_real;

static int __cdecl mi_objetivo(int x) { return g_objetivo_real(x) + 100; }
static int __cdecl mi_tabla(int x) { return g_tabla_real(x) + 1000; }

static void f_tick(float dt) { (void)dt; g_cuenta++; }
static void f_pose(int cabeza, void *self, float *out, float escala) { (void)cabeza; (void)self; (void)escala; out[11] += 1.0f; }
static void f_terminar(void)
{
    g_host->baul_guardar(g_host, 1, &g_cuenta, sizeof g_cuenta);
    g_host = NULL;
}

__declspec(dllexport) int mod_iniciar(const Anfitrion *a, Feature *f)
{
    HMODULE exe = GetModuleHandleA(NULL);
    unsigned char *obj = (unsigned char *)GetProcAddress(exe, "objetivo");
    void **tabla = (void **)GetProcAddress(exe, "tabla");
    if (a->version != ANFITRION_VERSION)
        return ANFITRION_VERSION;
    g_host = a;
    if (a->baul_leer(a, 1, &g_cuenta, sizeof g_cuenta))
        a->log(a, "baul recuperado: cuenta %d", g_cuenta);
    a->log(a, VERSION " (recarga %d)", a->recarga);
    g_objetivo_real = (FnObjetivo)a->enganchar_codigo(a, obj, "5589e58b4508", 6, (void *)mi_objetivo);
    g_tabla_real = (FnObjetivo)a->enganchar_puntero(a, tabla, (void *)mi_tabla);
    f->tam = sizeof *f;
    f->prioridad = 20;
    f->tick = f_tick;
    f->pose = f_pose;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
