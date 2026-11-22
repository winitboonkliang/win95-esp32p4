"""B83: the Rust-engine icons = B82's Codex masters with a rust-orange "R" corner tile (readable at 16x16 as an orange corner), then
B82's own tools/b82/mkico.py -> tools/b83/art/DOOMR1.ICO DOOMRU.ICO DOOMR2.ICO (+ ico_preview.png). usage: python mkico_rs.py"""
import os, shutil, subprocess, sys, tempfile
from PIL import Image, ImageDraw, ImageFont

here = os.path.dirname(os.path.abspath(__file__))
b82 = os.path.join(here, '..', 'b82')
out = os.path.join(here, 'art')
os.makedirs(out, exist_ok=True)
tmp = tempfile.mkdtemp()
for src in ('doom_icon.png', 'ultimate_icon.png', 'doom2_icon.png'):
    im = Image.open(os.path.join(b82, 'art', src)).convert('RGBA')
    bb = im.getbbox(); im = im.crop(bb)
    s = max(im.size); sq = Image.new('RGBA', (s, s)); sq.alpha_composite(im, ((s - im.size[0]) // 2, (s - im.size[1]) // 2))
    d = ImageDraw.Draw(sq); t = s * 45 // 100; x0 = s - t; y0 = s - t
    d.rounded_rectangle((x0, y0, s - 1, s - 1), radius=t // 6, fill=(206, 66, 43, 255), outline=(255, 255, 255, 255), width=max(2, s // 64))
    try:
        f = ImageFont.truetype('arialbd.ttf', t * 8 // 10)
    except OSError:
        f = ImageFont.load_default()
    d.text((x0 + t // 2, y0 + t // 2), 'R', fill=(255, 255, 255, 255), font=f, anchor='mm')
    sq.save(os.path.join(tmp, src)); sq.save(os.path.join(out, src.replace('_icon', '_rs_icon')))
subprocess.run([sys.executable, os.path.join(b82, 'mkico.py'), tmp], check=True)
for a, b in (('DOOM1.ICO', 'DOOMR1.ICO'), ('DOOMU.ICO', 'DOOMRU.ICO'), ('DOOM2.ICO', 'DOOMR2.ICO'), ('ico_preview.png', 'ico_preview_rs.png')):
    shutil.copy(os.path.join(tmp, a), os.path.join(out, b))
shutil.rmtree(tmp)
print('icons ->', out)
