@echo off
:: fails if the riscv object of the b40 crate references any libcore panic/bounds/unwrap symbol
pushd %~dp0
cargo +nightly build --release -Z build-std=core --target riscv32imafc-unknown-none-elf --no-default-features --features board || exit /b 1
copy /Y target\riscv32imafc-unknown-none-elf\release\libb40.a libb40.a >nul
set NM=C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-nm.exe
%NM% -A libb40.a | findstr /R /C:"b40-.*U .*panic_const" /C:"b40-.*U .*panic_bounds" /C:"b40-.*U .*unwrap_failed" /C:"b40-.*U .*slice_index" /C:"b40-.*U .*panic_fmt" && (echo NOPANIC FAIL & exit /b 1)
echo NOPANIC OK
popd
