"""Corre las funciones de pose de MotherVR 0.8.1 (dxgi.dll) en un emulador
con un SteamVR falso y mira donde queda la altura del visor en la salida."""
import struct, sys, math
import pefile
from unicorn import *
from unicorn.x86_const import *

pe = pefile.PE(sys.argv[1])
img = pe.get_memory_mapped_image()
BASE = 0x10000000
mu = Uc(UC_ARCH_X86, UC_MODE_32)
mu.mem_map(BASE, (len(img) + 0xfff) & ~0xfff)
mu.mem_write(BASE, img)
HEAP, STACK, STUB = 0x30000000, 0x40000000, 0x50000000
mu.mem_map(HEAP, 0x100000); mu.mem_map(STACK, 0x100000); mu.mem_map(STUB, 0x1000)
# stubs: 0 = ret (cdecl, eax=7), 0x10 = GetLastPoses (thiscall ret 0xc / 0x10)
mu.mem_write(STUB, b"\xb8\x07\x00\x00\x00\xc3")
TOKEN = 7
mu.mem_write(0x100676b0, struct.pack('<I', STUB))           # VR_GetInitToken
mu.mem_write(0x1006af78, struct.pack('<I', TOKEN))
COMP = HEAP; VT = HEAP + 0x100; POSE_SRC = HEAP + 0x1000
mu.mem_write(COMP, struct.pack('<I', VT))
mu.mem_write(0x1006af50, struct.pack('<I', COMP))
# GetLastPoses(this, poses, count, game_poses, game_count): copia POSE_SRC; ret 0x10
code = (b"\x56\x57\x8b\x7c\x24\x0c\xbe" + struct.pack('<I', POSE_SRC) +
        b"\xb9\x14\x00\x00\x00\xf3\xa5\x5f\x5e\x31\xc0\xc2\x10\x00")
mu.mem_write(STUB + 0x10, code)
mu.mem_write(VT + 0xc, struct.pack('<I', STUB + 0x10))

def pose(yaw, pitch, x, y, z):
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    # R = Ry(yaw) * Rx(pitch)
    R = [[cy, sy*sp, sy*cp], [0, cp, -sp], [-sy, cy*sp, cy*cp]]
    m = [R[0][0], R[0][1], R[0][2], x, R[1][0], R[1][1], R[1][2], y, R[2][0], R[2][1], R[2][2], z]
    return struct.pack('<12f', *m) + b"\0" * 24 + struct.pack('<IBB', 200, 1, 1) + b"\0\0"

SELF = HEAP + 0x4000
def correr(fn, p, escala):
    mu.mem_write(POSE_SRC, p)
    out = HEAP + 0x8000
    mu.mem_write(out, b"\0" * 48)
    sp = STACK + 0x80000
    ret = STUB + 0x80
    mu.mem_write(STUB + 0x80, b"\xf4")
    sp -= 4; mu.mem_write(sp, struct.pack('<f', escala))
    sp -= 4; mu.mem_write(sp, struct.pack('<I', out))
    sp -= 4; mu.mem_write(sp, struct.pack('<I', ret))
    mu.reg_write(UC_X86_REG_ESP, sp); mu.reg_write(UC_X86_REG_ECX, SELF)
    mu.emu_start(fn, ret, count=200000)
    return struct.unpack('<12f', mu.mem_read(out, 48)), struct.unpack('<f', mu.mem_read(SELF + 0x2c, 4))[0]

ok = True
for nombre, fn in (("cabeza", 0x100070e0), ("cuerpo", 0x10007220)):
    for escala in (1.0, 2.5):
        a, ya = correr(fn, pose(0.7, -0.3, 0.1, 1.60, -0.2), escala)
        b, yb = correr(fn, pose(0.7, -0.3, 0.1, 1.10, -0.2), escala)
        dif = [i for i in range(12) if abs(a[i] - b[i]) > 1e-5]
        esperado = abs(a[7] - (-escala * 1.60)) < 1e-4 and dif == [7] and abs(ya - 1.60) < 1e-6 and abs(yb - 1.10) < 1e-6
        ok &= esperado
        print("%s escala %.1f: out[7] parado %.3f, agachado %.3f; cambian %s; this+0x2c %.2f/%.2f -> %s"
              % (nombre, escala, a[7], b[7], dif, ya, yb, "OK" if esperado else "FALLA"))
print("TODO OK" if ok else "HAY FALLAS")
