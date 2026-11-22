# Notice - where the code comes from

This repository is a combined work. Every third-party file keeps its own copyright header and licence.

| part | origin | licence |
|---|---|---|
| `firmware/` emulator core and devices (files listed below) | [tiny386](https://github.com/hchunhui/tiny386) by Chunhui He, via our earlier ESP32-P4 port | BSD-3-Clause, `firmware/LICENSE-tiny386-BSD3.txt` |
| `firmware/vga.c`, `adlib.c`, `sb16.c` and other device models with a header | QEMU / TinyEMU authors (Fabrice Bellard, Vassili Karpov, ...) | MIT, see each file |
| `firmware/fmopl.c`, `firmware/esp/components/doomp4/opl/fmopl.c` | MAME fmopl (Tatsuyuki Satoh), LGPL fork | LGPL-2.1-or-later |
| `firmware/esp/components/doomp4/` | doomgeneric (ozkl) / Chocolate Doom (Simon Howard) / id Software Doom source, Nuked OPL3 (Alexey Khokholov) | GPL-2.0-or-later, `firmware/esp/components/doomp4/LICENSE` |
| `firmware/rust/doomrs/src/` | machine translation (c2rust) of the doomp4 C tree above, plus our hand-written `own/` | GPL-2.0-or-later (derived work) |
| `firmware/esp/main/gsl/gsl_point_id.c` | Silead GSLx680 touch reference code | GPL-2.0-or-later |
| `firmware/esp/main/jd9365/esp_lcd_jd9365.*` | Espressif panel driver | Apache-2.0 |
| `firmware/esp/components/fatfs/` | ESP-IDF 5.4.4 FatFs component (only `src/ffconf.h` changed: exFAT on) | FatFs licence (ChaN) + Apache-2.0 |
| `firmware/wasm/libc/nanoprintf.h` and other bundled helpers | see each file | see each file |
| everything else (new files, and our changes to the files above) | this project | GPL-2.0-or-later, `LICENSE` |

ESP-IDF itself (Apache-2.0) and the managed components in `firmware/esp/main/idf_component.yml` are dependencies, fetched by the build;
they are not in this repository.

## Files inherited from tiny386 / our earlier port (the first commit of the project)
Files not in this list were written in this project.

- `firmware/.gitignore`
- `firmware/Makefile`
- `firmware/adlib.c`
- `firmware/adlib.h`
- `firmware/amd64.c`
- `firmware/amd64.h`
- `firmware/amd64ins.def`
- `firmware/conf/gray386.ini`
- `firmware/conf/jslinux.ini`
- `firmware/conf/win98.ini`
- `firmware/conf/winxp.ini`
- `firmware/esp/.gitignore`
- `firmware/esp/CMakeLists.txt`
- `firmware/esp/dependencies.lock`
- `firmware/esp/esp-idf-6.0.patch`
- `firmware/esp/esp-idf.patch`
- `firmware/esp/main/CMakeLists.txt`
- `firmware/esp/main/board_jc8012p4a1c.h`
- `firmware/esp/main/common.h`
- `firmware/esp/main/esp_main.c`
- `firmware/esp/main/i2s.c`
- `firmware/esp/main/idf_component.yml`
- `firmware/esp/main/jd9365/esp_lcd_jd9365.c`
- `firmware/esp/main/jd9365/esp_lcd_jd9365.h`
- `firmware/esp/main/jd9365/ppa_rotator.cpp`
- `firmware/esp/main/jd9365/ppa_rotator.h`
- `firmware/esp/main/lcd_jd9365_p4.c`
- `firmware/esp/main/storage.c`
- `firmware/esp/main/usb_hid.c`
- `firmware/esp/main/usb_input.c`
- `firmware/esp/main/wifi.c`
- `firmware/esp/partitions.csv`
- `firmware/esp/sdkconfig.jc8012p4a1c`
- `firmware/esp/tiny386.ini`
- `firmware/esp/tiny386_elecrow7s3.ini`
- `firmware/fmopl.c`
- `firmware/fmopl.h`
- `firmware/fpu.c`
- `firmware/fpu.h`
- `firmware/glibc-fix/glibc-fix.c`
- `firmware/glibc-fix/glibc-fix.h`
- `firmware/i386.c`
- `firmware/i386.h`
- `firmware/i386ins.def`
- `firmware/i8042.c`
- `firmware/i8042.h`
- `firmware/i8254.c`
- `firmware/i8254.h`
- `firmware/i8257.c`
- `firmware/i8257.h`
- `firmware/i8259.c`
- `firmware/i8259.h`
- `firmware/ide.c`
- `firmware/ide.h`
- `firmware/ini.c`
- `firmware/ini.h`
- `firmware/kvm.c`
- `firmware/kvm.h`
- `firmware/linuxstart/Makefile`
- `firmware/linuxstart/config_linux-3.2`
- `firmware/linuxstart/config_linux-7.0`
- `firmware/linuxstart/config_linux-amd64-3.2`
- `firmware/linuxstart/config_linux-amd64-7.0`
- `firmware/linuxstart/libc.c`
- `firmware/linuxstart/libc.h`
- `firmware/linuxstart/linuxstart.c`
- `firmware/linuxstart/linuxstart.h`
- `firmware/linuxstart/linuxstart_head.S`
- `firmware/linuxstart/note.txt`
- `firmware/main.c`
- `firmware/misc.c`
- `firmware/misc.h`
- `firmware/ne2000.c`
- `firmware/ne2000.h`
- `firmware/osd/atlas.inl`
- `firmware/osd/microui.c`
- `firmware/osd/microui.h`
- `firmware/osd/osd.c`
- `firmware/osd/osd.h`
- `firmware/pc.c`
- `firmware/pc.h`
- `firmware/pci.c`
- `firmware/pci.h`
- `firmware/pcspk.c`
- `firmware/pcspk.h`
- `firmware/rawdraw/CNFA.c`
- `firmware/rawdraw/CNFA.h`
- `firmware/rawdraw/CNFA_alsa.c`
- `firmware/rawdraw/CNFA_winmm.c`
- `firmware/rawdraw/CNFG.c`
- `firmware/rawdraw/CNFG.h`
- `firmware/rawdraw/CNFGWinDriver.c`
- `firmware/rawdraw/CNFGXDriver.c`
- `firmware/rawdraw/alsa_wrapper.c`
- `firmware/rawdraw/main.c`
- `firmware/rawdraw/os_generic.h`
- `firmware/rawdraw/wifikbd.c`
- `firmware/rawdraw/xlib_wrapper.c`
- `firmware/sb16.c`
- `firmware/sb16.h`
- `firmware/scripts/build.sh`
- `firmware/scripts/sdl.patch`
- `firmware/scripts/slirp/Makefile`
- `firmware/scripts/slirp/glib.h`
- `firmware/scripts/slirp/libslirp-version.h`
- `firmware/scripts/slirp/patch`
- `firmware/sdl/main.c`
- `firmware/seabios/config`
- `firmware/seabios/patch`
- `firmware/simd.inc`
- `firmware/tools/initnet.c`
- `firmware/tools/wifikbd.c`
- `firmware/vga.c`
- `firmware/vga.h`
- `firmware/wasm/Makefile`
- `firmware/wasm/Makefile.amd64`
- `firmware/wasm/html/index.html`
- `firmware/wasm/html/main.js`
- `firmware/wasm/libc/assert.h`
- `firmware/wasm/libc/compiler-rt.c`
- `firmware/wasm/libc/ctype.h`
- `firmware/wasm/libc/errno.h`
- `firmware/wasm/libc/fcntl.h`
- `firmware/wasm/libc/inttypes.h`
- `firmware/wasm/libc/libc.c`
- `firmware/wasm/libc/math.h`
- `firmware/wasm/libc/nanoprintf.h`
- `firmware/wasm/libc/stdio.h`
- `firmware/wasm/libc/stdlib.h`
- `firmware/wasm/libc/string.h`
- `firmware/wasm/libc/time.h`
- `firmware/wasm/libc/unistd.h`
- `firmware/wasm/main.c`
- `firmware/win32.c`
