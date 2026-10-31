/* Tecla virtual: apretar una acción del juego como si fuera una tecla de
 * verdad (probado en el juego con agacharse, tecla_prueba v0.1).
 *
 * El juego pregunta en cada cuadro, para cada tecla de cada acción, si está
 * abajo: dispositivo->vtable[1](código). Envolvemos esa función en los
 * dispositivos de la acción y contestamos "abajo" mientras la apretamos.
 * El juego calcula solo recién apretado, mantenido y soltado. Como se
 * engancha el dispositivo, también lo ven los mapas de los menús que usen la
 * misma tecla (por ejemplo, Tab para cerrar el mapa). */
#ifndef TECLA_VIRTUAL_H
#define TECLA_VIRTUAL_H

#include "anfitrion.h"

/* Busca las teclas de la acción (índice del mapa de juego) y engancha sus
 * dispositivos. 1 = lista, 0 = todavía no hay mapa (probar más tarde),
 * -1 = no se pudo. Se puede llamar de nuevo con otra acción. */
int tv_preparar(const Anfitrion *a, int accion);
/* 1 = apretada, 0 = suelta. */
void tv_apretar(int abajo);
int tv_apretada(void);

/* Teclas sueltas de un dispositivo (tipo 1 = teclado; código DIK), además
 * de la acción: para cosas como mover el mapa con el stick derecho.
 * tv_tecla_preparar devuelve 1 si el dispositivo está enganchado. */
int tv_tecla_preparar(const Anfitrion *a, int tipo);
void tv_tecla(unsigned codigo, int abajo);
void tv_teclas_soltar(void);

/* El mapa de acciones de juego (o NULL) y el estado de una acción
 * (1 recién apretado, 2 mantenido, 4 soltado, 0x80 abajo). */
unsigned char *tv_mapa(unsigned delta);
int tv_estado(unsigned delta, int accion);

#endif
