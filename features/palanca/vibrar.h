/* Vibración de los mandos VR por OpenVR (IVRSystem_017, la de MotherVR).
 *
 * OpenVR solo deja pulsos de hasta 4 ms por llamada: para un "toque" más
 * largo hay que repetirlo en cada cuadro. Esto lo hace vibrar_tick, que se
 * llama desde el tick de la feature (hilo del juego).
 *
 * También lee los botones de los mandos (GetControllerState), para gestos
 * con botones. Mano 0 = izquierda, 1 = derecha. */
#ifndef VIBRAR_H
#define VIBRAR_H

/* Busca los mandos (cada 2 s alcanza: cambian si se apagan o se reconectan). */
void vibrar_buscar_mandos(void *sys);
/* Índice de OpenVR del mando (-1 si no está). */
int vibrar_indice(int mano);

/* Pide "veces" toques de "ms" milisegundos con "pausa_ms" entre ellos.
 * fuerza: 0 a 1. Pisa lo que estuviera vibrando en esa mano. */
void vibrar_pedir(int mano, int veces, float ms, float pausa_ms, float fuerza);
/* Corre los toques pendientes. dt en segundos. Devuelve 1 si mandó un pulso. */
int vibrar_tick(void *sys, float dt);
/* Corta todo (pausa, descarga). */
void vibrar_cortar(void);

/* Botones del mando (máscara de OpenVR) y gatillo de 0 a 1. 0 si no se pudo leer. */
int vibrar_botones(void *sys, int mano, unsigned long long *apretados, float *gatillo);

/* Stick (eje 0) de -1 a 1; x derecha, y arriba. 0 si no se pudo leer. */
int vibrar_stick(void *sys, int mano, float *x, float *y);

#define BOTON_AGARRE        (1ull << 2)
#define BOTON_GATILLO       (1ull << 33)

#endif
