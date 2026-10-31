#include <math.h>
#include "usar_gesto.h"

#define CUADROS_MIN     2           /* dos cuadros seguidos, para no tomar un salto del tracking */

/* Medida sin la altura: el frente es hacia donde mira el visor, aplastado
 * contra el piso. */
static void medir_plano(const float visor[3][4], const float mano[3][4], const float vel[3],
                        float *hacia_frente, float *delante, float *angulo)
{
    float f[3] = { -visor[0][2], 0.0f, -visor[2][2] };
    float n = sqrtf(f[0] * f[0] + f[2] * f[2]), lado;
    *hacia_frente = *delante = 0.0f;
    *angulo = 180.0f;
    if (!(n > 1e-3f))
        return;                     /* mirando justo arriba o abajo */
    f[0] /= n;
    f[2] /= n;
    *hacia_frente = vel[0] * f[0] + vel[2] * f[2];
    *delante = (mano[0][3] - visor[0][3]) * f[0] + (mano[2][3] - visor[2][3]) * f[2];
    /* Solo el desvío hacia los costados: empujar hacia abajo (una computadora
     * sobre un escritorio) también vale, salvo que vaya casi todo para abajo. */
    lado = vel[0] * -f[2] + vel[2] * f[0];
    *angulo = atan2f(fabsf(lado), *hacia_frente) * 57.29578f;
    if (fabsf(vel[1]) > 2.0f * *hacia_frente)
        *angulo = 90.0f;
}

/* Medida hacia donde mira el visor, con la altura: un cajón arriba de la
 * cabeza o abajo se empuja mirándolo. */
static void medir_mirada(const float visor[3][4], const float mano[3][4], const float vel[3],
                         float *hacia_frente, float *delante, float *angulo)
{
    float f[3] = { -visor[0][2], -visor[1][2], -visor[2][2] }, lado[3];
    float n = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    int j;
    *hacia_frente = *delante = 0.0f;
    *angulo = 180.0f;
    if (!(n > 1e-3f))
        return;
    for (j = 0; j < 3; j++)
        f[j] /= n;
    *hacia_frente = vel[0] * f[0] + vel[1] * f[1] + vel[2] * f[2];
    *delante = (mano[0][3] - visor[0][3]) * f[0] + (mano[1][3] - visor[1][3]) * f[1] + (mano[2][3] - visor[2][3]) * f[2];
    for (j = 0; j < 3; j++)
        lado[j] = vel[j] - *hacia_frente * f[j];
    *angulo = atan2f(sqrtf(lado[0] * lado[0] + lado[1] * lado[1] + lado[2] * lado[2]), *hacia_frente) * 57.29578f;
    /* Mirando casi al frente, la mano que cae no es un empujón. */
    if (fabsf(f[1]) < 0.5f && fabsf(vel[1]) > 2.0f * *hacia_frente)
        *angulo = 90.0f;
    /* Bajar la mano casi derecho (tirar de una palanca mirándola) tampoco:
     * empujar algo bajo siempre lleva algo de adelante. */
    if (vel[1] < 0.0f && sqrtf(vel[0] * vel[0] + vel[2] * vel[2]) < 0.5f * -vel[1])
        *angulo = 90.0f;
}

void usar_medir(const float visor[3][4], const float mano[3][4], const float vel[3],
                float *hacia_frente, float *delante, float *angulo)
{
    float f, d, a;
    medir_plano(visor, mano, vel, hacia_frente, delante, angulo);
    medir_mirada(visor, mano, vel, &f, &d, &a);
    /* Se queda con la que dé el empujón más largo y más derecho. */
    if (a < 90.0f && f * cosf(a / 57.29578f) > *hacia_frente * cosf(*angulo / 57.29578f)) {
        *hacia_frente = f;
        *delante = d;
        *angulo = a;
    }
}

int usar_paso(EstadoUsar *e, const GestoUsar *g, float hacia_frente, float delante, float angulo, float dt)
{
    if (e->t_espera > 0.0f)
        e->t_espera -= dt;
    /* Para el siguiente empujón la mano tiene que volver hacia vos. */
    if (!e->armado && (delante < g->alcance * 0.6f || hacia_frente < -0.3f))
        e->armado = 1;
    if (e->t_espera > 0.0f || !e->armado
        || hacia_frente < g->velocidad || delante < g->alcance || angulo > g->angulo) {
        e->cuadros = 0;
        return 0;
    }
    if (++e->cuadros < CUADROS_MIN)
        return 0;
    e->cuadros = 0;
    e->t_espera = g->espera;
    e->armado = 0;
    return 1;
}
