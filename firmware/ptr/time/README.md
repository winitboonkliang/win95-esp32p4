# B61 P4TIME - Windows clock right after a snapshot resume (no BIOS path)

## Host contract (firmware, branch blue-b61t: misc.c `p4time_read32`, pc.c port case, snapshot.c arms it)
- Port **0xF1E8**, 32-bit `IN eax, dx` only.
- Every successful snapshot resume arms it (the CMOS offset is set to the real time first).
- 1st read: `0xA5 | day << 8 | month << 16 | (year - 1980) << 24`, or 0 when nothing is pending. That read latches the time.
- 2nd read: `second | minute << 8 | hour << 16 | hundredths << 24`, then the port is disarmed.
- Values = the guest RTC view (host clock + offset, Thai local time), binary, not BCD.
- No BIOS code runs: no INT 1Ah, no CMOS polling by the guest.

## Guest block (time/p4time.S) - meant to be included unchanged by B41b's p4tick
Three parts, all inside `#ifdef P4TIME`:
1. **Poll** (in the idle callback, every 8th call, about 9 Hz): `mov edx,0xF1E8; in eax,dx; cmp al,0xA5`.
   On a hit it stores `tdate` and the 2nd read in `ttime`, then sets `pending`.
2. **resync** (System VM only; carry set = retry at the next idle call): one nested V86 exec, `INT 21h AH=2Bh` (CX year, DH month,
   DL day) and `AH=2Dh` (CH hour, CL minute, DH second, DL hundredths) from `tdate`/`ttime`. DOS keeps date/time and the BDA ticks,
   and Windows (GetLocalTime, file stamps) reads them.
3. `_SHELL_PostMessage(0xFFFF, WM_TIMECHANGE 0x1E, 0,0,0,0)` (SHELL device 0x17, service 6, cdecl). Without it Explorer's tray clock waits
   for its own minute timer (up to 60 s).

Data the block owns: `pending` (byte), `pollcnt` (byte), `tdate`, `ttime`, `done`, `vmh` (dwords), `sbuf` (0x6C bytes, Save_Client_State).
To include it in another VxD: copy the `#ifdef P4TIME` parts of `idle_cb` (poll) plus `resync` and the data, and call the poll from that VxD's idle callback.
The block is position independent (call/pop base) and uses no other services than the ones listed in study/IDLE_VXD/TOOLS.md.

## Build / install
- `time/mkovl61t.sh` (WSL, needs a WSL slot) builds `p4time.vxd` (`VXD_CFLAGS=-DP4TIME`, study/IDLE_VXD toolchain).
- It then builds `time/b61t_1.ovl` = base `gfx95/img/b29_1.ovl` (93 sectors, sha 8c100984, GFX95 stub + P4PTR) + `P4TIME.VXD`,
  with SYSTEM.INI `device=*vpowerd` -> `device=p4time.vxd` (110 sectors).
- A new overlay or new firmware invalidates the golden snapshot by design (disk signature / machine id), so the first boot rebuilds it.

## Proof
- PC: `time/test_time.sh`.
  - The golden snapshot is taken at the host time.
  - The resume runs with the host clock +25 h.
  - The taskbar shows +25 h in the first screenshot, 0.5 s after resume (`time/runs/clock_strip.png`).
- Board: `board2/run_time.py` (board A), results in `board2/t1/`.
