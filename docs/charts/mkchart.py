"""Benchmark charts for docs/PERFORMANCE.md (data copied from the lab reports, 21 Nov 2026).
usage: python mkchart.py <out dir>   -> doom_fps.svg, doom_rust.svg (needs matplotlib)"""
import sys, statistics as st
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
INK, MUTED, GRID, SURF = '#1f1f1e', '#6b6a64', '#e6e5e0', '#fcfcfb'
OLD, NEW = '#2a78d6', '#eb6834'


def style(ax, fig):
    fig.patch.set_facecolor(SURF); ax.set_facecolor(SURF)
    ax.grid(axis='y', color=GRID, zorder=0); ax.tick_params(colors=MUTED, length=0)
    for s in ax.spines.values(): s.set_visible(False)


# 1. timedemo fps, every demo1 + demo2 run (uncapped), old standalone port vs Doom launched from our Windows 95
runs = {
    ('320x200', 'chip v1.0'): ([57.19, 57.06, 57.08, 56.20, 55.96, 56.36], [78.57, 78.32, 78.60, 77.83, 77.83, 77.88]),
    ('320x200', 'chip v1.3'): ([57.28, 57.28, 56.15], [78.53, 78.53, 78.50, 77.88, 77.83, 77.88]),
    ('640x400', 'chip v1.0'): ([35.10, 35.08, 35.10, 35.12, 35.16, 35.14], [34.85, 34.84, 34.90, 34.91, 34.92]),
    ('640x400', 'chip v1.3'): ([34.97, 34.95, 35.19], [34.70, 34.83, 34.85, 34.91, 34.90, 34.91]),
}
cats = list(runs); w = 0.36
fig, ax = plt.subplots(figsize=(9, 4.6), dpi=150); style(ax, fig)
for i, k in enumerate(cats):
    a, b = st.mean(runs[k][0]), st.mean(runs[k][1])
    for x, v, c in ((i - w / 2 - 0.01, a, OLD), (i + w / 2 + 0.01, b, NEW)):
        ax.bar(x, v, w, color=c, zorder=3)
        ax.text(x, v + 1.2, '%.1f' % v, ha='center', va='bottom', fontsize=9, color=INK)
    if b / a > 1.05: ax.text(i, max(a, b) + 7, '+%.0f %%' % (100 * (b / a - 1)), ha='center', fontsize=9, color=MUTED)
ax.set_xticks(range(len(cats))); ax.set_xticklabels(['%s\n%s' % k for k in cats], fontsize=9, color=INK)
ax.set_ylabel('timedemo fps (mean of all demo1 + demo2 runs)', fontsize=9, color=MUTED); ax.set_ylim(0, 95)
ax.set_title('Doom on the ESP32-P4 10.1in: old standalone port vs Doom launched from our Windows 95', fontsize=10.5, color=INK, loc='left')
ax.legend(handles=[Patch(color=OLD, label='old standalone port'), Patch(color=NEW, label='Doom-only boot from the Win95 desktop')],
          frameon=False, fontsize=9, loc='upper right', labelcolor=INK)
fig.tight_layout(); fig.savefig(OUT + '/doom_fps.svg', facecolor=SURF)

# 2. C vs Rust engine, 640x400 timedemo demo1, chip v1.3, same host code
steps = [('C engine', 34.75), ('Rust,\nstraight c2rust', 34.40), ('+ own column/\nspan kernels', 36.69), ('+ 3D view on\nboth cores', 37.96)]
fig, ax = plt.subplots(figsize=(8, 4.2), dpi=150); style(ax, fig)
for i, (n, v) in enumerate(steps):
    ax.bar(i, v, 0.6, color=OLD if i == 0 else NEW, zorder=3)
    ax.text(i, v + 0.6, '%.2f' % v, ha='center', va='bottom', fontsize=9, color=INK)
ax.set_xticks(range(len(steps))); ax.set_xticklabels([s[0] for s in steps], fontsize=9, color=INK)
ax.set_ylabel('timedemo fps, 640x400 demo1', fontsize=9, color=MUTED); ax.set_ylim(0, 45)
ax.set_title('Same Doom engine in C and in Rust (bit-exact on 11,188 demo frames)', fontsize=10.5, color=INK, loc='left')
fig.tight_layout(); fig.savefig(OUT + '/doom_rust.svg', facecolor=SURF)
print('ok')
