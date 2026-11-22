# Roadmap

Honest status on 22 Nov 2026. "Board A" = chip v1.0, "board B" = chip v1.3.

## Done

- Windows 95 boots on our own firmware: own 64 KB BIOS, no POST, BIOS services in Rust (14 Nov).
- Own x86 hot paths, bit-exact; FPU on (16 Nov).
- Own display driver: 1280x800 65k colours, damage-only present through the PPA, host cursor overlay (15 Nov); native GDI drawing with
  PIE row loops (16-19 Nov).
- Touch screen as the Windows pointer (tap, double tap, long press, drag), USB keyboard/mouse (15 Nov).
- Read-only system disk + RAM overlay with SD spill; golden snapshot instant-on, resume ~0.5 s (20 Nov).
- Device Manager clean, our devices named "P4 ..." (20 Nov).
- Native Notepad (B76) from the normal Start menu entry (20 Nov).
- Doom, Ultimate Doom, Doom II from desktop icons as a second app; C and Rust engines; OPL music + effects; gamepads, mouse, keyboard,
  touch overlay; saves on SD; quit -> Windows in 2.6 s (21 Nov).
- Windows clock follows the RTC on every boot path, +-0.3 s (21 Nov).
- Demo-video fixes: console line length, Notepad key handling, key ring, Paint drag points (22 Nov).

## In progress (paused, code partly written)

- **Doom: direct RGB565 rendering** - the 8-bit -> RGB565 convert pass (10-14 ms, ~0.75 MB per frame) is the largest byte cost left
  at 640x400. Plan: the 3D view writes RGB565 directly, HUD/menu rows converted only where drawn; checked against the C engine by hashing.
- **Doom: render-only interpolation** - game logic stays at 35 Hz, frames drawn at the panel refresh rate.
- **Doom music cost** - music + effects cost 3 % fps (budget 2 %); the OPL render competes for PSRAM bandwidth.
- **Windows sound (B81)** - our own 16-bit wave-out driver `P4WAVE.DRV` and the host device (IRQ 11) are written in two competing
  versions; not yet run inside Windows. Our own system sounds (B49) are composed and rendered, host playback not built.
- **Core 1 without FreeRTOS (B80)** - two candidates (desktop -2.4 % to -11 %, Notepad open -6.5 % to -14 %), tests incomplete.
- **One master** - the clock fix, the UI fixes and the Doom branch are not merged into one master yet; this public tree is that merge
  (Windows tree + the latest Doom overlay change), built as a whole only in the lab, not from this exact snapshot.
- **Install kit** - production flash kit with release notes (including ESP-IDF security fixes) for both apps.

## Open bugs (known, not fixed)

- **Cold-boot bright flashes**: 3-4 one-frame flashes of an old desktop picture during the "Starting Windows 95" text phase.
  Probably a stale frame buffer shown on a mode switch. With instant-on, cold boots are rare (only after a card or snapshot change).
- **Console input**: the service console polls the 128-byte UART FIFO every 50 ms; a long line sent at once overflows and the rest is
  read as other commands. Host tools send <= 32-byte pieces for now. Fix: poll faster or use the UART driver's ring buffer.
- **C pointer path press order**: the pointer path used only before `P4PTR.DRV` loads (`b29_mouse_in`) can deliver a press before the
  preceding move, and calls the PS/2 mouse event under a critical section with a mutex inside.
- **Board B not updated**: the 22 Nov UI fixes were installed and checked only on board A.
- `shot --panel` on the console is parsed as a mode and can freeze presenting until reboot (lab tool only).
- Notepad: a second file open worked 2 of 3 times in one test; open time not reported in 2 of 5 runs on board B.
- Paint drops the start of the first stroke after opening.
- A time set inside Windows lasts only until the next resume (resume resets to the RTC). Long-term RTC drift: NTP planned.
- Any Doom start restarts the board: open Windows programs are not kept (Windows comes back to its desktop snapshot).

## Future ideas

- Our own 32-bit USER/GDI, hottest functions first (Windows 95's are mostly 16-bit, the slowest code for the emulator).
- More native apps with the Win95 look: MP3 player, photo viewer (P4 JPEG engine), file manager, calculator, clock with NTP.
- Wi-Fi through the ESP32-C6: file sharing with a PC, network time, BLE keyboard and mouse.
- Our own asset pack: icons, cursors, start-up sounds and a bitmap font family (Latin + Thai), so no Microsoft art is needed.
- Faster cold boot, instant open for apps (a snapshot per app).
- Reuse the P4 host layer for other classic games and for DOS programs.
