# Build it yourself

Honest warning first: this is a hobby lab project. Everything here has run on our boards, but this public tree is a cleaned snapshot
and was **not built from scratch in this exact form**. Expect to fix a path or two. Issues and notes are welcome.

## What you need

**Hardware**
- Guition **JC8012P4A1** (we used the JC8012P4A1C variant): ESP32-P4 **chip v1.x** (v1.0 and v1.3 tested; v3.x boards need ESP-IDF 6.x and
  are not supported by this tree), 32 MB PSRAM, 16 MB flash, 10.1" 1280x800 JD9365 MIPI-DSI panel, GSL3680 touch, ES8311 codec + speaker,
  RX8025T RTC (fit a CR1220 cell to keep the time), microSD slot.
- microSD card, exFAT or FAT32 (a fast card makes resume faster: 0.5 s vs 1 s on an old card).
- Optional: USB keyboard, USB mouse, USB gamepad (any HID pad, or XInput such as Xbox 360 wired / Logitech F310 in X mode).

**Software**
- **ESP-IDF 5.4.4** (newer releases drop P4 chip v1.0 by default).
- Rust **nightly** with `rust-src` and the target `riscv32imafc-unknown-none-elf`.
- Linux or WSL with `binutils` (32-bit `as`/`ld`), `python3`, `mtools`, `gcc` (for the guest drivers, BIOS and disk overlays).

## Things you must supply (not in this repository)

| file | where it comes from |
|---|---|
| Windows 95 disk image (`hd.img`) | your own licensed Windows 95, installed into a raw disk image with tiny386 on a PC (16 MB RAM machine, see the tiny386 README) |
| `fw/font8.bin` (2048 B, 8x8 glyphs, 128 chars), `fw/font16.bin` (4096 B, 8x16 glyphs, 256 chars) | VGA ROM fonts for our BIOS; dump them from a VGA BIOS you are allowed to use (e.g. SeaBIOS VGA BIOS built yourself) |
| `vgabios.bin` on the SD card | SeaBIOS VGA BIOS built as in the tiny386 README (`firmware/seabios/config` and `patch`); our native Notepad reads its 8x16 font |
| `bios.bin` (SeaBIOS) | only for installing Windows on the PC emulator and for `-DB60_FW=0` builds; the board uses our own BIOS |
| `esp/main/gsl/gsl_fw.h` | GSL3680 touch-controller firmware table for this panel: vendor data from the board maker's demo package (not redistributed here) |
| Doom WADs | shareware `doom1.wad`, your own `doom.wad` / `doom2.wad` (GOG, Steam), or Freedoom |

## 1. Rust libraries

```
cd firmware
./build_rustlibs.sh        # -> rust/libw95rs.a, rust/libb40.a, esp/main/lib/*.a
```

## 2. Our BIOS

```
cd firmware/fw && ./build_bios.sh     # needs font8.bin + font16.bin; -> bios.bin (embedded into the app), rust/fwhost/src/syms.rs
```

## 3. The two firmware apps

From `firmware/esp`, with the ESP-IDF 5.4.4 environment loaded:

```
# Windows app (factory partition)
idf.py -DBOARD=jc8012p4a1c -B build_win -DDOOM_ENGINE=none build

# Doom app (ota_0 partition), both engines (C + Rust)
idf.py -DBOARD=jc8012p4a1c -B build_doom -DSDKCONFIG=sdkconfig.doomapp -DDOOM_ENGINE=both -DB83_DOOM_APP=1 build
```

Managed components (`esp/main/idf_component.yml`) are fetched by the build. `DOOM_ENGINE` = `c`, `rs` or `both`.

## 4. Flash layout and flashing

`esp/partitions.csv` (16 MB):

| name | type | offset | size | content |
|---|---|---|---|---|
| nvs | data | 0x9000 | 24K | |
| phy_init | data | 0xf000 | 4K | |
| factory | app | 0x10000 | 1792K | Windows app |
| bios.bin, vgabios.bin, linuxstart.bin | data | 0x1d0000.. | | unused by the default build (kept from tiny386) |
| doom | app ota_0 | 0x200000 | 4M | Doom app |
| storage | data fat | 0x600000 | 10M | fallback FAT when no SD card |

```
esptool.py --chip esp32p4 -b 921600 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x2000   build_win/bootloader/bootloader.bin \
  0x8000   build_win/partition_table/partition-table.bin \
  0x10000  build_win/tiny386.bin \
  0x200000 build_doom/tiny386.bin
```

The bootloader is our own (ESP-IDF's + the Doom switch). Back up your board's flash first (`esptool.py read_flash 0 0x1000000 backup.bin`).

## 5. Windows drivers and disk overlays

The Windows image is never modified on the card; our drivers and registry changes travel as sector overlays.

```
bash gfx95/tools/build_all_drv.sh          # B29GFX.DRV (1280x800), B29G1024.DRV, B29G640.DRV  (from gfx95/driver/drv.S)
bash ptr/tools/build_drv.sh                # P4PTR.DRV (from ptr/driver/ptr.S)
bash guest/vxd/build_p4idle.sh            # P4IDLE.VXD (idle)
VXD_CFLAGS=-DP4TIME bash guest/vxd/build_vxd.sh ptr/time/p4time.S P4TIME p4time.vxd   # clock VxD, see ptr/time/README.md
bash guest/b76stub/build.sh                # B76APP.EXE, the native-app stub
W95P4_IMG=/path/to/hd.img bash ptr/tools/mkptrovl.sh   # driver overlay (b29_1.ovl / ptr_1.ovl)
bash tools/b83/mkovl_b83.sh                # b76.ovl: native Notepad + the six Doom shortcuts and icons
```

These overlay scripts were written for our lab folder layout (they read `W95P4` and expect the base overlay, the gate emulator and the
stub outputs in fixed places): read each script and set the paths. `gfx95/README.md` and `ptr/README.md` describe the pieces.

## 6. SD card layout

```
/hd.img           your Windows 95 disk image (opened read-only)
/tiny386.ini      see below
/vgabios.bin      VGA BIOS (font for the native Notepad)
/b29_1.ovl        driver + registry overlay (step 5)
/b76.ovl          native apps + Doom shortcuts overlay (step 5)
/DOOM/doom1.wad   and optionally doom.wad, doom2.wad      (saves go to /DOOM/savegame/<wad>/)
```

Files the board writes by itself: `b61snap.A` / `b61snap.B` (golden snapshots), `ovspill.bin` (disk write overlay), `DOOM/b85.cfg`.
Create an empty `/b61.off` to force cold boots (no snapshot resume).

`tiny386.ini` as used on our boards:

```
[esp]
ssid =
pass =
enable_usb = 1

[pc]
bios = /sdcard/bios.bin
vga_bios = /sdcard/vgabios.bin
mem_size = 16M
vga_mem_size = 2M
hda = /sdcard/hd.img
display = gfx95
display_mode = 1
hda_overlay = /sdcard/b29_%d.ovl
fill_cmos = 1

[display]
width = 1280
height = 800

[cpu]
gen = 4
fpu = 1
```

Leave `ssid`/`pass` empty unless you need Wi-Fi; never commit a filled-in ini. In your Windows image set `MSDOS.SYS` `AutoScan=0`.

## First power-on

The first boot is a cold boot (~80 s). When the desktop has been idle for 2 s, the board writes the golden snapshot; from then on every
power-on resumes in about a second. Double-click a Doom icon to switch apps; quit from Doom's menu to return.
