#!/bin/bash
# B61 P4TIME: build p4time.vxd (host clock port 0xF1E8) and the driver overlay b61t_1.ovl = base gfx95/img/b29_1.ovl (GFX95 stub + P4PTR)
# + P4TIME.VXD in WINDOWS/SYSTEM + SYSTEM.INI device=*vpowerd -> device=p4time.vxd. WSL, caller holds a WSL slot. Never writes images/.
set -e
P=${W95P4:?set W95P4 to your work folder}; T=$P/lab/BLUE/B61/time; X=$P/study/IDLE_VXD; B=$P/lab/BLUE/B61/wt
VXD_CFLAGS=-DP4TIME bash $X/build_vxd.sh $T/p4time.S P4TIME $T/p4time.vxd
W=~/b61t_ovl; rm -rf $W; mkdir -p $W; I=$W/mk.img
cp --sparse=always $P/images/win95_prod.img $I; chmod u+w $I
python3 -I -c "
import sys, struct
f = open(sys.argv[1], 'r+b'); o = open(sys.argv[2], 'rb').read()
for i in range(0, len(o), 516):
    l = struct.unpack_from('<I', o, i)[0]; f.seek(l * 512); f.write(o[i + 4:i + 516])
" $I $B/gfx95/img/b29_1.ovl
bash $X/install_vxd.sh $I $T/p4time.vxd 'device=p4time.vxd' 'device=*vpowerd'
python3 -I $B/gfx95/tools/mkovl.py $P/images/win95_prod.img $I $T/b61t_1.ovl
sha256sum $B/gfx95/img/b29_1.ovl $T/b61t_1.ovl $T/p4time.vxd; ls -l $T/b61t_1.ovl
rm -rf $W
