#!/bin/bash
# build a GFX95 display driver (WSL).  usage: build_drv.sh [W H OUT.DRV]  (default 1280 800 driver/B29GFX.DRV);  extra assembler flags via AS_FLAGS (e.g. -DDBG)
cd "$(dirname "$0")/.."
W=${1:-1280}; H=${2:-800}; OUT=${3:-driver/B29GFX.DRV}
python3 -I tools/gen_gdiinfo.py $W $H > /dev/null
cd driver && as --32 $AS_FLAGS --defsym W=$W --defsym H=$H -o drv.o drv.S && ld -m elf_i386 -Ttext=0 -o drv.elf drv.o 2>&1 | head && objcopy -O binary -j .text drv.elf text.bin && objcopy -O binary -j .fix drv.elf fix.bin && nm drv.elf > syms.txt && cd .. && python3 -I tools/mkne.py driver/text.bin driver/fix.bin driver/syms.txt $OUT
