#!/usr/bin/env python3
"""Shared helpers for the game art scripts (Gem Cascade, City Shield, Turbo Horizon):
an indexed canvas, a palette builder, dithered shading, a chunky logo font and the
art.h writer. Each game's tools/make_art.py imports this module."""
import math
from PIL import Image

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def rgb565(r, g, b): return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


class Palette:
    """Index 0 is transparent. The engine holds at most 64 colours."""
    def __init__(self):
        self.rgb = [(0, 0, 0)]
    def add(self, rgb):
        self.rgb.append(tuple(int(max(0, min(255, v))) for v in rgb))
        assert len(self.rgb) <= 64, "palette has more than 64 colours"
        return len(self.rgb) - 1
    def ramp(self, dark, light, n, mid=None):
        """n colours from dark to light (optionally through mid). Returns their indices."""
        out = []
        for i in range(n):
            t = i / (n - 1)
            if mid is None: c = [dark[k] + (light[k] - dark[k]) * t for k in range(3)]
            elif t < 0.5: c = [dark[k] + (mid[k] - dark[k]) * t * 2 for k in range(3)]
            else: c = [mid[k] + (light[k] - mid[k]) * (t - 0.5) * 2 for k in range(3)]
            out.append(self.add(c))
        return out


class Img:
    """A tiny indexed canvas (0 = transparent)."""
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]
    def get(self, x, y):
        return self.p[y][x] if 0 <= x < self.w and 0 <= y < self.h else 0
    def set(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h: self.p[y][x] = c
    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w): self.set(xx, yy, c)
    def stamp(self, rows, ox, oy, key, flip=False):
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch in key and key[ch]:
                    self.set(ox + (len(row) - 1 - x if flip else x), oy + y, key[ch])
    def blit(self, other, ox, oy):
        for y in range(other.h):
            for x in range(other.w):
                if other.p[y][x]: self.set(ox + x, oy + y, other.p[y][x])
    def copy(self):
        o = Img(self.w, self.h); o.p = [r[:] for r in self.p]; return o
    def flipped(self):
        o = Img(self.w, self.h); o.p = [r[::-1] for r in self.p]; return o
    def remap(self, table):
        o = self.copy()
        o.p = [[table.get(v, v) for v in r] for r in o.p]
        return o
    def outline(self, c, diag=False):
        add = []
        nb = [(1, 0), (-1, 0), (0, 1), (0, -1)] + ([(1, 1), (-1, -1), (1, -1), (-1, 1)] if diag else [])
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x]: continue
                if any(self.get(x + dx, y + dy) not in (0, c) for dx, dy in nb): add.append((x, y))
        for x, y in add: self.p[y][x] = c
        return self
    def scaled(self, w, h):
        """Nearest-neighbour resize (used for distance LODs)."""
        o = Img(w, h)
        for y in range(h):
            for x in range(w):
                o.p[y][x] = self.p[min(self.h - 1, int((y + 0.5) * self.h / h))][min(self.w - 1, int((x + 0.5) * self.w / w))]
        return o
    def png(self, pal, scale=1, bg=None):
        im = Image.new("RGBA", (self.w, self.h), bg or (0, 0, 0, 0))
        px = im.load()
        for y in range(self.h):
            for x in range(self.w):
                v = self.p[y][x]
                if v: px[x, y] = (*pal.rgb[v], 255)
        return im.resize((self.w * scale, self.h * scale), Image.NEAREST) if scale > 1 else im


def from_rows(rows, key, outline=None, diag=False):
    pad = 1 if outline else 0
    img = Img(max(len(r) for r in rows) + 2 * pad, len(rows) + 2 * pad)
    img.stamp(rows, pad, pad, key)
    return img.outline(outline, diag) if outline else img


def disc(img, cx, cy, r, c):
    for y in range(int(cy - r - 1), int(cy + r + 2)):
        for x in range(int(cx - r - 1), int(cx + r + 2)):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r: img.set(x, y, c)


def dither(v, ramp, x, y):
    """v in 0..1 -> a ramp colour, Bayer-dithered between neighbouring steps."""
    f = max(0.0, min(0.999, v)) * (len(ramp) - 1)
    i = int(f)
    if (f - i) * 16 > BAYER[y & 3][x & 3] and i + 1 < len(ramp): i += 1
    return ramp[i]


def in_poly(px, py, pts):
    inside = False
    j = len(pts) - 1
    for i in range(len(pts)):
        xi, yi = pts[i]; xj, yj = pts[j]
        if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi + 1e-9) + xi: inside = not inside
        j = i
    return inside


def fill_poly(img, pts, c):
    ys = [p[1] for p in pts]
    for y in range(int(min(ys)), int(max(ys)) + 1):
        for x in range(img.w):
            if in_poly(x + 0.5, y + 0.5, pts): img.set(x, y, c)


# ------------------------------------------------------------------ chunky 5x7 logo font
FONT = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01111", "10000", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00001", "00001", "00001", "10001", "10001", "01110"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "01010", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "11011", "10001"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    " ": ["00000"] * 7,
}


def logo(text, s, grad, shadow, outline, shine=None, italic=0):
    """Big gradient letters with a drop shadow. grad: 7 colours, top to bottom.
    italic: pixels of slant per glyph row (0 = upright)."""
    slant = int(italic * 7 * s)
    w = len(text) * 6 * s + 6 + slant; h = 7 * s + 6
    img = Img(w, h)
    for pass_ in (0, 1):
        ox, oy = (4, 4) if pass_ == 0 else (1, 1)
        for i, ch in enumerate(text):
            for gy, row in enumerate(FONT[ch]):
                for gx, bit in enumerate(row):
                    if bit != "1": continue
                    for yy in range(s):
                        for xx in range(s):
                            py = gy * s + yy
                            x0 = ox + (i * 6 + gx) * s + xx + int(italic * (7 * s - py)); y0 = oy + py
                            if pass_ == 0: img.set(x0, y0, shadow)
                            else:
                                c = grad[min(6, py * 7 // (7 * s))]
                                if shine and yy == 0 and (gy == 0 or FONT[ch][gy - 1][gx] != "1"): c = shine
                                img.set(x0, y0, c)
    return img.outline(outline)


# ------------------------------------------------------------------ art.h writer
class Header:
    def __init__(self, script, prefix, pal):
        self.prefix, self.pal = prefix, pal
        self.L = ["// AUTO-GENERATED by %s - edit that script, not this file" % script, "#pragma once",
                  "#include <stdint.h>", "#ifndef ARCADE_SPRITE_T", "#define ARCADE_SPRITE_T",
                  "struct Sprite { uint16_t w, h; const uint8_t* px; };", "#endif",
                  "static const uint16_t %s_PAL565[%d] = {%s};" % (prefix, len(pal.rgb), ", ".join("0x%04X" % rgb565(*c) for c in pal.rgb)),
                  "static const int %s_PAL_N = %d;" % (prefix, len(pal.rgb))]
    def line(self, s): self.L.append(s)
    def sprite(self, name, img):
        data = [v for row in img.p for v in row]
        self.L.append("static const uint8_t %s_px[%d] = {%s};" % (name, len(data), ",".join(map(str, data))))
        self.L.append("static const Sprite SPR_%s = {%d, %d, %s_px};" % (name.upper(), img.w, img.h, name))
    def group(self, name, imgs):
        for i, im in enumerate(imgs): self.sprite("%s%d" % (name, i), im)
        self.L.append("static const Sprite* const %s[%d] = {%s};" % (name.upper(), len(imgs), ", ".join("&SPR_%s%d" % (name.upper(), i) for i in range(len(imgs)))))
    def bytes(self, name, data):
        self.L.append("static const uint8_t %s[%d] = {%s};" % (name, len(data), ",".join(map(str, data))))
    def write(self, path):
        open(path, "w").write("\n".join(self.L) + "\n")
        print("wrote", path, "palette", len(self.pal.rgb))


def sheet(items, pal, scale=4, bg=(40, 40, 60, 255), cols=8):
    """Contact sheet of Img objects for previews."""
    cw = max(i.w for i in items) * scale + 4; ch = max(i.h for i in items) * scale + 4
    rows = (len(items) + cols - 1) // cols
    im = Image.new("RGBA", (cw * min(cols, len(items)), ch * rows), bg)
    for k, it in enumerate(items):
        im.alpha_composite(it.png(pal, scale), ((k % cols) * cw + 2, (k // cols) * ch + 2))
    return im
