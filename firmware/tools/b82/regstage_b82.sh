#!/bin/bash
# B82 copy of base/ptr/tools/regstage.sh (LEAD devmgr): DOS-stage registry edit - game classes .dm1/.dmu/.dm2 with own icons.
# LEAD devmgr: DOS-stage registry edit of a scratch Win95 image (called by mkptrovl.sh, WSL).  usage: regstage.sh IMAGE WORKDIR
# Boots IMAGE (read-write scratch copy, never the fixture) in the PC gate emulator built from base, with an AUTOEXEC.BAT that runs the real-mode
# REGEDIT on SYSTEM.DAT (ptr/reg/autoexec.bat + devmgr.reg) and then loops; the run stops by virtual time.  Then the two files are deleted,
# the FAT clean-shutdown bit is put back as it was, and the result is checked with creg.py.
set -e
P=${W95P4:?set W95P4 to your work folder}; B=${B:-$P/base}; I=$1; W=$2; R=$P/lab/LEAD/B82/reg; G=$P/tools/gate; export MTOOLS_SKIP_CHECK=1
mkdir -p $W/gb
FL="-O2 -w -I$B -DPTRDEV_STUB -DI386_ENABLE_FPU -DI386_ENABLE_MMX -DI386_ENABLE_SSE -DI386_ENABLE_SSE2 -DI386_ENABLE_SSE3 -DI386_ENABLE_SSSE3 -DUSE_FMOPL -Dusleep=gate_usleep"
( cd $W/gb
  for c in adlib fmopl fpu i386 i8042 i8254 i8257 i8259 ide ini ne2000 pc pci pcspk sb16 vga gfxcard ptrdev; do echo "gcc $FL -c $B/$c.c -o $c.o"; done > jobs.txt
  echo "gcc $FL -Dtime=gate_time -Dclock_gettime=gate_cgt -c $B/misc.c -o misc.o" >> jobs.txt
  echo "gcc $FL -I$G -c $G/gatemain.c -o gatemain.o" >> jobs.txt
  xargs -P2 -I{} sh -c '{}' < jobs.txt > build.log 2>&1; gcc *.o -o gate -lm >> build.log 2>&1 ) || { tail $W/gb/build.log; exit 1; }
OFF=32256
fat1() { python3 -c "
import sys,struct; f=open(sys.argv[1],'rb'); f.seek($OFF); bs=f.read(512); rs=struct.unpack_from('<H',bs,14)[0]
f.seek($OFF+rs*512+2); print(f.read(2).hex())" $I; }
F0=$(fat1)
mcopy -o -i $I@@$OFF $R/autoexec.bat ::AUTOEXEC.BAT
mcopy -o -i $I@@$OFF $R/b82.reg ::B82.REG
cat > $W/reg.ini <<EOT
[pc]
bios = $P/sdcard_kit/bios.bin
vga_bios = $P/sdcard_kit/vgabios.bin
mem_size = 16M
vga_mem_size = 2M
hda = $I
fill_cmos = 1

[display]
width = 1280
height = 800

[cpu]
gen = 4
fpu = 1
EOT
printf '40 shot regstage\n41 quit\n' > $W/reg.scr
( cd $W; TZ=UTC GATE_OUT=$W GATE_MAXVT=60 timeout 600 $W/gb/gate reg.ini reg.scr > reg.out 2>&1 ) || true
mdel -i $I@@$OFF ::AUTOEXEC.BAT ::B82.REG
F1=$(fat1)
if [ "$F0" != "$F1" ]; then python3 -c "
import sys,struct; f=open(sys.argv[1],'r+b'); f.seek($OFF); bs=f.read(512); rs=struct.unpack_from('<H',bs,14)[0]; nf=bs[16]; spf=struct.unpack_from('<H',bs,22)[0]
for k in range(nf): f.seek($OFF+(rs+k*spf)*512+2); f.write(bytes.fromhex(sys.argv[2]))" $I $F0; echo "regstage: FAT[1] $F1 -> restored $F0"; fi
mcopy -o -i $I@@$OFF ::WINDOWS/SYSTEM.DAT $W/SYSTEM.DAT
python3 $B/ptr/tools/creg.py $W/SYSTEM.DAT > $W/all_key.txt
for n in b82dm1 b82dmu b82dm2 DOOM1.ICO DOOMU.ICO DOOM2.ICO; do grep -aiq "$n" $W/all_key.txt || { echo "regstage_b82: '$n' NOT applied"; exit 1; }; done
echo "regstage_b82: game classes applied"
