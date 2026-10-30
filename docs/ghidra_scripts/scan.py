import sys;sys.path.insert(0,'/tmp/claude-0/gh/py')
from pe import *
import re,pickle
# linear disasm of .text, store (addr, mnem, ops) lines to a file once
import os
cache='/tmp/claude-0/gh/py/text.lst'
if not os.path.exists(cache):
    s=[x for x in P.sections if x.Name.startswith(b'.text')][0]
    base=s.VirtualAddress+IB; data=s.get_data()
    md.skipdata=True
    with open(cache,'w') as f:
        for i in md.disasm_lite(data,base):
            f.write(f"{i[0]:08x} {i[2]} {i[3]}\n")
