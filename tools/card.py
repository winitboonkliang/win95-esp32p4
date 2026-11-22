#!/usr/bin/env python3
"""card.py - work on the board's SD card over USB-Serial/JTAG without removing the card.

  python card.py [-p COM1] [--baud 921600] hello|info|df|ls [dir]|stat P|rm P|mv A B|mkdir P|sha256 P
  python card.py get REMOTE [LOCAL]          pull a file (streamed, CRC per 16 KiB frame)
  python card.py put LOCAL [REMOTE]          push a file, atomic (tmp + CRC + rename)
  python card.py patch REMOTE OFF LOCALFILE  write LOCALFILE bytes into REMOTE at byte offset OFF (in place, never grows)
  python card.py imgsync init|pull|push|status [--remote hd.img] [--seed images/win95_prod.img] [--no-sha]
  python card.py mt TOOL args...             run an mtools command (mdir, mcopy, mtype, mattrib, mdel) on the mirror via WSL
  python card.py reboot                      leave service mode, boot the emulator normally

Protocol + firmware: firmware/esp/main/cardsvc.c.
The board must be in service mode: this tool sends "svc!" to a running emulator (board restarts into the card service)
and waits for the COM port to come back. `shot` works against the running emulator (no service mode).
After every `put`, check the file on the card with `sha256` (a silent put failure once left an old file in place).
"""
import argparse, array, hashlib, json, os, struct, subprocess, sys, time, zlib
import serial, socket


class TcpPort:
    """pyserial-like wrapper over a TCP socket (Wi-Fi service, port 3868)"""
    def __init__(self, host, port=3868):
        self.sk = socket.create_connection((host, port), timeout=5)
        self.sk.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.timeout = 0.3; self.baudrate = 0
    def write(self, b): self.sk.settimeout(30); self.sk.sendall(b)   # read() leaves a 0.3 s timeout on the socket: a windowed put hits TCP backpressure and must wait, not raise
    def flush(self): pass
    def read(self, n=1):
        self.sk.settimeout(self.timeout)
        try: return self.sk.recv(n)
        except (socket.timeout, TimeoutError): return b''
    def reset_input_buffer(self):
        self.sk.settimeout(0.02)
        try:
            while self.sk.recv(65536): pass
        except (socket.timeout, TimeoutError, BlockingIOError): pass
    def close(self): self.sk.close()

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
MIRROR = os.path.join(ROOT, 'images', 'board_mirror.img')
SEED = os.path.join(ROOT, 'images', 'win95_prod.img')
CH = 16384
WINDOW = 1   # --window N: frames in flight (TCP only). 1 = classic stop-and-wait, the device protocol is unchanged for N=1
BLK = 4096


class Card:
    def __init__(self, port='COM1', force=False, baud=921600, host=None):
        # (lab board-lock check removed in the public copy; --force is accepted and ignored)
        self.port = port; self.baud = baud; self.host = host
        self.s = None
        if host:
            h, _, p = host.partition(':'); self.s = TcpPort(h, int(p or 3868))
            if not self._try_hello(2.0): sys.exit('no card service answered on ' + host)
            return
        self.enter()

    def _open(self, baud=None):
        s = serial.Serial()
        s.port = self.port; s.baudrate = baud or self.baud; s.dtr = False; s.rts = False; s.timeout = 0.3; s.write_timeout = 5
        s.open()
        try: s.set_buffer_size(rx_size=1 << 20, tx_size=1 << 20)
        except Exception: pass
        return s

    def _try_hello(self, tmo=1.0):
        self.s.reset_input_buffer(); self.s.write(b'\nhello\n')
        t = time.time(); b = b''
        while time.time() - t < tmo:
            b += self.s.read(256)
            if b'OK SVC1' in b: return b[b.index(b'OK SVC1'):].split(b'\n')[0].decode()
        return None

    def _hello_any(self, tmo):
        """try the target baud, then 115200 (fresh service mode); returns True when talking at self.baud"""
        for bd in dict.fromkeys((self.baud, 921600, 2000000, 115200)):
            try: self.s.baudrate = bd
            except Exception: return False
            if self._try_hello(tmo):
                if bd != self.baud:
                    self.s.write(b'baud %d\n' % self.baud); time.sleep(0.3); self.s.baudrate = self.baud
                    if not self._try_hello(1.5): return False
                return True
        return False

    def enter(self):
        try: self.s = self._open()
        except serial.SerialException: self.s = None
        if self.s and self._hello_any(0.5): return
        if self.s:
            self.s.baudrate = 115200; self.s.write(b'svc!\n'); self.s.flush(); time.sleep(0.3); self.s.close()
        t = time.time(); self.s = None
        while time.time() - t < 40:           # board restarts (USB re-enumerates / UART boot log), service mode comes up
            time.sleep(0.5)
            try: self.s = self._open(115200)
            except serial.SerialException: self.s = None; continue
            if self._hello_any(0.8): return
            self.s.close(); self.s = None
        sys.exit('board did not enter card service mode (is the board powered / port free?)')

    def line(self, tmo=30):
        b = b''; t = time.time()
        while time.time() - t < tmo:
            c = self.s.read(1)
            if c == b'\n': return b.rstrip(b'\r').decode('latin1')
            b += c
        raise TimeoutError('no reply line')

    def cmd(self, text, tmo=30):
        """send a command, return (list of info lines, final OK/ERR line)"""
        self.s.reset_input_buffer(); self.s.write(text.encode() + b'\n')
        out = []
        while True:
            l = self.line(tmo)
            if l.startswith('OK') or l.startswith('ERR') or l == 'READY': return out, l
            out.append(l)

    def rx_exact(self, n, tmo=10):
        b = bytearray(); t = time.time()
        while len(b) < n:
            d = self.s.read(n - len(b))
            if d: b += d; t = time.time()
            elif time.time() - t > tmo: raise TimeoutError('rx timeout')
        return bytes(b)

    @staticmethod
    def frame(off, data):
        h = struct.pack('<II', off, len(data))
        return h + data + struct.pack('<I', zlib.crc32(data, zlib.crc32(h)))

    # ---- high level ----
    def get(self, remote, local=None, off=0, ln=None, sink=None):
        if WINDOW > 1 and self.host:
            if ln is None:
                _, st = self.cmd('stat %s' % remote); ln = int(st.split()[1])
            _, r = self.cmd('get %s %d %d %d' % (remote, off, ln, WINDOW))
        else:
            _, r = self.cmd('get %s' % remote if ln is None else 'get %s %d %d' % (remote, off, ln))
        if not r.startswith('OK'): raise IOError(r)
        total = int(r.split()[1]); got = 0
        f = sink or open(local, 'wb')
        try:
            while True:
                h = self.rx_exact(8); o, n = struct.unpack('<II', h)
                d = self.rx_exact(n); c, = struct.unpack('<I', self.rx_exact(4))
                if c != zlib.crc32(d, zlib.crc32(h)):
                    self.s.write(b'N'); continue          # device resends the same frame
                if n == 0: break
                self.s.write(b'A')
                if sink: sink.write(o, d)
                else: f.write(d)
                got += n
        finally:
            if not sink: f.close()
        r = self.line()
        if not r.startswith('OK') or got != total: raise IOError('get failed %s got %d of %d' % (r, got, total))
        return got

    def _send_frames(self, items):
        """items: iterable of (off, bytes). Stop-and-wait per frame, resend on N."""
        if WINDOW > 1 and self.host:
            import collections
            pend = collections.deque()
            def ack():
                a = self.s.read(1)
                while a == b'': a = self.s.read(1)
                if a != b'A': raise IOError('windowed put: device answered %r, retry with --window 1' % a)
            for off, d in items:
                self.s.write(self.frame(off, d)); pend.append(off)
                if len(pend) >= WINDOW: ack(); pend.popleft()
            while pend: ack(); pend.popleft()
            self.s.write(self.frame(0, b''))
            return
        for off, d in items:
            fr = self.frame(off, d)
            for tries in range(6):
                self.s.write(fr)
                a = self.s.read(1)
                while a == b'': a = self.s.read(1)      # device may be busy writing the card
                if a == b'A': break
                if a == b'E': raise IOError('device error: ' + self.line())
            else: raise IOError('frame retries exhausted')
        self.s.write(self.frame(0, b''))

    def put(self, local, remote=None):
        remote = remote or os.path.basename(local); size = os.path.getsize(local); crc = 0
        with open(local, 'rb') as f:
            while True:
                d = f.read(1 << 20)
                if not d: break
                crc = zlib.crc32(d, crc)
        _, r = self.cmd('put %s %d %08x' % (remote, size, crc))
        if r != 'READY': raise IOError(r)
        def gen():
            with open(local, 'rb') as f:
                o = 0
                while True:
                    d = f.read(CH)
                    if not d: return
                    yield o, d; o += len(d)
        self._send_frames(gen())
        r = self.line(120)
        if not r.startswith('OK'): raise IOError(r)
        return size

    def patch(self, remote, items):
        _, r = self.cmd('patch %s' % remote)
        if r != 'READY': raise IOError(r)
        self._send_frames(items)
        r = self.line(120)
        if not r.startswith('OK'): raise IOError(r)
        return int(r.split()[1])

    def sha256(self, remote):
        out, r = self.cmd('sha256 ' + remote, tmo=600)
        if not r.startswith('OK'): raise IOError(r)
        return r.split()[1], int(r.split()[3])

    def blkhash(self, remote, blk=CH):
        _, r = self.cmd('blkhash %s %d' % (remote, blk), tmo=600)
        if not r.startswith('OK'): raise IOError(r)
        n = int(r.split()[1]); raw = self.rx_exact(n * 4 + 4, 60)
        if struct.unpack('<I', raw[-4:])[0] != zlib.crc32(raw[:-4]): raise IOError('blkhash list CRC mismatch')
        a = array.array('I'); a.frombytes(raw[:-4]); return a


def ranges(blocks, bs):
    """sorted block indexes -> [(byte_off, byte_len)] merged"""
    out = []
    for b in blocks:
        if out and out[-1][0] + out[-1][1] == b * bs: out[-1][1] += bs
        else: out.append([b * bs, bs])
    return out


def local_blocks(path, bs):
    a = array.array('I')
    with open(path, 'rb') as f:
        while True:
            d = f.read(bs)
            if not d: break
            a.append(zlib.crc32(d.ljust(bs, b'\0')))
    return a


def file_sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for d in iter(lambda: f.read(1 << 22), b''): h.update(d)
    return h.hexdigest()


OUT = None


def manifest_path(): return MIRROR + '.blk'


def save_manifest(remote):
    a = local_blocks(MIRROR, BLK)
    open(manifest_path(), 'wb').write(a.tobytes())
    json.dump({'remote': remote, 'size': os.path.getsize(MIRROR), 'bs': BLK}, open(MIRROR + '.json', 'w'))


def imgsync(c, sub, remote, seed, do_sha):
    t0 = time.time()
    if sub == 'diff':         # read-only: list the 16 KB blocks that differ from SEED; --out FILE = copy of SEED with the board's blocks patched in
        bs = CH; rb = c.blkhash(remote, bs); lb = local_blocks(seed, bs)
        need = [i for i in range(len(lb)) if i >= len(rb) or lb[i] != rb[i]]
        print('DIFF %d of %d blocks (%d KB each): %s' % (len(need), len(lb), bs // 1024, ' '.join(map(str, need))))
        if OUT:
            import shutil; shutil.copyfile(seed, OUT); size = os.path.getsize(seed)
            with open(OUT, 'r+b') as f:
                class W:
                    def write(self, o, d): f.seek(o); f.write(d)
                for off, ln in ranges(need, bs): c.get(remote, off=off, ln=min(ln, size - off), sink=W())
            print('board blocks written into', OUT)
        return
    if sub == 'restore':      # make the board's file equal to SEED (read-only source): only blocks whose CRC differs are sent
        bs = CH; rb = c.blkhash(remote, bs); lb = local_blocks(seed, bs); size = os.path.getsize(seed)
        st, r = c.cmd('stat ' + remote)
        if int(r.split()[1]) != size or len(rb) != len(lb): sys.exit('size differs: board %s local %d' % (r, size))
        need = [i for i in range(len(lb)) if lb[i] != rb[i]]
        print('blocks to send: %d of %d (%.1f MB)' % (len(need), len(lb), len(need) * bs / 1e6))
        def gen():
            with open(seed, 'rb') as f:
                for off, ln in ranges(need, bs):
                    f.seek(off); ln = min(ln, size - off)
                    while ln > 0:
                        d = f.read(min(CH, ln)); yield off, d; off += len(d); ln -= len(d)
        if need: sent = c.patch(remote, gen()); dt = time.time() - t0; print('patched %d bytes in %.1f s' % (sent, dt))
        bsha, bms = c.sha256(remote); lsha = file_sha(seed)
        print('sha256 board %s\n       seed  %s' % (bsha, lsha))
        if bsha != lsha: sys.exit('SHA MISMATCH')
        print('sha256 MATCH'); return
    if sub in ('init', 'pull'):
        bs = CH
        rb = c.blkhash(remote, bs)
        base = MIRROR if os.path.exists(MIRROR) and sub == 'pull' else (seed if os.path.exists(seed) else None)
        st, _ = c.cmd('stat ' + remote); size = int(_.split()[1])
        if base and os.path.getsize(base) == size:
            if base != MIRROR:
                print('seeding mirror from', base); import shutil; shutil.copyfile(base, MIRROR)
            lb = local_blocks(MIRROR, bs); need = [i for i in range(len(rb)) if i >= len(lb) or lb[i] != rb[i]]
        else:
            with open(MIRROR, 'wb') as f: f.truncate(size)
            need = list(range(len(rb)))
        print('blocks to fetch: %d of %d (%.1f MB)' % (len(need), len(rb), len(need) * bs / 1e6))
        got = 0
        with open(MIRROR, 'r+b') as f:
            class W:
                def write(self, o, d): f.seek(o); f.write(d)
            for off, ln in ranges(need, bs):
                ln = min(ln, size - off); got += c.get(remote, off=off, ln=ln, sink=W())
        dt = time.time() - t0
        print('pulled %.1f MB in %.1f s' % (got / 1e6, dt) + (' = %.2f MB/s' % (got / 1e6 / dt) if got else ''))
        if do_sha:
            bsha, bms = c.sha256(remote); lsha = file_sha(MIRROR)
            print('sha256 board %s\n       mirror %s' % (bsha, lsha))
            if bsha != lsha: sys.exit('SHA MISMATCH after pull')
            print('sha256 MATCH')
        save_manifest(remote)
    elif sub == 'status':
        old = array.array('I'); old.frombytes(open(manifest_path(), 'rb').read())
        new = local_blocks(MIRROR, BLK)
        ch = [i for i in range(len(new)) if new[i] != old[i]]
        print('changed 4K blocks vs last sync: %d (%.1f KB)' % (len(ch), len(ch) * BLK / 1024)); return
    elif sub == 'push':
        old = array.array('I'); old.frombytes(open(manifest_path(), 'rb').read())
        new = local_blocks(MIRROR, BLK); size = os.path.getsize(MIRROR)
        ch = [i for i in range(len(new)) if new[i] != old[i]]
        print('changed 4K blocks: %d (%.1f KB)' % (len(ch), len(ch) * BLK / 1024))
        if ch:
            rg = ranges(ch, BLK)
            def gen():
                with open(MIRROR, 'rb') as f:
                    for off, ln in rg:
                        f.seek(off); ln = min(ln, size - off)
                        while ln > 0:
                            d = f.read(min(CH, ln)); yield off, d; off += len(d); ln -= len(d)
            sent = c.patch(remote, gen())
            dt = time.time() - t0
            print('patched %d bytes in %.2f s = %.3f MB/s (incl. hashing)' % (sent, dt, sent / 1e6 / dt))
        if do_sha:
            t1 = time.time(); bsha, bms = c.sha256(remote); lsha = file_sha(MIRROR)
            print('sha256 board %s (%.1f s on board)\n       local %s' % (bsha, bms / 1000, lsha))
            if bsha != lsha: sys.exit('SHA MISMATCH - board image differs from mirror, do NOT boot; re-run push or pull')
            print('sha256 MATCH')
        save_manifest(remote)


def rgb565_to_image(buf, w, h):
    """raw RGB565 little-endian (what the board's console/DPI surfaces hold) -> PIL RGB image; low bits replicated so white stays 255"""
    import numpy as np
    from PIL import Image
    p = np.frombuffer(bytes(buf), dtype='<u2').reshape(h, w).astype(np.uint16)
    r = (p >> 11) & 31; g = (p >> 5) & 63; b = p & 31
    a = np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], axis=-1).astype(np.uint8)
    return Image.fromarray(a, 'RGB')


def decode_stripes(buf, w, h):
    """kind 2 payload from the board's HW JPEG: u32 nstripes, u32 stripe_rows, {u32 offset, u32 length} x n, then the stripe JPEGs"""
    import io
    from PIL import Image
    ns, sh = struct.unpack('<II', bytes(buf[:8])); im = Image.new('RGB', (w, h))
    for i in range(ns):
        off, n = struct.unpack('<II', bytes(buf[8 + 8 * i:16 + 8 * i]))
        im.paste(Image.open(io.BytesIO(bytes(buf[off:off + n]))).convert('RGB'), (0, i * sh))
    return im


def parse_shot_header(b):
    """find the board's "SHOT w h len crc panel ms kind" line anywhere in the bytes read so far: console text without a trailing newline
    (a [gfx95] stats line, a log) can sit in front of it on the same line.  Returns (fields, bytes after the line), ('ERR', msg) or None."""
    i = b.find(b'SHOT ')
    if i >= 0:
        j = b.find(b'\n', i)
        if j < 0: return None
        return b[i:j].decode('latin1').split(), b[j + 1:]
    k = b.find(b'ERR ')
    if k >= 0 and b.find(b'\n', k) >= 0: return 'ERR', b[k:b.find(b'\n', k)].decode('latin1')
    return None


def shot(port, mode, baud, out, jpg=False):
    """screenshot while Windows runs: console command `shot guest|panel BAUD` (no reboot into service mode).
    guest = the guest's 1280x800 surface; panel = what the panel displays (800x1280, rotated back here)."""
    s = serial.Serial(); s.port = port; s.baudrate = 115200; s.dtr = False; s.rts = False; s.timeout = 0.3; s.write_timeout = 5
    s.open()
    try: s.set_buffer_size(rx_size=1 << 20, tx_size=1 << 20)
    except Exception: pass
    t0 = time.time(); hdr = None; rest = b''
    # SEND PROTOCOL (tools/remote/TOOLS.md): a bare newline first, >= 300 ms, then the command; otherwise the first command after a quiet
    # period is swallowed by the console driver (that was the "no SHOT header" on COM37). Retry up to 3 times.
    for attempt in range(3):
        s.baudrate = 115200; s.reset_input_buffer()
        s.write(b'\n'); s.flush(); time.sleep(0.4)
        s.write(('shot %s %d%s\n' % (mode, baud, ' jpg' if jpg else '')).encode()); s.flush(); time.sleep(0.05)
        s.baudrate = baud
        b = b''; t = time.time()
        while time.time() - t < 6:
            b += s.read(256)
            r = parse_shot_header(b)
            if r and r[0] == 'ERR': sys.exit('board: ' + r[1])
            if r: hdr, rest = r; break
        if hdr: break
    if not hdr: s.baudrate = 115200; sys.exit('no SHOT header (is the emulator running, not service mode? is the build the shot build?)')
    w, h, ln, crc, panel, ems = int(hdr[1]), int(hdr[2]), int(hdr[3]), int(hdr[4], 16), int(hdr[5]), int(hdr[6]); kind = int(hdr[7]) if len(hdr) > 7 else 1
    c = Card.__new__(Card); c.s = s
    buf = bytearray(); pend = rest
    def rx_exact(n):
        nonlocal pend
        d = pend[:n]; pend = pend[n:]
        return d + (c.rx_exact(n - len(d), 1.5) if len(d) < n else b'')
    def drain():
        """a lost/extra byte desynchronises the stream; the board is stopped waiting for our ack after its frame, so after 0.3 s of
        silence everything of the bad frame has arrived: throw it away and ask for a resend"""
        s.timeout = 0.3
        while s.read(65536): pass
        s.timeout = 0.3
    bad = 0
    while True:
        try:
            hd = rx_exact(8); o, n = struct.unpack('<II', hd)
            if n > CH or o > ln: raise ValueError('bad header')
            d = rx_exact(n); cc, = struct.unpack('<I', rx_exact(4))
            ok = cc == zlib.crc32(d, zlib.crc32(hd))
        except (ValueError, TimeoutError, struct.error):
            ok = False
        if not ok:
            bad += 1
            if bad > 40: sys.exit('shot: too many corrupt frames')
            pend = b''; drain(); s.write(b'N'); continue
        if n == 0: break
        if o != len(buf): sys.exit('shot: frame offset %d expected %d' % (o, len(buf)))
        s.write(b'A'); buf += d
    dt = time.time() - t0; s.baudrate = 115200; s.close()
    # crc 0 = the board had no 2 MB block for a private copy and streamed the live buffer (B29s): every frame was CRC-checked, the image is not one instant
    if len(buf) != ln or (crc and zlib.crc32(bytes(buf)) != crc): sys.exit('shot CRC/size mismatch (%d of %d)' % (len(buf), ln))
    if kind == 0:
        im = rgb565_to_image(buf, w, h)
        if panel: im = im.rotate(90, expand=True)          # the DPI buffer is the frame rotated 90 deg CW: undo it
        im.save(out, quality=95) if out.lower().endswith(('.jpg', '.jpeg')) else im.save(out)
        what = '%dx%d raw565 %d bytes -> %s (%dx%d)' % (w, h, ln, 'png/jpg', im.size[0], im.size[1])
    elif kind == 2:
        im = decode_stripes(buf, w, h)
        if panel: im = im.rotate(90, expand=True)
        im.save(out, quality=95) if out.lower().endswith(('.jpg', '.jpeg')) else im.save(out)
        what = '%dx%d HW-JPEG stripes %d bytes -> %dx%d' % (w, h, ln, im.size[0], im.size[1])
    else:
        open(out, 'wb').write(buf); what = '%dx%d jpeg %d bytes' % (w, h, ln)
        if panel:
            from PIL import Image
            Image.open(out).rotate(90, expand=True).save(out, quality=95)
    if bad: what += ' [%d frames resent]' % bad
    if not crc: what += ' [live stream: no private copy on the board, image may be torn]'
    print('shot %s %s, capture %d ms, total %.1f s (%.3f MB/s) -> %s' % (mode, what, ems, dt, ln / 1e6 / max(dt, 1e-3), out))


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('-p', '--port', default='COM1'); ap.add_argument('--baud', type=int, default=921600); ap.add_argument('--force', action='store_true'); ap.add_argument('--window', type=int, default=1, help='frames in flight for put/get over --host (1 = stop-and-wait)'); ap.add_argument('--host', help='IP[:port] of the Wi-Fi service (TCP 3868) instead of the serial port')
    ap.add_argument('--remote', default='hd.img'); ap.add_argument('--seed', default=SEED); ap.add_argument('--no-sha', action='store_true'); ap.add_argument('--out')
    ap.add_argument('cmd'); ap.add_argument('args', nargs=argparse.REMAINDER)
    a = ap.parse_args(); k = a.cmd; x = a.args
    global OUT; OUT = a.out
    global WINDOW; WINDOW = max(1, a.window)
    if k == 'mt':
        mbr = open(MIRROR, 'rb').read(512); lba, = struct.unpack('<I', mbr[0x1be + 8:0x1be + 12])
        p = '/mnt/' + MIRROR[0].lower() + MIRROR[2:].replace('\\', '/')
        sys.exit(subprocess.call(['wsl', 'env', 'MTOOLS_SKIP_CHECK=1', x[0], '-i', '%s@@%d' % (p, lba * 512)] + x[1:]))
    if k == 'shot':        # runs against the live emulator, no service mode
        mode = 'panel' if '--panel' in x else 'guest'; o = [v for v in x if not v.startswith('--')]
        return shot(a.port, mode, a.baud, o[0] if o else os.path.join(ROOT, 'images', 'shot_%s.png' % time.strftime('%H%M%S')), '--jpg' in x)
    if k == 'wifi':        # switch the board to the Wi-Fi service app over serial, wait for its IP
        c = Card(a.port, a.force, a.baud)
        c.s.write(b'wifisvc' + bytes([10])); time.sleep(0.5); c.s.close()
        t = time.time()
        while time.time() - t < 120:
            time.sleep(2)
            try:
                s = c._open(115200); c.s = s
                if c._hello_any(1.0):
                    r = c.cmd('hello')[1]
                    if 'ip=' in r and 'ip=-' not in r: print(r); return
                    print('waiting for Wi-Fi:', r)
                s.close()
            except Exception: pass
        sys.exit('Wi-Fi service app did not report an IP')
    c = Card(a.port, a.force, a.baud, a.host)
    t = time.time()
    if k in ('hello', 'dbg', 'info', 'df', 'ls', 'stat', 'rm', 'mv', 'mkdir', 'reboot'):
        out, r = c.cmd(' '.join([k] + x)); print('\n'.join(out)); print(r)
    elif k == 'sha256': print(*c.sha256(x[0]))
    elif k == 'get':
        n = c.get(x[0], x[1] if len(x) > 1 else os.path.basename(x[0])); dt = time.time() - t
        print('got %d bytes, %.3f MB/s' % (n, n / 1e6 / max(dt, 1e-3)))
    elif k == 'put':
        n = c.put(x[0], x[1] if len(x) > 1 else None); dt = time.time() - t
        print('put %d bytes, %.3f MB/s' % (n, n / 1e6 / max(dt, 1e-3)))
    elif k == 'patch':
        d = open(x[2], 'rb').read(); print('patched', c.patch(x[0], ((int(x[1]) + o, d[o:o + CH]) for o in range(0, len(d), CH))))
    elif k == 'imgsync': imgsync(c, x[0], a.remote, a.seed, not a.no_sha)
    else: sys.exit('unknown command')


if __name__ == '__main__':
    main()
