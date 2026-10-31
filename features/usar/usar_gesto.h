/* Geometría del gesto "empujar con la palma" (sin Windows, para probarla
 * aparte). Matrices 3x4 de OpenVR: columnas 0-2 = ejes x, y, z del
 * dispositivo en el mundo, columna 3 = posición. El visor mira hacia -z. */
#ifndef USAR_GESTO_H
#define USAR_GESTO_H

typedef struct {
    float velocidad;        /* m/s mínimos de la mano hacia adelante */
    float alcance;          /* m mínimos de la mano por delante de los ojos */
    float angulo;           /* grados máximos entre el movimiento y el frente */
    float espera;           /* s entre un empujón y el siguiente */
} GestoUsar;

typedef struct {
    int cuadros;            /* cuadros seguidos cumpliendo */
    float t_espera;         /* > 0: recién empujó, no cuenta */
    int armado;             /* 1 = la mano volvió atrás desde el último empujón */
} EstadoUsar;

/* Mide el cuadro: cuánto va la mano hacia el frente (m/s), cuánto está por
 * delante de los ojos (m) y el ángulo entre su movimiento y el frente. El
 * frente es hacia donde mira el visor: con la altura (un cajón arriba o
 * abajo, mirándolo) o sin ella (empujar hacia abajo mirando al frente); se
 * queda con la que dé el empujón más derecho. */
void usar_medir(const float visor[3][4], const float mano[3][4], const float vel[3],
                float *hacia_frente, float *delante, float *angulo);

/* 1 en el cuadro en que se completa un empujón. */
int usar_paso(EstadoUsar *e, const GestoUsar *g, float hacia_frente, float delante, float angulo, float dt);

#endif
