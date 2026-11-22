# Try it

Two ways in, from easiest to most complete. Board: Guition **JC8012P4A1** (ESP32-P4, 10.1" 1280x800 touch, 16 MB flash,
32 MB PSRAM). Ready-made files are on the [Releases page](https://github.com/winitboonkliang/win95-esp32p4/releases).

## A. Doom only - 5 minutes, nothing to build

1. Download `doom-only-kit.zip` from the latest release and unzip it.
2. `pip install "esptool>=5"`, plug the board in over USB and find its serial port.
3. Flash (this replaces whatever app is on the board):
   ```
   esptool --chip esp32p4 -p COMx -b 921600 write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x2000 bootloader.bin 0x8000 partition-table.bin 0x200000 doom_app.bin
   ```
4. microSD (FAT32): create `DOOM/` and put **one** game data file in it named `doom1.wad`:
   the free shareware Doom (episode 1), or [Freedoom](https://freedoom.github.io) (free, open content; rename `freedoom1.wad`).
   Game data is not included: Doom's art, music and levels belong to id Software.
5. Insert the card and power on. Doom starts in about 3 s: 640x400 at Doom's own 35 fps, music and sound effects.
   Play with a USB gamepad, USB keyboard/mouse, or touch (the on-screen buttons appear when you touch the screen).

## B. Windows 95 + Doom - for builders

You need your own licensed Windows 95. The kit has everything else that may be shared.

1. Download `win95-doom-kit.zip`, flash it (the command is in its README: bootloader, partition table, Windows app at 0x10000,
   Doom app at 0x200000).
2. Prepare the SD card:
   - `tiny386.ini` and `vgabios.bin` from the kit's `sdcard/` folder;
   - `hd.img`: Windows 95 installed into a 512 MB raw disk image (install it on a PC emulator such as tiny386's PC build or QEMU);
   - `b29_1.ovl` and `b76.ovl`: the card overlays that add our drivers (display, touch pointer, clock, native Notepad, Doom icons)
     to **your** image. They contain sectors of your Windows, so you build them yourself with `firmware/ptr/tools/mkptrovl.sh`
     and the overlay scripts (see [BUILD.md](BUILD.md) and [CONFIG.md](CONFIG.md));
   - `DOOM/doom1.wad` (as in A) if you want the Doom icons to work.
3. Power on. The first boot is a cold boot (~82 s) that writes the instant-on snapshot; from then on the desktop is back in ~0.5 s.

## C. Build everything yourself

[BUILD.md](BUILD.md) has the full steps (ESP-IDF 5.4.4, Rust libraries, both apps, the overlays). [CONFIG.md](CONFIG.md) lists every
setting, the SD card and flash layouts and the service console. Open items you could pick up are in [ROADMAP.md](ROADMAP.md).

## If something goes wrong

| Symptom | Try |
|---|---|
| Black screen after flashing the Doom kit | no `DOOM/doom1.wad` on the card, or the card is not FAT32 |
| Board resets when the sound is loud | lower the volume (`doomvol 30` on the serial console; 70 is the cap) and use a good USB power supply |
| Touch does nothing | a USB mouse/keyboard/pad works the same; touch needs the GSL3680 firmware (included) and this exact panel |
| Windows: endless cold boots | delete `b61snap.A`, `b61snap.B`, `b61_disk.ovl` from the card, or create an empty `b61.off` to disable instant-on |
| Need logs | serial console at 115200 baud on the same USB port |

Please report what you tried (board revision, kit version, what you saw) in an issue - every report helps.
