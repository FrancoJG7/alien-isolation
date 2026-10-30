#include <math.h>
#include "mapa_gesto.h"

float gesto_medir(const float visor[3][4], const float mando[3][4], int eje, int signo, float *angulo)
{
    float d[3], n = 0.0f, p = 0.0f;
    int j;
    for (j = 0; j < 3; j++) {
        d[j] = visor[j][3] - mando[j][3];
        n += d[j] * d[j];
    }
    n = sqrtf(n);
    if (!(n > 1e-4f)) {
        *angulo = 180.0f;
        return n;
    }
    for (j = 0; j < 3; j++)
        p += signo * mando[j][eje] * d[j] / n;
    if (p > 1.0f)
        p = 1.0f;
    if (p < -1.0f)
        p = -1.0f;
    *angulo = acosf(p) * 57.29578f;
    return n;
}

int gesto_leer_eje(const char *t, int *eje, int *signo)
{
    *signo = 1;
    while (*t == ' ')
        t++;
    if (*t == '-' || *t == '+') {
        *signo = *t == '-' ? -1 : 1;
        t++;
    }
    if (*t == 'x' || *t == 'X')
        *eje = 0;
    else if (*t == 'y' || *t == 'Y')
        *eje = 1;
    else if (*t == 'z' || *t == 'Z')
        *eje = 2;
    else
        return 0;
    return 1;
}
