#!/usr/bin/env python3
"""mkvxd.py <flat.bin> <out.vxd> <NAME> [fixup src offsets...] - minimal LE (Win95 static VxD) writer, own code.
One object LCOD (R/W/X, 32-bit, resident, one 4 KB page), DDB at object offset 0 (entry ordinal 1, exported as NAME_DDB in the
non-resident name table: the loader finds the DDB through it), 32-bit-offset fixups to the object itself.
Layout learned from stock small VxDs (QUARTZ/JAVASUP/LOGGER/MSMOUSE), dumped field by field with lehdr.py."""
import struct, sys

code = open(sys.argv[1], 'rb').read(); out = sys.argv[2]; name = sys.argv[3].encode()
fix = [int(x, 0) for x in sys.argv[4:]]
import os
PG = int(os.environ.get('VXD_PG','0x1000'),0)
RES = os.environ.get('VXD_RES')
npg = (len(code) + PG - 1) // PG
desc = name + b' Virtual Device\t(Version 1.0)'

objtab = 0xC4
objmap = objtab + 24
resn = objmap + 4 * npg
rn = bytes([len(name)]) + name + b'\x00\x00' + b'\x00'
entry = resn + len(rn)
ent = bytes([1, 3, 1, 0, 3]) + struct.pack('<I', 0) + b'\x00'
fpt = (entry + len(ent) + 3) & ~3
pages = [b''] * npg                      # fixup records per page; source offset is relative to the page
for f in fix:
    t = struct.unpack_from('<I', code, f)[0]
    assert t < 0x10000, 'object bigger than 64 KB needs 32-bit target offsets'
    pages[f // PG] += struct.pack('<BBHBH', 7, 0, f % PG, 1, t)
recs = b''.join(pages)
offs = [0]
for p in pages: offs.append(offs[-1] + len(p))
fpg = struct.pack('<%dI' % (npg + 1), *offs)
frec = fpt + len(fpg)
imp = frec + len(recs)
objs = struct.pack('<IIIIII', len(code), 0, 0x2047, 1, npg, 0x444f434c)
omap = b''.join(bytes([0, 0, i + 1, 0]) for i in range(npg))
nonres = (bytes([len(desc)]) + desc + b'\x00\x00' + bytes([len(name) + 4]) + name + b'_DDB' + b'\x01\x00' + b'\x00')

hdr = bytearray(0xC4)
def P(o, fmt, *v): struct.pack_into(fmt, hdr, o, *v)
hdr[0:2] = b'LE'
P(0x08, '<HH', 2, 4); P(0x10, '<I', 0x38000); P(0x14, '<I', npg)
P(0x28, '<I', PG); P(0x2C, '<I', len(code) - (npg - 1) * PG)
P(0x30, '<I', len(fpg) + len(recs)); P(0x38, '<I', fpt - objtab)
P(0x40, '<I', objtab); P(0x44, '<I', 1); P(0x48, '<I', objmap)
P(0x58, '<I', resn); P(0x5C, '<I', entry)
P(0x68, '<I', fpt); P(0x6C, '<I', frec); P(0x70, '<I', imp); P(0x78, '<I', imp)
P(0x80, '<I', 0x1000); P(0x84, '<I', npg)
P(0x88, '<I', 0x1000 + npg * PG); P(0x8C, '<I', len(nonres))
res = open(RES,'rb').read() if RES else b''
if res: P(0xB8, '<II', 0x1000 + npg * PG + len(nonres), len(res))
P(0xC0, '<HH', 0, 0x0400)            # device id (undefined), DDK version

body = bytearray(bytes(hdr) + objs + omap + rn + ent)
body = body.ljust(0xC4 + fpt - objtab, b'\x00') + fpg + recs

f = bytearray(0x80)
f[0:0x1c] = struct.pack('<2s13H', b'MZ', 0x90, 3, 0, 4, 0, 0xffff, 0, 0xb8, 0, 0, 0, 0x40, 0)  # DOS header
f[0x40:0x4e] = bytes.fromhex('0e1fba0e00b409cd21b8014ccd21')   # push cs; pop ds; mov dx,0Eh; mov ah,9; int 21h; mov ax,4C01h; int 21h
f[0x4e:0x4e + 18] = b'VxD needs Win95' + bytes([13, 10, 36])
f[0x3c:0x40] = struct.pack('<I', 0x80)
f += body
f = f.ljust(0x1000, b'\x00')
f += code.ljust(PG, b'\x00')
f += nonres + res
open(out, 'wb').write(f)
print('wrote', out, len(f), 'bytes, code', len(code), 'fixups', fix)
