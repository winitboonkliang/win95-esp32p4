#!/bin/bash
# build ~/b29run/b29[_mN].img = copy of win95_prod.img with our drivers + display class registry (DOS-stage REGEDIT import).  WSL.
# usage: mkimg.sh [MODE]   MODE 1 (default) = 1280x800, 2 = 1024x640, 3 = 640x400 as the boot mode; all three driver files are always installed (the Display
# Properties slider lists the three MODES keys, each names its own driver file; a restart applies the change)
B=${W95P4:?set W95P4 to your work folder}/lab/BLUE/B29a; export MTOOLS_SKIP_CHECK=1
MODE=${1:-1}; SUF=""; [ "$MODE" != "1" ] && SUF="_m$MODE"
REG=$B/img/b29_m$MODE.reg; [ -f $REG ] || REG=$B/img/b29.reg
mkdir -p ~/b29run; I=~/b29run/b29_mk.img
cp --sparse=always ${W95P4:?set W95P4 to your work folder}/images/win95_prod.img $I; chmod u+w $I
mcopy -i $I@@32256 $B/driver/B29GFX.DRV ::WINDOWS/SYSTEM/B29GFX.DRV
[ -f $B/driver/B29G1024.DRV ] && mcopy -i $I@@32256 $B/driver/B29G1024.DRV ::WINDOWS/SYSTEM/B29G1024.DRV
[ -f $B/driver/B29G640.DRV ] && mcopy -i $I@@32256 $B/driver/B29G640.DRV ::WINDOWS/SYSTEM/B29G640.DRV
mcopy -i $I@@32256 $B/img/autoexec.bat ::AUTOEXEC.BAT
mcopy -i $I@@32256 $REG ::B29.REG
rm -rf $B/runs/mkimg$SUF; KEEP_IMG=1 bash $B/tools/run.sh $I mkimg$SUF $B/scripts/q12.scr
mv $B/runs/mkimg$SUF/disk.img ~/b29run/b29$SUF.img
mdel -i ~/b29run/b29$SUF.img@@32256 ::AUTOEXEC.BAT ::B29.REG
rm -f $I; ls -la ~/b29run/b29$SUF.img
