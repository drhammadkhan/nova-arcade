#!/usr/bin/env python3
"""
Generates ../assets.h (palette-indexed pixel art) for Nova Lance.
All art is drawn procedurally here so you can tweak shapes/colours and re-run:

    pip install pillow
    python3 make_assets.py            # writes ../assets.h and preview.png
"""
from PIL import Image, ImageDraw
import os

# ---------------------------------------------------------------- palette
# index 0 is always transparent
PAL = [
    (0, 0, 0),          # 0  transparent
    (13, 11, 30),       # 1  outline
    (27, 31, 74),       # 2  navy
    (51, 48, 122),      # 3  indigo
    (31, 111, 139),     # 4  teal
    (62, 198, 224),     # 5  cyan
    (184, 243, 255),    # 6  ice
    (253, 253, 248),    # 7  white
    (61, 74, 102),      # 8  steel dark
    (115, 132, 168),    # 9  steel
    (180, 194, 220),    # 10 steel light
    (138, 21, 56),      # 11 red dark
    (232, 56, 79),      # 12 red
    (255, 138, 61),     # 13 orange
    (255, 216, 74),     # 14 yellow
    (194, 58, 214),     # 15 magenta
    (255, 123, 213),    # 16 pink
    (30, 107, 58),      # 17 green dark
    (79, 214, 107),     # 18 green
    (182, 255, 110),    # 19 lime
    (74, 29, 107),      # 20 purple dark
    (122, 61, 184),     # 21 purple
    (224, 180, 138),    # 22 sand
    (122, 74, 58),      # 23 brown
    (255, 208, 176),    # 24 peach
]
T, OUT, NAVY, INDIGO, TEAL, CYAN, ICE, WHITE = 0, 1, 2, 3, 4, 5, 6, 7
STD, STL, STLL = 8, 9, 10
REDD, RED, ORANGE, YELLOW, MAG, PINK = 11, 12, 13, 14, 15, 16
GRND, GRN, LIME, PURD, PUR, SAND, BROWN, PEACH = 17, 18, 19, 20, 21, 22, 23, 24

BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def canvas(w, h):
    im = Image.new("P", (w, h), 0)
    im.putpalette([c for rgb in PAL for c in rgb] + [0] * (768 - 3 * len(PAL)))
    return im, ImageDraw.Draw(im)


def outline(im, colour=OUT, diagonal=False):
    """Add a 1px dark outline around every opaque shape (the classic sprite look)."""
    w, h = im.size
    px = im.load()
    add = []
    for y in range(h):
        for x in range(w):
            if px[x, y] != 0:
                continue
            nb = [(1, 0), (-1, 0), (0, 1), (0, -1)]
            if diagonal:
                nb += [(1, 1), (-1, -1), (1, -1), (-1, 1)]
            for dx, dy in nb:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and px[nx, ny] not in (0, colour):
                    add.append((x, y))
                    break
    for x, y in add:
        px[x, y] = colour
    return im


def dither_shade(im, cx, cy, r, ramp, lx=-0.6, ly=-0.7, mask=None):
    """Shade opaque pixels of a disc with a lit sphere look, Bayer dithered across a colour ramp."""
    w, h = im.size
    px = im.load()
    import math
    for y in range(h):
        for x in range(w):
            if px[x, y] == 0 or (mask and px[x, y] not in mask):
                continue
            dx, dy = (x - cx) / r, (y - cy) / r
            d2 = dx * dx + dy * dy
            if d2 > 1:
                continue
            dz = math.sqrt(1 - d2)
            lz = math.sqrt(max(0, 1 - lx * lx - ly * ly))
            lum = max(0.0, dx * lx + dy * ly + dz * lz)
            v = lum * (len(ramp) - 1)
            i = int(v)
            frac = v - i
            if frac * 16 > BAYER4[y & 3][x & 3] and i + 1 < len(ramp):
                i += 1
            px[x, y] = ramp[min(i, len(ramp) - 1)]


# ---------------------------------------------------------------- sprites
def player():
    im, d = canvas(32, 16)
    # wings (swept back)
    d.polygon([(8, 7), (11, 1), (15, 1), (19, 7)], fill=STL)
    d.polygon([(8, 8), (11, 14), (15, 14), (19, 8)], fill=STD)
    d.line([(11, 1), (15, 1)], fill=STLL)
    d.line([(12, 2), (14, 2)], fill=RED)
    d.line([(12, 13), (14, 13)], fill=REDD)
    # fuselage
    d.polygon([(3, 5), (21, 5), (30, 7), (30, 8), (21, 10), (3, 10)], fill=STL)
    d.line([(4, 5), (21, 5)], fill=STLL)
    d.line([(22, 6), (28, 7)], fill=STLL)
    d.line([(3, 9), (21, 9)], fill=STD)
    d.line([(3, 10), (21, 10)], fill=STD)
    d.line([(22, 9), (29, 8)], fill=STD)
    # stripe + engine block
    d.line([(6, 7), (17, 7)], fill=CYAN)
    d.rectangle([(1, 6), (4, 9)], fill=STD)
    d.line([(1, 6), (4, 6)], fill=STL)
    # canopy
    d.ellipse([(17, 4), (25, 8)], fill=TEAL)
    d.line([(19, 5), (22, 5)], fill=CYAN)
    d.point((20, 5), fill=ICE)
    d.point((21, 5), fill=WHITE)
    d.point((30, 7), fill=WHITE)
    return outline(im)


def flame(frame):
    im, d = canvas(10, 6)
    L = 9 if frame == 0 else 7
    d.ellipse([(10 - L, 1), (10 + 2, 4)], fill=RED)
    d.ellipse([(10 - L + 2, 1), (10 + 2, 4)], fill=ORANGE)
    d.ellipse([(10 - L + 4, 2), (10 + 2, 3)], fill=YELLOW)
    d.point((9, 2), fill=WHITE)
    d.point((9, 3), fill=WHITE)
    return im


def life_icon():
    im, d = canvas(11, 7)
    d.polygon([(1, 2), (7, 2), (10, 3), (7, 4), (1, 4)], fill=STL)
    d.polygon([(3, 2), (4, 0), (6, 0), (7, 2)], fill=STL)
    d.polygon([(3, 4), (4, 6), (6, 6), (7, 4)], fill=STD)
    d.point((7, 3), fill=CYAN)
    return outline(im)


def drone(frame):
    im, d = canvas(18, 18)
    d.polygon([(1, 9), (5, 6), (5, 12)], fill=MAG)      # side fins
    d.polygon([(16, 9), (12, 6), (12, 12)], fill=MAG)
    d.ellipse([(3, 2), (14, 15)], fill=PUR)
    dither_shade(im, 8.5, 8.5, 6.5, [PURD, PUR, MAG, PINK], mask=[PUR])
    eye = YELLOW if frame == 0 else ORANGE
    d.ellipse([(6, 6), (11, 11)], fill=RED)
    d.ellipse([(7, 7), (10, 10)], fill=eye)
    d.point((8, 7), fill=WHITE)
    return outline(im)


def dart():
    im, d = canvas(22, 13)
    d.polygon([(1, 6), (14, 1), (20, 2), (15, 6), (20, 10), (14, 11)], fill=RED)
    d.polygon([(1, 6), (15, 6), (20, 10), (14, 11)], fill=REDD)
    d.line([(3, 5), (14, 1)], fill=ORANGE)
    d.line([(14, 1), (19, 2)], fill=ORANGE)
    d.ellipse([(6, 4), (10, 7)], fill=YELLOW)
    d.point((7, 5), fill=WHITE)
    d.line([(19, 5), (21, 6)], fill=ORANGE)   # tiny thruster
    d.point((21, 6), fill=YELLOW)
    return outline(im)


def pod(frame):
    im, d = canvas(22, 22)
    d.rectangle([(0, 9), (7, 12)], fill=STD)           # cannon barrel
    d.line([(0, 9), (7, 9)], fill=STL)
    d.polygon([(6, 3), (15, 1), (21, 6), (21, 15), (15, 20), (6, 18), (4, 11)], fill=GRN)
    d.polygon([(4, 11), (21, 11), (21, 15), (15, 20), (6, 18)], fill=GRND)
    d.line([(6, 3), (15, 1)], fill=LIME)
    d.line([(15, 1), (20, 5)], fill=LIME)
    core = LIME if frame == 0 else WHITE
    d.ellipse([(10, 7), (16, 14)], fill=GRND)
    d.ellipse([(11, 8), (15, 13)], fill=core)
    d.point((12, 9), fill=WHITE)
    for x in (8, 18):
        d.point((x, 5), fill=GRND)
        d.point((x, 16), fill=OUT)
    return outline(im)


def boss(frame):
    im, d = canvas(84, 62)
    # rear engines
    for y in (14, 42):
        d.rectangle([(66, y), (82, y + 6)], fill=STD)
        d.line([(66, y), (82, y)], fill=STL)
        d.rectangle([(80, y + 1), (82, y + 5)], fill=ORANGE if frame == 0 else YELLOW)
    # hull
    d.ellipse([(4, 12), (78, 52)], fill=STL)
    dither_shade(im, 41, 26, 38, [STD, STL, STLL], lx=-0.4, ly=-0.8, mask=[STL])
    # panel seams
    for x in range(20, 72, 11):
        d.line([(x, 16), (x, 48)], fill=STD)
    d.line([(10, 32), (76, 32)], fill=STD)
    # dome
    d.ellipse([(26, 2), (58, 26)], fill=PUR)
    dither_shade(im, 42, 14, 16, [PURD, PUR, MAG, PINK, WHITE], mask=[PUR])
    d.line([(28, 22), (56, 22)], fill=OUT)
    # jaw + weak-point core
    d.polygon([(2, 26), (16, 20), (22, 32), (16, 44), (2, 38)], fill=STD)
    d.line([(2, 26), (16, 20)], fill=STL)
    c1, c2 = (RED, YELLOW) if frame == 0 else (ORANGE, WHITE)
    d.ellipse([(7, 25), (21, 39)], fill=REDD)
    d.ellipse([(9, 27), (19, 37)], fill=c1)
    d.ellipse([(12, 30), (16, 34)], fill=c2)
    # gun ports
    for (x, y) in ((30, 46), (46, 48), (30, 16), (62, 44)):
        d.rectangle([(x, y), (x + 4, y + 2)], fill=OUT)
        d.point((x + 1, y + 1), fill=RED)
    # hazard stripe
    for i in range(0, 20, 4):
        d.line([(34 + i, 38), (36 + i, 36)], fill=YELLOW)
    return outline(im)


def pbullet():
    im, d = canvas(12, 5)
    d.ellipse([(0, 0), (11, 4)], fill=TEAL)
    d.ellipse([(2, 1), (11, 3)], fill=CYAN)
    d.line([(5, 2), (10, 2)], fill=WHITE)
    d.point((4, 2), fill=ICE)
    return im


def pbullet_spread():
    im, d = canvas(8, 8)
    d.ellipse([(0, 0), (7, 7)], fill=TEAL)
    d.ellipse([(1, 1), (6, 6)], fill=CYAN)
    d.ellipse([(2, 2), (5, 5)], fill=WHITE)
    return im


def ebullet(frame):
    im, d = canvas(7, 7)
    a, b = (MAG, PINK) if frame == 0 else (RED, ORANGE)
    d.ellipse([(0, 0), (6, 6)], fill=a)
    d.ellipse([(1, 1), (5, 5)], fill=b)
    d.ellipse([(2, 2), (4, 4)], fill=WHITE)
    return im


GLYPH = {  # 3x5 font for pickups
    "P": ["111", "101", "111", "100", "100"],
    "S": ["111", "100", "111", "001", "111"],
    "B": ["110", "101", "110", "101", "110"],
}


def pickup(letter, body, dark):
    im, d = canvas(13, 13)
    d.rounded_rectangle([(0, 0), (12, 12)], radius=3, fill=dark)
    d.rounded_rectangle([(1, 1), (11, 11)], radius=3, fill=body)
    d.line([(3, 1), (9, 1)], fill=WHITE)
    for y, row in enumerate(GLYPH[letter]):
        for x, ch in enumerate(row):
            if ch == "1":
                d.rectangle([(4 + x * 2 - 1, 3 + y * 1.5), (4 + x * 2, 3 + y * 1.5 + 1)], fill=WHITE)
    return outline(im)


def planet():
    import math
    im, d = canvas(84, 60)
    cx, cy, r = 42, 30, 22
    px = im.load()
    # back half of ring
    def ring(front):
        for t in range(0, 720):
            a = t * math.pi / 360
            for rr, col in ((38, SAND), (36, BROWN), (34, SAND)):
                x = cx + rr * math.cos(a)
                y = cy + rr * 0.28 * math.sin(a)
                if (math.sin(a) > 0) == front:
                    ix, iy = int(round(x)), int(round(y))
                    if 0 <= ix < 84 and 0 <= iy < 60:
                        px[ix, iy] = col
    ring(False)
    d.ellipse([(cx - r, cy - r), (cx + r, cy + r)], fill=TEAL)
    dither_shade(im, cx, cy, r + 0.5, [NAVY, INDIGO, TEAL, CYAN, ICE], lx=-0.7, ly=-0.5, mask=[TEAL])
    # cloud bands
    for y in range(cy - r, cy + r):
        band = int((y - (cy - r)) / 5) % 3 == 1
        for x in range(cx - r, cx + r + 1):
            if band and px[x, y] in (TEAL, CYAN) and BAYER4[y & 3][x & 3] < 7:
                px[x, y] = INDIGO if px[x, y] == TEAL else TEAL
    ring(True)
    return im


# 5x7 font for the logo
FONT5 = {
    "N": ["10001", "11001", "10101", "10101", "10011", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "01010", "01010", "00100"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    " ": ["00000"] * 7,
}


def logo(text="NOVA LANCE", s=4):
    w = len(text) * 6 * s + 4
    h = 7 * s + 6
    im, d = canvas(w, h)
    grad = [WHITE, YELLOW, YELLOW, ORANGE, ORANGE, RED, MAG]  # per font row
    px = im.load()
    # drop shadow first
    for pass_ in (0, 1):
        ox, oy = (3, 3) if pass_ == 0 else (1, 1)
        for i, ch in enumerate(text):
            g = FONT5[ch]
            for gy, row in enumerate(g):
                for gx, bit in enumerate(row):
                    if bit != "1":
                        continue
                    x0 = ox + (i * 6 + gx) * s
                    y0 = oy + gy * s
                    for yy in range(s):
                        for xx in range(s):
                            if pass_ == 0:
                                px[x0 + xx, y0 + yy] = PURD
                            else:
                                c = grad[gy]
                                # bevel: top-left highlight, bottom-right darker
                                if yy == 0 and gy == 0:
                                    c = WHITE
                                elif yy == s - 1 and (gy == 6 or g[gy + 1][gx] != "1"):
                                    c = REDD if gy >= 4 else ORANGE
                                # scanline sparkle
                                if yy == 1 and xx == 1 and gy in (1, 2):
                                    c = WHITE
                                px[x0 + xx, y0 + yy] = c
    return outline(im)


SPRITES = [
    ("player", player()),
    ("flame0", flame(0)),
    ("flame1", flame(1)),
    ("lifeicon", life_icon()),
    ("drone0", drone(0)),
    ("drone1", drone(1)),
    ("dart", dart()),
    ("pod0", pod(0)),
    ("pod1", pod(1)),
    ("boss0", boss(0)),
    ("boss1", boss(1)),
    ("pbullet", pbullet()),
    ("pspread", pbullet_spread()),
    ("ebullet0", ebullet(0)),
    ("ebullet1", ebullet(1)),
    ("pickP", pickup("P", ORANGE, REDD)),
    ("pickS", pickup("S", CYAN, TEAL)),
    ("pickB", pickup("B", MAG, PURD)),
    ("planet", planet()),
    ("logo", logo()),
]


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = []
    out.append("// AUTO-GENERATED by tools/make_assets.py - edit that script, not this file.")
    out.append("#pragma once\n#include <stdint.h>\n")
    out.append("#ifndef ARCADE_SPRITE_T\n#define ARCADE_SPRITE_T\nstruct Sprite { uint16_t w, h; const uint8_t* px; };\n#endif\n")
    out.append("#define PAL_SIZE %d" % len(PAL))
    out.append("static const uint16_t PAL565[PAL_SIZE] = {" +
               ", ".join("0x%04X" % rgb565(*c) for c in PAL) + "};")
    names = [n.upper() for n, _ in SPRITES]
    out.append("enum PalIdx { " + ", ".join(
        "P_%d = %d" % (i, i) for i in range(len(PAL))) + " };\n")
    for name, im in SPRITES:
        w, h = im.size
        data = list(im.get_flattened_data()) if hasattr(im, "get_flattened_data") else list(im.getdata())
        out.append("static const uint8_t %s_px[%d] = {" % (name, w * h))
        for y in range(h):
            out.append("  " + ",".join(str(v) for v in data[y * w:(y + 1) * w]) + ",")
        out.append("};")
        out.append("static const Sprite SPR_%s = { %d, %d, %s_px };\n" % (name.upper(), w, h, name))
    with open(os.path.join(here, "..", "assets.h"), "w") as f:
        f.write("\n".join(out) + "\n")

    # preview sheet (4x zoom) so you can eyeball the art
    zoom = 4
    pad = 6
    W = 700
    x = y = pad
    rowh = 0
    placed = []
    for name, im in SPRITES:
        w, h = im.size[0] * zoom, im.size[1] * zoom
        if x + w > W:
            x = pad
            y += rowh + pad
            rowh = 0
        placed.append((im, x, y))
        x += w + pad
        rowh = max(rowh, h)
    sheet = Image.new("RGB", (W, y + rowh + pad), (40, 40, 56))
    for im, x, y in placed:
        rgba = im.convert("RGBA")
        px = rgba.load()
        src = im.load()
        for yy in range(im.size[1]):
            for xx in range(im.size[0]):
                if src[xx, yy] == 0:
                    px[xx, yy] = (0, 0, 0, 0)
        big = rgba.resize((im.size[0] * zoom, im.size[1] * zoom), Image.NEAREST)
        sheet.paste(big, (x, y), big)
    sheet.save(os.path.join(here, "preview.png"))
    print("wrote assets.h and preview.png")


if __name__ == "__main__":
    main()
