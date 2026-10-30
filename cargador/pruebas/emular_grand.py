"""Emula con unicorn el cargador de XInput de GRAND (parcheado y original)
y muestra qué nombre le pasa a LoadLibraryA."""
import sys, struct, pefile
from unicorn import *
from unicorn.x86_const import *

def correr(ruta):
    pe = pefile.PE(ruta)
    base = 0x10000000
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(base, 0x200000)
    for s in pe.sections:
        mu.mem_write(base + s.VirtualAddress, s.get_data())
    STK = 0x00200000; mu.mem_map(STK, 0x10000)
    STUB = 0x00300000; mu.mem_map(STUB, 0x1000)
    llamadas = []
    # IAT: GetSystemDirectoryA 0x10092044, LoadLibraryA 0x10092048, GetProcAddress 0x10092040, MessageBoxA 0x100921cc
    stubs = {0x10092044: 'GetSystemDirectoryA', 0x10092048: 'LoadLibraryA', 0x10092040: 'GetProcAddress',
             0x100921cc: 'MessageBoxA', 0x1009206c: 'ExitProcess'}
    nargs = {'GetSystemDirectoryA': 2, 'LoadLibraryA': 1, 'GetProcAddress': 2, 'MessageBoxA': 4, 'ExitProcess': 1}
    addr2name = {}
    for i, (iat, n) in enumerate(stubs.items()):
        a = STUB + i * 16
        mu.mem_write(iat, struct.pack('<I', a)); addr2name[a] = n
        mu.mem_write(a, b'\xc3')
    def hook(mu, addr, size, ud):
        if addr in addr2name:
            n = addr2name[addr]; esp = mu.reg_read(UC_X86_REG_ESP)
            args = struct.unpack('<%dI' % nargs[n], mu.mem_read(esp + 4, 4 * nargs[n]))
            if n == 'GetSystemDirectoryA':
                mu.mem_write(args[0], b'C:\\Windows\\SysWOW64\0'); ret = 19
                llamadas.append(n)
            elif n == 'LoadLibraryA':
                s = bytes(mu.mem_read(args[0], 64)).split(b'\0')[0]; llamadas.append('LoadLibraryA(%s)' % s.decode())
                ret = 0x55550000
            elif n == 'GetProcAddress':
                ret = 0x66660000
            else:
                llamadas.append(n); ret = 0; mu.emu_stop()
            mu.reg_write(UC_X86_REG_EAX, ret)
            retaddr = struct.unpack('<I', mu.mem_read(esp, 4))[0]
            mu.reg_write(UC_X86_REG_ESP, esp + 4 + 4 * nargs[n]); mu.reg_write(UC_X86_REG_EIP, retaddr)
        if addr == 0x10036884 or addr == 0x10036890:
            pass
    mu.hook_add(UC_HOOK_CODE, hook)
    # Parar en la llamada a FUN_100294e0 (después de todos los GetProcAddress).
    esp = STK + 0x8000
    mu.mem_write(esp, struct.pack('<III', 0xdeadbeef, 0x12340000, 1))
    mu.reg_write(UC_X86_REG_ESP, esp)
    stop = None
    # buscar la llamada a FUN_100294e0
    d = mu.mem_read(0x100367b0, 0x150)
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for i in md.disasm(bytes(d), 0x100367b0):
        if i.mnemonic == 'call' and i.op_str == '0x100294e0': stop = i.address
    mu.emu_start(0x100367b0, stop, count=2000)
    sd = struct.unpack('<I', mu.mem_read(0x100b62e8, 4))[0]
    return llamadas, sd

for r in sys.argv[1:]:
    ll, h = correr(r)
    print(r.split('/')[-1], '->', ll, 'handle guardado %#x' % h)
