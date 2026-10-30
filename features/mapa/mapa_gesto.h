/* Geometría del gesto "mirar el reloj" (sin Windows, para probarla aparte).
 * Matrices 3x4 de OpenVR: columnas 0-2 = ejes x, y, z del dispositivo en el
 * mundo, columna 3 = posición. */
#ifndef MAPA_GESTO_H
#define MAPA_GESTO_H

/* eje: 0 x, 1 y, 2 z; signo: 1 o -1.
 * Devuelve la distancia mando-visor (m) y en *angulo cuántos grados hay
 * entre ese eje del mando y la dirección del mando a los ojos. */
float gesto_medir(const float visor[3][4], const float mando[3][4], int eje, int signo, float *angulo);

/* Lee "x", "-x", "+y", ... Devuelve 0 si no entiende. */
int gesto_leer_eje(const char *texto, int *eje, int *signo);

#endif
