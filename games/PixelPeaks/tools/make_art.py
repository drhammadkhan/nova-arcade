#!/usr/bin/env python3
"""Pixel Peaks art: every sprite, tile and background layer, drawn in code.
   python3 games/PixelPeaks/tools/make_art.py   (from the repo root)
Writes games/PixelPeaks/art.h and, with --preview DIR, PNG previews."""
import math, os, random, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

# ------------------------------------------------------------------ master palette (index 0 = transparent)
PAL = [(0, 0, 0)]
NAMES = {}
def col(name, rgb):
    NAMES[name] = len(PAL); PAL.append(rgb); return NAMES[name]

K   = col("outline", (26, 16, 36))
# hero
SK  = col("skin", (255, 214, 180)); SK2 = col("skin2", (236, 166, 132)); SK3 = col("skin3", (196, 116, 98))
HA  = col("hair", (58, 34, 52));    HA2 = col("hair2", (104, 62, 84));   HA3 = col("hair3", (150, 96, 116))
GI  = col("gi", (252, 252, 246));   GI2 = col("gi2", (206, 214, 238));   GI3 = col("gi3", (146, 154, 200))
BE  = col("belt", (236, 76, 60));   BE2 = col("belt2", (166, 40, 44))    # recoloured per world at runtime
RI  = col("ribbon", (255, 104, 160)); RI2 = col("ribbon2", (204, 56, 116))
EY  = col("eye", (40, 26, 60));     BL  = col("blush", (255, 150, 164))
WH  = GI
# ground
GR1 = col("grass1", (44, 150, 76)); GR2 = col("grass2", (92, 200, 88)); GR3 = col("grass3", (170, 236, 112)); GR0 = col("grass0", (24, 96, 60))
DI1 = col("dirt1", (156, 98, 64));  DI2 = col("dirt2", (124, 74, 52));  DI3 = col("dirt3", (92, 52, 44));  DI4 = col("dirt4", (190, 132, 88))
PEB = DI4
ST1 = col("stone1", (146, 148, 170)); ST2 = col("stone2", (108, 110, 136)); ST3 = col("stone3", (188, 190, 208)); ST4 = col("stone4", (72, 72, 100))
SN1 = col("snow1", (244, 248, 255)); SN2 = col("snow2", (200, 216, 244))
WD1 = col("wood1", (196, 128, 74));  WD2 = col("wood2", (142, 86, 52));  WD3 = col("wood3", (236, 178, 112))
LQ1 = col("lacquer1", (214, 52, 52)); LQ2 = col("lacquer2", (140, 28, 44))
GO1 = col("gold1", (255, 214, 74));  GO2 = col("gold2", (255, 244, 170)); GO3 = col("gold3", (206, 140, 40))
MT1 = col("metal1", (214, 222, 236)); MT2 = ST2
WA1 = col("water1", (58, 128, 220)); WA2 = col("water2", (104, 176, 244)); WA3 = col("water3", (190, 228, 255)); WA0 = col("water0", (32, 76, 170))
SA1 = col("sakura1", (255, 186, 208)); SA2 = col("sakura2", (250, 138, 176)); SA3 = col("sakura3", (214, 92, 140))
BA1 = col("bark1", (104, 64, 62));  BA2 = col("bark2", (70, 42, 48))
PI1 = col("pine1", (38, 120, 92)); PI2 = col("pine2", (22, 82, 72))
BM1 = col("bamboo1", (140, 206, 96)); BM2 = col("bamboo2", (84, 156, 76))
VE1 = col("vermilion1", (232, 72, 50)); VE2 = col("vermilion2", (156, 40, 40))
CR1 = col("crow1", (64, 66, 118));  CR2 = col("crow2", (98, 104, 164))
MO1 = col("mochi1", (246, 242, 236)); MO2 = col("mochi2", (212, 200, 204))
CH1 = col("chestnut1", (160, 96, 52)); CH2 = col("chestnut2", (112, 64, 40)); CH3 = WD3
GLO = col("glow", (255, 206, 100))
PAPER = GI
assert len(PAL) <= 64, len(PAL)

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]

class Img:
    """A tiny indexed canvas."""
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]
    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else 0
    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h: self.p[y][x] = c
    def stamp(self, rows, ox, oy, key, flip=False):
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch in key and key[ch]:
                    xx = ox + (len(row) - 1 - x if flip else x)
                    self.set(xx, oy + y, key[ch])
    def copy(self):
        o = Img(self.w, self.h); o.p = [r[:] for r in self.p]; return o
    def flipped(self):
        o = Img(self.w, self.h); o.p = [r[::-1] for r in self.p]; return o
    def rot90(self):   # clockwise, square only
        o = Img(self.h, self.w)
        for y in range(self.h):
            for x in range(self.w): o.p[x][self.h - 1 - y] = self.p[y][x]
        return o
    def outline(self, c=K, diag=False):
        add = []
        nb = [(1, 0), (-1, 0), (0, 1), (0, -1)] + ([(1, 1), (-1, -1), (1, -1), (-1, 1)] if diag else [])
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x]: continue
                if any(self.get(x + dx, y + dy) not in (0, c) for dx, dy in nb): add.append((x, y))
        for x, y in add: self.p[y][x] = c
        return self
    def png(self, scale=1):
        im = Image.new("RGBA", (self.w, self.h))
        for y in range(self.h):
            for x in range(self.w):
                v = self.p[y][x]
                im.putpixel((x, y), (*PAL[v], 255) if v else (0, 0, 0, 0))
        return im.resize((self.w * scale, self.h * scale), Image.NEAREST) if scale > 1 else im

# ------------------------------------------------------------------ the hero: a little judoka
# Drawn as a rig: a hand-drawn head, a gi jacket, and limbs placed per pose. Front limbs get a
# thin darker edge where they cross the body (pixel artists call it "selout") so they read clearly.
HEAD = [   # facing right, 3/4 view; '.' empty. The ponytail is drawn separately so it can swing.
    "...HHHHHH...",
    ".HHHHhhhHHH.",
    "HHHhhHHHHHHH",
    "HHhHHHHHHHHH",
    "HHHHHHHHHHHH",
    "HHHHHHHSHHSH",
    "HHHHSSSSSSSS",
    "HHHSSWESSWES",
    "HHHSSEESSEES",
    "HHHSBBSSSSBS",
    ".HHSSSSSmSSs",
    "..HHssssss..",
]
HEAD_KEY = {"H": HA, "h": HA3, "S": SK, "s": SK2, "E": EY, "B": BL, "W": WH, "m": SK3}
PONY = [   # ribbon + ponytail, hangs off the back of the head
    "RR..",
    "rRR.",
    "HHh.",
    "HHH.",
    ".HH.",
    ".HH.",
    "..H.",
]
PONY_KEY = {"R": RI, "r": RI2, "H": HA, "h": HA2}
TORSO = [  # gi jacket: the lapels cross in a V over the chest, belt knotted at the front
    ".WWwSSwWW.",
    "WWWWwSwWWW",
    "WWWWWwWWWW",
    "WWWWwWWWWW",
    "WWWwWWWWWW",
    "gWWWWWWWWg",
    "BBBBBBBBBb",
    "gWWBbBWWWg",
]
TORSO_KEY = {"W": GI, "w": GI2, "g": GI3, "S": SK2, "B": BE, "b": BE2}

def limb(img, pts, width, fill, shade, tip=None, tiplen=0, mask=None):
    """A thick polyline limb (sleeve or trouser leg) with shading on its lower side and a skin tip."""
    drawn = set()
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1):
            x = x0 + (x1 - x0) * i / n; y = y0 + (y1 - y0) * i / n
            for dy in range(width):
                for dx in range(width):
                    drawn.add((round(x - width / 2 + dx + 0.5), round(y - width / 2 + dy + 0.5)))
    for (x, y) in drawn:
        below = (x, y + 1) not in drawn
        img.set(x, y, shade if below else fill)
        if mask is not None: mask.add((x, y))
    if tip:
        (tx, ty) = pts[-1]
        for (dx, dy) in tip:
            img.set(tx + dx, ty + dy, SK if (dx, dy) != tip[-1] else SK2)
            if mask is not None: mask.add((tx + dx, ty + dy))

def selout(img, mask, c=GI3):
    """Darken the edge of a front part where it overlaps something already drawn."""
    for (x, y) in list(mask):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n not in mask and img.get(*n) not in (0, K):
                if img.get(x, y) in (GI, GI2): img.set(x, y, c)
                break

HAND = [(0, 0), (1, 0), (0, 1), (1, 1)]
FOOT = [(0, 0), (1, 0), (2, 0), (1, -1), (0, -1)]
FOOT_T = [(0, 0), (1, 0), (0, -1), (1, -1)]   # tucked / pointing down

def hero(pose):
    """pose: dict of joint positions in a 24x28 box; head/torso offsets; ponytail swing."""
    W, H = 24, 28
    img = Img(W, H)
    hx, hy = pose.get("head", (7, 0))
    tx, ty = pose.get("torso", (7, 11))
    # back arm and back leg first
    limb(img, pose["barm"], 2, GI2, GI3, HAND)
    limb(img, pose["bleg"], 3, GI2, GI3, pose.get("bfoot", FOOT))
    img.stamp(TORSO, tx, ty, TORSO_KEY)
    fl = set()
    limb(img, pose["fleg"], 3, GI, GI2, pose.get("ffoot", FOOT), mask=fl)
    selout(img, fl)
    px, py = pose.get("pony", (hx - 3, hy + 2))
    fa = set()
    if pose.get("armsback"):   # bowing: the arms hang behind the lowered head
        limb(img, pose["farm"], 2, GI, GI2, HAND, mask=fa); selout(img, fa)
    img.stamp(PONY, px, py, PONY_KEY)
    img.stamp(HEAD, hx, hy, HEAD_KEY)
    if not pose.get("armsback"):
        limb(img, pose["farm"], 2, GI, GI2, HAND, mask=fa); selout(img, fa)
    img.outline()
    return img

# joint positions: shoulder ~ (torso.x+2..8, torso.y+1), hip ~ (torso.x+3/6, torso.y+7)
def P(**k): return k
IDLE0 = P(barm=[(9, 12), (8, 16), (8, 18)], farm=[(15, 12), (16, 16), (16, 18)],
          bleg=[(10, 19), (9, 23), (9, 25)], fleg=[(14, 19), (15, 23), (15, 25)])
IDLE1 = P(head=(7, 1), torso=(7, 12), pony=(4, 3),
          barm=[(9, 13), (8, 17), (8, 19)], farm=[(15, 13), (16, 17), (16, 19)],
          bleg=[(10, 20), (9, 23), (9, 25)], fleg=[(14, 20), (15, 23), (15, 25)])
RUN = [
    P(head=(8, 0), torso=(7, 11), pony=(4, 3), barm=[(9, 12), (12, 15), (14, 15)], farm=[(15, 12), (12, 16), (10, 17)],
      bleg=[(10, 19), (7, 21), (5, 23)], fleg=[(14, 19), (17, 22), (18, 25)], bfoot=FOOT_T),
    P(head=(8, 1), torso=(7, 12), pony=(4, 2), barm=[(9, 13), (11, 16), (12, 17)], farm=[(15, 13), (13, 17), (12, 18)],
      bleg=[(10, 20), (9, 22), (8, 25)], fleg=[(14, 20), (16, 22), (15, 25)]),
    P(head=(8, 0), torso=(7, 11), pony=(4, 1), barm=[(9, 12), (8, 15), (6, 16)], farm=[(15, 12), (17, 15), (19, 14)],
      bleg=[(10, 19), (13, 22), (14, 25)], fleg=[(14, 19), (11, 21), (9, 23)], ffoot=FOOT_T),
    P(head=(8, 0), torso=(7, 11), pony=(4, 3), barm=[(9, 12), (6, 15), (5, 16)], farm=[(15, 12), (18, 14), (20, 13)],
      bleg=[(10, 19), (14, 21), (17, 23)], fleg=[(14, 19), (11, 22), (8, 24)], bfoot=FOOT_T, ffoot=FOOT_T),
    P(head=(8, 1), torso=(7, 12), pony=(4, 2), barm=[(9, 13), (8, 16), (8, 18)], farm=[(15, 13), (15, 17), (15, 18)],
      bleg=[(10, 20), (12, 22), (13, 25)], fleg=[(14, 20), (12, 22), (11, 25)]),
    P(head=(8, 0), torso=(7, 11), pony=(4, 1), barm=[(9, 12), (11, 15), (13, 15)], farm=[(15, 12), (13, 16), (11, 17)],
      bleg=[(10, 19), (8, 22), (6, 24)], fleg=[(14, 19), (16, 22), (18, 24)], ffoot=FOOT_T),
]
JUMP = P(head=(8, 0), torso=(7, 11), pony=(4, 4), barm=[(9, 13), (5, 12), (3, 9)], farm=[(16, 13), (20, 12), (22, 9)],
         bleg=[(10, 19), (8, 21), (9, 23)], fleg=[(14, 19), (17, 20), (17, 23)], bfoot=FOOT_T, ffoot=FOOT_T)
FALL = P(head=(8, 1), torso=(7, 12), pony=(4, 0), barm=[(9, 14), (5, 14), (3, 13)], farm=[(16, 14), (20, 14), (22, 13)],
         bleg=[(10, 20), (9, 23), (8, 26)], fleg=[(14, 20), (16, 23), (17, 26)])
HURT = P(head=(6, 2), torso=(7, 12), pony=(3, 1), barm=[(9, 14), (5, 13), (3, 11)], farm=[(16, 14), (20, 13), (22, 11)],
         bleg=[(10, 20), (8, 23), (7, 26)], fleg=[(14, 20), (15, 23), (17, 25)])
BOW = P(armsback=True, head=(10, 5), torso=(8, 12), pony=(8, 5), barm=[(10, 13), (10, 17), (10, 19)], farm=[(15, 13), (15, 17), (15, 19)],
        bleg=[(11, 20), (11, 23), (11, 25)], fleg=[(14, 20), (14, 23), (14, 25)])

def hero_roll():
    """Curled up in a judo roll: a round bundle of gi with the belt and hair showing. 4 rotations."""
    s = 18
    img = Img(s, s)
    c = (s - 1) / 2
    for y in range(s):
        for x in range(s):
            d = math.hypot(x - c, y - c)
            if d <= 7.6:
                ang = math.atan2(y - c, x - c)
                v = GI
                if d > 5.8 and ang > 0.3: v = GI2
                if d > 6.8 and ang > 0.8: v = GI3
                if -2.6 < ang < -0.9 and d > 2.5: v = HA if d < 6.6 else HA2           # the head, tucked
                if abs(ang - 2.2) < 0.28 and d > 3: v = BE                            # the belt wrapping round
                if 0.2 < ang < 0.9 and 3.5 < d < 6: v = SK                           # a hand
                img.set(x, y, v)
    img.outline()
    return [img, img.rot90(), img.rot90().rot90(), img.rot90().rot90().rot90()]

HERO_FRAMES = [("idle0", IDLE0), ("idle1", IDLE1)] + [("run%d" % i, p) for i, p in enumerate(RUN)] + \
              [("jump", JUMP), ("fall", FALL), ("hurt", HURT), ("bow", BOW)]


# ------------------------------------------------------------------ helpers for the rest of the art
def noise_rng(seed): return random.Random(seed)

def from_rows(rows, key, outline=True, diag=False):
    pad = 1 if outline else 0            # leave room for the outline all round
    img = Img(max(len(r) for r in rows) + 2 * pad, len(rows) + 2 * pad)
    img.stamp(rows, pad, pad, key)
    return img.outline(diag=diag) if outline else img

def disc(img, cx, cy, r, c):
    for y in range(int(cy - r - 1), int(cy + r + 2)):
        for x in range(int(cx - r - 1), int(cx + r + 2)):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r: img.set(x, y, c)

def shade_ramp(v, ramp, x, y):
    """v in 0..1 -> ramp colour, Bayer-dithered between steps."""
    f = max(0.0, min(0.999, v)) * (len(ramp) - 1)
    i = int(f)
    if (f - i) * 16 > BAYER[y & 3][x & 3] and i + 1 < len(ramp): i += 1
    return ramp[i]

# ------------------------------------------------------------------ ground tiles (16 edge variants each)
# mask bits: 1 top open, 2 left open, 4 right open, 8 bottom open
def ground_tile(mask, mat, seed):
    rnd = noise_rng(seed)
    t = Img(16, 16)
    if mat == "snow":
        for y in range(16):                        # stone courses with offset joints
            row = y // 8; off = 4 if row % 2 else 0
            for x in range(16):
                jx = (x + off) % 8
                v = ST1
                if y % 8 == 7 or jx == 7: v = ST4
                elif y % 8 == 0 or jx == 0: v = ST3
                elif (x * 7 + y * 13 + seed) % 11 == 0: v = ST2
                t.set(x, y, v)
    else:
        base, dark, darker, light = DI1, DI2, DI3, DI4
        for y in range(16):
            for x in range(16):
                t.set(x, y, base)
        for _ in range(7):                         # darker clumps
            cx, cy = rnd.randrange(16), rnd.randrange(16)
            for dy in range(-1, 2):
                for dx in range(-2, 2):
                    if rnd.random() < 0.7: t.set((cx + dx) % 16, (cy + dy) % 16, dark)
        for _ in range(4):                         # pebbles with a lit top
            cx, cy = rnd.randrange(1, 14), rnd.randrange(1, 14)
            t.set(cx, cy, light); t.set(cx + 1, cy, light); t.set(cx, cy + 1, darker); t.set(cx + 1, cy + 1, darker)
        for _ in range(6):
            t.set(rnd.randrange(16), rnd.randrange(16), darker)
    edge_dark = ST4 if mat == "snow" else DI3
    edge_mid = ST2 if mat == "snow" else DI2
    if mask & 2:
        for y in range(16): t.set(0, y, edge_dark); t.set(1, y, edge_mid if t.get(1, y) != ST4 else ST4)
    if mask & 4:
        for y in range(16): t.set(15, y, edge_dark); t.set(14, y, edge_mid if t.get(14, y) != ST4 else ST4)
    if mask & 8:
        for x in range(16): t.set(x, 15, edge_dark); t.set(x, 14, edge_mid)
    if mask & 1:
        if mat == "snow":
            depth = [3, 4, 4, 5, 4, 4, 3, 4, 5, 5, 4, 3, 4, 4, 5, 4]
            for x in range(16):
                d = depth[(x + seed) % 16]
                for y in range(d + 1):
                    t.set(x, y, SN1 if y < d - 1 else SN2)
                if (x + seed) % 5 == 2: t.set(x, d + 1, SN2)      # little drips
            for x in range(1, 15, 5): t.set(x, 0, WH)
        else:
            moss = mat == "moss"
            top, mid, low, deep = (GR2, GR1, GR0, DI3) if moss else (GR3, GR2, GR1, GR0)
            depth = [4, 5, 4, 3, 4, 5, 5, 4, 3, 4, 5, 4, 4, 3, 4, 5]
            for x in range(16):
                d = depth[(x * 3 + seed) % 16]
                for y in range(d + 2):
                    v = top if y < 2 else mid if y < d else low if y == d else deep
                    t.set(x, y, v)
            # blades: the top row is mostly transparent with a few tufts poking up
            for x in range(16):
                t.set(x, 0, 0)
            for x in (2, 6, 7, 11, 14):
                t.set((x + seed) % 16, 0, top)
            if not moss:
                for x in (4, 12):                              # a highlight on the lip
                    t.set((x + seed) % 16, 1, WH if seed % 3 == 0 and x == 4 else GR3)
        if mask & 2:                                           # rounded top-left corner
            for (x, y) in ((0, 0), (1, 0), (0, 1)): t.set(x, y, 0)
        if mask & 4:
            for (x, y) in ((15, 0), (14, 0), (15, 1)): t.set(x, y, 0)
    return t

def plank_tile():
    t = Img(16, 16)
    for x in range(16):
        t.set(x, 0, WD3); t.set(x, 1, WD1); t.set(x, 2, WD1); t.set(x, 3, WD2); t.set(x, 4, K)
    for x in (3, 12): t.set(x, 2, WD2)
    t.set(0, 1, WD2); t.set(8, 1, WD2)
    for y in range(5, 9): t.set(2, y, BA1); t.set(13, y, BA1)       # little rope ties hanging down
    t.set(2, 9, BA2); t.set(13, 9, BA2)
    return t

def spike_tile():
    t = Img(16, 16)
    for i in range(4):
        ox = i * 4
        for y in range(8):
            half = (y + 1) // 2
            for x in range(2 - half, 2 + half):
                t.set(ox + x, 7 + y, MT1 if x < 2 else MT2)
        t.set(ox + 1, 7, WH)
    for x in range(16): t.set(x, 15, ST4)
    return t.outline()

def water_tiles():
    out = []
    for f in range(2):
        t = Img(16, 16)
        for y in range(16):
            for x in range(16):
                t.set(x, y, shade_ramp(0.9 - y / 22, [WA0, WA1, WA2], x + f * 2, y))
        for x in range(16):
            crest = 2 + int(1.5 * math.sin((x + f * 8) * math.pi / 8))
            for y in range(crest): t.set(x, y, 0)
            t.set(x, crest, WA3); t.set(x, crest + 1, WA2)
        out.append(t)
    body = Img(16, 16)
    for y in range(16):
        for x in range(16): body.set(x, y, shade_ramp(0.45 - y / 40, [WA0, WA1, WA2], x, y))
    return out + [body]

def box_tiles():
    frames = []
    for f in range(3):
        t = Img(16, 16)
        for y in range(16):
            for x in range(16):
                edge = x in (0, 15) or y in (0, 15)
                t.set(x, y, K if edge else LQ1)
        for x in range(1, 15): t.set(x, 1, GO1 if f < 2 else ST2); t.set(x, 14, GO3 if f < 2 else ST4)
        for y in range(1, 15): t.set(1, y, GO1 if f < 2 else ST2); t.set(14, y, GO3 if f < 2 else ST4)
        for y in range(2, 14):
            for x in range(2, 14): t.set(x, y, (LQ1 if y < 9 else LQ2) if f < 2 else (ST2 if y < 9 else ST4))
        if f < 2:                                  # a gold crest (a stylised blossom) in the middle
            for (dx, dy) in ((0, -3), (3, 0), (0, 3), (-3, 0)):
                disc(t, 7.5 + dx, 7.5 + dy, 1.6, GO1)
            disc(t, 7.5, 7.5, 1.4, GO2 if f == 1 else GO3)
            t.set(4, 3, GO2 if f == 0 else WH); t.set(3, 4, GO2)
        for (x, y) in ((2, 2), (13, 2), (2, 13), (13, 13)): t.set(x, y, GO2 if f < 2 else ST3)   # studs
        frames.append(t)
    return frames

def block_tile():
    t = Img(16, 16)
    for y in range(16):
        for x in range(16):
            v = ST1
            if x == 0 or y == 0: v = ST3
            if x == 15 or y == 15: v = ST4
            if (x == 1 or y == 1) and 0 < x < 15 and 0 < y < 15: v = ST3 if x + y < 20 else ST1
            if x == 14 or y == 14: v = ST2 if 0 < x < 15 and 0 < y < 15 else v
            t.set(x, y, v)
    for (x, y) in ((5, 6), (10, 9), (6, 11)): t.set(x, y, ST2)
    return t

# ------------------------------------------------------------------ items
def coin_frames():
    out = []
    for w in (5, 3, 1, 3):
        img = Img(12, 12)
        for y in range(12):
            for x in range(12):
                dx = (x - 5.5) / (w + 0.01); dy = (y - 5.5) / 5
                if dx * dx + dy * dy <= 1:
                    v = GO1
                    if dx < -0.35 or dy < -0.55: v = GO2
                    if dx > 0.45 or dy > 0.6: v = GO3
                    img.set(x, y, v)
        if w >= 3:
            for y in range(3, 9): img.set(6, y, GO3)       # the notch on the coin face
        out.append(img.outline())
    return out

HEART = ["..HH.HH..", ".HhHHHHH.", ".HhHHHHH.", ".HHHHHHH.", "..HHHHH..", "...HHH...", "....H...."]
HEART_KEY = {"H": LQ1, "h": WH}
HEART_EMPTY_KEY = {"H": ST4, "h": ST2}
SCROLL = [
    "WW........WW",
    "WPPPPPPPPPPW",
    "WPpppppppPPW",
    "WPPPPrrPPPPW",
    "WPpppRRppPPW",
    "WPPPPrrPPPPW",
    "WPppppppPPPW",
    "WPPPPPPPPPPW",
    "WW........WW",
]
SCROLL_KEY = {"W": WD2, "P": PAPER, "p": GI2, "r": LQ2, "R": LQ1}

# ------------------------------------------------------------------ enemies
MOCHI = [[
    "....MMMMMM....",
    "..MMMMMMMMMM..",
    ".MMMMMMMMMMMM.",
    ".MMMWMMMMWMMM.",
    "MMMEEMMMMEEMMM",
    "MMMEEMMMMEEMMM",
    "MMBBMMmmMMBBMM",
    "MMMMMMMMMMMMMM",
    "mMMMMMMMMMMMMm",
    ".mmMMMMMMMMmm.",
    "..mmmmmmmmmm..",
], [
    "..............",
    "...MMMMMMMM...",
    ".MMMMMMMMMMMM.",
    "MMMMWMMMMWMMMM",
    "MMMEEMMMMEEMMM",
    "MMMEEMMMMEEMMM",
    "MMBBMMmmMMBBMM",
    "MMMMMMMMMMMMMM",
    "mMMMMMMMMMMMMm",
    "mmMMMMMMMMMMmm",
    ".mmmmmmmmmmmm.",
]]
MOCHI_KEY = {"M": MO1, "m": MO2, "E": EY, "W": WH, "B": BL}
MOCHI_FLAT = ["..............", "..............", "..............", "..............", "..............",
              "..............", "..MMMMMMMMMM..", ".MMEEMMMMEEMM.", "MMMMMMMMMMMMMM", "mmmmmmmmmmmmmm", ".mmmmmmmmmmmm."]

CROW = [[
    "..........CC....",
    ".........CCCC...",
    "....CC..CCCCWCO.",
    "...CCCCCCCCCEOOO",
    "..CCcccCCCCCCO..",
    ".CCcccccCCCCC...",
    "CCcc..cccCCC....",
    "Cc.....ccCC.....",
    ".........OO.....",
], [
    "................",
    ".........CCCC...",
    ".........CCCWCO.",
    "..cccCCCCCCCEOOO",
    ".CCCCCCCCCCCCO..",
    "CCCCCCccCCCC....",
    ".CCCCccccCC.....",
    "..CCCc..........",
    ".........OO.....",
]]
CROW_KEY = {"C": CR1, "c": CR2, "W": WH, "E": EY, "O": GO3}

def chestnut(frame):
    img = Img(16, 16)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - 7.5, y - 8.5)
            if d <= 5.8:
                img.set(x, y, CH1 if (x - 7.5) * -0.6 + (y - 8.5) * -0.8 > -1.5 else CH2)
    for i in range(12):                                   # spikes all round, shimmying per frame
        a = i * math.pi / 6 + frame * 0.26
        for r in (6.4, 7.3):
            img.set(round(7.5 + math.cos(a) * r), round(8.5 + math.sin(a) * r), CH3 if r > 7 else CH1)
    for (x, y) in ((5, 7), (6, 7), (9, 7), (10, 7)): img.set(x, y, WH)
    img.set(6, 8, EY); img.set(10, 8, EY)
    img.set(4, 6, EY); img.set(5, 6, EY); img.set(10, 6, EY); img.set(11, 6, EY)   # cross eyebrows
    img.set(7, 11, EY); img.set(8, 11, EY)
    return img.outline()

# ------------------------------------------------------------------ scenery
def sakura_tree():
    rnd = noise_rng(7)
    img = Img(56, 64)
    for y in range(34, 64):                               # trunk with a slight lean and a lit side
        cx = 27 + int(2 * math.sin(y / 9))
        w = 3 + (y - 34) // 10
        for x in range(cx - w, cx + w + 1):
            img.set(x, y, BA1 if x < cx else BA2)
    for (x0, y0, x1, y1) in ((27, 40, 14, 26), (28, 38, 42, 24), (27, 36, 26, 18)):
        n = 20
        for i in range(n + 1):
            x = x0 + (x1 - x0) * i / n; y = y0 + (y1 - y0) * i / n
            img.set(round(x), round(y), BA1); img.set(round(x) + 1, round(y), BA2)
    for (cx, cy, r) in ((14, 22, 11), (28, 14, 13), (42, 21, 11), (21, 30, 9), (36, 30, 9), (28, 25, 10)):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                d = math.hypot(x - cx, y - cy)
                if d <= r - rnd.random() * 1.8:
                    lit = ((x - cx) * -0.5 + (y - cy) * -0.85) / r
                    img.set(x, y, shade_ramp(0.55 + lit * 0.5, [SA3, SA2, SA1], x, y))
    for _ in range(30):                                   # a few bright petals
        img.set(rnd.randrange(6, 50), rnd.randrange(4, 38), WH if rnd.random() < 0.3 else SA1)
    return img.outline()

def pine_tree():
    img = Img(40, 60)
    for y in range(44, 60):
        for x in range(18, 22): img.set(x, y, BA1 if x < 20 else BA2)
    for tier, (top, bot, half) in enumerate(((2, 20, 9), (12, 34, 14), (24, 48, 19))):
        for y in range(top, bot):
            w = int(half * (y - top) / (bot - top)) + 1
            for x in range(20 - w, 20 + w):
                img.set(x, y, PI1 if x < 20 - w // 3 else PI2)
        for x in range(20 - half, 20 + half):                     # snow along each tier's edge
            if (x * 5) % 7 < 4: img.set(x, bot - 1, SN1); img.set(x, bot - 2, SN2 if (x % 3) else SN1)
        for y in range(top, top + 3): img.set(20, y, SN1)
    return img.outline()

def bamboo():
    img = Img(30, 72)
    for (x0, w, c1, c2) in ((4, 3, BM2, PI1), (12, 4, BM1, BM2), (22, 3, BM1, BM2)):
        for y in range(4 if x0 != 12 else 0, 72):
            for x in range(x0, x0 + w):
                img.set(x, y, c1 if x < x0 + w - 1 else c2)
            if y % 14 == 7:
                for x in range(x0 - 1, x0 + w + 1): img.set(x, y, c2)
    for (x, y, d) in ((8, 10, 1), (17, 5, -1), (26, 16, 1), (2, 22, -1), (16, 28, 1)):   # leaf sprays
        for i in range(7):
            img.set(x + d * i, y - i // 2, BM1 if i % 2 else PI1)
            img.set(x + d * i, y - i // 2 + 1, BM2)
    return img.outline()

TORO = [   # stone lantern
    "....SSSSSSS.....",
    "..SSSSSSSSSSS...",
    ".SSsssssssssSS..",
    "....sSSSSSs.....",
    "....SGGGGGS.....",
    "....SGgggGS.....",
    "....SGGGGGS.....",
    "...SSSSSSSSS....",
    ".....sSSSs......",
    "......SSs.......",
    "......SSs.......",
    "......SSs.......",
    "....SSSSSSs.....",
    "...SSSSSSSSs....",
]
TORO_KEY = {"S": ST1, "s": ST2, "G": GLO, "g": GO1}

def lantern(lit):
    """Checkpoint: a round paper lantern (chochin) hanging from a wooden post. Lit = glowing."""
    img = Img(20, 38)
    body = [GLO, GO1, GO3] if lit else [PAPER, GI2, GI3]
    for y in range(4, 19):                     # the paper globe, ribbed
        t = (y - 11.5) / 7.5
        w = int(7.5 * math.sqrt(max(0, 1 - t * t))) + 1
        for x in range(10 - w, 10 + w):
            v = body[0] if x < 10 + w // 3 else body[1]
            if y % 3 == 0: v = body[2] if not lit else GO3
            img.set(x, y, v)
    for x in range(6, 14): img.set(x, 3, HA); img.set(x, 19, HA)            # black caps
    for x in range(8, 12): img.set(x, 2, HA); img.set(x, 20, HA)
    if lit:
        img.set(7, 8, WH); img.set(7, 9, GO2); img.set(8, 7, GO2)
    for y in range(21, 24): img.set(10, y, LQ1)                             # red tassel
    img.set(9, 24, LQ1); img.set(11, 24, LQ2)
    for y in range(0, 38):                                                  # the post and arm
        img.set(18, y, WD1); img.set(19, y, WD2)
    for x in range(10, 19): img.set(x, 0, WD2); img.set(x, 1, WD1)
    img.set(10, 2, HA)
    return img.outline()

def torii():
    img = Img(72, 70)
    for x in range(0, 72):                                # kasagi: the curved black top beam
        lift = int(3 * ((x - 35.5) / 36) ** 4 * 3)
        for y in range(6 - lift, 11 - lift): img.set(x, y, HA if y < 9 - lift else HA2)
        for y in range(11 - lift, 14 - lift): img.set(x, y, VE1 if x % 70 else VE2)
    for x in range(6, 66):                                # nuki: the lower tie beam
        for y in range(24, 28): img.set(x, y, VE1 if y < 26 else VE2)
    for x0 in (12, 55):                                   # the two pillars
        for y in range(12, 70):
            for x in range(x0, x0 + 6): img.set(x, y, VE1 if x < x0 + 4 else VE2)
        for y in range(62, 70):
            for x in range(x0 - 1, x0 + 7): img.set(x, y, HA)
    for y in range(14, 24):                               # gold plaque
        for x in range(31, 41): img.set(x, y, GO1 if 32 <= x <= 39 and 15 <= y <= 22 else GO3)
    return img.outline()

BUSH = ["....GGGG....GGG....", "..GGGgGGGG.GGgGG...", ".GGgGGGGGGGGGGGGG..", "GGGGGGGGGGGGGGGGGG.", "GGGGGGGGGGGGGGGGGGG", "ggGGGGGGGGGGGGGGGgg", ".ggggggggggggggggg."]
BUSH_KEY = {"G": GR2, "g": GR1}
FLOWERS = [".P...Y..", "PpP.YyY.", ".P...Y..", ".G.G.G..", ".G.G.G.."]
FLOWERS_KEY = {"P": SA2, "p": WH, "Y": GO1, "y": GO3, "G": GR1}
ROCK = ["....SSSSS....", "..SSsSSSSSs..", ".SSSSSSSSSSs.", "SSSSSSSSSSSss", "sssssssssssss"]
ROCK_KEY = {"S": ST1, "s": ST2}

def cloud(w, h, seed):
    rnd = noise_rng(seed)
    img = Img(w, h)
    bumps = [(rnd.uniform(0.15, 0.85) * w, h * 0.55, rnd.uniform(0.22, 0.32) * w) for _ in range(5)]
    for y in range(h):
        for x in range(w):
            inside = any(((x - cx) / r) ** 2 + ((y - cy) / (r * 0.62)) ** 2 <= 1 for cx, cy, r in bumps) and y < h - 2
            if inside: img.set(x, y, SN2 if y > h * 0.62 else WH)
    return img

# world colour tables for the parallax layers (index 1..7) and the sky gradient
WORLDS = [
    dict(name="BLOSSOM HILLS", sky=((72, 142, 232), (196, 228, 255)), sun=(255, 244, 200),
         far=[(152, 172, 218), (178, 198, 236), (112, 142, 204), (136, 166, 222), (174, 198, 236), (214, 226, 250), (246, 250, 255)],
         mid=[(86, 108, 150), (66, 86, 128), (126, 196, 124), (92, 164, 112), (255, 196, 218), (234, 150, 188), (80, 140, 100)],
         ground="grass", belt=((236, 76, 60), (166, 40, 44))),
    dict(name="BAMBOO GROVE", sky=((84, 162, 160), (214, 238, 206)), sun=(255, 250, 220),
         far=[(132, 176, 168), (156, 196, 184), (104, 150, 138), (126, 170, 154), (160, 200, 184), (196, 224, 214), (232, 246, 238)],
         mid=[(66, 112, 84), (48, 94, 72), (86, 146, 92), (66, 122, 80), (100, 166, 96), (132, 196, 110), (54, 110, 70)],
         ground="moss", belt=((250, 200, 60), (196, 140, 30))),
    dict(name="MISTY PEAKS", sky=((42, 50, 112), (246, 166, 150)), sun=(255, 214, 170),
         far=[(120, 104, 160), (150, 128, 180), (84, 78, 136), (108, 96, 156), (156, 124, 170), (206, 190, 220), (246, 236, 246)],
         mid=[(60, 60, 100), (44, 46, 84), (70, 84, 110), (54, 64, 94), (36, 72, 84), (28, 56, 70), (230, 236, 250)],
         ground="snow", belt=((80, 190, 100), (40, 130, 70))),
]

# ------------------------------------------------------------------ parallax layers (values 1..7, 0 = clear)
# Their colours come from each world's table at run time, so one drawing serves every world.
LW = 512
def periodic(x, terms, seed):
    rnd = noise_rng(seed)
    return sum(a * math.sin(2 * math.pi * (k * x / LW) + rnd.uniform(0, 6.28)) for k, a in terms)

def far_layer():
    H = 112
    img = Img(LW, H)
    BACK = [(2, 16), (5, 9), (11, 4), (23, 2)]
    FRONT = [(3, 14), (7, 7), (13, 3), (29, 1.5)]
    back = [44 + periodic(x, BACK, 3) for x in range(LW)]
    front = [70 + periodic(x, FRONT, 9) for x in range(LW)]
    def lit(h, x):
        # light comes from the left: a face is lit where the ridge (smoothed over 9 px) rises to the right
        return h[(x + 4) % LW] - h[(x - 4) % LW] < 0
    for x in range(LW):
        lb, lf = lit(back, x), lit(front, x)
        for y in range(H):
            if y >= back[x]:
                v = 2 if lb else 1
                if y < back[x] + 7 and back[x] < 34: v = 7 if lb else 6          # snowy peaks
                img.set(x, y, v)
            if y >= front[x]:
                v = 4 if lf else 3
                if y < front[x] + 5 and front[x] < 60: v = 7 if lf else 6
                if y > front[x] + 18: v = 5 if BAYER[y & 3][x & 3] < (y - front[x] - 18) else v   # mist at the base
                img.set(x, y, v)
    return img

def mid_layer(kind):
    H = 96
    img = Img(LW, H)
    rnd = noise_rng({"hills": 11, "bamboo": 12, "pines": 13}[kind])
    for x in range(LW):
        top = 56 + periodic(x, [(3, 8), (6, 4), (15, 2)], 21)
        for y in range(int(top), H): img.set(x, y, 3 if y < top + 3 else 4)
    if kind == "hills":
        for i in range(10):                                   # blossom trees along the hills
            cx = int(i * LW / 10 + rnd.uniform(0, 30)) % LW
            base = int(56 + periodic(cx, [(3, 8), (6, 4), (15, 2)], 21))
            r = rnd.randint(8, 13)
            for y in range(base - 2 * r, base + 1):
                for x in range(cx - r, cx + r + 1):
                    d = math.hypot((x - cx), (y - (base - r - 4)) * 1.2)
                    if d <= r - rnd.random() * 1.2: img.set(x % LW, y, 5 if (x - cx) + (y - base) < -r else 6)
            for y in range(base - r, base + 1): img.set(cx % LW, y, 2); img.set((cx + 1) % LW, y, 2)
        px = 300                                              # a pagoda on the skyline
        base = int(56 + periodic(px, [(3, 8), (6, 4), (15, 2)], 21))
        for tier in range(4):
            y = base - 10 - tier * 9; w = 14 - tier * 3
            for yy in range(y, y + 3):
                for x in range(px - w - (y + 2 - yy), px + w + (y + 2 - yy)): img.set(x, yy, 2)
            for yy in range(y + 3, y + 9):
                for x in range(px - w + 3, px + w - 3): img.set(x, yy, 1)
        for yy in range(base - 50, base - 44): img.set(px, yy, 2)
    elif kind == "bamboo":
        for i in range(70):
            x0 = rnd.randrange(LW); w = rnd.choice((2, 2, 3, 4)); v = rnd.choice((5, 5, 6, 2))
            top = rnd.randrange(0, 30)
            for y in range(top, H):
                for x in range(x0, x0 + w): img.set(x % LW, y, v if x < x0 + w - 1 else 7 if v != 2 else 2)
                if y % 13 == (x0 % 13): 
                    for x in range(x0 - 1, x0 + w + 1): img.set(x % LW, y, 7)
            for k in range(3):                                # leaves
                ly = rnd.randrange(top, top + 40); d = rnd.choice((-1, 1))
                for j in range(8): img.set((x0 + d * j) % LW, ly - j // 3, 6)
    else:
        for i in range(26):
            cx = rnd.randrange(LW); base = int(56 + periodic(cx, [(3, 8), (6, 4), (15, 2)], 21)) + 2
            h = rnd.randint(16, 30)
            for y in range(base - h, base + 1):
                w = int((y - (base - h)) * 0.42) + 1
                for x in range(cx - w, cx + w + 1): img.set(x % LW, y, 5 if x < cx else 6)
            img.set(cx % LW, base - h, 7); img.set(cx % LW, base - h + 1, 7)
    return img

# ------------------------------------------------------------------ logo: "PIXEL PEAKS" in a chunky pixel font
FONT = {
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    " ": ["00000"] * 7,
}
def logo(text="PIXEL PEAKS", s=4):
    grad = [WH, SN2, WA2, WA1, SA2, SA3, VE2]           # snowy white down to blossom pink, like a mountain at dusk
    w = len(text) * 6 * s + 6; h = 7 * s + 6
    img = Img(w, h)
    for pass_ in (0, 1):
        ox, oy = (4, 4) if pass_ == 0 else (1, 1)
        for i, ch in enumerate(text):
            for gy, row in enumerate(FONT[ch]):
                for gx, bit in enumerate(row):
                    if bit != "1": continue
                    for yy in range(s):
                        for xx in range(s):
                            x0 = ox + (i * 6 + gx) * s + xx; y0 = oy + gy * s + yy
                            if pass_ == 0: img.set(x0, y0, HA2)
                            else:
                                c = grad[gy]
                                if yy == 0 and gy == 0: c = WH
                                elif yy == s - 1 and (gy == 6 or FONT[ch][gy + 1][gx] != "1"): c = VE2 if gy >= 4 else WA1
                                if yy == 1 and xx == 1 and gy in (1, 2): c = WH
                                img.set(x0, y0, c)
    return img.outline()

# ------------------------------------------------------------------ export to art.h
def rgb565(r, g, b): return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

MATS = ["grass", "moss", "snow"]
TILES_PER_MAT = 18          # 16 edge masks + 2 alternative fills

def export(path):
    L = ["// AUTO-GENERATED by games/PixelPeaks/tools/make_art.py - edit that script, not this file", "#pragma once",
         "#include <stdint.h>", "#ifndef ARCADE_SPRITE_T", "#define ARCADE_SPRITE_T",
         "struct Sprite { uint16_t w, h; const uint8_t* px; };", "#endif",
         "static const uint16_t PP_PAL565[%d] = {%s};" % (len(PAL), ", ".join("0x%04X" % rgb565(*c) for c in PAL)),
         "static const int PP_PAL_N = %d;" % len(PAL),
         "enum { PC_BELT = %d, PC_BELT2 = %d };   // palette slots recoloured per world" % (BE, BE2)]
    def arr(name, img):
        data = [v for row in img.p for v in row]
        L.append("static const uint8_t %s_px[%d] = {%s};" % (name, len(data), ",".join(map(str, data))))
        L.append("static const Sprite SPR_%s = {%d, %d, %s_px};" % (name.upper(), img.w, img.h, name))
    def group(name, imgs):
        for i, im in enumerate(imgs): arr("%s%d" % (name, i), im)
        L.append("static const Sprite* const %s[%d] = {%s};" % (name.upper(), len(imgs), ", ".join("&SPR_%s%d" % (name.upper(), i) for i in range(len(imgs)))))
    group("hero", [hero(p) for _, p in HERO_FRAMES])
    L.append("enum { H_IDLE0, H_IDLE1, H_RUN0, H_JUMP = H_RUN0 + 6, H_FALL, H_HURT, H_BOW };")
    group("roll", hero_roll())
    group("coin", coin_frames())
    arr("heart", from_rows(HEART, HEART_KEY)); arr("heart_empty", from_rows(HEART, HEART_EMPTY_KEY))
    arr("scroll", from_rows(SCROLL, SCROLL_KEY))
    group("mochi", [from_rows(m, MOCHI_KEY) for m in MOCHI]); arr("mochi_flat", from_rows(MOCHI_FLAT, MOCHI_KEY))
    group("crow", [from_rows(c, CROW_KEY) for c in CROW])
    group("chestnut", [chestnut(0), chestnut(1)])
    arr("sakura", sakura_tree()); arr("pine", pine_tree()); arr("bamboo", bamboo())
    arr("toro", from_rows(TORO, TORO_KEY)); arr("lantern_off", lantern(False)); arr("lantern_on", lantern(True))
    arr("torii", torii()); arr("bush", from_rows(BUSH, BUSH_KEY)); arr("flowers", from_rows(FLOWERS, FLOWERS_KEY))
    arr("logo", logo()); arr("rock", from_rows(ROCK, ROCK_KEY)); arr("cloud0", cloud(48, 18, 1)); arr("cloud1", cloud(40, 16, 2))
    # tiles: 16x16, 0 = transparent
    tiles = []
    for mi, mat in enumerate(MATS):
        for m in range(16): tiles.append(ground_tile(m, mat, m * 3 + mi))
        tiles.append(ground_tile(0, mat, 101 + mi)); tiles.append(ground_tile(0, mat, 202 + mi))
    extra = [plank_tile(), spike_tile()] + water_tiles() + box_tiles() + [block_tile()]
    tiles += extra
    L.append("enum { T_GROUND = 0, T_PER_MAT = %d, T_PLANK = %d, T_SPIKE, T_WATER0, T_WATER1, T_WATERB, T_BOX0, T_BOX1, T_BOXUSED, T_BLOCK, T_COUNT };" % (TILES_PER_MAT, len(MATS) * TILES_PER_MAT))
    L.append("static const uint8_t TILE_PX[%d][256] = {" % len(tiles))
    for t in tiles: L.append("  {%s}," % ",".join(str(v) for row in t.p for v in row))
    L.append("};")
    # parallax layers
    for name, img in (("far", far_layer()), ("mid0", mid_layer("hills")), ("mid1", mid_layer("bamboo")), ("mid2", mid_layer("pines"))):
        data = [v for row in img.p for v in row]
        L.append("static const uint8_t LAYER_%s[%d] = {%s};" % (name.upper(), len(data), ",".join(map(str, data))))
    L.append("static const int LAYER_W = %d, FAR_H = 112, MID_H = 96;" % LW)
    L.append("static const uint8_t* const LAYER_MID[3] = {LAYER_MID0, LAYER_MID1, LAYER_MID2};")
    # world colour tables
    L.append("struct WorldArt { const char* name; uint8_t sky[2][3]; uint8_t sun[3]; uint8_t far[7][3]; uint8_t mid[7][3]; uint8_t belt[2][3]; };")
    fmt = lambda c: "{%d, %d, %d}" % c
    rows = []
    for w in WORLDS:
        rows.append('  {"%s", {%s}, %s, {%s}, {%s}, {%s}}' % (w["name"], ", ".join(map(fmt, w["sky"])), fmt(w["sun"]),
                    ", ".join(map(fmt, w["far"])), ", ".join(map(fmt, w["mid"])), ", ".join(map(fmt, w["belt"]))))
    L.append("static const WorldArt WORLD_ART[3] = {\n%s\n};" % ",\n".join(rows))
    open(path, "w").write("\n".join(L) + "\n")
    print("wrote", path, "palette", len(PAL), "tiles", len(tiles))

if __name__ == "__main__":
    export(os.path.join(HERE, "..", "art.h"))
    if "--preview" in sys.argv:
        out = sys.argv[sys.argv.index("--preview") + 1]
        frames = [hero(p) for _, p in HERO_FRAMES] + hero_roll()
        sheet = Image.new("RGBA", (len(frames) * 26 * 6, 30 * 6), (90, 150, 210, 255))
        for i, f in enumerate(frames): sheet.alpha_composite(f.png(6), (i * 26 * 6, 6))
        sheet.save(os.path.join(out, "hero.png"))
