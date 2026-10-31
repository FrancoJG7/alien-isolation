# Mods VR para Alien: Isolation

Mods de gameplay para jugar Alien: Isolation en VR con MotherVR y GRAND (versión de Steam).

## Cómo está armado

- `cargador/`: `xinput_ag.dll`, que el juego carga como XInput. Carga cada DLL de `mods_vr\` como una "feature", la recarga sola cuando cambia (con el juego abierto) y relee cada `.ini` cada 1 segundo. Si cambia el cargador, hay que reiniciar el juego.
- `features/<nombre>/`: fuentes y `.ini` por defecto de cada feature.
- `mods_vr/`: las DLL ya compiladas de la última versión de cada feature.
- `docs/`: notas, análisis con Ghidra e ideas.

| Feature | Versión | Qué hace |
|---|---|---|
| agachar | v0.2.4 | Agacharte de verdad agacha a Ripley; calibrar con los agarres y los brazos adelante |
| manos | v0.8 | Las manos no atraviesan paredes y vibran al tocar |
| mapa | v0.7.1 | Mapa en la muñeca, con el stick derecho |
| pausa | v0.1 | Pausa al sacarte el visor |
| usar | v0.1.6 | Usar con un empujón de la palma |
| dedos | v0.1.4 | Cerrar las manos con el gatillo o el agarre |
| pisadas | v0.1.0 | Sentís los pasos del alien en los mandos |
| bengala | v0.1.0 | Prender la bengala raspándola con la otra mano |

## Compilar e instalar

Hace falta `i686-w64-mingw32-gcc` (en Windows: MSYS2 con `mingw-w64-i686-gcc`).

```
./compilar.sh            # todo
./compilar.sh dedos      # solo una feature
./instalar.sh dedos      # copia esa DLL a mods_vr (guarda la vieja con su version; los .ini solo si no existen)
```

Con el juego abierto, la feature nueva se recarga sola en un segundo. Todo queda anotado en `ai_mods.log`, en la carpeta del juego.
