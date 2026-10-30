#!/bin/bash
# Copia las DLL compiladas a la carpeta del juego. Los .ini solo se copian si
# todavía no existen (así no se pisan tus ajustes). Con el juego abierto,
# xinput_ag.dll recarga sola cada feature que cambia en mods_vr.
# El cargador (xinput_ag.dll) solo se copia si se pide: ./instalar.sh cargador
set -e
cd "$(dirname "$0")"
JUEGO=${JUEGO:-"/c/Program Files (x86)/Steam/steamapps/common/Alien Isolation"}
[ -d "$JUEGO/mods_vr" ] || { echo "no encuentro $JUEGO/mods_vr"; exit 1; }
for dll in salida/mods_vr/*.dll; do
    n=$(basename "$dll" .dll)
    cp "$dll" "$JUEGO/mods_vr/$n.dll"
    echo "copiado: mods_vr/$n.dll"
    if [ ! -f "$JUEGO/mods_vr/$n.ini" ] && [ -f "features/$n/$n.ini" ]; then
        cp "features/$n/$n.ini" "$JUEGO/mods_vr/$n.ini"
        echo "copiado: mods_vr/$n.ini (no existia)"
    fi
done
if [ "$1" = cargador ]; then
    cp salida/xinput_ag.dll "$JUEGO/xinput_ag.dll"
    echo "copiado: xinput_ag.dll (hay que reiniciar el juego)"
fi
