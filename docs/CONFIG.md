# Configuration reference

Everything you can set on the board, where it lives, and what our tools write. Values are the ones we run on the
JC8012P4A1 (ESP32-P4, 10.1" 1280x800) on 22 Nov 2026. See [BUILD.md](BUILD.md) for building and flashing.

## 1. SD card layout (FAT32, root = `/sdcard`)

| File | What it is | Where it comes from | Written by the board? |
|---|---|---|---|
| `tiny386.ini` | Emulator settings (section 2) | You (copy the example below) | No |
| `bios.bin`, `vgabios.bin` | PC BIOS and VGA BIOS images | You (see BUILD.md "parts you must supply") | No |
| `hd.img` | The Windows 95 hard-disk image | You: your own licensed Windows 95, installed into a raw disk image | **Never** (read-only, see `b61_disk.ovl`) |
| `b29_1.ovl` | Card overlay: our Windows drivers (B29GFX.DRV display, P4PTR.DRV pointer, P4TIME clock) patched over `hd.img` sectors at boot | Built by `firmware/gfx95` + `firmware/ptr/tools/mkptrovl.sh` from **your** `hd.img` | No |
| `b76.ovl` | Card overlay: native apps (Notepad stub), Doom icons and shortcuts (.lnk/.ico/.dm1/.dmu/.dm2) | Built by the B76/B82 overlay scripts from your `hd.img` | No |
| `b61snap.A` / `b61snap.B` | "Golden" desktop snapshot (instant-on, two slots) | Written once at the first desktop after a cold boot | Yes, once |
| `b61_disk.ovl` | Disk writes of the golden session (the RAM overlay, saved) | Written with the snapshot | Yes, once |
| `b61.cfg` | Snapshot bookkeeping | Board | Yes |
| `ovspill.bin` | Overflow of the RAM disk overlay during a session | Board | Yes |
| `b61.off` | **Switch:** present = always cold boot, no resume | You (empty file) | No |
| `b61_maint` | **Switch:** present = maintenance boot (writes kept in the RAM overlay, saved as the new golden overlay) | You (empty file) | No |
| `DOOM/doom1.wad` `DOOM/doom.wad` `DOOM/doom2.wad` | Doom game data: shareware, Ultimate Doom, Doom II | **You** (buy the game, or shareware `doom1.wad`, or Freedoom renamed) - never shipped here | No |
| `DOOM/b85.cfg` | Doom controls/overlay settings (section 5) | Board writes defaults on first start | Yes |
| `DOOM/savegame/` | Doom saves | Board | Yes |
| `bl.txt` | Optional: backlight percent 1..100 (5 kHz PWM; no file = 100 %) | You | No |
| `pclk.txt` | Optional maintenance: panel pixel clock in MHz (20..80); refresh = pclk / 1.1546 M | You | No |
| `rot180.txt` | Optional: present = panel and touch rotated 180 degrees | You | No |
| `b29.ini` | Optional maintenance: if present it is used **instead of** `tiny386.ini` (your ini stays untouched) | You | No |
| `b29d.txt` | Optional: display present tuning (settle / cap in microseconds) | You | No |

Safety rule we follow: our tools only ever put/remove the overlay and snapshot files. `hd.img`, `bios.bin`, `vgabios.bin` and
`tiny386.ini` are never written by the board or the tools.

## 2. `tiny386.ini` (the one we run)

```ini
[esp]
; USB HID host: keyboard + mouse (plug in before boot)
enable_usb = 1
; Wi-Fi (optional, via the on-board ESP32-C6); leave empty if unused
ssid =
pass =

[pc]
bios = /sdcard/bios.bin
vga_bios = /sdcard/vgabios.bin
mem_size = 16M
vga_mem_size = 2M
hda = /sdcard/hd.img
fill_cmos = 1

[display]
width = 1280
height = 800

[cpu]
gen = 4      ; 486 class
fpu = 1
```

`firmware/esp/tiny386.ini` in this repo is the upstream tiny386 example (480x320, 7.5 MB RAM) for smaller boards; use the one above
for the 10.1" board. `mem_size` is the guest RAM in PSRAM; Windows 95 is comfortable at 16M.

## 3. Flash layout (16 MB)

Windows build (`partitions.csv`):

| Name | Type | Offset | Size | Content |
|---|---|---|---|---|
| nvs | data/nvs | 0x9000 | 24K | |
| phy_init | data/phy | 0xF000 | 4K | |
| factory | app | 0x10000 | 1792K | **Windows app** (emulator + host drivers) |
| bios.bin / vgabios.bin / linuxstart.bin | data | 0x1D0000.. | 128K / 60K / 4K | optional built-in copies |
| doom | app/ota_0 | 0x200000 | 4M | **Doom app** (built separately, flashed here) |
| storage | data/fat | 0x600000 | 10M | spare |

The Doom app is built with `partitions_doomapp.csv` (same offsets, Doom as the app at 0x200000). Windows -> Doom: the Windows app writes a
flag in RTC memory and deep-sleeps 200 ms; our bootloader component (`bootloader_components/main`) boots the slot the flag names.
Quit Doom -> back to the factory slot. Keep the bootloader's image check on (a wrong slot must not boot silently).

## 4. Build options

| Item | Value | Why |
|---|---|---|
| ESP-IDF | 5.4.4 | the version everything was measured on |
| Board define | `-DBOARD=jc8012p4a1c` (sdkconfig.jc8012p4a1c) | panel JD9365, touch GSL3680, codec ES8311 |
| CPU | 360 MHz (`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360`) | |
| PSRAM | hex mode, 200 MHz (`CONFIG_SPIRAM_MODE_HEX`, `CONFIG_SPIRAM_SPEED_200M`) | memory bandwidth is the main limit |
| Optimisation | `CONFIG_COMPILER_OPTIMIZATION_PERF` | |
| FreeRTOS tick | 1000 Hz | |
| Windows app | `idf.py -B build_win -DBOARD=jc8012p4a1c -DDOOM_ENGINE=none build` | no Doom code in the Windows image |
| Doom app | `idf.py -B build_doom -DSDKCONFIG=<abs path>/sdkconfig.doomapp -DDOOM_ENGINE=both -DB83_DOOM_APP=1 build` | `both` = C and Rust engines (icons choose); `c` or `rust` for one |
| Rust libraries | `firmware/build_rustlibs.sh` (target `riscv32imafc-unknown-none-elf`) -> `esp/main/lib/*.a` | prebuilt `.a` are not in the repo |

Trap we hit: `CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP=y` lets the heap hand out the 31 KB LP SRAM, which is ~38x slower; hot
stacks there made Doom crawl. Allocate hot buffers with `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`, but leave room for the SDMMC DMA
bounce buffers.

## 5. Doom settings (`/sdcard/DOOM/b85.cfg`)

| Key | Default | Meaning |
|---|---|---|
| `touch_overlay` | 1 | 0 off, 1 auto (on-screen buttons only for 5 s after a touch), 2 always |
| `fps_overlay` | 0 | 1 = fps / frame time box (also: keyboard \` or pad Back+Y) |
| `pad_sensitivity` | 10 | 10 = 1.0x, 1..30 |
| `pad_invert_y` | 0 | |
| `pad_deadzone` | 9 | 0..40 |
| `mouse_move_y` | 0 | 1 = mouse Y walks |
| `defaults_version` | - | internal |

Engine settings (detail, volumes, screen size, key/pad mapping) are in the engine's own `default.cfg` next to the WADs.
Codec volume default is 30 % (console `doomvol 0..70` while Doom runs; 70 is the hard cap - loud output can brown out a small board).

## 6. Service console (USB serial, 115200 8N1)

Type a command and Enter. Lines up to 159 characters; send long lines in pieces of <= 32 bytes (the board polls a 128-byte FIFO).

| Command | While | What it does |
|---|---|---|
| `b76keys <text>` | Windows | types like a USB keyboard. `^` = Ctrl+Esc, `|` = Enter, `~` = Alt+F4, \`u \`d \`l \`r = arrows, \`e = Esc, \`w / \`W = 0.8 / 2.5 s pause |
| `ptr <dx> <dy> <buttons>` | Windows | relative pointer move / buttons (Windows doubles moves above its threshold) |
| `doom go [wad0\|wad1\|wad2] [engc]` | Windows | restart into the Doom app (wad0 shareware, wad1 Ultimate, wad2 Doom II; default Rust engine, `engc` = C) |
| `doom quit` | Doom | back to Windows |
| `doomkey <keys>` | Doom | `<` `>` `^` `v` arrows, `!` fire, space use, `|` Enter, `~` Esc |
| `doomvol <0..70>` | Doom | codec volume |
| `doom st` / `doom where` / `doom ur` | Doom | state / memory placement / display underruns |
| `wclock` | Windows | sync the Windows clock to the board RTC now |
| `settime <YYYY-MM-DD HH:MM:SS>` | any | set the board RTC |
| `shot` | Windows | screenshot of the guest screen over the serial line (`tools/card.py shot out.png`) |
| `touchstat` / `touch ...` | any | touch controller statistics / synthetic touch |
| `text` | Windows | dump the text-mode screen |
| `heapmap` / `prof` / `x86` | any | memory map / profiler / emulator state |
| `svc!` | any | reboot into service mode (`card.py` file transfer: ls, put, rm, sha256) |

## 7. Tools

| Tool | Use |
|---|---|
| `tools/card.py` | SD card over USB serial: `ls`, `put <local> <name>`, `rm <name>`, `sha256 <name>`, `shot`, `reboot`. Always check `sha256` on the card after a `put`. |
| `firmware/ptr/tools/mkptrovl.sh`, `regstage.sh` | build `b29_1.ovl` / stage registry changes (device names) from your `hd.img` |
| `firmware/gfx95/` | the display driver (`B29GFX.DRV`) source and build |
| `firmware/guest/` | guest-side sources: P4IDLE VxD, B76 app stub |
| `firmware/build_rustlibs.sh` | rebuild the Rust static libraries |
