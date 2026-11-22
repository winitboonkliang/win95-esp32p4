# Roadmap and status

Honest status on 22 Nov 2026. "Board A" = ESP32-P4 chip v1.0, "board B" = chip v1.3 (same 10.1" panel).
Help is welcome on anything below: open an issue first so we can share what we already measured.

## Done

| Feature | Since | Proof / numbers |
|---|---|---|
| Windows 95 boots on our firmware: own 64 KB BIOS, BIOS services in Rust | 14 Nov | cold boot to desktop ~82 s |
| Own x86 hot paths (bit-exact against the reference), FPU on | 16 Nov | oracle runs |
| Display driver B29GFX.DRV: 1280x800 65k colours, damage-only present through the PPA, host cursor | 15 Nov | [PERFORMANCE.md](PERFORMANCE.md) |
| Native GDI drawing with PIE SIMD row loops | 16-19 Nov | PIE self-test at boot |
| Touch screen as the Windows pointer (P4PTR.DRV): tap, double tap, long press = right click, drag; USB keyboard/mouse | 15 Nov | |
| Instant-on: read-only system disk + RAM overlay, golden snapshot resume | 20 Nov | resume ~0.5 s |
| Device Manager clean, our devices named "P4 ..." | 20 Nov | [pictures/05](pictures/05_device_manager_p4.png) |
| Native Notepad (B76, Rust) from the normal Start menu entry | 20 Nov | [pictures/06](pictures/06_notepad.jpg) |
| Doom / Ultimate Doom / Doom II from desktop icons as a second app; C and Rust engines; OPL music + effects; pads, keyboard, mouse, touch; saves | 21 Nov | 320x200 78.5 fps timedemo, 640x400 ~35 fps, quit -> Windows 2.6 s |
| Windows clock follows the board RTC on every boot path | 21 Nov | +-0.3 s |
| Demo-video fixes: console line 160 chars, Notepad keys (debug log stall), key ring, drag keeps every point, Start-menu keys, Doom touch buttons only after a touch | 22 Nov | 40 min Ultimate Doom run, no hang |

## In progress (paused, code partly written)

| Item | State | Done so far | What is left | Where | Good first task? |
|---|---|---|---|---|---|
| Doom: direct RGB565 rendering at 640x400 | design + measurements | the 8-bit -> RGB565 convert costs 10-14 ms and ~0.75 MB per frame; it is the biggest byte cost left | 3D view writes RGB565 directly, HUD/menu converted only where drawn; check against the C engine by frame hashes | `esp/main/b82doom.c`, `esp/components/doomp4`, `rust/doomrs` | no |
| Doom: render-only interpolation | idea + plan | game logic stays at 35 Hz | draw in-between frames at the panel refresh (~41.6 Hz in the Doom app); measure smoothness | same | no |
| Doom music cost | measured | music + effects cost 3 % fps (budget 2 %) | OPL render competes for PSRAM bandwidth: buffer placement, block size | `esp/main/b84*.c` | yes |
| Windows sound (wave-out) | two competing versions written | own 16-bit `P4WAVE.DRV` + host device (IRQ 11); own system sounds composed | run inside Windows, Sound Properties preview, 0 % idle cost | `firmware/gfx95`-style guest driver + host `i2s.c` | yes |
| Core 1 without FreeRTOS (bare loop for the presenter) | two candidates | desktop -2.4 % to -11 %, Notepad open -6.5 % to -14 % | finish the A/B tests, pick one | `esp/main/b29_present.c` | no |
| One master branch | this repo is that merge | Windows tree + clock fix + UI fixes + Doom overlay change | build this exact tree end-to-end and publish binaries | whole tree | yes (build it and report!) |
| Install kit | planned | | flash kit (bootloader + both apps + card files) with release notes incl. ESP-IDF security fixes | `docs/BUILD.md` | yes |

## Open bugs (known, not fixed)

| Bug | Impact | Suspected cause | Where to look | Fix idea |
|---|---|---|---|---|
| Cold-boot bright flashes: 3-4 one-frame flashes of an old desktop picture during "Starting Windows 95" | cosmetic, cold boot only (rare with instant-on) | stale frame buffer shown on a display mode switch | `esp/main/lcd_jd9365_p4.c`, mode switch in `b29_present.c` | clear / blank the back buffer before switching modes |
| Service console drops bytes on long fast lines | tools only | the console polls the 128-byte UART FIFO every 50 ms | `esp/main/cardsvc.c` listener | poll faster or install the UART driver ring buffer (tools send <= 32-byte pieces meanwhile) |
| Pointer path before P4PTR.DRV loads: press can arrive before the preceding move; PS/2 event under a critical section with a mutex inside | early boot only | `b29_mouse_in` ordering | `esp/main/b29_present.c` ~146-160 | flush move, then press, then this event's move; call `ps2_mouse_event` outside the critical section |
| Board B (chip v1.3) not updated with the 22 Nov fixes | our lab | board was offline | - | install + re-run the checks |
| `shot --panel` can freeze presenting until reboot | lab tool | console parses `--panel` as a mode | `cardsvc.c` | parse flags properly |
| Notepad: second file open 2 of 3 in one test; open time missing in 2 of 5 runs on board B | minor | not analysed | `tools/b82/b76host` | add a test, measure |
| Paint drops the start of the first stroke after opening | minor (Paint repaints) | guest busy | - | none needed for real use |
| A time set inside Windows lasts until the next resume | by design | resume returns to the snapshot | `rtc.c`, `b86*` | NTP over Wi-Fi |
| Starting Doom restarts the board: open Windows programs are not kept | by design | Doom gets the whole chip | `b82doom.c` | keep as is, or snapshot on the way out |

## Future ideas

| Idea | Why | Size |
|---|---|---|
| Our own 32-bit USER/GDI, hottest functions first | Windows 95's are mostly 16-bit, the slowest code for the emulator | big |
| More native apps in the Win95 look: MP3 player, photo viewer (P4 JPEG engine), file manager, calculator, NTP clock | instant, smooth apps that still feel like 1995 | medium each |
| Wi-Fi through the ESP32-C6: file sharing with a PC, network time, BLE keyboard/mouse | | medium |
| Own asset pack: icons, cursors, start-up sounds, a bitmap font family (Latin + Thai) | no Microsoft art needed in anything shipped | medium |
| Faster cold boot; a snapshot per app for instant open | | medium |
| Reuse the P4 host layer for other classic games and DOS programs | more 90s memories on the same board | open |
