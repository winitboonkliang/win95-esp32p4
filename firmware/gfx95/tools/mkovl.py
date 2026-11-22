# sector overlay = diff of two images: python3 mkovl.py base.img patched.img out.ovl   (records: u32 lba + 512 bytes)
import sys, struct
a = open(sys.argv[1], 'rb'); b = open(sys.argv[2], 'rb'); out = open(sys.argv[3], 'wb'); i = 0; n = 0
while True:
    x = a.read(1 << 20); y = b.read(1 << 20)
    if not x: break
    if x != y:
        for s in range(0, len(x), 512):
            if x[s:s + 512] != y[s:s + 512]:
                out.write(struct.pack('<I', i * 2048 + s // 512) + y[s:s + 512]); n += 1
    i += 1
print('overlay sectors', n, 'bytes', n * 516)
