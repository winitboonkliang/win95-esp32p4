# BLUE B29a own NE linker for the minidriver: python3 -I mkne.py text.bin fix.bin syms.txt out.drv
# segments: 1 = code (movable, preload, relocs from fix.bin), 2 = data (fixed, DGROUP).  All fixups additive, no chains.
import struct, sys, re
text=open(sys.argv[1],'rb').read(); fix=open(sys.argv[2],'rb').read()
syms={}
for l in open(sys.argv[3]):
    p=l.split()
    if len(p)==3: syms[p[2]]=int(p[0],16)
DATASIZE=int(re.search(r'DATASIZE,\s*(0x[0-9a-f]+)',open('driver/layout.inc').read()).group(1),16)
EXPORTS=[(1,'BITBLT','x_BitBlt'),(2,'COLORINFO','x_ColorInfo'),(3,'CONTROL','x_Control'),(4,'DISABLE','x_Disable'),(5,'ENABLE','x_Enable'),
 (6,'ENUMDFONTS','x_EnumDFonts'),(7,'ENUMOBJ','x_EnumObj'),(8,'OUTPUT','x_Output'),(9,'PIXEL','x_Pixel'),(10,'REALIZEOBJECT','x_RealizeObject'),
 (11,'STRBLT','x_StrBlt'),(12,'SCANLR','x_ScanLR'),(13,'DEVICEMODE','x_DeviceMode'),(14,'EXTTEXTOUT','x_ExtTextOut'),(15,'GETCHARWIDTH','x_GetCharWidth'),
 (16,'DEVICEBITMAP','x_DeviceBitmap'),(17,'FASTBORDER','x_FastBorder'),(18,'SETATTRIBUTE','x_SetAttribute'),(19,'DIBBLT','x_DIBBlt'),
 (20,'CREATEDIBITMAP','x_CreateDIBitmap'),(21,'DIBTODEVICE','x_DIBToDevice'),(22,'SETPALETTE','x_SetPalette'),(23,'GETPALETTE','x_GetPalette'),
 (24,'SETPALETTETRANSLATE','x_SetPaletteTranslate'),(25,'GETPALETTETRANSLATE','x_GetPaletteTranslate'),(26,'UPDATECOLORS','x_UpdateColors'),
 (27,'STRETCHBLT','x_StretchBlt'),(28,'STRETCHDIBITS','x_StretchDIBits'),(29,'SELECTBITMAP','x_SelectBitmap'),(30,'BITMAPBITS','x_BitmapBits'),
 (31,'REENABLE','x_ReEnable'),(101,'INQUIRE','x_Inquire'),(102,'SETCURSOR','x_SetCursor'),(103,'MOVECURSOR','x_MoveCursor'),(104,'CHECKCURSOR','x_CheckCursor'),
 (500,'USERREPAINTDISABLE','x_UserRepaintDisable'),(700,'VALIDATEMODE','x_ValidateMode')]
# relocations
rel=b''; n=0
for i in range(0,len(fix),20):
    site,typ,kind,a,b=struct.unpack_from('<IIIII',fix,i)
    if kind==1: rel+=struct.pack('<BBHHH',typ,1|4,site,a,b)
    else: rel+=struct.pack('<BBHBBH',typ,0|4,site,a,0,b)
    n+=1
codeseg=text+struct.pack('<H',n)+rel
# tables
def pstr(s): return bytes([len(s)])+s.encode()
imp=b'\0'+pstr('KERNEL'); kofs=1; dofs=len(imp); imp+=pstr('DIBENG')
modref=struct.pack('<HH',kofs,dofs)
res=pstr('DISPLAY')+struct.pack('<H',0)+b'\0'
ent=b''; cur=1
for o,nm,sy in EXPORTS:
    while cur<o:
        g=min(255,o-cur); ent+=bytes([g,0]); cur+=g
    ent+=bytes([1,0xff])+struct.pack('<BHBH',3,0x3fcd,1,syms[sy]); cur+=1
ent+=b'\0'
nonres=pstr('B29GFX display minidriver (BLUE B29a)')+struct.pack('<H',0)
for o,nm,sy in EXPORTS: nonres+=pstr(nm)+struct.pack('<H',o)
nonres+=b'\0'
NE=0x80
segtab=b''  # filled later
off=NE+0x40
segtab_off=0x40; nseg=2
segtab_sz=8*nseg
res_off=segtab_off+segtab_sz   # resource table offset == resident name offset: no resources
resn_off=res_off
modref_off=resn_off+len(res)
imp_off=modref_off+len(modref)
ent_off=imp_off+len(imp)
hdr_end=ent_off+len(ent)
nonres_file=(NE+hdr_end+15)&~15
pos=(nonres_file+len(nonres)+15)&~15
code_pos=pos; pos=(pos+len(codeseg)+15)&~15
data_pos=pos; pos=(pos+DATASIZE+15)&~15
seg=struct.pack('<HHHH',code_pos>>4,len(text),0xd70|0x0100&0xd70|0,len(text))
seg=struct.pack('<HHHH',code_pos>>4,len(text),0xd70,len(text))+struct.pack('<HHHH',data_pos>>4,DATASIZE,0xc61,DATASIZE)
hdr=b'NE'+bytes([5,0x3c])+struct.pack('<HHIHHHHIIHHHHHHHHIHHHBB',ent_off,len(ent),0,0x8301,2,0,0,(1<<16)|syms['start'],0,nseg,2,len(nonres),segtab_off,res_off,resn_off,modref_off,imp_off,nonres_file,len(EXPORTS),4,0,2,8)
hdr+=b'\0'*(0x40-len(hdr)); hdr=bytearray(hdr); hdr[0x3e:0x40]=struct.pack('<H',0x0400)
# fix: code segment flag relocs (0x100) must be set
seg=bytearray(seg); struct.pack_into('<H',seg,4,0xd70|0x0100)
blob=bytearray(hdr)+seg+res+modref+imp+ent
mz=bytearray(0x80); mz[0:2]=b'MZ'; struct.pack_into('<H',mz,2,0x80); struct.pack_into('<H',mz,4,1); struct.pack_into('<H',mz,8,4); struct.pack_into('<I',mz,0x3c,NE)
mz[0x40:0x40+14]=bytes.fromhex('0e1fba0e00b409cd21b8014ccd21')
out=bytearray(mz)+blob
out+=b'\0'*(nonres_file-len(out)); out+=nonres
out+=b'\0'*(code_pos-len(out)); out+=codeseg
out+=b'\0'*(data_pos-len(out)); out+=b'\0'*DATASIZE
open(sys.argv[4],'wb').write(out)
print('wrote',sys.argv[4],len(out),'bytes; text',len(text),'relocs',n,'exports',len(EXPORTS))
