#!/bin/bash
# Compila el cargador y todas las features (o las que se pasen: ./compilar.sh dedos mapa).
# En Windows: MSYS2 con mingw-w64-i686-gcc (terminal "MSYS2 MINGW32" o con
# C:\msys64\mingw32\bin en el PATH). En Linux: gcc-mingw-w64-i686.
# Las DLL quedan en salida/ (salida/mods_vr/*.dll y salida/xinput_ag.dll).
set -e
cd "$(dirname "$0")"
CC=${CC:-$(command -v i686-w64-mingw32-gcc || echo gcc)}
FLAGS="-O2 -Wall -Wno-cast-function-type -shared -static -static-libgcc -s"
mkdir -p salida/mods_vr

if [ $# -eq 0 ]; then
    set -- cargador $(ls features)
fi
for n in "$@"; do
    if [ "$n" = cargador ]; then
        (cd cargador && $CC $FLAGS -o ../salida/xinput_ag.dll cargador.c diferidos.c tecla.c xinput_ag.def -Wl,--enable-stdcall-fixup)
        echo "ok: xinput_ag.dll"
        continue
    fi
    d=features/$n
    [ -d "$d" ] || { echo "no existe la feature $n"; exit 1; }
    fuentes=$(ls $d/*.c | grep -v '/prueba_')
    rm -f salida/mods_vr/$n.dll     # si no compila, que instalar.sh no encuentre la vieja
    $CC $FLAGS -o salida/mods_vr/$n.dll $fuentes
    echo "ok: mods_vr/$n.dll"
done
