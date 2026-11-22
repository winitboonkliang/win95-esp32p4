# B45 BLUE - one pointer path: touch (GSL3680), USB (later BLE) -> input events Windows receives

## Result first (what Windows must see)
Windows 95 USER has one entry for pointer input: the event procedure it hands to the mouse driver's `Enable`.
Called with AX = flags (move, left/right down/up, absolute), BX = x, CX = y (absolute: 0..65535 over the screen).
Everything else in the stock path (IRQ 12, 8042 byte protocol, MSMOUSE.VXD packet decode, VMOUSE, relative
acceleration) exists only to produce those calls from a 1995 serial/PS/2 mouse. We produce the calls directly:

```
GSL3680 report (I2C, INT edge)  --\
USB HID report (relative)       ----> Rust engine (host, no_std) --> overlay position (host draws pointer at once)
synthetic/recorded trace (test) --/        |  gesture decision, 30 Hz move pacing, exact position before buttons
                                           v
                     record queue (flags, x16, y16), coalesced; IRQ 10 while non-empty
                                           v
            P4PTR.DRV (our MOUSE.DRV, 880 bytes): ring-3 ISR drains the queue: IN eax,E4h / IN ax,E6h
                                           v
                     USER event proc  (lcall [lpEventProc]) -> Windows logic (click, double click, menus, drag)
```

## Parts (all own code)
| part | file | what |
|---|---|---|
| engine | core/src/lib.rs (Rust, no_std, ~300 lines) | chip coords -> screen pixel (landscape + BOARD_ROTATE_180), gesture state machine, absolute coordinate, queue + coalescing, C ABI `b45_*` |
| PC trace test | core/tests/trace.rs + traces/*.trace/.expect | deliverable 2: synthetic traces -> Windows events, bit-exact against hand-written / generator-built expectations |
| device glue | emu/ptrdev.c (C glue only) | ports E4h/E6h, IRQ 10 edge, enable/disable, ISR-done marker (guest cost measurement), lock for the board's two cores |
| guest driver | driver/ptr.S -> P4PTR.DRV (16-bit NE, asm, no imports) | stock MOUSE.DRV exports/ordinals; Enable hooks INT 72h via DPMI 0205h and unmasks IRQ 10; ISR = drain loop + one EOI pair |
| NE writer | tools/mkne_ptr.py | fixed (page-locked) code + data segments, additive internal fixups |
| board touch | esp/main/gsl/gsl_touch.c | GSL3680 init (Doom_P4 sequence, ported to i2c_master on the shared bus 0), INT-driven reads, 4 ms ticks while active |
| board glue | esp/main/b45_board.c | overlay follow + latency stamps, console `touch NAME` replay, `touchstat` |

## Gestures (engine, all times host us)
- finger lands: overlay jumps there; nothing to Windows yet (a tap, a long press and a drag start the same way).
- moves more than **12 px** from the landing point (dead zone) -> **drag**: left down AT THE LANDING POINT, then moves at <= 30 Hz, release = up at the exact last finger position.
- lifts inside the dead zone before 650 ms -> **tap**: left down at the landing point, left up **60 ms later** (hold).
- stays inside the dead zone for **650 ms** -> **right click** at the landing point (down, up 60 ms later); the context menu opens while the finger is still down; nothing more until lift.
- **double tap**: a second landing within 450 ms of the first lift and within 24 px of it is snapped onto the first tap's pixel, so Windows' own double-click test (same pixel, its double-click time) always passes. No double-click logic of ours beyond the snap.
- USB relative: host applies the measured Win95 default acceleration (|d|<=2 -> d, else 2d) and sends ABSOLUTE positions too: one coordinate system, coalescing stays exact (B29a found relative coalescing is not exact because Windows' acceleration is per packet).
- Windows moves the pointer itself (SetCursorPos): the display driver's MoveCursor OUT calls `b45_sync`, adopted when nothing of ours is in flight.
- both devices at once: button state sent to Windows = touch OR USB, one record per change.

Two facts found by building it (PC run s1/s1b, shots/pc_s1*_grid4.png):
1. USER applies the button flag of a record BEFORE its move. A record "move to Start + left down" clicked at the OLD position.
   -> every button record is preceded by a move-only record when the position changed ("exact position right before any button event").
2. A down and an up delivered back to back (same interrupt) are seen too early by apps that poll the button state (Start button, icon drag detection).
   -> 60 ms hold (one record per interrupt). With both fixes: Start opens, My Computer opens on double tap, long press gives the desktop menu (shots/pc_s1c_grid4.png).

## Absolute coordinate
x16 = ceil((4p+1) * 65536 / (4n)): a quarter pixel into pixel p, so floor or round of x16*n/65536 (and of x16*n/65535) all give p
(unit test abs16_roundtrip, every p for n = 640/800/1024/1280). Measured in Win95: 9/9 grid points and 400/400 tap points land on
the exact pixel (MoveCursor position == record pixel).

## Rate and coalescing
- Moves to Windows <= 30 Hz (33.3 ms pacing); a move-only record still in the queue is overwritten by the newer one (the guest
  always gets the latest position, never a backlog). Button records are never dropped or merged.
- The host overlay moves at report rate (GSL ~100 Hz+), independent of the guest.

## Guest delivery path (why IRQ + ring-3 ISR)
- Hardware IRQ 10 (free on this machine: PIT 0, kbd 1, COM 4, RTC 8, PS/2 12, IDE 14/15). The ISR is a DPMI protected-mode
  handler in our FIXED code segment (fixed DLL segments are page-locked), so it may run at any time in the System VM.
- Ports E4h/E6h are not trapped by any VxD -> IN/OUT go straight to the host (no VMM exception per access).
- One IRQ per batch: the ISR loops until the queue reads empty, so N queued records cost one interrupt.
- `out E4h,2` at the end of the ISR = "done" marker: host counts guest instructions from raising the IRQ to it.
- VMOUSE/MSMOUSE stay loaded but idle (no PS/2 packets once our driver is enabled); the stock MOUSE.DRV is not loaded.

## Mapping chip -> screen
Engine "raw" = (x, y) as the Doom_P4 driver delivers them after its mirror_y (portrait 800 x 1280).
Screen (no rotation) x = raw_y, y = 799 - raw_x; BOARD_ROTATE_180 then x = 1279 - x, y = 799 - y.
ASSUMPTION (needs one real finger test, lead asked): Doom's display orientation = the base before BOARD_ROTATE_180.

## Not done / skipped (see SKIPS.md)
Multi-finger gestures (2-finger scroll/right click), wheel, BLE (C6 not ready), DOS boxes (IRQ 10 would reflect into a DOS VM), mouse
speed from Control Panel for USB (host uses the default acceleration).
