#!/usr/bin/env python3
"""Generates the browser player's app icons (web/play/icon-192.png, icon-512.png).
   python3 tools/art/player_icon.py   (run from the repo root)"""
import os, sys
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, "..", "..", "games", "NovaLance", "tools"))
import make_assets as base

N = 64   # draw at 64x64 pixel-art resolution, then scale up without smoothing
im = Image.new("RGB", (N, N))
px = im.load()
for y in range(N):
    t = y / (N - 1)
    # synthwave sky over a dark floor with a magenta grid
    if y < 40:
        c = tuple(int(a + (b - a) * (y / 39)) for a, b in zip((5, 4, 26), (70, 22, 90)))
    else:
        c = (12, 6, 34)
    for x in range(N):
        px[x, y] = c
for y in range(41, N):                                  # horizontal grid lines, closer together near the horizon
    if (y - 41) in (0, 2, 5, 9, 14, 21):
        for x in range(N): px[x, y] = (200, 60, 170)
for k in range(-6, 7):                                  # converging vertical lines
    for y in range(41, N):
        x = int(32 + k * (y - 38) * 0.9)
        if 0 <= x < N: px[x, y] = (200, 60, 170)
for y in range(22, 41):                                 # sun
    for x in range(N):
        if (x - 32) ** 2 + (y - 40) ** 2 <= 17 ** 2 and not (y > 30 and (y - 30) % 3 == 0):
            px[x, y] = tuple(int(a + (b - a) * ((y - 22) / 18)) for a, b in zip((255, 216, 74), (255, 79, 150)))
src = base.logo("NA", 3)                                # the logo's pixel font, in the palette's gradient
pal = base.PAL
ox, oy = (N - src.size[0]) // 2, 4
sp = src.load()
for y in range(src.size[1]):
    for x in range(src.size[0]):
        v = sp[x, y]
        if v: px[ox + x, oy + y] = pal[v]
out = os.path.join(here, "..", "..", "web", "play")
for size in (192, 512):
    im.resize((size, size), Image.NEAREST).save(os.path.join(out, "icon-%d.png" % size), optimize=True)
    print("wrote icon-%d.png" % size)
