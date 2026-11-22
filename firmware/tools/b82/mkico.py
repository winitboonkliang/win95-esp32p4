"""B82: Win95 .ICO (32x32 + 16x16, each 4 bpp 16-colour + 8 bpp) from the Codex 256 px masters (art/*_icon.png).
usage: python mkico.py <art dir>  -> art/DOOM1.ICO DOOMU.ICO DOOM2.ICO + art/ico_preview.png"""
import struct, sys, os
from PIL import Image
A = sys.argv[1]
PAL16 = [0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0, 0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF]


def small(master, n):
    im = Image.open(master).convert('RGBA')
    bb = im.getbbox(); im = im.crop(bb)
    s = max(im.size); sq = Image.new('RGBA', (s, s)); sq.alpha_composite(im, ((s - im.size[0]) // 2, (s - im.size[1]) // 2))
    return sq.resize((n, n), Image.LANCZOS)


def quant(im, pal):
    """nearest colour (perceptual weights), alpha < 128 = transparent; returns (indices, mask)"""
    w, h = im.size; px = im.load(); idx = []; mask = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            mask.append(a < 128)
            best = min(range(len(pal)), key=lambda i: 2 * (r - (pal[i] >> 16)) ** 2 + 4 * (g - (pal[i] >> 8 & 255)) ** 2 + 3 * (b - (pal[i] & 255)) ** 2)
            idx.append(best)
    return idx, mask


def dib(im, bpp, pal):
    w, h = im.size; idx, mask = quant(im, pal)
    pal = pal + [0] * ((1 << bpp) - len(pal))
    hdr = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, bpp, 0, 0, 0, 0, 1 << bpp, 0)
    ptab = b''.join(struct.pack('<BBBB', c & 255, c >> 8 & 255, c >> 16, 0) for c in pal)
    rows = b''; mrows = b''
    for y in range(h - 1, -1, -1):   # bottom-up
        r = bytearray()
        if bpp == 4:
            for x in range(0, w, 2): r.append((0 if mask[y * w + x] else idx[y * w + x]) << 4 | (0 if mask[y * w + x + 1] else idx[y * w + x + 1]))
        else:
            for x in range(w): r.append(0 if mask[y * w + x] else idx[y * w + x])
        r += b'\0' * ((-len(r)) % 4); rows += bytes(r)
        m = bytearray((w + 7) // 8)
        for x in range(w):
            if mask[y * w + x]: m[x // 8] |= 0x80 >> (x % 8)
        m += b'\0' * ((-len(m)) % 4); mrows += bytes(m)
    return hdr + ptab + rows + mrows


def pal256(images):
    """one 256-colour palette for the 8 bpp images: the 16 system colours + the 240 most used of the icon"""
    q = Image.new('RGB', (sum(i.size[0] for i in images), 32))
    x = 0
    for i in images: q.paste(i.convert('RGB'), (x, 0)); x += i.size[0]
    p = q.quantize(240).getpalette()[:240 * 3]
    return PAL16 + [p[3 * k] << 16 | p[3 * k + 1] << 8 | p[3 * k + 2] for k in range(240)]


prev = Image.new('RGBA', (3 * 150, 120), (0, 128, 128, 255))
for k, (src, out) in enumerate((('doom_icon.png', 'DOOM1.ICO'), ('ultimate_icon.png', 'DOOMU.ICO'), ('doom2_icon.png', 'DOOM2.ICO'))):
    s32, s16 = small(os.path.join(A, src), 32), small(os.path.join(A, src), 16)
    p8 = pal256([s32, s16])
    imgs = [(32, dib(s32, 4, PAL16)), (16, dib(s16, 4, PAL16)), (32, dib(s32, 8, p8)), (16, dib(s16, 8, p8))]
    hdr = struct.pack('<HHH', 0, 1, len(imgs)); off = 6 + 16 * len(imgs); ents = b''; data = b''
    for (n, d), bpp in zip(imgs, (4, 4, 8, 8)):
        ents += struct.pack('<BBBBHHII', n, n, 16 if bpp == 4 else 0, 0, 1, bpp, len(d), off + len(data)); data += d
    open(os.path.join(A, out), 'wb').write(hdr + ents + data)
    for n, im, yy in ((32, s32, 4), (16, s16, 84)):   # preview of the 16-colour versions, x3
        idx, mask = quant(im, PAL16); v = Image.new('RGBA', (n, n))
        for i, c in enumerate(idx): v.putpixel((i % n, i // n), (0, 0, 0, 0) if mask[i] else (c >> 16, c >> 8 & 255, c & 255, 255) if False else (PAL16[c] >> 16, PAL16[c] >> 8 & 255, PAL16[c] & 255, 255))
        v = v.resize((n * (2 if n == 32 else 2), n * 2), Image.NEAREST); prev.alpha_composite(v, (k * 150 + 4, yy))
        if n == 32:
            c8 = im.copy(); prev.alpha_composite(c8.resize((64, 64), Image.NEAREST), (k * 150 + 76, 4))
    print(out, os.path.getsize(os.path.join(A, out)))
prev.save(os.path.join(A, 'ico_preview.png'))
