#!/bin/bash
# Builds the Rust static libraries the ESP-IDF projects link (the prebuilt .a files are not in the public repository).
# Needs: rustup nightly with rust-src (for -Z build-std=core), and the ESP-IDF riscv32-esp-elf toolchain on PATH (export.sh of IDF 5.4.4).
# Collected from the per-crate lab scripts (rust/*/build_fw.bat, the B45/B76 lib scripts); not run in exactly this form - report problems.
set -e
cd "$(dirname "$0")"
TGT=riscv32imafc-unknown-none-elf
NIGHTLY="cargo +nightly build --release -Z build-std=core --target $TGT"
X=riscv32-esp-elf
mkdir -p esp/main/lib

# weaken the Rust panic handler: several staticlibs each define one, the linker must keep only one
weaken() { local s; s=$($X-nm "$1" 2>/dev/null | awk '/ T .*rust_begin_unwind/{print $3; exit}'); [ -n "$s" ] && $X-objcopy --weaken-symbol="$s" "$1" || true; }

# 1. Win95 firmware host: B40 timer (8254) + B60 own guest BIOS services (fwhost) in one staticlib -> rust/libw95rs.a (default, B60_FW=1)
( cd rust/w95rs && $NIGHTLY ) && cp rust/w95rs/target/$TGT/release/libw95rs.a rust/libw95rs.a
# 1b. B40 alone (only for -DB60_FW=0, SeaBIOS boot) -> rust/libb40.a
( cd rust/b40 && $NIGHTLY --no-default-features --features fw ) && cp rust/b40/target/$TGT/release/libb40.a rust/libb40.a

# 2. GFX95 linear presenter -> esp/main/lib/libpresent_core.a
( cd rust/present_core && cargo build --release --target $TGT ) && cp rust/present_core/target/$TGT/release/libpresent_core.a esp/main/lib/

# 3. B45 touch/pointer engine -> esp/main/lib/libb45ptr.a (soft-float popcount objects removed, panic handler weak)
( cd rust/b45ptr && cargo build --release --features capi --target $TGT )
L=esp/main/lib/libb45ptr.a; cp rust/b45ptr/target/$TGT/release/libb45ptr.a $L
for o in $($X-ar t $L | grep popcount); do $X-ar d $L "$o"; done
weaken $L

# 4. B76 native Notepad host app -> esp/main/lib/libb76pad.a
( cd tools/b82/b76host && cargo build --release --target $TGT ) && cp tools/b82/b76host/target/$TGT/release/libb76pad.a esp/main/lib/
weaken esp/main/lib/libb76pad.a

# 5. Doom engine in Rust (only the Doom app links it) -> esp/main/lib/libdoomrs_p.a, every symbol renamed rs_* so it sits beside the C engine
( cd rust/doomrs && $NIGHTLY )
python3 rust/doomrs/prefix.py rust/doomrs/target/$TGT/release/libdoomrs.a esp/main/lib/libdoomrs_p.a "$(dirname "$(command -v $X-nm)")"

ls -l rust/*.a esp/main/lib/*.a
