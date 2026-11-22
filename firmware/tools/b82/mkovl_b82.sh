#!/bin/bash
# B82 copy of lab:APPS/B76/tools/mkovl.sh: same b76.ovl + the Doom entries (empty DOOM.B76 documents: the B76 resident opens Doom).
# Builds lab:APPS/B76/board/b76.ovl ([u32 lba][512 B] records, the b29 overlay format) = every sector that differs between
#   (fixture + base gfx95 b29_1.ovl)  and  (the same + C:\WINDOWS\B76APP.EXE, Start Menu entry "Notepad (native).b76", WIN.INI run=)
# so it is loaded AFTER b29_1.ovl (it is a superset of b29's FAT/dir sectors it touches). The fixture is only read. WSL.
set -e
P=${W95P4:?set W95P4 to your work folder}; B=$P/lab/APPS/B76; D=$P/lab/LEAD/B82; W=$HOME/b76_ovl_$$; mkdir -p $W $B/board
trap "rm -rf $W" EXIT
export MTOOLS_SKIP_CHECK=1
cp --sparse=always $P/images/win95_prod.img $W/a.img; chmod u+w $W/a.img
apply() { python3 -c "
import struct,sys
d=open(sys.argv[1],'rb').read(); f=open(sys.argv[2],'r+b')
for i in range(0,len(d),516): f.seek(struct.unpack_from('<I',d,i)[0]*512); f.write(d[i+4:i+516])" $1 $2; }
apply ${B29SRC:-$P/base/gfx95/img/b29_1.ovl} $W/a.img
cp --sparse=always $W/a.img $W/b.img
OFF=$(python3 -c "import struct;print(struct.unpack_from('<I',open('$W/b.img','rb').read(512),454)[0]*512)")
I=$W/b.img@@$OFF
mcopy -o -i $I $B/stub/out/B76APP.EXE ::WINDOWS/B76APP.EXE
# Start > Notepad, Run "notepad", programs that spawn it: NOTEPAD.EXE = our stub (host app opens a window when started under that name).
# The 1995 program stays as C:\WINDOWS\OLDNOTE.EXE (same name length) with a Start entry "Notepad (classic)" = Notepad.lnk with the name patched.
mren -i $I ::WINDOWS/NOTEPAD.EXE ::WINDOWS/OLDNOTE.EXE
mcopy -o -i $I $B/stub/out/B76APP.EXE ::WINDOWS/NOTEPAD.EXE
AC="::WINDOWS/Start Menu/Programs/Accessories"
mcopy -n -i $I "$AC/Notepad.lnk" $W/n.lnk < /dev/null
python3 -c "
import sys; p=sys.argv[1]; d=open(p,'rb').read(); n=d.upper().count(b'NOTEPAD.EXE'); assert n>=1, 'no NOTEPAD.EXE in lnk'
import re; d=re.sub(rb'(?i)notepad\.exe', b'OLDNOTE.EXE', d); open(p,'wb').write(d); print('lnk patched', n)" $W/n.lnk
mcopy -o -i $I $W/n.lnk "$AC/Notepad (classic).lnk"
# Start > Notepad = the document C:\WINDOWS\NOTEPAD.B76 (same name length as the exe): the shell opens it through the b76app DDE path, which
# activates the new window (a forwarded launch from a second process left the window inactive on the PC, 19 Nov)
: > $W/NOTEPAD.B76; mcopy -o -i $I $W/NOTEPAD.B76 ::WINDOWS/NOTEPAD.B76
python3 -c "
import sys,re; p=sys.argv[1]; d=open(p,'rb').read(); d=re.sub(rb'(?i)oldnote\.exe', b'NOTEPAD.B76', d); open(p,'wb').write(d)" $W/n.lnk
mcopy -o -i $I $W/n.lnk "$AC/Notepad.lnk"
mcopy -n -i $I ::WINDOWS/WIN.INI $W/WIN.INI < /dev/null
python3 - $W/WIN.INI <<'PY'
import sys, re
p = sys.argv[1]; s = open(p, 'rb').read()
lines = s.split(b'\r\n'); out = []; sec = None; done = False
for l in lines:
    if l.startswith(b'['):
        if sec == b'[windows]' and not done: out.append(b'run=C:\\WINDOWS\\B76APP.EXE'); done = True
        sec = l.strip().lower()
    if sec == b'[windows]' and l.lower().startswith(b'run='):
        l = b'run=C:\\WINDOWS\\B76APP.EXE' + (b' ' + l[4:].strip() if l[4:].strip() else b''); done = True
    out.append(l)
assert done
open(p, 'wb').write(b'\r\n'.join(out))
PY
mcopy -o -i $I $W/WIN.INI ::WINDOWS/WIN.INI
# B82: desktop + Start > Programs > Games entries, one document type per game (own icon: art/*.ICO from the Codex masters, tools/mkico.py).
# Shareware Doom ships; Ultimate Doom / Doom II need the owner's own WAD on the card (else a message box).
: > $W/EMPTY
mmd -i $I "::WINDOWS/Start Menu/Programs/Games" 2>/dev/null || true
for f in DOOM1 DOOMU DOOM2; do mcopy -o -i $I $D/art/$f.ICO ::WINDOWS/$f.ICO; done
# the documents live in C:\WINDOWS; Desktop + Games get real .LNK shortcuts with IconLocation = our .ICO (Explorer shows the icon without
# any registry entry; the resident registers .dm1/.dmu/.dm2 at login for the open). Template = the image's own Notepad.lnk (tools/mklnk.py).
for f in DOOM.DM1 DOOMU.DMU DOOM2.DM2; do mcopy -o -i $I $W/EMPTY ::WINDOWS/$f; done
mcopy -n -i ${A_IMG:-$W/a.img}@@$OFF "::WINDOWS/Start Menu/Programs/Accessories/Notepad.lnk" $W/tpl.lnk < /dev/null
mk() { python3 $D/tools/mklnk.py $W/tpl.lnk "$W/$1.lnk" $2 "$3" 'C:\WINDOWS\'"$4" >/dev/null && mcopy -o -i $I "$W/$1.lnk" "$5/$1.lnk"; }
mk "Doom" DOOM.DM1 '..\DOOM.DM1' DOOM1.ICO "::WINDOWS/Desktop"
mk "Ultimate Doom" DOOMU.DMU '..\DOOMU.DMU' DOOMU.ICO "::WINDOWS/Desktop"
mk "Doom II" DOOM2.DM2 '..\DOOM2.DM2' DOOM2.ICO "::WINDOWS/Desktop"
mk "Doom" DOOM.DM1 '..\..\..\DOOM.DM1' DOOM1.ICO "::WINDOWS/Start Menu/Programs/Games"
mk "Ultimate Doom" DOOMU.DMU '..\..\..\DOOMU.DMU' DOOMU.ICO "::WINDOWS/Start Menu/Programs/Games"
mk "Doom II" DOOM2.DM2 '..\..\..\DOOM2.DM2' DOOM2.ICO "::WINDOWS/Start Menu/Programs/Games"
# game classes in the registry before Explorer draws the desktop (icons + hidden extensions at once): DOS-stage REGEDIT in the gate emulator
[ "$OFF" = 32256 ] || { echo "partition offset $OFF != 32256 (regstage_b82.sh assumes it)"; exit 1; }
# (regstage_b82.sh works but REGEDIT rewrites all of SYSTEM.DAT: 1418 sectors = ~0.7 MB of the Windows overlay PSRAM; the resident
#  registers the types at login and presses F5 on the desktop instead, b76glue.c b76g_desk_refresh)
mdir -i $I "::WINDOWS/Desktop" "::WINDOWS/Start Menu/Programs/Games" | grep -i doom
mdir -i $I ::WINDOWS/B76APP.EXE ::WINDOWS/NOTEPAD.EXE ::WINDOWS/OLDNOTE.EXE "$AC" | grep -i "b76\|notepad\|oldnote" || true
python3 - $W/a.img $W/b.img ${OVLOUT:-$D/board/b76.ovl} <<'PY'
import sys, struct
a = open(sys.argv[1], 'rb'); b = open(sys.argv[2], 'rb'); o = open(sys.argv[3], 'wb'); n = 0; lba = 0
while True:
    x = a.read(1 << 20); y = b.read(1 << 20)
    if not x: break
    if x != y:
        for k in range(0, len(x), 512):
            if x[k:k+512] != y[k:k+512]: o.write(struct.pack('<I', lba + k // 512) + y[k:k+512]); n += 1
    lba += len(x) // 512
print('b76.ovl sectors', n)
PY
sha256sum ${OVLOUT:-$D/board/b76.ovl}
