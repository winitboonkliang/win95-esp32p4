# usage: gen_reg.py MODE(1|2|3) > img/b29_mN.reg   - display class registry for boot mode N: 1 = 1280x800, 2 = 1024x640 (1.25x), 3 = 640x400 (2x)
import sys

M = {1: (1280, 800, 'b29gfx.drv'), 2: (1024, 640, 'b29g1024.drv'), 3: (640, 400, 'b29g640.drv')}
n = int(sys.argv[1])
w, h, drv = M[n]
base = r'HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Class\Display\0000'
out = ['REGEDIT4', '', '[' + base + r'\DEFAULT]', '"Mode"="16,%d,%d"' % (w, h), '"drv"="%s"' % drv, '"vdd"="*vdd"', '']
for k in (1, 2, 3):
    ww, hh, d = M[k]
    out += ['[' + base + r'\MODES\16\%d,%d]' % (ww, hh), '"drv"="%s"' % d, '']
out += [r'[HKEY_LOCAL_MACHINE\Config\0001\Display\Settings]', '"BitsPerPixel"="16"', '"Resolution"="%d,%d"' % (w, h)]
print('\r\n'.join(out))
