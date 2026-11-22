"""B82: Win95 .LNK (MS-SHLLINK, the layout of the image's own Notepad.lnk) with our icon: target C:\\WINDOWS\\<file>, IconLocation = an .ICO.
usage: python mklnk.py <template Notepad.lnk> <out.lnk> <TARGET 8.3 name in C:\\WINDOWS> <relative path from the .lnk folder> <icon path>
Copies the template's header + ID-list root/drive/WINDOWS items, replaces the file item, LinkInfo path and relative path, adds the icon."""
import struct, sys

tpl, out, name, rel, icon = sys.argv[1:6]
d = open(tpl, 'rb').read()
flags = struct.unpack_from('<I', d, 0x14)[0]
assert flags == 0x0B, 'template flags %x (expected IDList+LinkInfo+RelativePath)' % flags
hdr = bytearray(d[:0x4C])

# ---- ID list: keep every item but the last (the file), append our file item
p = 0x4C; idl_size = struct.unpack_from('<H', d, p)[0]; q = p + 2; items = []
while True:
    n = struct.unpack_from('<H', d, q)[0]
    if n == 0: break
    items.append(d[q:q + n]); q += n
last = items[-1]                                  # file item: size(2) type(1) pad(1) filesize(4) date(2) time(2) attr(2) name\0
assert last[2] == 0x32, 'last ID item is not a file item'
nm = name.upper().encode('ascii') + b'\0'
if len(nm) % 2: nm += b'\0'
fi = bytes([0x32, 0]) + struct.pack('<I', 0) + last[8:12] + struct.pack('<H', 0x20) + nm
fi = struct.pack('<H', len(fi) + 2) + fi
idl = b''.join(items[:-1]) + fi + b'\0\0'
idlist = struct.pack('<H', len(idl)) + idl

# ---- LinkInfo: same volume ID, new local base path
li = d[p + 2 + idl_size:]
li_size, li_hsize, li_flags, vol_off, base_off, cnrl_off, suf_off = struct.unpack_from('<IIIIIII', li, 0)
assert li_hsize == 0x1C and li_flags & 1
vol = li[vol_off:base_off]
base = ('C:\\WINDOWS\\' + name.upper()).encode('ascii') + b'\0'
body = vol + base + b'\0'                        # common path suffix = ""
vol_off2 = 0x1C; base_off2 = vol_off2 + len(vol); suf_off2 = base_off2 + len(base)
linfo = struct.pack('<IIIIIII', 0x1C + len(body), 0x1C, 1, vol_off2, base_off2, 0, suf_off2) + body

def cstr(s): b = s.encode('ascii'); return struct.pack('<H', len(b)) + b

struct.pack_into('<I', hdr, 0x14, 0x0B | 0x40)   # + HasIconLocation
struct.pack_into('<I', hdr, 0x34, 0)              # file size of the target (empty document)
struct.pack_into('<i', hdr, 0x38, 0)              # icon index
res = bytes(hdr) + idlist + linfo + cstr(rel) + cstr(icon) + b'\0\0\0\0'   # terminal ExtraData block
open(out, 'wb').write(res)
print(out, len(res))
