#!/usr/bin/env python3
"""Turbo Horizon art: the player's car (rear view, steering and brake frames), four rival
cars, roadside scenery for the three stages and the logo. Scenery is drawn big (it is
scaled down with distance at run time) and lit from the left.
   python3 games/TurboHorizon/tools/make_art.py [--preview DIR]   (from the repo root)"""
import math, os, random, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "art"))
from pixart import Palette, Img, Header, dither, logo, sheet, disc, fill_poly, FONT

pal = Palette()
K = pal.add((12, 10, 22))
WH = pal.add((255, 255, 255))
RED = pal.ramp((70, 0, 24), (255, 150, 150), 5, (226, 30, 52))
GLS = pal.ramp((14, 22, 44), (140, 190, 230), 4, (40, 70, 110))
GRY = pal.ramp((30, 30, 40), (232, 232, 242), 5, (110, 112, 128))
TL = [pal.add((120, 0, 16)), pal.add((255, 40, 50)), pal.add((255, 210, 190))]
BLU = pal.ramp((10, 20, 70), (150, 210, 255), 4, (40, 90, 210))
YEL = pal.ramp((90, 50, 0), (255, 246, 160), 4, (250, 196, 30))
GRN = pal.ramp((6, 40, 30), (150, 236, 140), 4, (40, 150, 80))
BRK = pal.ramp((50, 28, 20), (190, 130, 80), 3)                 # bark
ROCK = pal.ramp((60, 20, 16), (250, 170, 110), 4, (190, 80, 46))
MAG = [pal.add((150, 20, 110)), pal.add((255, 80, 200))]
CYA = [pal.add((20, 110, 150)), pal.add((110, 240, 255))]
BLD = pal.ramp((18, 14, 40), (80, 70, 130), 3)
WIN = [pal.add((255, 210, 110)), pal.add((140, 230, 255))]
GLOW = [pal.add((255, 236, 150)), pal.add((255, 255, 220))]
ORG = [pal.add((200, 80, 10)), pal.add((255, 150, 40))]

def shade_rows(img, x0, x1, y0, y1, ramp, light):
    for y in range(y0, y1):
        for x in range(x0, x1):
            img.set(x, y, dither(light(x, y), ramp, x, y))

# ------------------------------------------------------------------ cars (rear views)
def car(w, h, body, style, steer=0, brake=False):
    """style: 'sport' (player), 'coupe', 'van', 'sedan', 'pickup'. steer -2..2 shows the side."""
    img = Img(w, h)
    cx = w // 2
    side = steer * 2                     # how much side panel shows
    # tyres
    tw = w // 7; ty0 = h - h // 3
    for x0 in (2 + max(0, -side), w - 2 - tw - max(0, side)):
        img.rect(x0, ty0, tw, h - ty0, GRY[0])
        for y in range(ty0 + 1, h, 2): img.rect(x0 + 1, y, tw - 2, 1, K)
    # body block
    bt = int(h * (0.42 if style != "van" else 0.12)); bb = h - 3
    bl, br = 1, w - 2
    for y in range(bt, bb):
        t = (y - bt) / (bb - bt)
        inset = 2 if y in (bt, bb - 1) else (1 if y in (bt + 1, bb - 2) else 0)
        for x in range(bl + inset, br - inset + 1):
            v = 0.85 - t * 0.6 + (0.1 if x < cx else 0)
            if (steer > 0 and x > br - side * 2) or (steer < 0 and x < bl - side * 2): v -= 0.25   # side panel
            img.set(x, y, dither(v, body, x, y))
    # shoulder highlight
    img.rect(bl + 3, bt + 1, br - bl - 5, 1, body[-1])
    # cabin
    if style == "van":
        img.rect(bl + 6, bt + 3, (br - bl) // 2 - 7, int(h * 0.22), GLS[1])
        img.rect(cx + 1, bt + 3, (br - bl) // 2 - 7, int(h * 0.22), GLS[1])
        img.rect(cx, bt + 1, 1, bb - bt - 4, body[0])                # rear door split
        img.rect(bl + 7, bt + 4, 3, 2, GLS[3])
        img.rect(cx + 2, bt + 4, 3, 2, GLS[3])
    else:
        ct = 1 if style == "sport" else 2
        ch = bt - ct
        topw = int(w * (0.42 if style == "sport" else 0.5)); botw = int(w * (0.72 if style == "sport" else 0.74))
        if style == "pickup": topw, botw, ct = int(w * 0.5), int(w * 0.6), int(h * 0.12)
        off = -steer
        for y in range(ct, bt + 1):
            t = (y - ct) / max(1, bt - ct)
            hw = (topw + (botw - topw) * t) / 2
            for x in range(int(cx - hw + off * (1 - t)), int(cx + hw + off * (1 - t)) + 1):
                edge = y == ct or x <= int(cx - hw + off * (1 - t)) or x >= int(cx + hw + off * (1 - t))
                c = body[2] if edge else GLS[0 if (x - y) % 9 > 2 else 2]
                if not edge and y < ct + 2: c = body[3]
                img.set(x, y, c)
        if style == "sport":   # spoiler
            sy = bt - 2
            img.rect(2, sy, w - 4, 2, body[1]); img.rect(2, sy, w - 4, 1, body[3])
            img.rect(10, sy + 2, 2, 2, K); img.rect(w - 12, sy + 2, 2, 2, K)
    # rear panel with tail lights
    py = bt + (bb - bt) // 3 if style != "van" else bb - 9
    ph = max(3, (bb - bt) // 4) if style != "van" else 4
    img.rect(bl + 4, py, br - bl - 7, ph, K)
    lw = max(4, w // 6)
    for lx in (bl + 4, br - 3 - lw):
        img.rect(lx, py, lw, ph, TL[2] if brake else TL[1])
        img.rect(lx, py + ph - 1, lw, 1, TL[1] if brake else TL[0])
        if style == "sport":
            img.rect(lx + lw // 2, py, 1, ph, K)
    if brake: img.rect(cx - 4, max(0, (bt if style != 'sport' else bt - 3) - 1), 8, 1, TL[1])
    # plate and exhausts
    img.rect(cx - 5, py + ph + 1, 10, 3, WH); img.rect(cx - 4, py + ph + 2, 8, 1, GRY[2])
    if style == "sport":
        for ex in (cx - 13, cx + 10): img.rect(ex, bb - 2, 3, 2, GRY[3]); img.set(ex + 1, bb - 1, K)
    return img.outline(K)

PLAYER_W, PLAYER_H = 80, 38
def player_frames():
    out = []
    for brake in (False, True):
        for steer in (0, 1, 2):
            out.append(car(PLAYER_W, PLAYER_H, RED, "sport", steer, brake))
    return out

def rivals():
    return [car(70, 36, BLU, "coupe"), car(62, 52, YEL, "van"), car(70, 38, GRY[1:], "sedan"), car(72, 42, GRN, "pickup")]

# ------------------------------------------------------------------ scenery
def palm(seed):
    rng = random.Random(seed)
    w, h = 72, 136
    img = Img(w, h)
    # trunk: a gentle curve made of rings
    bx, top = 36 + rng.randint(-3, 3), 26
    lean = rng.choice([-1, 1]) * rng.uniform(8, 14)
    pts = []
    for i in range(h - top):
        t = i / (h - top)
        x = bx + lean * (1 - t) ** 2 - lean * 0.2
        y = h - 1 - i
        r = 3.6 - t * 1.4
        for xx in range(int(x - r), int(x + r) + 1):
            v = 0.75 - (xx - (x - r)) / (2 * r) * 0.6
            if (i % 6) < 1: v -= 0.3
            img.set(xx, y, dither(v, BRK, xx, y))
        pts.append((x, y))
    tx, tyy = pts[-1]
    # fronds
    for k in range(9):
        a = -math.pi / 2 + (k - 4) * 0.42 + rng.uniform(-0.1, 0.1)
        length = rng.uniform(34, 42)
        droop = rng.uniform(0.9, 1.4)
        for s in range(int(length)):
            t = s / length
            fx = tx + math.cos(a) * s
            fy = tyy + math.sin(a) * s * (1 - t) + droop * (s ** 2) / length * 0.9
            spread = 6 * math.sin(t * math.pi) + 1
            for d in range(-int(spread), int(spread) + 1):
                px, py = fx + d * -math.sin(a) * 0.5, fy + abs(d) * 0.55 + d * 0.35
                v = 0.8 - t * 0.35 - (0.25 if d > 0 else 0) + (0.15 if k < 4 else -0.1)
                img.set(px, py, dither(v, GRN, int(px), int(py)))
    # coconuts
    for d in (-2, 2): disc(img, tx + d, tyy + 3, 1.8, BRK[0])
    return img.outline(K)

def rock():
    w, h = 72, 40
    img = Img(w, h)
    rng = random.Random(4)
    prof = [int(14 + 10 * math.sin(x * 0.09) + 6 * math.sin(x * 0.23 + 1) + rng.random() * 2) for x in range(w)]
    for x in range(2, w - 2):
        ht = min(h - 2, prof[x] + (x * (w - x)) // 90)
        for y in range(h - ht, h):
            strata = ((y + int(3 * math.sin(x * 0.2))) // 5) % 2
            v = 0.8 - (x / w) * 0.5 - (y - (h - ht)) / ht * 0.25 - strata * 0.12
            img.set(x, y, dither(v, ROCK, x, y))
    return img.outline(K)

def mesa():
    w, h = 120, 64
    img = Img(w, h)
    for x in range(w):
        top = 8 + int(3 * math.sin(x * 0.3)) if 14 < x < w - 14 else 8 + abs(x - (14 if x <= 14 else w - 14)) * 3
        for y in range(top, h):
            strata = ((y + int(2 * math.sin(x * 0.15))) // 6) % 2
            v = 0.75 - (x / w) * 0.45 - strata * 0.14 + (0.15 if y < top + 2 else 0)
            img.set(x, y, dither(v, ROCK, x, y))
    return img.outline(K)

def cactus():
    w, h = 40, 72
    img = Img(w, h)
    def col(x0, y0, y1, r):
        for y in range(y0, y1):
            for x in range(int(x0 - r), int(x0 + r) + 1):
                v = 0.8 - (x - (x0 - r)) / (2 * r) * 0.7
                if (x - int(x0 - r)) % 3 == 1: v += 0.12   # ribs
                img.set(x, y, dither(v, GRN, x, y))
        disc(img, x0, y0, r, GRN[2])
        disc(img, x0 - 1, y0 - 0.5, r - 1.5, GRN[3])
    col(20, 6, h, 5.5)
    col(8, 22, 44, 3.5); img.rect(8, 40, 10, 5, GRN[1])
    col(32, 30, 50, 3.5); img.rect(24, 46, 9, 5, GRN[1])
    for _ in range(10):
        x, y = random.randint(14, 26), random.randint(8, 70); img.set(x, y, YEL[3])
    return img.outline(K)

def text_into(img, s, x, y, c, scale=1):
    for i, ch in enumerate(s):
        for gy, row in enumerate(FONT[ch]):
            for gx, bit in enumerate(row):
                if bit == "1": img.rect(x + (i * 6 + gx) * scale, y + gy * scale, scale, scale, c)

def billboard(kind):
    w, h = 96, 76
    img = Img(w, h)
    for px in (18, 74):
        img.rect(px, 40, 5, h - 40, GRY[1]); img.rect(px, 40, 2, h - 40, GRY[3])
    img.rect(4, 4, w - 8, 42, K)
    for y in range(6, 44):
        for x in range(6, w - 6):
            t = (y - 6) / 38
            if kind == 0: c = dither(1 - t, [MAG[0], MAG[1], ORG[1], YEL[3]], x, y)
            elif kind == 1: c = dither(0.2 + t * 0.6, [BLU[0], BLU[1], CYA[0], CYA[1]], x, y)
            else: c = dither(0.9 - t * 0.7, [GRN[0], GRN[1], GRN[2], YEL[2]], x, y)
            img.set(x, y, c)
    word = ["NOVA", "TURBO", "ARCADE"][kind]
    sc = 3 if len(word) <= 4 else 2
    tw = len(word) * 6 * sc - sc
    text_into(img, word, (w - tw) // 2 + 1, 13 + (3 if sc == 2 else 0) + 1, K, sc)
    text_into(img, word, (w - tw) // 2, 13 + (3 if sc == 2 else 0), WH, sc)
    img.rect(6, 38, w - 12, 2, WH if kind != 1 else YEL[3])
    for x in range(10, w - 10, 12): img.rect(x, 2, 2, 3, GRY[2])   # lamps on top
    return img.outline(K)

def lamp():
    w, h = 40, 120
    img = Img(w, h)
    img.rect(4, 14, 4, h - 14, GRY[1]); img.rect(4, 14, 2, h - 14, GRY[3])
    img.rect(2, h - 6, 8, 6, GRY[0])
    for x in range(6, 32): img.set(x, 12 - int(4 * math.sin((x - 6) / 26 * math.pi / 2)), GRY[2]); img.set(x, 13 - int(4 * math.sin((x - 6) / 26 * math.pi / 2)), GRY[1])
    img.rect(26, 8, 12, 4, GRY[1]); img.rect(27, 12, 10, 2, GLOW[1])
    for y in range(14, 20):
        for x in range(25 - (y - 14), 39 + (y - 14)):
            if (x + y) % 2 == 0: img.set(x, y, GLOW[0])
    return img.outline(K)

def tower(seed):
    rng = random.Random(seed)
    w, h = 64, 150
    img = Img(w, h)
    top = 20 + rng.randint(0, 20)
    for y in range(top, h):
        for x in range(4, w - 4):
            img.set(x, y, BLD[2] if x < 8 else BLD[1] if x < w - 14 else BLD[0])
    for y in range(top + 6, h - 6, 6):
        for x in range(10, w - 10, 6):
            if rng.random() < 0.6: img.rect(x, y, 3, 3, WIN[0 if rng.random() < 0.7 else 1])
    nc = MAG if seed % 2 else CYA
    img.rect(4, top, w - 8, 3, nc[1]); img.rect(4, top + 3, w - 8, 1, nc[0])
    img.rect(w // 2 - 1, top - 14, 2, 14, GRY[2]); img.set(w // 2, top - 15, TL[1])
    return img.outline(K)

def chevron():
    w, h = 32, 44
    img = Img(w, h)
    img.rect(14, 24, 4, h - 24, GRY[1])
    img.rect(2, 2, w - 4, 22, YEL[2])
    for i in range(3):
        for y in range(4, 22):
            x = 6 + i * 8 + (abs(y - 13) * 6) // 9
            img.rect(x, y, 3, 1, K)
    return img.outline(K)

def bush():
    w, h = 48, 26
    img = Img(w, h)
    for (cx, cy, r) in ((12, 16, 9), (24, 12, 11), (36, 16, 9), (20, 19, 7), (30, 19, 7)):
        for y in range(h):
            for x in range(w):
                d = math.hypot(x - cx, y - cy)
                if d <= r:
                    v = 0.75 - (x - cx + y - cy) / (2 * r) * 0.5 + (0.1 if (x * 3 + y * 5) % 7 == 0 else 0)
                    img.set(x, y, dither(v, GRN, x, y))
    return img.outline(K)

def banner(word):
    w, h = 176, 26
    img = Img(w, h)
    for y in range(h):
        for x in range(w):
            img.set(x, y, WH if ((x // 8 + y // 8) % 2 == 0 and (y < 4 or y >= h - 4)) else (K if y < 4 or y >= h - 4 else RED[2] if y < 13 else RED[1]))
    tw = len(word) * 12 - 2
    text_into(img, word, (w - tw) // 2 + 1, 7, K, 2)
    text_into(img, word, (w - tw) // 2, 6, WH, 2)
    return img

def post():
    img = Img(10, 90)
    img.rect(1, 0, 8, 90, GRY[1]); img.rect(1, 0, 3, 90, GRY[3]); img.rect(7, 0, 2, 90, GRY[0])
    for y in range(0, 90, 10): img.rect(1, y, 8, 1, GRY[0])
    return img

def build_logo():
    grad = [WH, YEL[3], YEL[2], ORG[1], ORG[0], MAG[1], MAG[0]]
    return logo("TURBO HORIZON", 3, grad, BLD[0], K, shine=WH, italic=0.12)

SCENERY = [("palm0", lambda: palm(1)), ("palm1", lambda: palm(7)), ("rock", rock), ("mesa", mesa), ("cactus", cactus),
           ("bill0", lambda: billboard(0)), ("bill1", lambda: billboard(1)), ("bill2", lambda: billboard(2)),
           ("lamp", lamp), ("tower0", lambda: tower(1)), ("tower1", lambda: tower(2)), ("chevron", chevron),
           ("bush", bush), ("bannerc", lambda: banner("CHECKPOINT")), ("banners", lambda: banner("START")),
           ("post", post)]

def export(path):
    h = Header("games/TurboHorizon/tools/make_art.py", "TH", pal)
    h.group("pcar", player_frames())
    h.group("rival", rivals())
    for name, fn in SCENERY: h.sprite(name, fn())
    h.line("static const Sprite* const SCENERY[%d] = {%s};" % (len(SCENERY), ", ".join("&SPR_%s" % n.upper() for n, _ in SCENERY)))
    h.line("enum { %s, SC_COUNT };" % ", ".join("SC_%s" % n.upper() for n, _ in SCENERY))
    h.sprite("logo", build_logo())
    h.write(path)

if __name__ == "__main__":
    export(os.path.join(HERE, "..", "art.h"))
    if "--preview" in sys.argv:
        out = sys.argv[sys.argv.index("--preview") + 1]
        sheet(player_frames() + rivals(), pal, 3, cols=5, bg=(90, 90, 100, 255)).save(os.path.join(out, "th_cars.png"))
        sheet([fn() for _, fn in SCENERY], pal, 2, cols=6, bg=(120, 160, 210, 255)).save(os.path.join(out, "th_scenery.png"))
        build_logo().png(pal, 2, (20, 14, 40, 255)).save(os.path.join(out, "th_logo.png"))
