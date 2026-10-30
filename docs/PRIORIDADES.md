# Qué tan fácil es cada idea después del análisis con Ghidra

26/10/2026. Reordena las ideas de `IDEAS.md` (números 1 a 17) e `IDEAS_v2.md` (F, M, D, E, N, X) con lo que salió en `ANALISIS_GHIDRA.md`. Nada probado en el juego.

## La llave: una sola prueba destraba muchas ideas

Ripley lee sus botones de una tabla del juego en cada cuadro (correr, agacharse, apuntar, disparar, recargar, asomarse, armas, tracker, linterna, objetos). Si nuestro DLL puede escribir en esa tabla en el momento justo, cualquier gesto se convierte en "apretar ese botón" **sin buscar cada acción por dentro**, y el juego sigue aplicando sus reglas. Es lo que falló con agacharse en la v0.1.4, pero ahí lo escribíamos solo para un lector (MonitorActionMap), no en la tabla que lee Ripley.

**Prueba propuesta:** una versión de diagnóstico con una tecla que "aprieta" la linterna escribiendo en la tabla, y otra que "mantiene" correr. Si la linterna se prende y Ripley corre, se abre todo el grupo B de abajo. Si no, hay que encontrar quién llena la tabla (candidatos 0x5a9380 y 0x5a8160) y escribir justo después.

## A. Fáciles ahora (no hace falta buscar nada más en el juego)

| Idea | Por qué es fácil ahora |
|---|---|
| F1 Techo del ducto | `pj+0x2b4` (escondida o forzada) + envoltorio de pose |
| F2 Correa invisible, E4 aviso de pared, E6 recentrar al salir del escondite | Solo envoltorio de pose y `+0x2b4` |
| F3 Vibración al agacharse | `+0x2af` ya vigilado; falta ver si la háptica llega |
| F4 Latido por distancia, E3 latido del lado del alien | Lista de NPC + tipo de personaje (`[npc+0x64]+4`, 1 = alien, a confirmar con log) |
| **N9 Vibración por daño y latido con vida baja** | Vida en `pj+0x33c` / `+0x33e` (antes moderada) |
| **E5 Tus pasos en el mando** | Posición de Ripley + `pj+0x2b8` (corriendo) |
| **17 Temblor al apuntar** | Objeto equipado en el gestor de equipo + acción de apuntar |
| **F8 Log de "por qué me vio"** | Enganche único en 0x729120: tipo de sentido en `+0x64`, objetivo en `+0x42c`, activación en `+0x5c` |
| **M7 Latido según cuánto le falta para verte** | Leer `+0x5c` del sentido de vista del alien hacia Ripley (antes moderada) |
| F5 Altura que se aprende sola, F6 modo sentado, F7 recargar .ini y tecla de pánico | Lógica propia |
| E1 Pausa al sacarte el visor, E2 latido audible, E7 mano en la oreja, E8 taparte los oídos, E11 jadeo | OpenVR + audio y volumen de Windows |
| E9 Resumen de la partida | Con F8 hecho |
| 8 Dificultad VR | Con F8 se ajusta con datos |

## B. Fáciles o moderadas **si la prueba de la tabla sale bien**

| Idea | Acción de la tabla |
|---|---|
| M8 Tapar la linterna con la mano (y la vieja 5) | Linterna (29); estado en `pj+0x2ba` |
| M4 Trotar para correr | Correr (8, mantenido) |
| 4 Enfocar el tracker acercándolo a la cara | Tracker (28, mantenido) |
| N6 Dos manos = apuntar | Apuntar (10) |
| **D8 Recarga con gesto** | Recargar (16). Era factible, pasa a fácil |
| **D7 Fundas en el cuerpo** | Armas 1 a 5 (19-23) y objetos (31-36). Era factible, pasa a moderada |
| **D5 Tirar con fuerza** | Disparar/tirar (11) mantenido según la velocidad de la mano. Era factible, pasa a moderada |
| 13 Botiquín al cuello | Objeto del botiquín (31-36, falta ver cuál) |
| 7 Manivela, N8 soplete | Machacar botón (15) o usar |
| N14 Salir del locker empujando | Usar o atrás, a confirmar |
| **M2 Asomarse con el cuerpo, M10 cobertura baja** | Asomarse (18). No hace falta encontrar quién lo lee. Sigue el problema de la cámara que se mueve dos veces, que se arregla en la pose |
| Melee por gesto (JayP) | Disparar con la llave equipada |

## C. Moderadas (necesitan algo más, pero hay pista concreta)

| Idea | Qué falta |
|---|---|
| M1 Cámara en muertes y animaciones | Mapear los números de estado de Ripley (`[pj+0x388]+0x1e`) con un log en el juego |
| M3 Contener la respiración + `AireSegundos` | El juego del aire es un script del nivel: buscar la duración con OpenCAGE. El gesto depende de la prueba de la tabla |
| M5 Sobresalto, N2 chocar mandos, N3 jadeo con ruido, N15 temblar en el locker, 10 y 12 | Probar en el juego la llamada de ruido (la misma del micrófono) |
| N1 Tracker al pecho, N10 bips que vibran | Engancharse al evento del tracker (tipo 2) |
| M6 Inmovilidad, N4 cuclillas, N5 mirada fija | En 0x729120 se puede escalar la activación solo para Ripley; es balance, ir con cuidado |
| M9 Rendija del locker | Con F2 hecho, falta la posición de la cámara del locker |
| E10 Mapa en la muñeca | Ver si el mapa es "menú de HUD" (38) o tiene su propia acción |

## D. Siguen siendo difíciles (factibles)

| Idea | Por qué |
|---|---|
| D2 empujar para abrir, D3 botones con el dedo, X3 agarrar del piso, X4 entrar al locker, N11 escaleras, 15 guardar con la tarjeta | Saber qué objeto tenés delante sigue sin encontrarse (vive en la HUD) |
| D1 El alien ve tu cabeza | Toca la visión; F2 + M2 cubren casi todo |
| X1 manos arriba, X2 apuntar a humanos, X5 brillo del tracker, X6 lado del golpe, X7 empujar androides | Mecánicas nuevas o IA humana |
| D4 Oreja contra la puerta | Wwise; E7 lo reemplaza bastante bien |
| N12 IED al piso, N13 pilas de la linterna | Hay que ver si el juego permite plantar o cambiar pilas a mano |

## Orden que propongo

1. **Versión de diagnóstico (v0.2.0):** prueba de la tabla de acciones (linterna y correr con tecla), prueba de vibración, log de estados de Ripley y log de "por qué me vio". Un solo build y una sola sesión de prueba tuya responden casi todas las dudas.
2. Con eso: F1, F2, F3, N9, F4/M7 y E1.
3. Si la tabla funciona: M8 linterna, D8 recarga, M2 asomarse, D7 fundas.
4. Después M1 (cámara en muertes) con los números de estado del log.

---

# Actualización después de la segunda tanda de Ghidra

Ver `ANALISIS_GHIDRA_2.md`.

## Cambia la prueba de la tabla de botones

Ahora sabemos cómo el juego decide si un botón está apretado: le pregunta al teclado (o al mando) "¿está abajo esta tecla?". La forma limpia es una **tecla virtual**: envolver esa pregunta y contestar "sí" para la tecla de la acción que queremos. El juego la trata igual que si la apretaras vos. Esto hace al grupo B bastante más seguro que antes; la versión de diagnóstico prueba esto mismo.

## Ideas nuevas

| Idea | Nivel | Cómo se juega | Qué la hace posible |
|---|---|---|---|
| N-1 Vibración del juego en los mandos VR | Fácil si el juego la manda sin mando de Xbox; moderada si no | Los golpes, explosiones y agarres que hacían vibrar el joystick ahora vibran en tus mandos | PadRumbleImpulse + nuestro DLL ya es el XInput |
| N-2 Sacudidas de cámara suaves | Moderada | Las sacudidas de explosiones y golpes se sienten, pero al 30 % (o apagadas), para no marear | Todas pasan por el efecto CameraShake |
| N-3 Contar balas con el cuerpo | Fácil | Al disparar la última bala, un "clic" distinto en el mando; con pocas balas, pulso de aviso | Función 0x8177b0 de la HUD |
| N-4 Respiración de Ripley según tu cuerpo | Moderada | Si te moviste mucho de verdad, Ripley respira agitada (con su propia voz, no un sonido nuestro) | Parámetro BREATH_RATE del sonido de cada personaje |
| N-5 Aviso cuando el alien revisa tu escondite | Moderada | Escondida, el latido cambia (o se corta) justo cuando el alien decide revisar dónde estás | Condiciones ATTACK_HIDING_PLAYER y COULD_RESPOND_TO_HIDING_PLAYER |
| N-6 "Estás iluminada" | Moderada a factible | Un pulso leve cuando entrás a una zona con luz, donde el alien te ve mejor | Muestreador de luz por personaje + curva de la IA |
| N-7 Minijuegos con las manos | Moderada | Hackear y el teclado numérico girando la muñeca o tocando | Son scripts que leen la tabla de botones: con la tecla virtual alcanza |

## Cambios en las ideas de antes

- **C1 cámara en muertes:** sigue moderada. El número de estado no tiene nombre en el exe; se mapea con el log.
- **Grupo D (objeto delante):** sigue bloqueado. No apareció dónde guarda el juego el objeto de "usar".
- **X1/X2 (manos arriba, apuntar a humanos):** siguen difíciles; la conducta humana está en datos, no en el exe.
- **B5 recarga con gesto:** suma que ahora podemos saber cuántas balas quedan (N-3), así que el gesto puede no hacer nada si el tambor está lleno.

## Orden actualizado

1. Versión de diagnóstico: tecla virtual (linterna, correr, recargar), prueba de vibración, ver si llegan las vibraciones del juego (N-1), log de estados y log de "por qué me vio".
2. Fáciles del grupo A, más N-1 y N-3.
3. Grupo B con la tecla virtual.
4. N-2 sacudidas suaves y C1 cámara en muertes.
