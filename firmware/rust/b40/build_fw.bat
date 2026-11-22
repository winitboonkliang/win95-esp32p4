@echo off
:: Rust PIT for the Win95 firmware (no_std, C ABI). Output: base\rust\libb40.a
pushd %~dp0
cargo +nightly build --release -Z build-std=core --target riscv32imafc-unknown-none-elf --no-default-features --features fw || (popd & exit /b 1)
copy /Y target\riscv32imafc-unknown-none-elf\release\libb40.a ..\libb40.a >nul
popd
