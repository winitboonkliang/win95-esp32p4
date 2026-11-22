# B45 BLUE skips (rule 17: team-local during the round)

| skipped | where (stock path) | 1995 reason | why our machine does not need it |
|---|---|---|---|
| PS/2 aux byte protocol (3-byte packets, sync bit, overflow bits) | 8042 + MSMOUSE.VXD packet decode | a serial/PS/2 mouse can only send bytes | the host has the finished position; it hands USER the call directly (one IN per record) |
| IRQ 12 / 8042 status polling per byte | VMOUSE, MSMOUSE.VXD | byte-at-a-time device | one IRQ 10 per batch of records, ports not trapped |
| relative motion + Windows acceleration for touch | USER (relative events) | mice only report deltas | touch is absolute; USB is converted to absolute on the host (same default curve), so coalescing is exact |
| VMOUSE event callback (VMD API 3) | stock MOUSE.DRV Enable | VMOUSE owns the hardware and serves DOS boxes | no DOS boxes in v1 (lead decision); our driver keeps the event proc itself |
| MOUSEGETINTVECT real vector, GETSETMOUSEDATA, CPLAPPLET, EXTRAPOINTS, POWEREVENTPROC, MOUSEREDETECT | stock MOUSE.DRV | DOS mouse sharing, control panel page, wheel probing, APM resume redetect | return the stock driver's "no VMOUSE" values; nothing to probe on a fixed board |
| mouse speed / threshold from WIN.INI for touch | USER | relative devices only | absolute events are not accelerated by USER |
| vendor touch gesture code in Doom_P4 (pinch zoom, distance debounce) | esp_lcd_gsl3680.c | phone-style zoom | Windows gets a pointer, not zoom gestures |

Kept (our chip / board needs them): GSL3680 RAM firmware load at every power-on, the Doom_P4 init order (soft ICE halt, reset, load,
start, reset, start), touch init on core 1 (I2C ISR core), the vendor ID/filter algorithm gsl_point_id.c (copied unchanged; it also
writes the noise-hopping mask back to the chip).
