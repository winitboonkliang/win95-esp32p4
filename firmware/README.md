# firmware

The board firmware tree: the x86 emulator (forked from [tiny386](https://github.com/hchunhui/tiny386), its original README is `README-tiny386.md`),
the ESP-IDF project for both apps (`esp/`), Rust crates (`rust/`, `tools/b82/b76host`), our Windows 95 guest drivers (`gfx95/`, `ptr/`, `guest/`)
and the overlay tools (`tools/`).

Build and flash: see `../docs/BUILD.md`. Architecture: `../docs/ARCHITECTURE.md`. Licences: `../NOTICE.md`.

Comments in the code refer to work items as `Bnn` (for example B29 = display, B45 = pointer, B61 = snapshot, B76 = native apps,
B82-B85 = Doom) and to `lab:` notes; those lab notes are not published, the numbers that matter are in `../docs/PERFORMANCE.md`.
"Board A" is our ESP32-P4 chip v1.0 board, "board B" the chip v1.3 board.
