@echo off
:: Rust side of the Win95 firmware: B40 timer + B60 own firmware host. Output: base\rust\libw95rs.a
pushd %~dp0
cargo +nightly build --release -Z build-std=core --target riscv32imafc-unknown-none-elf || (popd & exit /b 1)
copy /Y target\riscv32imafc-unknown-none-elf\release\libw95rs.a ..\libw95rs.a >nul
popd
