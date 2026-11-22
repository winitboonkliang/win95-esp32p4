# B45 pointer path: GSL3680 touch + USB mouse -> input events Windows 95 receives (BLUE B45, R9 winner, merged by lead decision)

Opt-in, on top of GFX95: the guest gets the pointer driver only from the overlay. Ini (SD card `b29.ini`, read before `tiny386.ini`):
`[pc] display = gfx95`, `display_mode = 1|2|3`, `hda_overlay = /sdcard/ptr_%d.ovl` (ptr_N.ovl = gfx95/img/b29_N.ovl + P4PTR.DRV + SYSTEM.INI
`mouse.drv=p4ptr.drv`, built on fixture v2 569dd914). Without that overlay the pointer device stays dormant (no IRQ, idle cost one flag test per step).

- engine: rust/b45ptr (no_std; gestures, absolute mapping, 30 Hz pacing, coalescing queue, set_pointer_abs A1-A5); `cargo test` = bit-exact trace test
  (rust/b45ptr/traces, hand + generated expectations). Board lib esp/main/lib/libb45ptr.a (popcount objects removed, panic handler weakened).
- device: ptrdev.c (ports E4h/E6h, IRQ 10); guest driver ptr/driver/ptr.S -> P4PTR.DRV (own MOUSE.DRV, 880 B, IRQ 10 ring-3 ISR -> USER event proc).
- board: esp/main/b45_board.c (overlay follow, `touch NAME` replay with golden fnv, `touchstat`, `set_pointer_abs()`, `pointer_abs_consumed()`),
  esp/main/gsl/ (GSL3680 on the shared I2C bus, Doom_P4 init + vendor gsl_point_id.c unchanged), USB mouse routed through the engine once P4PTR runs.
- Design, measurements, skips: ptr/DESIGN.md, ptr/SKIPS.md, lab:BLUE/B45/REPORT.md; judge verdict lab:judge/R9_B45/VERDICT.md.
