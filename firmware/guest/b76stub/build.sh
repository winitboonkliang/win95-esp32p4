#!/bin/bash
# builds B76APP.EXE (WSL: GNU as/ld + python3). Output: stub/out/B76APP.EXE, stub/out/imports.rs
set -e; cd "$(dirname "$0")"; mkdir -p out
python3 mkpe.py out/B76APP.EXE out/imports.inc out/imports.rs
cp out/imports.inc .
as --32 -o out/b76app.o b76app.S
ld -m elf_i386 -Ttext 0x401000 -e _start --oformat binary -o out/code.bin out/b76app.o
python3 mkpe.py out/B76APP.EXE out/imports.inc out/imports.rs out/code.bin
rm imports.inc
