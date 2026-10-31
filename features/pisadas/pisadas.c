/* pisadas.dll (en mods_vr): sentís en los mandos cada paso del alien.
 *
 * Con el alien caminando cerca (menos de Distancia metros por el camino),
 * cada vez que avanza una Zancada se manda un golpe sordo a los dos mandos,
 * más fuerte cuanto más cerca. Si está en otro piso (más de AlturaMax de
 * diferencia), no se siente.
 *
 * De dónde sale todo: el gestor de música del juego ([0x16b1758], ver
 * ANALISIS_GHIDRA_3.md) ya elige al alien y calcula su distancia por el
 * camino (navmesh) para la música dinámica:
 *   +0xe5 activo, +0x188 alien, +0x18c distancia (m), +0x190 estado
 *   (1 presente, 2 "fuera": ¿ductos?), +0x1d4 sigilo, +0x1d8 peligro.
 * La posición del alien y la de Ripley están en +0x70..+0x78 del personaje.
 * Los pasos salen de cuánto se movió el alien entre cuadros.
 *
 * Nada de esto está probado en el juego: Log=1 anota lo que se lee del
 * gestor (cada 2 s mientras haya alien) para confirmar las unidades, cuál
 * es el eje de la altura y qué es el estado 2. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "anfitrion.h"
#include "vibrar.h"

#ifndef VERSION
#define VERSION "pisadas v0.1.0"
#endif

#define DIR_MANAGER_PJ      0x016f0c88u
#define DIR_MUSICA          0x016b1758u
#define OFS_POS             0x70
#define SALTO_MAX           2.0f      /* m en un cuadro: teletransporte o cambio de alien, no es un paso */

static const Anfitrion *g_host;
static unsigned g_delta;
static int g_pausado;

/* Configuración */
static int g_activado = 1;
static float g_distancia = 12.0f;   /* m por el camino */
static float g_zancada = 1.1f;      /* m que avanza el alien por paso */
static float g_altura_max = 2.5f;   /* m de diferencia de altura; 0 = no mirar */
static int g_eje_altura = 1;        /* 0 x, 1 y, 2 z del mundo del juego */
static float g_fuerza_min = 0.15f, g_fuerza_max = 0.8f;
static float g_ms = 28.0f;          /* duración del golpe */
static int g_solo_presente = 1;     /* 1 = no con el estado 2 */
static int g_log = 1;

/* Estado */
static void *g_alien_antes;
static float g_pos_antes[3];
static float g_recorrido;
static float g_t_mandos = 10.0f, g_t_log = 10.0f;
static int g_estado_antes = -1, g_pasos;

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

static int leer_pos(const unsigned char *p, float v[3])
{
    if (!p || IsBadReadPtr(p + OFS_POS, 12))
        return 0;
    memcpy(v, p + OFS_POS, 12);
    return v[0] == v[0] && v[1] == v[1] && v[2] == v[2];    /* no NaN */
}

static void sin_alien(void)
{
    g_alien_antes = NULL;
    g_recorrido = 0.0f;
}

static void f_tick(float dt)
{
    void *sys = g_host->openvr();
    unsigned char *mus, *alien, *pj;
    float dist, sigilo, peligro, pa[3], pr[3], dy = 0.0f, mov = 0.0f;
    int estado, j, h, hay_pj;

    if (dt > 0.1f)
        dt = 0.1f;
    g_t_mandos += dt;
    if (g_t_mandos > 2.0f) {
        g_t_mandos = 0.0f;
        vibrar_buscar_mandos(sys);
    }
    vibrar_tick(sys, dt);
    if (!g_activado || g_pausado) {
        sin_alien();
        return;
    }

    mus = leer_ptr((void *)(UINT_PTR)(DIR_MUSICA + g_delta));
    if (!mus || IsBadReadPtr(mus, 0x1dc) || !mus[0xe5]) {
        sin_alien();
        return;
    }
    alien = *(unsigned char **)(mus + 0x188);
    dist = *(float *)(mus + 0x18c);
    estado = *(int *)(mus + 0x190);
    sigilo = *(float *)(mus + 0x1d4);
    peligro = *(float *)(mus + 0x1d8);
    if (estado != g_estado_antes) {
        logf_("estado del alien %d -> %d (distancia %.1f m)", g_estado_antes, estado, dist);
        g_estado_antes = estado;
    }
    if (!alien || !leer_pos(alien, pa)) {
        sin_alien();
        return;
    }

    pj = jugador();
    hay_pj = pj && leer_pos(pj, pr);
    h = g_eje_altura;
    if (hay_pj)
        dy = pa[h] - pr[h];

    /* Cuánto avanzó el alien en el plano (sin la altura). */
    if (alien == g_alien_antes) {
        for (j = 0; j < 3; j++)
            if (j != h)
                mov += (pa[j] - g_pos_antes[j]) * (pa[j] - g_pos_antes[j]);
        mov = sqrtf(mov);
        if (mov > SALTO_MAX)
            mov = 0.0f;
    } else {
        g_recorrido = 0.0f;
    }
    g_alien_antes = alien;
    memcpy(g_pos_antes, pa, sizeof pa);

    g_t_log += dt;
    if (g_t_log > 2.0f && dist < 40.0f) {
        g_t_log = 0.0f;
        logf_("alien %p estado %d dist %.1f m sigilo %.2f peligro %.2f | alien (%.1f %.1f %.1f) ripley (%.1f %.1f %.1f) altura %+.1f | %.2f m/s, %d pasos",
              alien, estado, dist, sigilo, peligro, pa[0], pa[1], pa[2],
              hay_pj ? pr[0] : 0.0f, hay_pj ? pr[1] : 0.0f, hay_pj ? pr[2] : 0.0f, dy,
              dt > 0.0f ? mov / dt : 0.0f, g_pasos);
    }

    if ((g_solo_presente && estado != 1) || !(dist < g_distancia) ||
        (g_altura_max > 0.0f && fabsf(dy) > g_altura_max)) {
        g_recorrido = 0.0f;
        return;
    }
    g_recorrido += mov;
    if (g_recorrido >= g_zancada) {
        float cerca = 1.0f - dist / g_distancia, f;
        if (cerca < 0.0f)
            cerca = 0.0f;
        f = g_fuerza_min + (g_fuerza_max - g_fuerza_min) * cerca * cerca;
        g_recorrido -= g_zancada;
        if (g_recorrido > g_zancada)
            g_recorrido = 0.0f;
        vibrar_pedir(0, 1, g_ms, 0.0f, f);
        vibrar_pedir(1, 1, g_ms, 0.0f, f);
        g_pasos++;
    }
}

/* --- .ini --- */
static float leer_float(const char *clave, float def, float min, float max)
{
    char buf[32], d[32];
    float v;
    snprintf(d, sizeof d, "%g", def);
    GetPrivateProfileStringA("Pisadas", clave, d, buf, sizeof buf, g_host->ini);
    v = (float)atof(buf);
    return v >= min && v <= max ? v : def;
}

static void f_config(void)
{
    const char *ruta = g_host->ini;
    char buf[8];
    g_activado = GetPrivateProfileIntA("Pisadas", "Activado", 1, ruta);
    g_distancia = leer_float("Distancia", 12.0f, 1.0f, 60.0f);
    g_zancada = leer_float("Zancada", 1.1f, 0.2f, 5.0f);
    g_altura_max = leer_float("AlturaMax", 2.5f, 0.0f, 20.0f);
    GetPrivateProfileStringA("Pisadas", "EjeAltura", "y", buf, sizeof buf, ruta);
    g_eje_altura = buf[0] == 'x' || buf[0] == 'X' ? 0 : buf[0] == 'z' || buf[0] == 'Z' ? 2 : 1;
    g_fuerza_min = leer_float("FuerzaMin", 0.15f, 0.0f, 1.0f);
    g_fuerza_max = leer_float("FuerzaMax", 0.8f, 0.0f, 1.0f);
    g_ms = leer_float("Ms", 28.0f, 4.0f, 200.0f);
    g_solo_presente = GetPrivateProfileIntA("Pisadas", "SoloPresente", 1, ruta);
    g_log = GetPrivateProfileIntA("Pisadas", "Log", 1, ruta);
    logf_("ini: Activado=%d Distancia=%.1f Zancada=%.2f AlturaMax=%.1f EjeAltura=%c Fuerza %.2f-%.2f Ms=%.0f SoloPresente=%d",
          g_activado, g_distancia, g_zancada, g_altura_max, "xyz"[g_eje_altura], g_fuerza_min, g_fuerza_max, g_ms, g_solo_presente);
}

static void f_pausa(int pausado)
{
    g_pausado = pausado;
    if (pausado)
        vibrar_cortar();
}

static void f_terminar(void)
{
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
    f->prioridad = 70;
    f->tick = f_tick;
    f->config = f_config;
    f->pausa = f_pausa;
    f->terminar = f_terminar;
    return ANFITRION_VERSION;
}
