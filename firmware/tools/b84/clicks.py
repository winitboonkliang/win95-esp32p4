"""B84 host A/B analysis of mixtest output: python clicks.py <outdir>
Click score per event (start / stop / volume change) = peak |2nd difference| in the 1 ms after the sample where the mixer applies
the event, over the RMS 2nd difference of the 20 ms before it (the signal's own roughness), in dB. A step in the waveform is a large
2nd difference. Also: share of energy above 6 kHz (images / zipper of the resampler) and the peak level."""
import sys, wave, numpy as np

def rd(p):
    w = wave.open(p); return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float) / 32768, w.getframerate()

d = sys.argv[1]
ev = np.loadtxt(d + '/events.csv', delimiter=',', skiprows=1)
rows = []
for name, blk in (('A_old', 512), ('B_flat', 128), ('B_b84', 128)):
    x, sr = rd(f'{d}/{name}.wav')
    d2 = np.abs(np.diff(x, 2))
    sc = {0: [], 1: [], 2: []}
    for t, k, ch in ev:
        i = int(t * sr / 1000) // blk * blk          # the block where this mixer applies the event
        w = int(0.001 * sr); pre = d2[max(0, i - int(0.02 * sr)):i]
        if i + w >= len(d2) or len(pre) < 10: continue
        base = np.sqrt(np.mean(pre ** 2)) + 1e-4     # floor ~ -80 dBFS: silence before an event is not an infinite ratio
        sc[int(k)].append(20 * np.log10(d2[i - 2:i + w].max() / base + 1e-12))
    X = np.abs(np.fft.rfft(x)) ** 2; f = np.fft.rfftfreq(len(x), 1 / sr)
    hf = 100 * X[f > 6000].sum() / X.sum()
    allsc = np.concatenate([sc[0], sc[1], sc[2]])
    rows.append((name, sr, np.median(sc[0]), np.median(sc[1]), np.median(sc[2]), 100 * np.mean(allsc > 12), np.percentile(allsc, 95), hf,
                 20 * np.log10(np.abs(x).max() + 1e-12)))
print('| mix | rate | click start dB (median) | click stop dB | click vol-change dB | events > 12 dB | p95 dB | energy > 6 kHz | peak dBFS |')
print('|---|---|---|---|---|---|---|---|---|')
for r in rows:
    print('| %s | %d | %.1f | %.1f | %.1f | %.0f %% | %.1f | %.2f %% | %.1f |' % r)
