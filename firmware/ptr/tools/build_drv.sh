#!/bin/bash
B=${B45:-$(cd "$(dirname "$0")/.." && pwd)}   # the ptr/ folder of this tree
cd $B/driver && as --32 "$@" -o ptr.o ptr.S && ld -m elf_i386 -Ttext=0 -o ptr.elf ptr.o && objcopy -O binary -j .text ptr.elf text.bin && objcopy -O binary -j .fix ptr.elf fix.bin && nm ptr.elf > syms.txt && python3 -I $B/tools/mkne_ptr.py text.bin fix.bin syms.txt P4PTR.DRV && objdump -D -b binary -m i8086 text.bin > text.dis && sha256sum P4PTR.DRV
