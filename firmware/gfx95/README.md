# GFX95: paravirtual 16 bpp display for the Win95 guest, three user-selectable sizes that fill the 1280x800 panel (BLUE B29a, merged by lead decision)

Opt-in. Stock VGA stays the default. Enable with `[pc] display = gfx95` in the ini (see img/b29.ini.example; the file name `b29.ini` on the SD card is
read before `tiny386.ini`, so the owner's ini is never touched). Without the key the card does not exist for the guest (ports 0xE8/EA/EC are dormant).

What it is
- Card GFX95 (gfxcard.c): ports 0xE8 index / 0xEA data / 0xEC pointer dword. The Win95 display minidriver (gfx95/driver/B29GFX.DRV, own source drv.S,
  own NE writer tools/mkne.py) sets 1280x800 RGB565 and reports a pointer position and shape; the pointer is a host overlay (zero guest work per move).
- Dirty tracking in the emulator memory write path (pc.c iomem_write*): every guest write to the linear frame buffer marks a 16x16 tile; the presenter
  (esp/main/b29_present.c, task on core 1) takes and clears the tiles before reading the pixels. No GDI path, DIBENG direct write or XOR outline can be missed.
- Presenter: Rust LinearPresenter (rust/present_core, lib/libpresent_core.a): buffer-age damage, PPA 90 deg rotation of the dirty rects, CPU pointer overlay.
- Board facts kept (found on board A): esp_cache_msync needs a 128 B aligned start and size (the guest surface base is not; msync128() aligns and logs failures);
  the PPA writes the panel buffer, so dirty CPU lines are written back and dropped before each PPA job; esp_lcd_panel_draw_bitmap only SELECTS the buffer, so
  the presenter waits for the DPI refresh-done event before writing the other buffer (<= 1 present per refresh).
- Driver delivery without touching the SD image: ini `hda_overlay = /sdcard/b29.ovl` preloads 30 sectors into the appliance overlay (ide.c b29_preload).

Not merged (skips)
- the on-board bench and verify harness (b29_bench.c, P1 replay): the shared injector (inject.c `p1`) is the fixture now; B29V verify stays in b29_present.c as a request-only debug path;
- the software-pointer oracle build (NOHW), the board B variants, debug/profile ports 0x3f/0x40/0x41 (kept for the PC harness behind GFX95_TEST);
- PC test harness (emulator main.c with scripted input, shadow-panel model): lab:BLUE/B29a/emu + scripts (needs -DGFX95_TEST);
- cursor sub-rect presenter micro-optimisations beyond what the Rust core already does.

Rebuild
- firmware: python tools/remote_build/rbuild.py <worktree>/esp -DBOARD=jc8012p4a1c --bdir build_b29a build -j 2
- presenter lib: python tools/remote_build/rbuild.py <worktree>/rust/present_core --cargo build --release --target riscv32imafc-unknown-none-elf, then copy the .a to esp/main/lib/
- driver: bash gfx95/tools/build_drv.sh (WSL; GNU as/ld/objcopy); image overlay: gfx95/tools/mkimg.sh + mkovl.py (paths in those scripts point at the BLUE team folder).
Evidence: lab:BLUE/B29a/REPORT.md (Updates 17-19), runs/ab_v9.log (same-session A/B with the shared injector: -14.8 % / -23.5 % guest instructions per packet).

Three modes (owner request 15 Nov)
- 1280x800 (1:1), 1024x640 (PPA scale 1.25x: 16 px source tiles become 20 px, exact), 640x400 (PPA scale 2x: 16 px become 32 px, exact). The presenter (rust/present_core linear.rs
  `set_mode`) keeps the damage tiles 16 px aligned in scaled modes so every PPA block lands on whole, 4-aligned panel pixels; the pointer is composited in panel space with the shape
  scaled the same way (40 px / 64 px arrow). Tests: rust/present_core/tests/modes.rs (random damage + pointer against an independent scaled-rotate model, all three modes).
- Windows side: three driver files built from drv.S (`--defsym W= H=`; each Enable()s its own size, every ValidateMode() accepts all three) and three MODES keys, so Display Properties >
  Settings > Desktop area lists the three sizes; picking another one loads that mode's driver at the next start (Win95 restart). tools/build_all_drv.sh, tools/gen_reg.py.
- Boot default: ini `[pc] display_mode = 1|2|3`; `hda_overlay = /sdcard/b29_%d.ovl` loads img/b29_N.ovl (driver + registry for that boot mode).
- Pointer / touch: `gfx_pointer_range(&w, &h)` (gfxcard.h) returns the active guest size, the range an absolute pointer or touch position must map onto.
- Scaling filter: the PPA SRM scaler has no filter option; for these upscales it replicates source pixels (nearest), so no column is ever dropped (1.25x repeats one of every four).

Default display (15 Nov, lead decision)
- GFX95 is the DEFAULT display of base: `[pc] display = vga` restores the stock VGA path, `display_mode = 1|2|3` (default 1 = 1280x800) picks the boot size. LCD_PCLK_MHZ default is 40 (30 MHz gave stripes on the glass).
- The driver reaches the guest through the preloaded overlay `/sdcard/b29_<display_mode>.ovl` (img/b29_1..3.ovl here, built against disk fixture v2 569dd914 with tools/mkimg.sh + mkovl.py; they MUST be rebuilt when the disk image changes: they carry registry/FAT sectors). Copy them to the SD card root. Without the file Windows runs on its stock VGA driver and the card stays dormant.
- No `/sdcard/pclk.txt` should be left on a card (it overrides the pixel clock).
