#!/bin/bash
# build_vxd.sh <src.S> <NAME> [out.vxd]
# Own VxD toolchain: GNU as (intel syntax) + ld (flat, org 0) + vxd/mkvxd.py (LE writer). Run in WSL.
# Rules for the source: everything in .text (DDB first at offset 0), 32-bit code, `.long label` pointers are
# found automatically (R_386_32 relocations -> LE fixups), VMM services via `int 0x20; .word SERVICE, DEVICE_ID`.
set -e
H=$(cd "$(dirname "$0")" && pwd)
SRC=$(readlink -f "$1"); NAME=$2; OUT=${3:-${SRC%.*}.vxd}
T=$(mktemp -d); trap 'rm -rf $T' EXIT
gcc -m32 $VXD_CFLAGS -c "$SRC" -o $T/o.o
ld -m elf_i386 -Ttext 0 --oformat binary -e 0 -o $T/flat.bin $T/o.o
# every absolute 32-bit pointer inside the object needs a loader fixup (the VxD is not loaded at a fixed address)
FIX=$(objdump -r $T/o.o | awk '/R_386_32/ {print "0x"$1}' | sed 's/:$//' | tr '\n' ' ')
python3 $H/mkvxd.py $T/flat.bin "$OUT" "$NAME" $FIX
