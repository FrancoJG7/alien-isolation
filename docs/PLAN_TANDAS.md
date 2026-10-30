# Plan de tandas (elegido por Franco, 26/10/2026)

Lo que Franco marcó en el documento "Elegí qué implementar" (https://claude.ai/artifact/MNA3fQWBasTtZySnv9Etex), ordenado en tandas. Cada tanda es una versión que Franco prueba antes de seguir. Todas se construyen sobre las fuentes de agachar v0.1.9. Códigos: ver `DETALLE.md`, `IDEAS_v3.md` y `PRIORIDADES.md`.

**Marcadas:** diagnóstico; A1, A2, A3, A4, A10, A11, A12, A15, A16, A17a (latido audible), A17c (taparse los oídos), N-1, N-3; B1, B4, B9, B10, B11, N-7; C1, C2, C3, C4, C6, C7, N-2; 1, 2, 8, 9, 13, 17, 23, 29, 30 (sumada el 26/10 por sugerencia), 34.
**No marcadas** (al 26/10): el resto, incluidas 37-50 y las difíciles.

## Tandas

| Tanda | Nombre | Qué trae | Depende de |
|---|---|---|---|
| T0 | Diagnóstico (v0.2.0) | Tecla virtual de prueba (linterna, correr, recargar), prueba de vibración, ver si llegan las vibraciones del juego, log de estados de Ripley, A11 log de "por qué me vio", A15 .ini en vivo + tecla de pánico, 34 ajustar en vivo | Nada |
| T1 | Cuerpo honesto | A1 techo del ducto, A2 correa, A3 aviso de pared, A4 recentrar al salir, 29 calibrar con gesto, A16 pausa al sacarte el visor | T0 (vibración para A3) |
| T2 | Sentir al alien | A12 latido por activación (con A6 como respaldo interno), 1 pasos del alien, 2 alien por arriba, 23 ignorar otro piso, A10 temblor al apuntar, A17a latido audible | T0 (vibración, log de sentidos) |
| T3 | Tu estado al tacto | 13 linterna al tacto, 17 disparo con peso, N-3 contar balas, N-1 vibración del juego (si T0 muestra que llega) | T0 |
| T4 | Gestos | B1 tapar la linterna, B4 dos manos, B11 asomarse con el cuerpo, 30 mando sin tracking sin gestos | T0 (tecla virtual) |
| T5 | El locker | B10 salir empujando, C2 contener la respiración con AireSegundos, C6 rendija, 8 silencio en el locker, 9 encierro del locker, A17c taparse los oídos | T1 (correa), T4 (tecla virtual), OpenCAGE para el aire |
| T6 | Cámara | C1 cámara en muertes y animaciones, N-2 sacudidas suaves | T0 (números de estado) |
| T7 | Ruido | Juntar el micrófono con la v0.1.9 + C3 cosas que hacen ruido + C4 tracker al pecho y bips | Llamada de ruido probada |
| T8 | Manos en el mundo | B9 manivela y soplete, C7 mapa en la muñeca, N-7 minijuegos | T0, T6 (estados) |

## Por qué en este orden

- T0 primero: responde si la vibración y la tecla virtual funcionan; de eso dependen T1 a T5.
- T1 antes que las vibraciones: cambia cómo se siente esconderse y es lo más barato.
- T2 y T3 juntan todo lo que vibra, con un mezclador de vibraciones (prioridad: daño > alien > estado) para que no se pise todo.
- T5 junta todo lo del locker en una sola prueba.
- T7 al final de lo "seguro" porque toca a la IA (ruidos) y suma el micrófono.

## Opinión sobre lo marcado

- La selección es coherente: casi todo apunta a esconderse y al locker, que es donde la VR más suma. No hay nada marcado que choque con otra cosa.
- **Buenas combinaciones:** 8 (silencio en el locker) necesita que exista un latido, así que va con A12 y A17a. 9 (encierro) y A17c (taparse los oídos) tocan el mismo volumen, así que van con un solo control de volumen que tome el más bajo de los dos.
- **Lo más delicado:** B11 asomarse con el cuerpo (puede mover la cámara dos veces: se corrige en la pose, pero va a llevar pruebas) y C3 ruidos (cambia el balance de la IA: va apagado por defecto y con multiplicador en el .ini).
- **N-1 vibración del juego:** solo si T0 muestra que el juego la manda sin mando de Xbox. Si no la manda, no hay que engañarlo, porque eso rompió el menú en la v0.2.1.
- **A6 no marcada:** no hace falta, A12 la incluye.
- **Sumada (Franco aceptó):** 30 "mando sin tracking, sin gestos". Evita que B1, B4 y B11 se disparen solos cuando un mando pierde el seguimiento. Iría en T4.
- **Elegidas pero dependientes:** C1, B9 y N-7 dependen del log de estados de T0; C2 depende de encontrar la duración del aire con OpenCAGE.

## Qué falta saber para las ideas elegidas (26/10)

**Con Ghidra (lo puedo hacer yo):**
1. Qué tecla trae asignada cada acción por defecto (linterna, apuntar, asomarse, usar…). Hace falta para la tecla virtual (T0).
2. Cómo recorrer la lista de personajes y leer su posición y velocidad (A12, 1, 2, 23, 8).
3. Si el juego solo manda vibración cuando hay un mando de Xbox conectado (N-1).
4. Dónde se aplica la cámara de las animaciones y muertes respecto de la pose de MotherVR (C1), y el punto de CameraShake (N-2).
5. La posición de la cámara dentro del locker (C6) y cómo salir de él (B10).
6. La llamada que genera ruido para la IA (C3) y el evento de los bips del tracker (C4).
7. Qué acciones leen la manivela, el soplete y cada minijuego (B9, N-7), y si el mapa usa el menú de la HUD (C7).

**Con archivos del juego (OpenCAGE):**
8. La duración del juego del aire en el locker (C2). Necesito los archivos del nivel o que Franco lo mire con OpenCAGE.

**Solo jugando (versión de diagnóstico T0):**
9. Si la tecla virtual funciona, si la vibración llega a los mandos, los números de estado de Ripley y si llegan las vibraciones del juego.
