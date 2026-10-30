# Análisis Ghidra 3: sentidos del alien y lista de personajes (29/10/2026)

Todo estático (AI.exe Steam, base 0x400000, sumar `delta` del anfitrión). Nada probado en el juego.

## Resumen

1. **El juego ya calcula la "tensión" con el alien** para la música dinámica. Está en un objeto global y se puede **leer sin enganchar nada** desde `tick` (hilo del juego):
   - qué alien es, a cuántos metros está **por el camino** (navmesh, no en línea recta), si está "presente" o no, y dos valores de 0 a 1: "cerca pero sin notarte" y "te está notando".
2. **Los sentidos de cada NPC hacia Ripley también se leen directo** (sin enganchar 0x729120): cuánto se llenó cada sentido (vista, ruido de movimiento, linterna...) y los umbrales de cada nivel. El juego tiene su propia función de consulta.
3. Con 1 y 2 alcanzan para A12 (latido por activación), A7 (lado del alien), 1 (pasos), 2 (alien por arriba), 23 (otro piso), 6 (patrón por sentido), 7 (modo entrenamiento) y 11 (volver a respirar).

## 1. Gestor de música / tensión: `[0x16b1758]`

Se actualiza cada cuadro: SOUND_UPDATE 0x783650 → 0x6beb10 → **0x6bcea0** (solo si `+0xe5` != 0).

| Campo | Qué es |
|---|---|
| `+0xe5` | byte: el sistema está activo |
| `+0x188` | puntero al alien elegido (tipo 1; si hay varios, el más cercano según `[npc+0x1320]+0x38`). 0 = no hay alien |
| `+0x18c` | distancia del alien a Ripley **por el camino** (m). Arranca en 100. Usa `[npc+0x1324]` (+0x34 nodo, +0x38 distancia) y el grafo `FUN_008546b0(7)` |
| `+0x190` | estado: 0 sin alien, **1 alien presente** (`[alien+0x11a4]` == 0), **2 alien "fuera"** (`+0x11a4` != 0; probablemente en los ductos o fuera de escena: confirmar con log) |
| `+0x1d4` | **(1 − atención) × cercanía**: alto = está cerca y no te nota (tensión de sigilo) |
| `+0x1d8` | **(0.5 × cercanía + 0.5) × atención × factor[estado]**, recortado a 0..1: alto = te está notando. factor = `+0x1e8` (estado 1) o `+0x1e4` |

- cercanía = 1 − min(1, distancia × 0.05) → 0 a 20 m o más.
- atención (estado 1) = `FUN_006d5270(sensor, nivel 2, tipo 7)` = activación del sentido COMBINED / umbral del nivel 2, recortado a 0..1. En otro estado: `FUN_006e0460(jugador)`.
- Los valores van a Wwise como RTPC (0x855810 → 0xc2b620), en `+0x1ac`, `+0x1b4`, `+0x1bc`, `+0x1c4`.
- Si no hay alien, el mismo código busca el **humano hostil más cercano que te tiene de objetivo** (`+0x34a8` != 0 y objetivo == jugador); su tipo queda en una variable local.

**Para el mod:** cada cuadro leer `g = *(int*)(0x16b1758 + delta)`; si `g` y `*(char*)(g+0xe5)`: `alien = *(int*)(g+0x188)`, `dist = *(float*)(g+0x18c)`, `estado = *(int*)(g+0x190)`, `sigilo = *(float*)(g+0x1d4)`, `peligro = *(float*)(g+0x1d8)`.

## 2. Sentidos de un NPC hacia un objetivo

- **Gestor de sentidos del NPC:** `gs = [npc+0x34d4]`. `gs+0x8c` = el NPC dueño. `gs+0xb4 + tipo*2` / `+0xb5` = sentido pausado; `gs+0xc4` = todo pausado.
- **Un "sensor" por objetivo**, en lista: `nodo = [gs+0xc]`; `nodo+0` sensor, `nodo+8` siguiente. El sensor cuyo `+0x42c` == jugador es el del alien hacia Ripley. El juego lo busca con **0x6d57f0** (thiscall, ECX = gs; args `int *salida, objetivo`; suma una referencia en `sensor+0x14`, hay que devolverla: `--[s+0x14]` y si llega a 0 llamar `[[s+0x10]+4]`. Más simple: recorrer la lista a mano, sin tocar referencias).
- **Sensor:**
  - `+8` NPC, `+0x42c` objetivo, `+0x430` handle del objetivo.
  - `+0x3dc..0x3e4` última posición del objetivo vista, `+0x46c` hora, `+0x470` velocidad del objetivo hacia el NPC.
  - `+0x474` **visibilidad actual** del objetivo (entra al sentido de vista en 0x731be0 → 0x72ef40). `+0x460` otro valor de vista (distancia o alcance), `+0x490`/`+0x491`/`+0x495` banderas.
  - **8 sentidos** de 0x74 bytes desde `+0x18`: `sentido(t) = sensor + 0x18 + t*0x74`. Tipos: 0 vista, 1 ruido de arma, 2 ruido de movimiento, 3 daño, 4 tocado, 5 lanzallamas, 6 linterna, 7 COMBINED.
- **Sentido** (relativo a su comienzo):
  - `+0x5c` **activación** (0..1); `+0x54` hora del último estímulo; `+0x64` tipo; `+0x68` → sensor.
  - `+0x70` está sintiendo ahora (byte); `+0x71`, `+0x72` banderas.
  - `+0x00..+0x2c` posición del estímulo al pasar cada uno de los 4 niveles (3 floats c/u); `+0x34..+0x40` hora de cada nivel; `+0x6c..+0x6f` dato por nivel.
  - O sea, activación del tipo t = `*(float*)(sensor + 0x74 + t*0x74)`. Vista = `sensor+0x74`; COMBINED = `sensor+0x3a0`.
- **Umbrales de los 4 niveles:** `umbral(t, n) = *(float*)([npc+0x1154] + base[t] + n*4 + modo*0x1a8)` con
  - base = {0: 0x2e4, 1: 0x318, 2: 0x388, 3: 0x3c0, 4: 0x3f8, 5: 0x42c, 6: 0x350, 7: 0x44c} (de 0x6cbfe0);
  - modo = `[npc+0x4274]` (0..3) = **SENSE_SET** que pone el árbol de conducta o el nivel (NPC_SetSenseSet, DecoratorSetSenseSet; 0x785ab0, 0x793b00).
- **Consultas del propio juego:**
  - **0x6d5330** (thiscall, ECX = sensor; `tipo, nivel, solo_si_activo`): 1 si el sentido llegó a ese nivel.
  - **0x6d5270** (thiscall, ECX = sensor; `nivel, tipo`): activación / umbral(nivel).
- **0x729120** (entrada única para subir un sentido): thiscall, ECX = sentido, `ret 0x24` (9 args). Prólogo `64 A1 00 00 00 00` (mov eax, fs:[0]; 6 bytes enteros, sin saltos): se puede enganchar si hace falta saber **qué** subió y **desde dónde** (arg 1 = posición del estímulo), pero para A12 no hace falta.
- La caída de la activación con el tiempo la hace otro código (0x72ab20 llama a 0x729120 con modo 2 desde 0x72f310). El log lo va a mostrar.

## 3. Lista de personajes

- `[[0x16f0c88]+8]+0xd0` array, `+0xd4` cantidad (confirmado de nuevo en 0x6bcea0 y 0x734690). Hay **una segunda lista** en `+0x120` / `+0x124` que el juego recorre igual (¿personajes de otro tipo o inactivos? log).
- Tipo `[npc+0x64]+4` (1 = alien, otra vez confirmado: 0x6bcea0 elige al alien con `== 1`).
- Posición `npc+0x70..0x78`; la velocidad sale de la diferencia entre cuadros.
- **¿Está activo?** El juego saltea a los que dan 1 en **0x78b330** (fastcall, ECX = npc; mira `[vt+0x1c]()+0x2cf4` y `+0x2f90` en 1..3). Llamarla desde `tick` antes de usar un NPC.
- Distancia ya calculada por el juego: `[npc+0x1320]+0x38` (la usa para elegir al alien más cercano; confirmar con log que son metros hasta Ripley).

## 4. Qué destraba

| Idea | Con qué |
|---|---|
| A12 latido por activación | `+0x1d8` del gestor de música (o activación COMBINED / umbral) |
| A6 latido por distancia (respaldo) | `+0x18c` (camino real, mejor que línea recta) |
| A7 latido del lado del alien | posición del alien `+0x188` → `+0x70` contra la orientación del visor |
| 1 pasos del alien | velocidad del alien por diferencia de posición + `+0x18c` < 10 |
| 2 alien por arriba, 12 se fue a los ductos | altura del alien − la tuya, y `+0x190` pasa a 2 |
| 23 ignorar si está en otro piso | altura |
| 6 patrón por sentido, 7 modo entrenamiento | activación por tipo (vista 0, movimiento 2, linterna 6) |
| 8 silencio en el locker | `pj+0x2b4` + `+0x18c` chico |
| 11 volver a respirar | `+0x18c` pasa de < 8 a > 20 |
| N-6 "estás iluminada" (parcial) | `sensor+0x474` visibilidad |

## 5. Lo que falta confirmar con un log (versión de diagnóstico)

Cada 0.5 s, cuando cambia algo: los campos del gestor de música; por cada NPC activo, tipo, distancia, altura relativa, `+0x11a4`, `[+0x1320]+0x38`, modo (`+0x4274`) y la activación de los 8 sentidos de su sensor hacia Ripley con los umbrales del nivel 0..3. Responde: unidad de la distancia, qué significa el estado 2, cómo sube y baja la activación, y qué umbrales tiene el alien.
