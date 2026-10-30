# Jugar con hand tracking del Quest 3 (idea, 26/10/2026)

Nada probado. Lo de Steam Link y Virtual Desktop es de memoria y hay que verificarlo.

## Por qué no es imposible

- MotherVR lee los mandos por OpenVR (SteamVR). Steam Link y Virtual Desktop pueden hacer pasar las manos por "mandos" virtuales: pellizco = gatillo, etc. Para el juego y para nuestro DLL, cada mano sigue siendo un mando con posición.
- Todos nuestros gestos usan la **posición** de la mano (tapar la linterna, dos manos, asomarse, correa, agacharse), así que andarían igual con las manos.
- Lo que falte se puede apretar con la **tecla virtual** (T0).

## Cómo sería

| Parte | Cómo | Dificultad |
|---|---|---|
| Probar sin mod | Steam Link o Virtual Desktop con manos activadas, ver si MotherVR responde al pellizco | Solo probar |
| Gestos del plan (B1, B4, B11, B10, 29, 30) | Igual que con mandos | Gratis |
| Caminar | Pellizcar con la izquierda y arrastrar = palanca (nosotros convertimos el arrastre en W/A/S/D con la tecla virtual), o trotar en el lugar para avanzar (M4) | Moderada |
| Girar | Girar el cuerpo de verdad, o pellizco con la derecha hacia un costado = giro por pasos | Fácil a moderada |
| Disparar / usar | Pellizco del índice (lo da el emulador) | Lo da el emulador |
| Armas, objetos, tracker | Gestos con la tecla virtual (fundas, levantar el antebrazo, etc.) | Moderada |
| Menús | Pellizco como gatillo apuntando | Lo da el emulador |

## Lo que se pierde

- **Vibración:** las manos no vibran. T2 y T3 casi todo es vibración, así que habría que pasarlo a sonido (latido audible, clics).
- **Manos fuera de la vista:** detrás de la espalda o en la cadera el visor no las ve, así que las fundas y la correa al hombro andan peor.
- **Puntería:** las manos tiemblan más y el pellizco mueve la mano al disparar.
- **Luz de tu pieza:** con poca luz real, el seguimiento falla.

## Orden sugerido

1. Probar sin mod si el juego responde a las manos por Steam Link o Virtual Desktop.
2. Si responde, al plan solo le suma una opción `ModoManos=1`: caminar con pellizco y arrastre, giro con gesto, y vibraciones cambiadas por sonidos.
