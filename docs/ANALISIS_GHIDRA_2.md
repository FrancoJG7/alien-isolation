# Análisis con Ghidra, segunda tanda (26/10/2026)

AI.exe Steam (md5 bb0d932f…), direcciones sin rebasar. Solo análisis estático: **nada probado en el juego**. Sigue a `ANALISIS_GHIDRA.md`.

## Resumen

| # | Parte | Resultado | Ideas nuevas o destrabadas |
|---|---|---|---|
| 1 | Quién llena la tabla de botones | **Encontrado**, con una forma limpia de "apretar" botones | Todo el grupo B, más minijuegos |
| 2 | Estados de Ripley | El exe no tiene nombres para los números; sale con log | C1 (cámara en muertes) |
| 3 | Interacción ("usar") | **No encontrado** | Sigue bloqueado |
| 4 | Comportamiento del alien | Hay condiciones "revisar al jugador escondido"; la conducta vive en datos | Aviso cuando revisa tu escondite |
| 5 | Respiración de Ripley | Parámetro de sonido BREATH_RATE por personaje | Respiración del juego según tu cuerpo |
| 6 | Sacudidas de cámara | Todas pasan por un mismo efecto "CameraShake" | Sacudidas suaves para VR |
| 7 | Luz y oscuridad | La IA usa la luz que cae sobre cada personaje, con una curva en datos | "Estás iluminada" |
| 8 | Armas | Función lista que da balas cargadas y de reserva | Contar balas con el cuerpo |
| 9 | Androides y humanos | Pistas pobres: conducta en árboles de datos | Siguen difíciles |
| 10 | Minijuegos | Son entidades de script que leen la misma tabla de botones | Con la 1, moderados |
| Extra | Vibración del propio juego | Existe PadRumbleImpulse | Pasar la vibración del juego a los mandos VR |

## 1. La tabla de botones por dentro

- Actualización por cuadro: **0x986d80** (fastcall, ECX = mapa de acciones). La llama 0x9909fa bajo el candado `0x17582e4`, para tres mapas: `+0x48` (el de juego, el que lee Ripley), `+0x2998` y `+0x3a0c` (menús, supongo).
- Cada acción (cada 0x40 bytes desde `mapa+0x34`): `+0x34` puntero a combinaciones (8 bytes: `+4` código de tecla, `+6` tipo de dispositivo, `+7` banderas), `+0x38` cantidad, `+0x3c` estado: bit 1 recién apretado, 2 mantenido, 4 soltado, 0x20 inhibido hasta soltar (InhibitActionsUntilRelease), 0x80 abajo ahora.
- Para saber si una tecla está abajo, 0x986d80 le pregunta al dispositivo: `mapa+0x103c + tipo*4` → objeto dispositivo, función virtual `+4` (EstaAbajo(código)). Los códigos de teclado son scan codes de DirectInput (0x986d80 y 0xa0ed50).
- **Forma limpia de apretar botones:** envolver la función `+4` del dispositivo teclado (como ya hacemos con las poses de MotherVR) y devolver "abajo" para la tecla asignada a la acción que queremos. El juego hace todo lo demás (recién apretado, mantenido, soltado, inhibir) igual que con una tecla real. Plan B: escribir el estado justo después de 0x986d80.
- Lista de tareas por cuadro del motor (0x7d6370): START_NEXT_FRAME, PLAYER_CAMERA_UPDATE, CAMERA_UPDATE (0x7b4090), CONTROLLER_UPDATE (0x783430), AI_START_THINK, …, CPU_LIGHT_SAMPLER_UPDATE (0x7755c0), SOUND_UPDATE (0x783650), PLATFORM_UPDATE (0x7ca180). Sirve para elegir dónde enganchar cada cosa.

## 2. Estados de Ripley

- `[pj+0x388]+0x1e` es un número de estado (se vieron 6, 7, 0x19, 0x1a). Los textos ATOMIC_* son de la IA de los NPC, no de este número. Lo más rápido es un log en el juego que anote el número cuando cambia (escalera, locker, muerte, palanca, ducto).

## 3. Interacción

- No encontré dónde guarda el juego el objeto que tenés delante. La HUD (0x83e320, 0x84bbe0) solo muestra u oculta la indicación. Con un log que anote la indicación visible y los punteros del controlador del jugador se podría encontrar en el juego.

## 4. Alien

- Tipo de personaje confirmado otra vez: 0x791030 le crea el sonido `%s_BREATH` solo a los de `[x+0x64]+4 == 1` → 1 = alien.
- Condiciones del árbol de conducta: ATTACK_HIDING_PLAYER (0x8b8bb0) y COULD_RESPOND_TO_HIDING_PLAYER (0x8b9610). Engancharlas dice cuándo el alien está considerando tu escondite.
- La conducta en sí (búsqueda, olfatear, lockers) está en árboles de datos (LegendPlugin), editables con OpenCAGE, no en el exe.

## 5. Respiración de Ripley

- Cada personaje tiene un bloque de parámetros de sonido en `+0x11a0` (constructor 0x783dc0) con BREATH_RATE (`+0xf0`), VELOCITY y MOVEMENT_SPEED, que se mandan a Wwise. Falta ver quién lo calcula; leerlo en vivo con log y probar a pisarlo es lo siguiente.
- Entidades de script: CHR_ModifyBreathing, CHR_HoldBreath (el juego del aire del locker es Custom_Hiding_Controller, ver la primera tanda).

## 6. Sacudidas de cámara

- Todas las sacudidas se crean como efecto "CameraShake" (creación 0x4293b0; ejemplo en 0x665260, con intensidad en `+0x128` y posición en `+0x108`). 0x419ea0 lee sus parámetros (amplitudes, frecuencias, duración) y 0x430120 los aplica. Un solo lugar para multiplicarlas por 0,3 o apagarlas.

## 7. Luz y oscuridad

- Hay un muestreador de luz por personaje en CPU (tarea CPU_LIGHT_SAMPLER_UPDATE → 0x9c6b20 sobre `0x1765c90`) y una curva AILightCurveSettings (datos) que la IA usa. Falta ver dónde se lee el valor de Ripley.

## 8. Armas

- Objeto en la mano: `[[pj+0x31c]+0x3c]+0x3bec` (vtable `+0x20` nombre, `+0x48` es arma).
- **0x8177b0** (cdecl): recibe el personaje y devuelve balas cargadas, de reserva y nombres (lo usa el contador de la HUD, 0x8433e0). Se puede llamar desde nuestro DLL.

## 9. Androides y humanos

- Hay actividades como ACT_THREAT_AWARE, pero la reacción a que les apunten o a manos arriba no aparece en el exe: sería agregar conducta en los árboles de datos. Sigue difícil.

## 10. Minijuegos

- UI_ReactionGame, UI_Keypad, HackingGame y RewireSystem son entidades de script que leen la tabla de botones. Con la "tecla virtual" del punto 1 se pueden manejar con gestos (girar la muñeca = izquierda/derecha).

## Extra: vibración del propio juego

- Existe la entidad PadRumbleImpulse: el juego manda vibraciones al mando (golpes, explosiones, cuando te agarra un androide). Nuestro DLL ya es el XInput, así que recibe esas órdenes por XInputSetState si el juego las manda, y puede pasarlas a los mandos VR. Duda: si el juego no manda vibración cuando no hay mando de Xbox, hay que engañarlo, y eso fue lo que rompió el menú en la v0.2.1. Probar primero si llegan.
