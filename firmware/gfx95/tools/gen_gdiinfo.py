# usage: gen_gdiinfo.py [W H]  -> driver/gdiinfo.inc: the 0x58-byte GDIINFO template (bytes 0..0x17 and 0x30..0x57 used), own numbers per mode.
# The physical size stays 217 x 136 mm in every mode (the 1280x800 panel is the same glass), so the logical DPI follows the mode: 96 is patched in by the driver.
import struct, sys
W = int(sys.argv[1]) if len(sys.argv) > 1 else 1280
H = int(sys.argv[2]) if len(sys.argv) > 2 else 800
ws = [0] * 44
ws[0] = 0x0400; ws[1] = 1; ws[2] = 217; ws[3] = 136; ws[4] = W; ws[5] = H; ws[6] = 0; ws[7] = 1   # ver tech horzmm vertmm horzres vertres bpp(patched) planes
a = 509; b = 424
ws[24:44] = [0x0820, 0x0618, W, (-H) & 0xffff, 0x5140, 0x3cf0, W, (-H) & 0xffff, 0x0145, 0x0145, a, (-b) & 0xffff, 0x0659, 0x0659, a // 2, (-(b // 2)) & 0xffff, 0x0924, 0x0924, a // 2, (-(b // 2)) & 0xffff]
d = struct.pack('<44H', *ws)
open('driver/gdiinfo.inc', 'w').write('\t.byte ' + ','.join(str(x) for x in d) + '\n')
print(len(d), W, H)
