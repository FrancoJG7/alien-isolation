"""Parchea el XINPUT1_3.dll de GRAND para que cargue xinput_ag.dll.

GRAND carga el XInput de Windows con GetSystemDirectoryA + "\\XINPUT1_3.dll".
El parche saltea GetSystemDirectoryA y cambia el nombre por "xinput_ag.dll",
así Windows lo busca primero en la carpeta del juego. xinput_ag.dll reenvía
todo al XInput real.

Uso:  python parchear_grand.py XINPUT1_3.dll
Deja el original como XINPUT1_3.dll.original y escribe el parcheado encima.
Si el archivo no es el GRAND esperado, no toca nada.
"""
import hashlib
import shutil
import struct
import sys

MD5_ORIGINAL = "9d9baa61d34b8e3233b777dfe031014f"
MD5_PARCHEADO = "d26a5956f6781c7e94cbb6b040a560c7"
IMAGE_BASE = 0x10000000

# (va, bytes originales, bytes nuevos)
PARCHES = [
    # call [GetSystemDirectoryA]  ->  add esp, 8 ; xor eax, eax ; nop
    (0x100367E8, bytes.fromhex("ff1544200910"), bytes.fromhex("83c40831c090")),
    # "\XINPUT1_3.dll" (16 bytes copiados con movups)  ->  "xinput_ag.dll"
    (0x100A1FA4, b"\\XINPUT1_3.dll\0\0", b"xinput_ag.dll\0\0\0"),
]
# Relocación del operando del call que se borra (si no, Windows la pisaría).
RELOC_A_BORRAR = 0x100367EA


def secciones(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = struct.unpack_from("<H", d, pe + 20)[0]
    tabla = pe + 24 + opt
    res = []
    for i in range(nsec):
        o = tabla + 40 * i
        vsize, va, rsize, raw = struct.unpack_from("<IIII", d, o + 8)
        res.append((va, max(vsize, rsize), raw))
    reloc_dir = struct.unpack_from("<II", d, pe + 24 + 96 + 5 * 8)
    return res, reloc_dir


def rva_a_off(secs, rva):
    for va, size, raw in secs:
        if va <= rva < va + size:
            return rva - va + raw
    raise ValueError("RVA fuera de secciones: %x" % rva)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    ruta = sys.argv[1]
    d = bytearray(open(ruta, "rb").read())
    md5 = hashlib.md5(d).hexdigest()
    if md5 == MD5_PARCHEADO:
        print("Este XINPUT1_3.dll ya estaba parcheado. No hace falta hacer nada.")
        return 0
    if md5 != MD5_ORIGINAL:
        print("Este XINPUT1_3.dll no es el GRAND esperado (md5 %s). No se toco nada." % md5)
        return 1
    secs, (reloc_rva, reloc_size) = secciones(d)

    for va, orig, nuevo in PARCHES:
        o = rva_a_off(secs, va - IMAGE_BASE)
        if bytes(d[o:o + len(orig)]) != orig:
            print("Bytes inesperados en %08x. No se toco nada." % va)
            return 1
        d[o:o + len(nuevo)] = nuevo

    # Buscar y anular la relocación del operando del call.
    objetivo = RELOC_A_BORRAR - IMAGE_BASE
    o = rva_a_off(secs, reloc_rva)
    fin = o + reloc_size
    borrada = False
    while o < fin:
        pagina, tam = struct.unpack_from("<II", d, o)
        if tam == 0:
            break
        for i in range((tam - 8) // 2):
            pos = o + 8 + 2 * i
            ent = struct.unpack_from("<H", d, pos)[0]
            if ent >> 12 == 3 and pagina + (ent & 0xFFF) == objetivo:
                struct.pack_into("<H", d, pos, 0)  # IMAGE_REL_BASED_ABSOLUTE = relleno
                borrada = True
        o += tam
    if not borrada:
        print("No encontre la relocacion esperada. No se toco nada.")
        return 1

    shutil.copyfile(ruta, ruta + ".original")
    open(ruta, "wb").write(d)
    print("Listo. GRAND parcheado (md5 %s)." % hashlib.md5(d).hexdigest())
    print("Original guardado como %s.original" % ruta)
    return 0


if __name__ == "__main__":
    sys.exit(main())
