import sys;sys.path.insert(0,'/tmp/claude-0/gh/py')
from pe import *
import re
data={}
secs=[(s.VirtualAddress+IB,s.get_data()) for s in P.sections]
def find_all(b):
    for base,d in secs:
        for m in re.finditer(re.escape(b),d): yield base+m.start()
def vtables(cls):
    res=[]
    for s in find_all(('.?AV'+cls+'@CATHODE@@\0').encode()):
        td=s-8
        for col in find_all(struct.pack('<I',td)):
            if d32(col-12)!=0: continue  # COL signature 0 at col-12? col = ref-12
            c=col-12
            for meta in find_all(struct.pack('<I',c)):
                res.append(meta+4)
    return res
if __name__=='__main__':
    for c in sys.argv[1:]:
        for v in vtables(c):
            print(c,hex(v),' '.join(hex(d32(v+4*i)) for i in range(24)))
