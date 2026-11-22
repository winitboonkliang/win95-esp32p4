#!/usr/bin/env python3
"""creg.py SYSTEM.DAT [path-prefix] : dump a Win95 CREG registry (keys + values) as text. Read only."""
import sys, struct
d = open(sys.argv[1], 'rb').read(); pre = (sys.argv[2] if len(sys.argv) > 2 else '').lower()
assert d[:4] == b'CREG'
rgdb_off, = struct.unpack_from('<I', d, 8); nrgdb, = struct.unpack_from('<H', d, 16)
K = 0x20; assert d[K:K+4] == b'RGKN'
root, = struct.unpack_from('<I', d, K + 8)
blocks = {}; o = rgdb_off
for b in range(nrgdb):
    assert d[o:o+4] == b'RGDB', hex(o)
    sz, = struct.unpack_from('<I', d, o + 4)
    p = o + 0x20
    while p < o + sz - 16:
        esz, kid, bn, used, nl, nv, _ = struct.unpack_from('<IHHIHHI', d, p)
        if esz == 0: break
        if kid != 0xFFFF and used:
            name = d[p+20:p+20+nl].decode('latin1'); q = p + 20 + nl; vals = []
            for _ in range(nv):
                t, _u, vnl, dl = struct.unpack_from('<IIHH', d, q)
                vn = d[q+12:q+12+vnl].decode('latin1'); data = d[q+12+vnl:q+12+vnl+dl]
                vals.append((vn, t, data)); q += 12 + vnl + dl
            blocks[(bn, kid)] = (name, vals)
        p += esz
    o += sz
def fmt(t, v):
    if t == 1: return '"%s"' % v.rstrip(b'\0').decode('latin1')
    return 'hex:' + v.hex()
def walk(off, path):
    while off != 0xFFFFFFFF:
        n = K + off
        inuse, h, nf, par, ch, nx, kid, bn = struct.unpack_from('<IIIIIIHH', d, n)
        nm, vals = blocks.get((bn, kid), ('?', []))
        p = (path + chr(92) + nm) if path else nm
        q = p.lstrip(chr(0) + ' ' + chr(92)).lower()
        if q.startswith(pre) or pre.startswith(q):
            if q.startswith(pre):
                print('[' + p + ']')
                for vn, t, v in vals: print('  %s = %s' % (vn or '@', fmt(t, v)))
            walk(ch, p)
        nx_ = nx; off = nx_
walk(root, '')
