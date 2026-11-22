# Architecture

The idea in one line: keep the real Windows 95 binaries, and rebuild the *machine under them* for the ESP32-P4. Windows gets small
drivers of our own that talk straight to the host (paravirtual), so it never touches slow emulated 1995 hardware on its hot paths.
We started from tiny386 and an earlier port of ours; most hot parts have since been rewritten and measured on the board.

## The board

| part | what it is | how we use it |
|---|---|---|
| ESP32-P4 (chip v1.0 and v1.3 tested) | 2 RISC-V cores at 360 MHz, PIE SIMD extension, PPA 2D engine, JPEG/H.264 engines | core 0 = x86 emulator, core 1 = host services |
| 32 MB PSRAM | ~88 MB/s streaming past the L2 cache (measured) | guest RAM (16 MB), frame buffers, overlays. PSRAM bandwidth is the real limit everywhere |
| JD9365 10.1" 1280x800 MIPI-DSI panel | DPI pixel clock set by us | 40 MHz for Windows (below ~40 MHz the panel shows stripes), 48 MHz in Doom. 180-degree rotation via the panel's own mirror bits, zero cost |
| GSL3680 touch | I2C, shared bus with the codec and the RTC | pointer for Windows, touch overlay in Doom |
| RX8025T RTC | battery-backed clock on the same I2C bus | Windows time at boot and resync |
| ES8311 codec + speaker | I2S | Doom music and effects (Windows sound is planned) |
| microSD | SDMMC, exFAT | Windows disk image, overlays, snapshots, WADs, saves |
| ESP32-C6 | Wi-Fi/BLE co-processor over SDIO | used by the file service in the lab; not needed to run Windows or Doom |

## Windows side: the x86 machine (`firmware/`)

- **CPU**: tiny386's i386 core with our hot paths (fetch/decode fast paths, TLB-hit path, fused compare+branch), FPU on (486DX class).
  Bit-exact against the reference on boot vectors; measured +28 % CPU over the starting point.
- **Firmware**: no SeaBIOS on the board. Our own 64 KB BIOS (`fw/bios.S`) is linked into the app; there is no POST, the host builds the
  post-POST machine state and jumps to the boot sector. The few BIOS calls DOS/Windows still make trap through hypercall ports into
  Rust (`rust/fwhost`). It includes a minimal Plug and Play BIOS header so Device Manager is clean.
- **Timer**: the 8254 is Rust (`rust/b40`): guest time comes from the board clock, no lost ticks, deadline-based HLT so an idle Windows
  lets the core sleep. Windows never executes HLT by itself; our tiny `P4IDLE.VXD` (`guest/vxd/p4idle.S`, 109 bytes of code) does it.
- **Clock**: at boot the host waits for the RTC second edge; `esp/main/b86clock.c` compares Windows time with the RTC every 10 s and asks
  our `P4TIME` VxD (`ptr/time/p4time.S`, port 0xF1E8) to resync on drift > 2 s. Windows time stays within +-0.3 s.
- **Disk**: the Windows image on the SD card is opened **read-only**. All writes go to a RAM overlay that spills to `/sdcard/ovspill.bin`.
  Driver/registry changes ship as sector overlays (`b29_N.ovl`, `b76.ovl`) applied on top of the image at boot. Windows uses its
  32-bit disk driver (ESDI_506) against our IDE model.
- **Display**: `gfx95/driver/drv.S` is our own Windows 95 display minidriver (`B29GFX.DRV`, written in assembly, NE file produced by our
  own packer `gfx95/tools/mkne.py`). It exposes a linear 1280x800x16 surface (also 1024x640 and 640x400, scaled by the PPA).
  Hot GDI calls (BitBlt, DIB blits, StretchDIBits, ExtTextOut) trap to the host and run natively on the P4 with PIE SIMD row loops
  (`esp/main/gfx_pie.S`, `eto_pie.S`, `gfxddi.c`, `gfxnat.c`). The host presents only damaged rectangles through the PPA
  (`rust/present_core`, `esp/main/b29_present.c`), on core 1, paced to the panel. The mouse cursor is a host overlay.
- **Pointer**: `ptr/driver/ptr.S` = `P4PTR.DRV`, an absolute pointer driver on its own IRQ. Touch reports go through the Rust gesture
  engine `rust/b45ptr` (tap, double tap, long press = right click, drag), tested against recorded traces (`rust/b45ptr/traces`).
- **Native apps (B76)**: `guest/b76stub/b76app.S` builds a tiny Win32 program with our own PE writer. It only runs a message loop and
  sends each window message to the host with one `OUT` instruction (port 0xF1E0); the host app (Rust, `tools/b82/b76host`) decides
  what to draw and replies with a short "call script". Notepad works this way, with near-zero x86 work while typing.
- **Golden snapshot (B61)**: after the desktop is reached and idle for 2 s, the whole machine (RAM, devices, overlay) is written to the
  SD card, zero pages dropped and compressed; `esp/main/lz4_dec.S` decodes it. Power-on restores it in ~0.5 s. A snapshot is only
  accepted when it carries the "desktop reached" flag and the matching firmware id, so a hung boot can never become the golden one.
  An empty file `/sdcard/b61.off` forces cold boots.

## Doom side: a second app (`firmware/esp/main/b82doom.c`, `b83*`, `b84*`, `b85*`, `esp/components/doomp4`, `rust/doomrs`)

- **Two apps, one flash** (16 MB): `factory` = Windows app, `ota_0` at 0x200000 (4 MB) = Doom app with both engines (C and Rust).
  See `esp/partitions.csv`.
- **The switch needs no flash write**: the Windows desktop shortcut sets a retained LP register (magic + engine + WAD) and restarts via a
  200 ms deep sleep. Our bootloader (`esp/bootloader_components/main`, ESP-IDF 5.4.4's plus a few lines) reads the register and boots
  `ota_0`; anything else, including a cold power-up or an empty Doom partition, boots Windows. Quit reverses it and Windows resumes
  from its snapshot (2.6 s end to end).
- **Whole chip for the game**: no emulator while Doom runs, all PSRAM, both cores.
- **Present**: 8-bit frame -> RGB565 by an RV32 assembly kernel (`b82_pie.S`, checked against a C oracle at every start) on core 0,
  then PPA scale + rotate into one of 3 DPI buffers on a core-1 task, flipped on vsync.
- **Engine**: doomgeneric 640x400 tree (`esp/components/doomp4`, GPL). `rust/doomrs` is the same engine translated with c2rust and then
  optimised by hand (`own/`: column/span kernels, 3D view drawn by both cores). A PC oracle hashes every frame and the game state on the
  shareware demos: Rust == C on all 11,188 frames. The Rust library's symbols are renamed `rs_*` so both engines live in one app;
  host code reaches engine symbols only through `esp/main/b83api.h`.
- **Sound** (`b84mix.c`, `b84snd.c`): 44.1 kHz mono mixer with gain ramps, speaker EQ, bass enhancement and a -3 dBFS limiter; music from
  the OPL emulator (fmopl) running in PSRAM; ES8311 volume default 30 %.
- **Controls** (`b85*.c`): any USB HID gamepad by parsing its report descriptor, XInput pads by our own USB host client (rumble, LED),
  mouse, keyboard, translucent touch overlay (shown for 5 s after a touch). Saves and settings on the SD card.

## Why it is fast enough

Measured first, then built: the software mouse cursor was ~75 % of the display cost in stock Windows, so it became a host overlay; the
panel scan-out alone takes ~106 MB/s of PSRAM bandwidth, so only changed rectangles are presented; boot was the biggest wait, so it was
replaced by a snapshot. Hardware engines (PPA, PIE SIMD, the second core) are used before plain loops, and every change is checked
on the real board, not only on a PC build.
