@echo off
:: B83 Rust Doom engine for the P4 -> esp\main\lib\libdoomrs_p.a (symbols rs_*, beside the C engine) (doomp4 component links it when DOOM_RS=1, the default here)
:: Sources: lib.rs + src\ are generated from the C engine (teams\LEAD\B83\tools\regen.sh); hand-written: b83.rs, own\
pushd %~dp0
cargo +nightly build --release -Z build-std=core --target riscv32imafc-unknown-none-elf || (popd & exit /b 1)
python prefix.py target\riscv32imafc-unknown-none-elf\release\libdoomrs.a ..\..\esp\main\lib\libdoomrs_p.a C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin || (popd & exit /b 1)
popd
