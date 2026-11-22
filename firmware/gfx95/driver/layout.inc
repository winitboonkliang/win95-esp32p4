# data segment layout (DGROUP) shared by drv.S and tools/mkne.py
	.equ D_DEV,      0x20      # dword  lpDevice saved by Enable (appended to the DIBENG *EXT calls)
	.equ D_LFBSEL,   0x24      # word   selector of the linear framebuffer
	.equ D_ENABLED,  0x26      # word
	.equ D_HWCUR,    0x28      # word   overlay pointer active
	.equ D_REEN,     0x2a      # word   inside ReEnable (no VDD notification)
	.equ D_REPAINT,  0x2c      # word
	.equ D_ROWS,     0x2e      # word   SetCursor scratch
	.equ D_NB,       0x30      # word
	.equ D_CB,       0x32      # word
	.equ D_XOFF,     0x34      # word
	.equ D_SHAPEBUF, 0x40      # 264 bytes: hot x, hot y, w, h, AND[32x4], XOR[32x4]
	.equ D_PSP,      0x150     # word   RED B29b: bytes used in D_PSTACK
	.equ D_PSTACKN,  32
	.equ D_PSTACK,   0x160     # 32 x (IP, CS) parked far returns
	.equ DATASIZE,   0x1e0
	.ifndef W
	.equ W, 1280
	.endif
	.ifndef H
	.equ H, 800
	.endif
	.equ BPP, 16
	.equ PITCH, W*2          # 2560 / 2048 / 1280 B: all multiples of 128, the LFB stride equals 2*W
