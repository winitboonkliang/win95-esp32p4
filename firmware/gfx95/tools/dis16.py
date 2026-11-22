# own 16-bit NE disassembler: python3 -I dis16.py file seg start len  (relocations annotated)
import struct,sys,capstone
sys.path.insert(0,__import__('os').path.dirname(__file__))
from ne import rd
d,ne,h=rd(sys.argv[1]); shift=h['shift'] or 9; seg=ne+h['segoff']
mr=ne+h['modoff']; imp=ne+h['impoff']; mods=[]
for i in range(h['nmod']):
    o=struct.unpack_from('<H',d,mr+2*i)[0]; mods.append(d[imp+o+1:imp+o+1+d[imp+o]].decode())
sn=int(sys.argv[2]); st=int(sys.argv[3],0); ln=int(sys.argv[4],0)
s,l,f,m=struct.unpack_from('<HHHH',d,seg+8*(sn-1)); l=l or 0x10000
code=d[s<<shift:(s<<shift)+l]; rel={}
if f&0x100:
    p=(s<<shift)+l; n=struct.unpack_from('<H',d,p)[0]; p+=2
    for r in range(n):
        t,rf,ro,a,b=struct.unpack('<BBHHH',d[p+8*r:p+8*r+8])
        nm=('%s.%d'%(mods[a-1],b)) if (rf&3)==1 else 'seg%d:%x'%(a&255,b)
        sz={0:1,2:2,3:4,5:2,11:6,13:4}.get(t&15,2)
        if rf&4: rel[ro]=nm
        else:
            q=ro;n2=0
            while q!=0xffff and q+sz<=len(code) and n2<5000:
                rel[q]=nm; q=struct.unpack_from('<H',code,q)[0]; n2+=1
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_16)
for i in md.disasm(code[st:st+ln],st):
    r=[v for k,v in rel.items() if i.address<=k<i.address+i.size]
    print('%04x: %-18s %s %s %s'%(i.address,i.bytes.hex(),i.mnemonic,i.op_str,('; '+r[0]) if r else ''))
