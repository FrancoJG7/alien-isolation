import pefile,struct,sys,capstone
P=pefile.PE('/tmp/claude-0/gh/work/AI.exe',fast_load=True)
IB=P.OPTIONAL_HEADER.ImageBase
def rd(va,n): return P.get_data(va-IB,n)
def d32(va): return struct.unpack('<I',rd(va,4))[0]
def cstr(va):
    b=rd(va,200); return b.split(b'\0')[0].decode('latin1')
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
def dis(va,n=40):
    out=[]
    for i in md.disasm(rd(va,n*8),va):
        out.append(f"{i.address:08x} {i.mnemonic} {i.op_str}")
        if len(out)>=n or i.mnemonic=='ret': break
    return "\n".join(out)
