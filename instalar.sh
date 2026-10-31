#!/bin/bash
# Copia a la carpeta del juego las DLL compiladas de las features que se
# pasan (./instalar.sh dedos mapa). Sin argumentos no hace nada: hay que
# nombrar lo que se instala, para no pisar features que no se probaron.
# Antes de reemplazar una DLL, guarda la vieja como <feature>.dll.<version>.bak
# (la versión sale del texto "<feature> vX.Y.Z" que lleva adentro).
# Los .ini solo se copian si todavía no existen (así no se pisan tus ajustes).
# Con el juego abierto, xinput_ag.dll recarga sola cada feature que cambia.
# El cargador se instala con ./instalar.sh cargador (hay que reiniciar el juego).
set -e
cd "$(dirname "$0")"
JUEGO=${JUEGO:-"/c/Program Files (x86)/Steam/steamapps/common/Alien Isolation"}
[ -d "$JUEGO/mods_vr" ] || { echo "no encuentro $JUEGO/mods_vr"; exit 1; }
[ $# -gt 0 ] || { echo "uso: ./instalar.sh <feature>... (por ejemplo ./instalar.sh dedos)"; exit 1; }

respaldar() {   # $1 = archivo instalado, $2 = nombre
    [ -f "$1" ] || return 0
    v=$(grep -aoE "$2 v[0-9]+(\.[0-9]+)*" "$1" | head -1 | sed "s/^$2 //")
    [ -n "$v" ] || v=$(date +%Y%m%d-%H%M%S)
    [ -f "$1.$v.bak" ] || cp "$1" "$1.$v.bak"
    echo "respaldo: $(basename "$1").$v.bak"
}

for n in "$@"; do
    if [ "$n" = cargador ]; then
        respaldar "$JUEGO/xinput_ag.dll" cargador
        cp salida/xinput_ag.dll "$JUEGO/xinput_ag.dll"
        echo "copiado: xinput_ag.dll (hay que reiniciar el juego)"
        continue
    fi
    dll=salida/mods_vr/$n.dll
    [ -f "$dll" ] || { echo "no esta $dll: corre antes ./compilar.sh $n"; exit 1; }
    respaldar "$JUEGO/mods_vr/$n.dll" "$n"
    cp "$dll" "$JUEGO/mods_vr/$n.dll"
    echo "copiado: mods_vr/$n.dll"
    if [ ! -f "$JUEGO/mods_vr/$n.ini" ] && [ -f "features/$n/$n.ini" ]; then
        cp "features/$n/$n.ini" "$JUEGO/mods_vr/$n.ini"
        echo "copiado: mods_vr/$n.ini (no existia)"
    fi
done
