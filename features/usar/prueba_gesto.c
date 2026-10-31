/* gcc -O2 -o prueba_gesto prueba_gesto.c usar_gesto.c -lm && ./prueba_gesto */
#include <stdio.h>
#include <string.h>
#include "usar_gesto.h"

static int fallas;
#define VER(c, msg) do { if (!(c)) { printf("FALLA: %s\n", msg); fallas++; } } while (0)

/* Visor en (0, 1.6, 0) mirando hacia -z, o girado 90 grados (mirando a +x). */
static void visor_mirando(float v[3][4], int girado)
{
    memset(v, 0, sizeof(float) * 12);
    if (!girado) {
        v[0][0] = 1; v[1][1] = 1; v[2][2] = 1;
    } else {                        /* z del visor = -x del mundo: mira a +x */
        v[0][2] = -1; v[1][1] = 1; v[2][0] = 1;
    }
    v[1][3] = 1.6f;
}

static void mano_en(float m[3][4], float x, float y, float z)
{
    memset(m, 0, sizeof(float) * 12);
    m[0][0] = m[1][1] = m[2][2] = 1;
    m[0][3] = x; m[1][3] = y; m[2][3] = z;
}

int main(void)
{
    GestoUsar g = { 1.0f, 0.30f, 40.0f, 0.6f };
    EstadoUsar e;
    float v[3][4], m[3][4], f, d, a;
    float vel_frente[3] = { 0, 0, -1.5f }, vel_lado[3] = { 1.5f, 0, 0 }, vel_x[3] = { 1.5f, 0, 0 };
    int i, n;

    visor_mirando(v, 0);
    mano_en(m, 0.1f, 1.3f, -0.45f);
    usar_medir(v, m, vel_frente, &f, &d, &a);
    VER(f > 1.4f && d > 0.44f && a < 1.0f, "medir de frente");

    memset(&e, 0, sizeof e);
    e.armado = 1;
    VER(!usar_paso(&e, &g, f, d, a, 0.011f), "un cuadro no alcanza");
    VER(usar_paso(&e, &g, f, d, a, 0.011f), "dos cuadros empujan");
    for (n = 0, i = 0; i < 100; i++)
        n += usar_paso(&e, &g, f, d, a, 0.011f);
    VER(n == 0, "sin volver la mano no repite");
    usar_paso(&e, &g, -0.5f, 0.1f, 180, 0.011f);
    VER(!usar_paso(&e, &g, f, d, a, 0.011f) && usar_paso(&e, &g, f, d, a, 0.011f), "vuelve la mano y empuja de nuevo");

    memset(&e, 0, sizeof e);
    e.armado = 1;
    usar_medir(v, m, vel_lado, &f, &d, &a);
    VER(!usar_paso(&e, &g, f, d, a, 0.011f) && !usar_paso(&e, &g, f, d, a, 0.011f), "de costado no empuja");

    mano_en(m, 0.1f, 1.3f, -0.1f);
    usar_medir(v, m, vel_frente, &f, &d, &a);
    VER(!usar_paso(&e, &g, f, d, a, 0.011f) && !usar_paso(&e, &g, f, d, a, 0.011f), "mano cerca del cuerpo no empuja");

    visor_mirando(v, 1);
    mano_en(m, 0.45f, 1.3f, 0.0f);
    usar_medir(v, m, vel_x, &f, &d, &a);
    VER(f > 1.4f && d > 0.44f && a < 1.0f, "girado 90 grados");

    {
        float vel_abajo[3] = { 0, -1.5f, -1.5f }, vel_cae[3] = { 0, -3.0f, -1.0f };
        visor_mirando(v, 0);
        mano_en(m, 0.1f, 1.2f, -0.4f);
        usar_medir(v, m, vel_abajo, &f, &d, &a);
        VER(f > 1.4f && a < 1.0f, "empujar hacia abajo y adelante vale");
        usar_medir(v, m, vel_cae, &f, &d, &a);
        VER(a >= 89.0f, "casi todo hacia abajo no vale");
    }

    {
        /* Mirando 35 grados hacia arriba (o abajo) y empujando hacia donde mira. */
        float c = 0.819f, sn = 0.574f, vel_arriba[3] = { 0, 1.0f * sn, -1.0f * c }, vel_abajo2[3] = { 0, -1.0f * sn, -1.0f * c };
        memset(v, 0, sizeof v);
        v[0][0] = 1; v[1][1] = c; v[2][1] = sn; v[1][2] = -sn; v[2][2] = c;   /* z del visor = (0, -sn, c): mira a (0, sn, -c) */
        v[1][3] = 1.6f;
        mano_en(m, 0.1f, 1.6f + 0.4f * sn, -0.4f * c);
        usar_medir(v, m, vel_arriba, &f, &d, &a);
        VER(f > 0.95f && d > 0.35f && a < 2.0f, "mirando arriba, empujar hacia arriba vale entero");
        v[2][1] = -sn; v[1][2] = sn;
        mano_en(m, 0.1f, 1.6f - 0.4f * sn, -0.4f * c);
        usar_medir(v, m, vel_abajo2, &f, &d, &a);
        VER(f > 0.95f && d > 0.35f && a < 2.0f, "mirando abajo, empujar hacia abajo vale entero");
        v[2][1] = sn; v[1][2] = -sn;
        usar_medir(v, m, vel_lado, &f, &d, &a);
        VER(a > 60.0f, "mirando arriba, de costado sigue sin valer");
    }

    {
        /* Mirando 50 grados hacia abajo: tirar de una palanca (casi vertical) no
         * es empujón; empujar algo bajo (60 grados abajo) sí. */
        float c = 0.643f, sn = 0.766f, vel_tiron[3] = { 0, -1.5f, -0.3f }, vel_bajo[3] = { 0, -1.3f, -0.75f };
        memset(v, 0, sizeof v);
        v[0][0] = 1; v[1][1] = c; v[2][1] = -sn; v[1][2] = sn; v[2][2] = c;
        v[1][3] = 1.6f;
        mano_en(m, 0.1f, 1.6f - 0.4f * sn, -0.4f * c);
        usar_medir(v, m, vel_tiron, &f, &d, &a);
        VER(a >= 89.0f, "tirar de una palanca hacia abajo no empuja");
        usar_medir(v, m, vel_bajo, &f, &d, &a);
        VER(a < 20.0f && f > 1.2f, "empujar algo bajo mirandolo vale");
    }

    printf(fallas ? "%d fallas\n" : "ok\n", fallas);
    return fallas != 0;
}
