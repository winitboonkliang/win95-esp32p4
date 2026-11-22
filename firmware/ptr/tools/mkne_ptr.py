# BLUE B45 NE writer for P4PTR.DRV (adapted from our B29a mkne.py): python3 -I mkne_ptr.py text.bin fix.bin syms.txt out.drv
# segment 1 = code FIXED (interrupt handler lives here: fixed DLL segments are page-locked), segment 2 = data FIXED. No imports.
import struct, sys
text = open(sys.argv[1], 'rb').read(); fix = open(sys.argv[2], 'rb').read()
syms = {}
for l in open(sys.argv[3]):
    p = l.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
DATASIZE = 0x30
# stock MOUSE.DRV ordinals and names (WEP is resident, the rest non-resident, as in the stock file)
EXPORTS = [(1, 'INQUIRE', 'x_Inquire'), (2, 'ENABLE', 'x_Enable'), (3, 'DISABLE', 'x_Disable'), (4, 'MOUSEGETINTVECT', 'x_GetIntVect'),
           (5, 'GETSETMOUSEDATA', 'x_GetSetData'), (6, 'CPLAPPLET', 'x_CplApplet'), (7, 'POWEREVENTPROC', 'x_PowerEvent'),
           (8, 'EXTRAPOINTS', 'x_ExtraPoints'), (9, 'WEP', 'x_Wep'), (10, 'MOUSEREDETECT', 'x_Redetect')]
rel = b''; n = 0
for i in range(0, len(fix), 20):
    site, typ, kind, a, b = struct.unpack_from('<IIIII', fix, i)
    assert kind == 0, 'no imports in this driver'
    rel += struct.pack('<BBHBBH', typ, 4, site, a, 0, b); n += 1   # additive: no chain through the target word
codeseg = text + struct.pack('<H', n) + rel
def pstr(s): return bytes([len(s)]) + s.encode()
imp = b'\0'
modref = b''
res = pstr('MOUSE') + struct.pack('<H', 0) + pstr('WEP') + struct.pack('<H', 9) + b'\0'
ent = b''; cur = 1
for o, nm, sy in EXPORTS:
    assert o == cur
    ent += bytes([1, 0x01]) + struct.pack('<BH', 3, syms[sy]); cur += 1   # fixed-segment bundles: flag byte 3 (exported, shared data)
ent += b'\0'
nonres = pstr('P4PTR pointer driver (BLUE B45)') + struct.pack('<H', 0)
for o, nm, sy in EXPORTS:
    if nm != 'WEP': nonres += pstr(nm) + struct.pack('<H', o)
nonres += b'\0'
NE = 0x80; nseg = 2
segtab_off = 0x40; res_off = segtab_off + 8 * nseg; resn_off = res_off
modref_off = resn_off + len(res); imp_off = modref_off + len(modref); ent_off = imp_off + len(imp)
hdr_end = ent_off + len(ent)
nonres_file = (NE + hdr_end + 15) & ~15
pos = (nonres_file + len(nonres) + 15) & ~15
code_pos = pos; pos = (pos + len(codeseg) + 15) & ~15
data_pos = pos
seg = struct.pack('<HHHH', code_pos >> 4, len(text), 0x0d60, len(text)) + struct.pack('<HHHH', data_pos >> 4, DATASIZE, 0x0c61, DATASIZE)
hdr = b'NE' + bytes([5, 0x3c]) + struct.pack('<HHIHHHHIIHHHHHHHHIHHHBB', ent_off, len(ent), 0, 0x8301, 2, 0, 0, (1 << 16) | syms['start'], 0, nseg, 0,
    len(nonres), segtab_off, res_off, resn_off, modref_off, imp_off, nonres_file, 0, 4, 0, 2, 8)
hdr += b'\0' * (0x40 - len(hdr)); hdr = bytearray(hdr); hdr[0x3e:0x40] = struct.pack('<H', 0x0400)
blob = bytearray(hdr) + seg + res + modref + imp + ent
mz = bytearray(0x80); mz[0:2] = b'MZ'; struct.pack_into('<H', mz, 2, 0x80); struct.pack_into('<H', mz, 4, 1); struct.pack_into('<H', mz, 8, 4); struct.pack_into('<I', mz, 0x3c, NE)
mz[0x40:0x40 + 14] = bytes.fromhex('0e1fba0e00b409cd21b8014ccd21')
out = bytearray(mz) + blob
out += b'\0' * (nonres_file - len(out)); out += nonres
out += b'\0' * (code_pos - len(out)); out += codeseg
out += b'\0' * (data_pos - len(out)); out += b'\0' * DATASIZE
open(sys.argv[4], 'wb').write(out)
print('wrote', sys.argv[4], len(out), 'bytes; text', len(text), 'relocs', n, 'exports', len(EXPORTS))
