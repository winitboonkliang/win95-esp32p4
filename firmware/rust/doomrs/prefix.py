#!/usr/bin/env python3
"""B83: libdoomrs.a -> libdoomrs_p.a with every symbol the archive defines renamed rs_<name>, so the Rust engine links beside the C
engine (same C symbol names) in the Doom app. Not renamed: __* and the libc names in LIBC (newlib/libgcc versions keep winning, as before) and
symbols the archive only references (host glue DG_*, b82_*, b83_*, libc). usage: prefix.py <in.a> <out.a> <toolchain bin dir>"""
import subprocess, sys, os, tempfile

src, dst, tb = sys.argv[1:4]
nm = os.path.join(tb, 'riscv32-esp-elf-nm')
oc = os.path.join(tb, 'riscv32-esp-elf-objcopy')
out = subprocess.run([nm, '-g', '--defined-only', '-f', 'posix', src], capture_output=True, text=True, check=True).stdout
LIBC = {'bcmp', 'memcmp', 'memcpy', 'memmove', 'memset', 'strlen'}   # compiler_builtins' weak copies: newlib keeps winning
names = set()
for ln in out.splitlines():
    p = ln.split()
    if len(p) >= 2 and not ln.endswith(':') and p[1] not in ('U', 'w', 'v'):
        n = p[0]
        if n.startswith('__') or n in LIBC:
            continue
        names.add(n)
with tempfile.NamedTemporaryFile('w', delete=False, suffix='.txt') as f:
    for n in sorted(names):
        f.write('%s rs_%s\n' % (n, n))
    lst = f.name
subprocess.run([oc, '--redefine-syms=' + lst, src, dst], check=True)
os.unlink(lst)
print('prefix: %d symbols -> rs_' % len(names))
