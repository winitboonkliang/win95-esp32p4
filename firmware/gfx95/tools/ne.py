# BLUE B29a own NE reader: python3 -I ne.py file  -> header, segments, imports, entries, per-seg relocs
import struct, sys
def rd(path):
    d=open(path,'rb').read(); ne=struct.unpack_from('<I',d,0x3c)[0]
    h=dict(zip('ver min entoff entlen crc flags autods heap stack csip sssp nseg nmod nonres segoff resoff resnoff modoff impoff nonresoff nmov shift nres os'.split(),
      struct.unpack_from('<BBHHIHHHHIIHHHHHHHHIHHBB',d,ne+2)))
    return d,ne,h
if __name__=='__main__':
    d,ne,h=rd(sys.argv[1]); print({k:(hex(v)) for k,v in h.items()})
    shift=h['shift'] or 9
    seg=ne+h['segoff']
    for i in range(h['nseg']):
        s,l,f,m=struct.unpack_from('<HHHH',d,seg+8*i); print('seg',i+1,'sec',hex(s<<shift),'len',hex(l),'flags',hex(f),'min',hex(m))
    # module refs
    mr=ne+h['modoff']; imp=ne+h['impoff']; mods=[]
    for i in range(h['nmod']):
        o=struct.unpack_from('<H',d,mr+2*i)[0]; n=d[imp+o]; mods.append(d[imp+o+1:imp+o+1+n].decode()); 
    print('modrefs',mods)
    # resident names
    p=ne+h['resnoff']
    while d[p]:
        n=d[p]; print('resname',d[p+1:p+1+n].decode(),struct.unpack_from('<H',d,p+1+n)[0]); p+=n+3
    # entry table
    p=ne+h['entoff']; end=p+h['entlen']; ordn=1
    while p<end and d[p]:
        cnt=d[p]; ind=d[p+1]; p+=2
        for _ in range(cnt):
            if ind==0xff: fl,_i3f,sg,off=struct.unpack_from('<BHBH',d,p); p+=6; print('ent',ordn,'seg',sg,hex(off),'fl',fl)
            elif ind==0: pass
            else: fl,off=struct.unpack_from('<BH',d,p); p+=3; print('ent',ordn,'seg',ind,hex(off),'fl',fl)
            ordn+=1
    # relocs
    for i in range(h['nseg']):
        s,l,f,m=struct.unpack_from('<HHHH',d,seg+8*i)
        if f&0x100:
            p=(s<<shift)+(l or 0x10000); n=struct.unpack_from('<H',d,p)[0]; p+=2
            print('seg',i+1,'relocs',n)
            for r in range(n):
                st,rf,ro,a,b=struct.unpack('<BBHHH',d[p+8*r:p+8*r+8])
                ty=rf&3
                desc={0:'internal seg=%d off=%x'%(a&255,b),1:'impord mod=%s ord=%d'%(mods[a-1],b),2:'impname mod=%s'%mods[a-1],3:'osfixup'}[ty] if ty!=0 else 'internal seg=%d off=%x'%(a&255,b)
                print('  src=%d(%s) at %04x add=%d %s'%(st&15,{0:'lobyte',2:'sel',3:'far',5:'off',11:'far32',13:'off32'}.get(st&15,st&15),ro,(rf>>2)&1,desc))
