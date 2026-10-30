# Análisis con Ghidra para las ideas (26/10/2026)

AI.exe Steam (md5 bb0d932f267c0ce1b1045e68a3d12ffb), Ghidra 11.4.2, direcciones sin rebasar (base 0x400000). Solo análisis estático: **nada probado en el juego**. `pj` = personaje del jugador (GetPlayer 0x78baf0).

## Resumen

| Punto | Estado | Lo más importante |
|---|---|---|
| A3 pedidos de Ripley | Casi todo | El control de Ripley lee correr, agacharse, apuntar, disparar, linterna y tracker de la misma tabla de acciones. Asomarse sigue sin aparecer |
| A1 arma, vida | Bien | Vida en `pj+0x33c`; arma equipada en el gestor de equipo |
| A2 sentidos | Bien | Se puede leer cuánto se llenó cada sentido del alien hacia Ripley; el tipo de personaje distingue al alien |
| A7 cinemática / control | A medias | Hay un estado actual del jugador con número; falta mapear los números |
| A4 objeto delante | No | Vive en la HUD; hace falta más trabajo |
| Aire del locker | Pista | Lo maneja el script del escondite (CATHODE), no el exe |

## A3. El control de Ripley y la tabla de acciones

- Tabla global de acciones: `[0x1759b44]`, una entrada cada 0x40 bytes. El control de Ripley guarda un puntero a `[0x1759b44]+0x48` en `[pj+0x31c]+0x44` y lee de ahí cada cuadro (por ejemplo 0x7b0b10, 0x7a37e0).
- Desplazamiento relativo de cada acción = `0x3c + índice*0x40`, con el índice del menú de controles (0x803d70, tabla en 0x174a4e8). Bit 1 = recién apretado, bit 2 = mantenido. Confirmados en código:

| Acción | Índice | Relativo | Absoluto | Quién la lee |
|---|---|---|---|---|
| Correr | 8 | 0x23c | 0x284 | 0x679d60 (bit 2) |
| Agacharse | 9 | 0x27c | 0x2c4 | 0x677c00 (bit 1) |
| Apuntar | 10 | 0x2bc | 0x304 | 0x678260 |
| Disparar/tirar | 11 | 0x2fc | 0x344 | 0x678260 |
| Recargar | 16 | 0x43c | 0x484 | 0x677df0 (bit 1) |
| ¿Usar/agarrar? | 17 | 0x47c | 0x4c4 | 0x678200 (pickup, bit 2) |
| Asomarse | 18 | 0x4bc | 0x504 | sin lector directo encontrado |
| Tracker | 28 | 0x73c | 0x784 | 0x67a590, 0x67c5a0 (bit 2) |
| Linterna | 29 | 0x77c | 0x7c4 | 0x5974d8 (bit 1) |

- **Lo nuevo:** MonitorActionMap (0x5a8790, el que tocamos en la v0.1.4) es solo un lector más. Ripley lee la tabla por su cuenta. Escribir el bit en la tabla en el momento justo (después de que el juego la llena con el mando o el teclado y antes de que se actualice Ripley) debería funcionar para todas estas acciones. Falta encontrar ese momento y probarlo.
- Estado de Ripley:
  - `pj+0x2b8` = corriendo (lo escribe 0x679d60).
  - `pj+0x2ba` = linterna prendida. El botón llama 0x8cd440 (prender) o 0x8cd410 (apagar) según este byte. Escribir el byte solo no prende la luz: hay que pasar por ahí.
  - `pj+0x2b0`, `+0x2b2`, `+0x2b4` = estados que impiden agacharse (`+0x2b4` = escondida, lo limpia 0x7cb370).
  - `pj+0x3e38..0x3e3b` = banderas de postura (`+0x3e3a` = pedido de agachado, ya conocido).
  - `[pj+0x388]` = estado actual del jugador, número en `+0x1e` (se ven 6, 7, 0x19, 0x1a). Falta saber qué es cada uno.
- Asomarse: "Peek_System" es un mensaje que se manda al entrar a ciertos estados (0x670c30). El lector del botón no aparece con búsqueda directa; probablemente pasa por una función que recibe el índice.

## A1. Arma, equipo y vida

- **Vida:** `pj+0x33c` (ushort, actual), `pj+0x33e` (ushort, máximo), `pj+0x340` (float, multiplicador). Porcentaje = actual / (máximo × multiplicador) (0x836ab0, la HUD).
- **Gestor de equipo:** `[[pj+0x31c]+0x3c]`.
  - `+0x3bc0` = personaje, `+0x3bc8` = objeto equipado (`+0x54` = listo, `+0x6c` = otro estado; vtable `+0x40` = es arma, `+0x48`, `+0x50`).
  - `+0x3bec` / `+0x3bf0` / `+0x3bf4` = cambio de objeto pendiente (0x773a90, 0x773bd0).
  - `+0x3be8`, `+0x3be9` = banderas del tracker (0x67c5a0, 0x773bb0).
- Recargar: 0x677df0 lee la acción de recargar y llama 0x7d7860 y 0x7ce740.

## A2. Sentidos del alien

- **Tipo de personaje:** `[npc+0x64]+4`. Por el orden de los textos (CharacterClass: PLAYER, ALIEN, ANDROID, CIVILIAN, SECURITY, FACEHUGGER, INNOCENT, MELEE_HUMAN, ANDROID_HEAVY), 1 = alien. Lo usa 0x7cb370 para avisarle al alien cuando Ripley deja de estar agachada. Verificar con log.
- **Cuánto se llenó un sentido:** en el objeto sentido (el que recibe 0x729120), `+0x5c` = activación acumulada de 0 a 1, `+0x64` = tipo de sentido (0 = vista), `+0x34..0x40` = hora en que pasó cada uno de los 4 niveles, `+0x68` → gestor (con el NPC en `+8`). El objetivo es `+0x42c`.
- 0x729120 es la entrada única para "por qué me vio" (F8) y para el latido por activación (M7).
- Los bips del tracker siguen siendo el evento tipo 2 de `microfono/NOTAS.md`.

## A7. Cinemática y control

- `[pj+0x64]+0x1c` = es el jugador; `[pj+0x64]+0x1f` se usa junto con él para habilitar el sistema de asomarse (0x670c30). Sigue siendo el mejor candidato a "jugador con control".
- El número de estado en `[pj+0x388]+0x1e` es la forma más directa de saber si Ripley está en escalera, escondite o animación. Mapear los números con un log en el juego es más rápido que en Ghidra.

## Aire del locker

- El juego del aire lo controla la entidad de script Custom_Hiding_Controller (métodos Get_In, Add_NPC, Start_Breathing_Game, End_Breathing_Game; despachador 0x9105f0). El exe solo reenvía; la duración debería estar en los datos del nivel (CATHODE), editables con OpenCAGE. Es la mejor pista para `AireSegundos`.

## Qué falta

1. Momento justo para escribir en la tabla de acciones (quién la llena: candidatos 0x5a9380 y 0x5a8160).
2. Lector del botón de asomarse.
3. Números de estado de `[pj+0x388]+0x1e` (con log).
4. Objeto de interacción (A4): está del lado de la HUD (0x83e320, `+0x350`, `+0x3f4`); no se siguió.
