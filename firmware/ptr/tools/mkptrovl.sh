#!/bin/bash
# ptr_1.ovl = gfx95/img/b29_1.ovl (GFX95 driver overlay) + ptr/driver/P4PTR.DRV + gfx95/driver/B29GFX.DRV + SYSTEM.INI mouse.drv=p4ptr.drv.
# Linux/WSL with mtools. Inputs: W95P4_IMG = your own Windows 95 disk image (only read), B = this firmware folder.
set -e
B=${B:-$(cd "$(dirname "$0")/../.." && pwd)}; IMG=${W95P4_IMG:?set W95P4_IMG to your Windows 95 disk image}; OUT=$B/ptr/img/ptr_1.ovl; export MTOOLS_SKIP_CHECK=1
W=$(mktemp -d); I=$W/mk.img
trap 'rm -rf $W' EXIT
cp --sparse=always $IMG $I; chmod u+w $I
python3 -I -c "
import sys, struct
f = open(sys.argv[1], 'r+b'); o = open(sys.argv[2], 'rb').read()
for i in range(0, len(o), 516):
    l = struct.unpack_from('<I', o, i)[0]; f.seek(l * 512); f.write(o[i + 4:i + 516])
" $I $B/gfx95/img/b29_1.ovl
mcopy -n -i $I@@32256 ::WINDOWS/SYSTEM.INI $W/SYSTEM.INI
sed -i 's/^mouse\.drv=mouse\.drv\r$/mouse.drv=p4ptr.drv\r/' $W/SYSTEM.INI
grep -c "^mouse.drv=p4ptr.drv" $W/SYSTEM.INI
grep -i "^display.drv\|^mouse.drv" $W/SYSTEM.INI
# BOOT 19 Nov: with a non-stock mouse.drv Windows itself rewrites [386Enh] mouse=*vmouse, msmouse.vxd to *vmd and restarts
# ("Windows is now restarting"); the appliance drops the rewrite, so every boot ran twice (+60 % instructions, 72 -> 110 s). Pre-apply it.
sed -i 's/^mouse=\*vmouse, msmouse\.vxd\r$/mouse=*vmd\r/' $W/SYSTEM.INI
test "$(grep -c '^mouse=\*vmd' $W/SYSTEM.INI)" = 1
mcopy -o -i $I@@32256 $W/SYSTEM.INI ::WINDOWS/SYSTEM.INI
mcopy -o -i $I@@32256 $B/ptr/driver/P4PTR.DRV ::WINDOWS/SYSTEM/P4PTR.DRV
mcopy -o -i $I@@32256 $B/gfx95/driver/B29GFX.DRV ::WINDOWS/SYSTEM/B29GFX.DRV
# LEAD devmgr 20 Nov: with [386Enh] mouse=*vmd the stock PS/2 mouse devnode's loader *vmouse never loads -> Device Manager "!" Code 8.
# Our pointer needs no device loader: drop DevLoader from Class\Mouse\000 (real-mode REGEDIT in a DOS-stage boot of this scratch image).
B=$B bash $B/ptr/tools/regstage.sh $I $W
python3 -I $B/gfx95/tools/mkovl.py $IMG $I $OUT
sha256sum $IMG | cut -c1-8; sha256sum $OUT $B/gfx95/img/b29_1.ovl; ls -l $OUT
