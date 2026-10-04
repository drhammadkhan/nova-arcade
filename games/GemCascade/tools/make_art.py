#!/usr/bin/env python3
"""Gem Cascade art: seven faceted gems, the special-gem overlays, sparkles and the logo.
   python3 games/GemCascade/tools/make_art.py [--preview DIR]   (from the repo root)
Each gem is a convex outline with a flat "table" in the middle and bevelled facets round
it. Every facet is lit from the top left, so the cuts read clearly at 22 pixels across."""
import math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "art"))
from pixart import Palette, Img, Header, dither, in_poly, logo, sheet, disc

pal = Palette()
K = pal.add((14, 8, 28))            # outline
WH = pal.add((255, 255, 255))
# six-step ramps, darkest first
GEMS = [
    ("ruby",     (70, 6, 30),   (255, 196, 206), (236, 32, 72)),
    ("sapphire", (10, 22, 92),  (200, 236, 255), (40, 112, 240)),
    ("emerald",  (4, 56, 40),   (210, 255, 200), (30, 190, 96)),
    ("topaz",    (100, 50, 0),  (255, 252, 200), (255, 204, 40)),
    ("amethyst", (48, 10, 84),  (240, 210, 255), (168, 72, 236)),
    ("pearl",    (88, 92, 132), (255, 255, 255), (214, 220, 242)),
    ("amber",    (96, 28, 4),   (255, 230, 170), (252, 132, 28)),
]
RAMPS = [pal.ramp(d, l, 6, m) for _, d, l, m in GEMS]
PINK = pal.add((255, 170, 220)); AQUA = pal.add((150, 240, 255))       # opal fire
FIRE = pal.ramp((150, 20, 0), (255, 250, 190), 4, (255, 120, 20))       # flame aura
METAL = pal.ramp((40, 36, 70), (226, 222, 255), 5, (120, 112, 176))      # UI frames

S = 24
LIGHT = (-0.55, -0.83)

def poly(kind):
    c = 11.5
    if kind == 0:   # ruby: octagon (round brilliant)
        return [(c + 11 * math.cos(a), c + 11 * math.sin(a)) for a in [math.pi / 8 + i * math.pi / 4 for i in range(8)]]
    if kind == 1:   # sapphire: cushion square
        return [(3, 1), (20, 1), (22, 3), (22, 20), (20, 22), (3, 22), (1, 20), (1, 3)]
    if kind == 2:   # emerald: tall step cut
        return [(6, 0), (17, 0), (21, 4), (21, 19), (17, 23), (6, 23), (2, 19), (2, 4)]
    if kind == 3:   # topaz: kite
        return [(11.5, 0), (23, 10), (11.5, 23), (0, 10)]
    if kind == 4:   # amethyst: trillion (triangle)
        return [(11.5, 1), (23, 21), (0, 21)]
    if kind == 6:   # amber: hexagon
        return [(c + 11.5 * math.cos(a), c + 11.5 * math.sin(a)) for a in [i * math.pi / 3 for i in range(6)]]
    return None

def facet_gem(kind):
    ramp = RAMPS[kind]
    img = Img(S, S)
    pts = poly(kind)
    cx = sum(p[0] for p in pts) / len(pts); cy = sum(p[1] for p in pts) / len(pts)
    if kind == 4: cy = 14
    tab = [(cx + (x - cx) * 0.5 - 0.5, cy + (y - cy) * 0.5 - 0.5) for x, y in pts]
    n = len(pts)
    angs = [math.atan2(y - cy, x - cx) for x, y in pts]
    for y in range(S):
        for x in range(S):
            px, py = x + 0.5, y + 0.5
            if not in_poly(px, py, pts): continue
            if in_poly(px, py, tab):
                v = 0.62 - 0.22 * ((px - cx) * 0.55 + (py - cy) * 0.83) / 11
                # a soft diagonal reflection band across the table
                if -3 < (px - cx) + (py - cy) - 2 < 1: v += 0.2
            else:
                a = math.atan2(py - cy, px - cx)
                for i in range(n):
                    a0, a1 = angs[i], angs[(i + 1) % n]
                    d0 = (a - a0) % (2 * math.pi); span = (a1 - a0) % (2 * math.pi)
                    if d0 <= span: break
                (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % n]
                ex, ey = x1 - x0, y1 - y0
                L = math.hypot(ex, ey); nx, ny = ey / L, -ex / L
                if nx * (x0 - cx) + ny * (y0 - cy) < 0: nx, ny = -nx, -ny
                dot = nx * LIGHT[0] + ny * LIGHT[1]
                v = 0.40 + 0.5 * dot
                # split each crown facet in two so it glitters
                v += 0.12 if d0 < span / 2 else -0.06
                img.set(x, y, ramp[max(0, min(5, int(v * 5.99)))])   # facets are flat: no dither
                continue
            img.set(x, y, dither(v, ramp, x, y))
    # rim: darkest shade on the outer edge, light edge on the top-left of the table
    for y in range(S):
        for x in range(S):
            if not img.p[y][x]: continue
            if any(img.get(x + dx, y + dy) == 0 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                img.p[y][x] = ramp[1] if (x - cx) + (y - cy) < -4 else ramp[0]
    # glints
    tx, ty = int(tab[0][0] if kind != 3 else cx), int(cy - 3)
    gx, gy = int(cx - 4), int(cy - 4)
    for dx, dy in ((0, 0), (1, 0), (0, 1)): img.set(gx + dx, gy + dy, WH)
    img.set(int(cx + 3), int(cy + 3), ramp[5])
    return img.outline(K)

def pearl():
    ramp = RAMPS[5]
    img = Img(S, S)
    cx = cy = 11.5
    for y in range(S):
        for x in range(S):
            dx, dy = (x + 0.5 - cx) / 10.5, (y + 0.5 - cy) / 10.5
            r2 = dx * dx + dy * dy
            if r2 > 1: continue
            z = math.sqrt(1 - r2)
            v = 0.25 + 0.6 * max(0, -0.5 * dx - 0.6 * dy + 0.62 * z) + 0.25 * z * z
            c = dither(v, ramp, x, y)
            # opal fire: pink and aqua flecks on the shoulders
            if 0.35 < r2 < 0.8 and (x * 7 + y * 13) % 11 == 0: c = PINK if dx > dy else AQUA
            img.set(x, y, c)
    for y in range(S):
        for x in range(S):
            if img.p[y][x] and any(img.get(x + a, y + b) == 0 for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                img.p[y][x] = ramp[0]
    disc(img, 8, 7, 2.2, WH)
    img.set(15, 16, ramp[5]); img.set(16, 15, ramp[5])
    return img.outline(K)

def gem(kind): return pearl() if kind == 5 else facet_gem(kind)

def nova(frame):
    """Hypercube gem: a prism that cycles every gem colour."""
    img = Img(S, S)
    cx = cy = 11.5
    for y in range(S):
        for x in range(S):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            if abs(dx) + abs(dy) > 11.5 and max(abs(dx), abs(dy)) > 8.5: continue
            if max(abs(dx), abs(dy)) > 10.5: continue
            a = (math.atan2(dy, dx) / (2 * math.pi) + frame / 4) % 1
            k = [0, 6, 3, 2, 1, 4][int(a * 6)]
            r = math.hypot(dx, dy) / 11
            v = 0.95 - r * 0.6
            img.set(x, y, dither(v, RAMPS[k], x, y))
    for i in range(-3, 4):
        img.set(11 + i, 11, WH); img.set(11, 11 + i, WH)
    for d in (-1, 1): img.set(11 + d, 11 + d, WH); img.set(11 + d, 11 - d, WH)
    return img.outline(K)

def flame_aura(frame):
    """Fire ring drawn behind a flame gem (32x32, centred on the gem)."""
    img = Img(32, 32)
    for y in range(32):
        for x in range(32):
            dx, dy = x + 0.5 - 16, y + 0.5 - 16
            r = math.hypot(dx, dy)
            a = math.atan2(dy, dx)
            flick = 1.0 * math.sin(a * 7 + frame * 2.1) + 0.8 * math.sin(a * 11 - frame * 1.3)
            edge = 13.0 + flick + max(0, -dy) / 16 * 2.5     # flames lick upwards
            if 9.0 < r < edge:
                v = 1 - (r - 9.0) / (edge - 9.0)
                img.set(x, y, dither(v, FIRE, x, y))
    return img

def star_overlay(frame):
    """Lightning cross drawn over a star gem."""
    img = Img(S, S)
    for i in range(S):
        if frame == 0:
            img.set(i, 11, WH if 6 < i < 17 else AQUA); img.set(11, i, WH if 6 < i < 17 else AQUA)
        else:
            img.set(i, i, WH if 6 < i < 17 else AQUA); img.set(i, 23 - i, WH if 6 < i < 17 else AQUA)
    for dx, dy in ((0, 0), (1, 0), (0, 1), (-1, 0), (0, -1)): img.set(11 + dx, 11 + dy, WH)
    return img

def sparkle(frame):
    img = Img(7, 7)
    r = [1, 3, 2][frame]
    for i in range(-r, r + 1):
        img.set(3 + i, 3, AQUA if abs(i) == r else WH); img.set(3, 3 + i, AQUA if abs(i) == r else WH)
    return img

def cursor():
    img = Img(28, 28)
    for i in range(7):
        for (x, y) in ((i, 0), (0, i), (27 - i, 0), (27, i), (i, 27), (0, 27 - i), (27 - i, 27), (27, 27 - i)):
            img.set(x, y, WH)
            img.set(x + (1 if x == 0 else -1 if x == 27 else 0), y + (1 if y == 0 else -1 if y == 27 else 0), METAL[3])
    return img

def build_logo():
    grad = [RAMPS[3][5], RAMPS[3][4], RAMPS[6][4], RAMPS[0][4], RAMPS[0][3], RAMPS[4][3], RAMPS[4][2]]
    return logo("GEM CASCADE", 4, grad, RAMPS[4][0], K, shine=WH)

def export(path):
    h = Header("games/GemCascade/tools/make_art.py", "GC", pal)
    h.group("gem", [gem(k) for k in range(7)])
    h.group("nova", [nova(f) for f in range(4)])
    h.group("aura", [flame_aura(f) for f in range(3)])
    h.group("staro", [star_overlay(f) for f in range(2)])
    h.group("sparkle", [sparkle(f) for f in range(3)])
    h.sprite("cursor", cursor())
    h.sprite("logo", build_logo())
    h.line("static const uint8_t GEM_RGB[7][3] = {%s};   // particle colours" % ", ".join("{%d, %d, %d}" % g[3] for g in GEMS))
    h.line("enum { GC_WHITE = %d, GC_METAL0 = %d };" % (WH, METAL[0]))
    h.write(path)

if __name__ == "__main__":
    export(os.path.join(HERE, "..", "art.h"))
    if "--preview" in sys.argv:
        out = sys.argv[sys.argv.index("--preview") + 1]
        items = [gem(k) for k in range(7)] + [nova(f) for f in range(4)] + [flame_aura(f) for f in range(3)] + [star_overlay(0), sparkle(1), cursor()]
        sheet(items, pal, 6, cols=7).save(os.path.join(out, "gc_gems.png"))
        build_logo().png(pal, 2, (20, 14, 40, 255)).save(os.path.join(out, "gc_logo.png"))
