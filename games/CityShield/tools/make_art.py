#!/usr/bin/env python3
"""City Shield art: six city skylines, rubble, missile bases, the ground strip, the
distant ridge, bomber, satellite, moon, crosshair and logo.
   python3 games/CityShield/tools/make_art.py [--preview DIR]   (from the repo root)"""
import math, os, random, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "art"))
from pixart import Palette, Img, Header, dither, logo, sheet, disc, from_rows, fill_poly

pal = Palette()
K = pal.add((10, 8, 24))
WH = pal.add((255, 255, 255))
BLD = pal.ramp((22, 26, 58), (120, 146, 196), 5, (52, 64, 112))      # buildings, dark to light
GLASS = pal.ramp((24, 44, 92), (110, 190, 240), 3)                  # glass towers
WIN = [pal.add((255, 214, 110)), pal.add((255, 246, 200)), pal.add((120, 230, 255))]   # lit windows
NEON = [pal.add((255, 70, 150)), pal.add((70, 255, 190))]
RUB = pal.ramp((30, 22, 30), (130, 100, 90), 4)                      # rubble
EMB = [pal.add((255, 120, 40)), pal.add((255, 200, 80))]             # embers
GRD = pal.ramp((14, 12, 30), (86, 70, 128), 5)                       # ground rock
GRS = pal.ramp((20, 60, 64), (110, 210, 150), 4)                     # moss on top
MET = pal.ramp((40, 44, 70), (220, 230, 250), 5)                     # base metal
RED = [pal.add((160, 20, 40)), pal.add((255, 70, 70))]
GOLD = pal.ramp((120, 70, 10), (255, 236, 140), 3)
SOL = pal.ramp((20, 40, 120), (100, 170, 255), 3)
MOON = pal.ramp((90, 96, 140), (250, 248, 230), 5)
RIDGE = pal.ramp((10, 10, 28), (40, 36, 80), 3)
CYAN = pal.add((90, 230, 255))

GROUND_Y = 206                 # top of the ground image on screen
BASE_X = [24, 160, 296]
CITY_X = [58, 90, 122, 182, 214, 246]   # left edge of each 28-wide city
CITY_W, CITY_H = 28, 22
FLAT = 222                     # screen row the cities stand on

def ground():
    w, h = 320, 240 - GROUND_Y
    img = Img(w, h)
    rng = random.Random(7)
    top = []
    for x in range(w):
        y = FLAT - GROUND_Y + 1 + int(1.2 * math.sin(x * 0.11) + 0.8 * math.sin(x * 0.037 + 2))
        for bx in BASE_X:   # mounds under the bases
            d = abs(x - bx)
            if d < 30: y = min(y, int(4 + (d / 30) ** 2 * 13 - (3 if d < 12 else 0) * (1 - d / 12)))
        top.append(max(2, y))
    for x in range(w):
        for y in range(top[x], h):
            depth = (y - top[x]) / 12
            v = 0.55 - depth * 0.5 + 0.12 * math.sin(x * 0.7 + y * 1.3) + (0.25 if (x * 13 + y * 7) % 23 == 0 else 0)
            c = dither(v, GRD, x, y)
            if y - top[x] < 3: c = GRS[max(0, 3 - (y - top[x]) - (1 if x % 5 == 0 else 0))]
            img.set(x, y, c)
        img.set(x, top[x] - 1, GRS[3] if (x * 7) % 11 < 3 else 0)   # tufts
    # rocks on the slopes
    for _ in range(40):
        x = rng.randrange(w); y = top[x] + rng.randrange(4, 14)
        img.set(x, y, GRD[4]); img.set(x + 1, y, GRD[3]); img.set(x, y + 1, GRD[1])
    return img, top

def city(seed):
    rng = random.Random(seed * 31 + 5)
    img = Img(CITY_W, CITY_H)
    x = 0
    while x < CITY_W:
        bw = rng.randint(4, 8); bh = rng.randint(7, CITY_H - 3)
        if x + bw > CITY_W: bw = CITY_W - x
        glass = rng.random() < 0.3
        ramp = GLASS if glass else BLD
        for yy in range(CITY_H - bh, CITY_H):
            for xx in range(x, x + bw):
                edge = xx == x + bw - 1
                img.set(xx, yy, ramp[0] if edge else ramp[1 if not glass else 1] if (xx - x) > bw // 2 else ramp[2])
        # roof details
        img.rect(x, CITY_H - bh, bw, 1, ramp[-1])
        if rng.random() < 0.4 and bw >= 5:
            img.set(x + bw // 2, CITY_H - bh - 1, MET[2]); img.set(x + bw // 2, CITY_H - bh - 2, MET[2])
            img.set(x + bw // 2, CITY_H - bh - 3, RED[1])
        # windows
        for yy in range(CITY_H - bh + 2, CITY_H - 1, 2):
            for xx in range(x + 1, x + bw - 1, 2):
                if rng.random() < 0.55: img.set(xx, yy, WIN[rng.choice([0, 0, 1, 2])])
        if rng.random() < 0.25 and bh > 10:   # neon sign
            img.rect(x + 1, CITY_H - bh + 3, max(1, bw - 2), 1, NEON[rng.randrange(2)])
        x += bw + (1 if rng.random() < 0.3 else 0)
    return img.outline(K)

def rubble(seed):
    rng = random.Random(seed + 99)
    img = Img(CITY_W, CITY_H)
    for x in range(CITY_W):
        h = int(3 + 3 * abs(math.sin(x * 0.5 + seed)) + rng.random() * 2)
        if x % 9 == 4: h += 4       # a broken wall still standing
        for y in range(CITY_H - h, CITY_H):
            img.set(x, y, RUB[1 + ((x + y) % 3 == 0)] if y > CITY_H - h else RUB[3])
    for _ in range(5):
        img.set(rng.randrange(CITY_W), CITY_H - rng.randrange(1, 4), EMB[rng.randrange(2)])
    return img.outline(K)

def base():
    img = Img(30, 14)
    # armoured dome
    for y in range(14):
        for x in range(30):
            dx, dy = (x + 0.5 - 15) / 13, (y + 0.5 - 14) / 11
            if dx * dx + dy * dy <= 1:
                z = math.sqrt(max(0, 1 - dx * dx - dy * dy))
                v = 0.25 + 0.6 * max(0, -0.6 * dx - 0.5 * dy + 0.6 * z)
                img.set(x, y, dither(v, MET, x, y))
    # launch slit and lamps
    img.rect(13, 3, 4, 3, K)
    img.set(14, 4, RED[1]); img.set(15, 4, RED[0])
    for x in range(4, 27): img.set(x, 13, MET[0])
    for lx in (6, 23): img.set(lx, 10, CYAN)
    return img.outline(K)

def base_ruin():
    img = Img(30, 14)
    for x in range(30):
        h = int(2 + 3 * abs(math.sin(x * 0.7)) + (x % 7 == 3) * 3)
        for y in range(14 - h, 14): img.set(x, y, MET[1] if (x + y) % 3 else RUB[1])
    img.set(10, 12, EMB[0]); img.set(18, 11, EMB[1])
    return img.outline(K)

AMMO = ["W", "W", "w"]
def ammo_icon():
    img = Img(2, 4)
    img.set(0, 0, RED[1]); img.set(1, 0, RED[1])
    for y in range(1, 4): img.set(0, y, WH); img.set(1, y, MET[3])
    return img

BOMBER = [
    "..........mm..........",
    ".........mMMm.........",
    "...mmmmmmMMMMmmmmmmm..",
    ".mMMMMMMMMMMMMMMMMMMMr",
    "mMMWMMMMMMMMMMMMMMMMMm",
    ".mmmmmmmmMMMMmmmmmmm..",
    ".........mMMm.........",
    "..........mm..........",
]
def bomber(frame):
    img = Img(24, 10)
    img.stamp(BOMBER, 1, 1, {"m": MET[1], "M": MET[2], "W": CYAN, "r": RED[frame]})
    img.set(2 if frame else 3, 5, RED[1])
    return img.outline(K)

def satellite(frame):
    img = Img(22, 12)
    img.rect(8, 3, 6, 6, GOLD[1]); img.rect(8, 3, 6, 1, GOLD[2]); img.rect(8, 8, 6, 1, GOLD[0])
    for x0 in (1, 15):
        for y in range(2, 10):
            for x in range(x0, x0 + 6):
                img.set(x, y, SOL[2] if (x + y + frame) % 4 == 0 else SOL[1] if y % 2 else SOL[0])
    img.rect(7, 5, 1, 2, MET[3]); img.rect(14, 5, 1, 2, MET[3])
    img.set(11, 2, MET[3]); img.set(11, 1, RED[frame])
    return img.outline(K)

def moon():
    img = Img(26, 26)
    rng = random.Random(3)
    craters = [(rng.uniform(5, 20), rng.uniform(5, 20), rng.uniform(1.2, 3.2)) for _ in range(7)]
    for y in range(26):
        for x in range(26):
            dx, dy = (x + 0.5 - 13) / 12, (y + 0.5 - 13) / 12
            if dx * dx + dy * dy > 1: continue
            z = math.sqrt(1 - dx * dx - dy * dy)
            v = 0.15 + 0.85 * max(0, -0.7 * dx - 0.3 * dy + 0.65 * z)
            for cx, cy, r in craters:
                d = math.hypot(x - cx, y - cy)
                if d < r: v -= 0.18
                elif d < r + 1 and x < cx: v += 0.08
            img.set(x, y, dither(v, MOON, x, y))
    return img

def crosshair():
    img = Img(13, 13)
    for i in range(13):
        if i in (5, 6, 7): continue
        img.set(i, 6, WH); img.set(6, i, WH)
    for (x, y) in ((1, 1), (11, 1), (1, 11), (11, 11)):
        img.set(x, y, CYAN); img.set(x + (1 if x < 6 else -1), y, CYAN); img.set(x, y + (1 if y < 6 else -1), CYAN)
    img.set(6, 6, CYAN)
    return img

def target_mark():
    img = Img(5, 5)
    for i in range(5): img.set(i, i, CYAN); img.set(4 - i, i, CYAN)
    return img

def ridge():
    """Distant mountain ridge: one height per column (drawn in two shades at run time)."""
    hs = []
    for x in range(320):
        h = 22 + 10 * math.sin(x * 0.021 + 1) + 6 * math.sin(x * 0.063) + 3 * math.sin(x * 0.17 + 2)
        hs.append(int(max(4, h)))
    return hs

def skyline_far():
    """Far city glow silhouette heights (blocky)."""
    rng = random.Random(11)
    hs = []; x = 0
    while x < 320:
        w = rng.randint(3, 9); h = rng.randint(4, 18)
        hs += [h] * w; x += w
    return hs[:320]

def build_logo():
    grad = [WH, CYAN, GLASS[2], GLASS[1], NEON[0], RED[1], RED[0]]
    return logo("CITY SHIELD", 4, grad, BLD[0], K, shine=WH)

def export(path):
    h = Header("games/CityShield/tools/make_art.py", "CS", pal)
    g, top = ground()
    h.sprite("ground", g)
    h.bytes("GROUND_TOP", top)
    h.line("static const int GROUND_Y = %d, FLAT_Y = %d, CITY_W = %d, CITY_H = %d;" % (GROUND_Y, FLAT, CITY_W, CITY_H))
    h.line("static const int BASE_X[3] = {%s};" % ", ".join(map(str, BASE_X)))
    h.line("static const int CITY_X[6] = {%s};" % ", ".join(map(str, CITY_X)))
    h.group("city", [city(i) for i in range(6)])
    h.group("rubble", [rubble(i) for i in range(6)])
    h.sprite("base", base()); h.sprite("base_ruin", base_ruin()); h.sprite("ammo", ammo_icon())
    h.group("bomber", [bomber(0), bomber(1)])
    h.group("sat", [satellite(0), satellite(1)])
    h.sprite("moon", moon()); h.sprite("cross", crosshair()); h.sprite("mark", target_mark())
    h.bytes("RIDGE_H", ridge()); h.bytes("SKYLINE_H", skyline_far())
    h.sprite("logo", build_logo())
    h.line("enum { CS_WHITE = %d, CS_CYAN = %d, CS_WIN0 = %d };" % (WH, CYAN, WIN[0]))
    h.write(path)

if __name__ == "__main__":
    export(os.path.join(HERE, "..", "art.h"))
    if "--preview" in sys.argv:
        out = sys.argv[sys.argv.index("--preview") + 1]
        items = [city(i) for i in range(6)] + [rubble(0), base(), base_ruin(), bomber(0), satellite(0), moon(), crosshair()]
        sheet(items, pal, 5, cols=7, bg=(30, 30, 70, 255)).save(os.path.join(out, "cs_sprites.png"))
        g, _ = ground()
        g.png(pal, 3, (30, 30, 70, 255)).save(os.path.join(out, "cs_ground.png"))
        build_logo().png(pal, 2, (20, 14, 40, 255)).save(os.path.join(out, "cs_logo.png"))
