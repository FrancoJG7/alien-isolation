#include <math.h>
#include "usar_gesto.h"

#define CUADROS_MIN     2           /* dos cuadros seguidos, para no tomar un salto del tracking */

void usar_medir(const float visor[3][4], const float mano[3][4], const float vel[3],
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
