#!/usr/bin/env python3
"""Pixel Peaks levels, built with a tiny layout language and written to levels.h.
   python3 games/PixelPeaks/tools/make_levels.py   (from the repo root)

Map characters (15 rows, 16 px tiles, row 14 is the bottom of the screen):
  .  air            #  ground (auto-shaped)   X  stone block     =  wooden plank (jump through from below)
  ^  spikes         ~  water (deadly)          ?  lucky box: coin  !  lucky box: heart
  o  coin           S  secret scroll (3 per level)
  b  mochi blob     c  crow (swoops)           k  spiky chestnut (only a roll beats it)
  M  moving plank, 3 wide, slides 4 tiles right and back
  P  start          C  checkpoint lantern      G  goal gate
  t  tree (per world)  l  stone lantern  u  bush  v  flowers  r  rock      (scenery)

Reach: a full jump rises 68 px (4.3 tiles) and carries 5.6 tiles. Levels keep a margin: climbs of at
most 3 tiles, gaps of at most 4, and nothing to collect more than 4 rows above where she can stand.
check_reach() enforces this for every coin, scroll, lucky box, plank and the goal."""
import os

ROWS = 15

class Lvl:
    def __init__(self, w, world, name):
        self.w, self.world, self.name = w, world, name
        self.g = [["."] * w for _ in range(ROWS)]

    def put(self, x, y, ch): self.g[y][x] = ch
    def ground(self, x0, x1, top=12):
        for x in range(x0, x1 + 1):
            for y in range(top, ROWS): self.g[y][x] = "#"
    def fill(self, x0, x1, y0, y1, ch):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): self.g[y][x] = ch
    def row(self, x0, x1, y, ch): self.fill(x0, x1, y, y, ch)
    def surface(self, x):
        for y in range(ROWS):
            if self.g[y][x] in "#X^": return y
        raise ValueError("no ground at x=%d in %s" % (x, self.name))
    def on(self, x, ch):
        """Stand something on the ground at column x."""
        self.g[self.surface(x) - 1][x] = ch
    def coins(self, x0, x1, y):
        for x in range(x0, x1 + 1): self.g[y][x] = "o"
    def arc(self, x0, n, y):
        for i in range(n):
            self.g[y - (1 if 0 < i < n - 1 else 0) - (1 if n > 4 and 1 < i < n - 2 else 0)][x0 + i] = "o"

    def check(self):
        flat = "".join("".join(r) for r in self.g)
        assert flat.count("P") == 1 and flat.count("G") == 1, self.name
        assert flat.count("S") == 3, "%s has %d scrolls" % (self.name, flat.count("S"))
        self.check_reach()
        return self

    def check_reach(self):
        """Flood from the start over the places she can stand, using her jump reach, then make sure
        every scroll and the goal can be touched. Conservative: ignores ceilings, counts a moving plank
        as a plank along its whole path."""
        g = self.g
        def cell(x, y): return g[y][x] if 0 <= x < self.w and 0 <= y < ROWS else ("#" if y >= 0 else ".")
        floor = set()
        for y in range(ROWS):
            for x in range(self.w):
                if cell(x, y) == "M":
                    for dx in range(7): floor.add((x + dx, y))
        def solid(x, y): return cell(x, y) in "#X?!" or (x, y) in floor or cell(x, y) == "="
        def standable(x, y): return cell(x, y) not in "#X?!^~" and y + 1 < ROWS and solid(x, y + 1) and cell(x, y + 1) != "^"
        stands = [(x, y) for y in range(ROWS - 1) for x in range(self.w) if standable(x, y)]
        start = next((x, y) for y in range(ROWS) for x in range(self.w) if g[y][x] == "P")
        seen, todo = {start}, [start]
        while todo:
            x, y = todo.pop()
            for (x2, y2) in stands:
                if (x2, y2) in seen: continue
                dx, up = abs(x2 - x), y - y2
                # a jump carries her ~5 columns between standing spots on the level, 4 when climbing 2-3 tiles
                if (up <= 1 and dx <= 5) or (2 <= up <= 3 and dx <= 4) or (up < 0 and dx <= 5 + (-up) // 2):
                    seen.add((x2, y2)); todo.append((x2, y2))
        def show(x):   # the neighbourhood, with the places she can reach marked '*'
            for yy in range(ROWS):
                print("".join("*" if (xx, yy) in seen else cell(xx, yy) for xx in range(max(0, x - 20), min(self.w, x + 12))))
        problems = []
        for y in range(ROWS):
            for x in range(self.w):
                ch = g[y][x]
                if ch in "oSG":            # touch it: up to 4 rows above a standing spot, or drop onto it from one
                    ok = any(abs(x - sx) <= 3 and -6 <= sy - y <= 4 for (sx, sy) in seen)
                elif ch in "?!":           # bump it from below
                    ok = any(abs(x - sx) <= 1 and 1 <= sy - y <= 4 for (sx, sy) in seen)
                elif ch == "=" and (x == 0 or g[y][x - 1] != "="):   # every plank run must be reachable
                    run = range(x, next((xx for xx in range(x, self.w) if g[y][xx] != "="), self.w))
                    ok = any((xx, y - 1) in seen for xx in run)
                else:
                    continue
                if not ok: problems.append((ch, x, y))
        for ch, x, y in problems:
            show(x)
            print("%s: %s at column %d row %d can't be reached" % (self.name, ch, x, y))
        assert not problems, "%s: %d unreachable things" % (self.name, len(problems))

# ---------------------------------------------------------------- 1: BLOSSOM HILLS (a gentle start)
def blossom():
    L = Lvl(180, 0, "BLOSSOM HILLS")
    L.ground(0, 40)
    L.put(3, 11, "P")
    for x, ch in ((1, "u"), (7, "t"), (12, "l"), (18, "v"), (26, "v")): L.on(x, ch)
    L.put(14, 9, "?"); L.put(15, 9, "!"); L.put(16, 9, "?")   # hop up on the boxes for the coins
    L.coins(14, 16, 6)
    L.on(23, "b")
    L.ground(29, 40, 11); L.ground(34, 40, 10)
    L.on(37, "u")
    L.arc(41, 3, 8)                                  # first little gap
    L.ground(44, 63)
    L.row(48, 51, 9, "="); L.coins(48, 51, 8)
    L.row(54, 56, 6, "="); L.put(55, 5, "S")         # scroll 1: up the planks
    L.put(59, 6, "c")
    L.on(61, "b"); L.on(46, "v")
    L.ground(64, 80); L.on(65, "C"); L.on(69, "v"); L.on(72, "t")
    L.on(77, "k")                                    # first chestnut: roll into it or hop over
    L.coins(81, 84, 9)                               # water hop
    L.fill(81, 84, 13, 14, "~")
    L.ground(85, 104)
    L.put(90, 11, "^"); L.put(91, 11, "^")
    L.put(95, 9, "?"); L.coins(94, 96, 6)
    L.on(98, "b"); L.on(103, "b"); L.on(87, "r")
    L.fill(100, 100, 11, 11, "X"); L.fill(101, 101, 10, 11, "X"); L.fill(102, 102, 9, 11, "X")
    L.put(105, 10, "M"); L.coins(107, 110, 7)        # first moving plank over a wide gap
    L.ground(111, 142)
    L.ground(118, 124, 10); L.on(121, "t"); L.put(121, 5, "c")
    L.on(128, "C")
    L.row(127, 129, 8, "="); L.row(131, 133, 5, "="); L.put(132, 4, "S")   # scroll 2
    L.on(136, "b"); L.on(140, "k"); L.on(114, "u")
    L.arc(143, 3, 9)
    L.ground(146, 179)
    L.fill(150, 151, 9, 11, "X"); L.put(150, 8, "S")                       # scroll 3 on the pillar
    L.on(155, "b"); L.on(158, "v"); L.on(160, "l")
    L.on(166, "G"); L.on(173, "l"); L.on(176, "t")
    return L.check()

# ---------------------------------------------------------------- 2: BAMBOO GROVE (planks, crows, water)
def bamboo():
    L = Lvl(196, 1, "BAMBOO GROVE")
    L.ground(0, 30)
    L.put(3, 11, "P")
    for x, ch in ((6, "t"), (11, "u"), (16, "l"), (22, "t")): L.on(x, ch)
    L.put(18, 9, "?"); L.put(19, 9, "?"); L.coins(18, 19, 6)
    L.on(26, "b")
    L.fill(31, 35, 13, 14, "~"); L.row(32, 34, 10, "="); L.coins(32, 34, 9)
    L.ground(36, 58); L.ground(42, 48, 10); L.ground(45, 48, 8)
    L.on(40, "k"); L.put(46, 4, "c"); L.on(52, "b"); L.on(56, "b")
    L.row(50, 52, 6, "="); L.put(51, 3, "S")                              # scroll 1: high over the hill
    L.put(59, 10, "M"); L.coins(60, 64, 8)
    L.ground(66, 88); L.on(67, "C"); L.on(72, "t"); L.on(80, "t")
    L.put(74, 8, "!"); L.put(75, 8, "?")
    L.on(78, "k"); L.on(84, "b"); L.put(82, 5, "c")
    L.fill(89, 92, 13, 14, "~"); L.coins(89, 92, 9)
    L.ground(93, 104, 11); L.ground(97, 104, 9)
    L.row(98, 104, 8, "^")                                                 # a spiky ledge: jump over it
    L.fill(98, 104, 8, 8, "."); L.row(98, 104, 8, "o")
    L.ground(105, 118)
    L.on(108, "b"); L.on(113, "k"); L.put(111, 6, "c")
    L.row(115, 118, 9, "="); L.row(120, 123, 6, "="); L.row(125, 128, 8, "=")   # plank stepping stones over a pit
    L.coins(120, 122, 5); L.put(121, 2, "S")                               # scroll 2, above the top plank
    L.fill(119, 131, 13, 14, "~")
    L.ground(132, 150); L.on(134, "C"); L.on(139, "t"); L.on(146, "t")
    L.on(142, "b"); L.on(148, "b"); L.put(144, 5, "c")
    L.put(151, 10, "M"); L.coins(152, 155, 7)
    L.ground(157, 195)
    L.fill(163, 164, 9, 11, "X"); L.put(163, 8, "S")                       # scroll 3 on the block tower
    L.on(160, "k"); L.on(168, "b"); L.on(171, "k"); L.on(175, "u")
    L.on(182, "G"); L.on(179, "l"); L.on(189, "t"); L.on(192, "l")
    return L.check()

# ---------------------------------------------------------------- 3: MISTY PEAKS (spikes, stone and high jumps)
def peaks():
    L = Lvl(210, 2, "MISTY PEAKS")
    L.ground(0, 26)
    L.put(3, 11, "P")
    for x, ch in ((7, "t"), (13, "l"), (19, "r"), (24, "t")): L.on(x, ch)
    L.on(16, "b"); L.put(12, 9, "?"); L.coins(11, 13, 6)
    L.ground(27, 44, 10); L.ground(33, 44, 8)
    L.on(30, "k"); L.put(38, 4, "c"); L.on(41, "b")
    L.row(34, 36, 7, "^")
    L.fill(45, 48, 12, 14, "~")                                            # a drop into a basin
    L.ground(45, 48, 13); L.fill(45, 48, 12, 12, "~")
    L.coins(45, 48, 9)
    L.ground(49, 66); L.on(51, "C"); L.on(56, "t")
    L.fill(59, 59, 10, 11, "X"); L.fill(62, 62, 9, 11, "X"); L.fill(65, 65, 8, 11, "X")
    L.put(65, 7, "o"); L.put(62, 8, "o")
    L.put(67, 8, "M"); L.coins(68, 71, 5)                                  # moving plank high over a pit
    L.put(70, 3, "S")                                                      # scroll 1 above it
    L.ground(73, 96); L.row(76, 78, 11, "^"); L.coins(76, 78, 8)
    L.on(82, "k"); L.on(86, "b"); L.put(85, 5, "c"); L.on(90, "k")
    L.put(88, 8, "!"); L.on(94, "t")
    L.arc(97, 4, 8)
    L.ground(101, 124); L.on(103, "C")
    L.fill(108, 111, 9, 11, "X"); L.fill(112, 115, 7, 11, "X")
    L.row(112, 115, 6, "o"); L.on(119, "b"); L.on(122, "k"); L.put(117, 4, "c")
    L.put(125, 11, "M"); L.fill(125, 131, 13, 14, "~")
    L.ground(132, 150); L.row(135, 136, 11, "^"); L.row(141, 142, 11, "^")
    L.on(138, "b"); L.put(139, 6, "c"); L.on(146, "k")
    L.row(143, 145, 9, "="); L.row(147, 149, 6, "="); L.put(148, 5, "S")   # scroll 2
    L.fill(151, 154, 13, 14, "~"); L.coins(151, 154, 9)
    L.ground(155, 176); L.on(157, "C"); L.on(162, "t"); L.on(170, "t")
    L.on(160, "b"); L.on(165, "k"); L.on(168, "b"); L.put(166, 5, "c"); L.put(172, 4, "c")
    L.fill(174, 175, 9, 11, "X"); L.put(174, 8, "S")                       # scroll 3 on the last pillar
    L.arc(177, 4, 9)
    L.ground(181, 209)
    L.on(184, "k"); L.on(188, "b")
    L.on(196, "G"); L.on(193, "l"); L.on(203, "t"); L.on(206, "l")
    return L.check()

LEVELS = [blossom(), bamboo(), peaks()]

def export(path):
    out = ["// AUTO-GENERATED by games/PixelPeaks/tools/make_levels.py - edit that script, not this file",
           "#pragma once", "#include <stdint.h>",
           "struct LevelDef { const char* name; uint8_t world; uint16_t w; const char* const* rows; };"]
    for i, L in enumerate(LEVELS):
        out.append("static const char* const LEVEL%d_ROWS[%d] = {" % (i, ROWS))
        for r in L.g: out.append('  "%s",' % "".join(r))
        out.append("};")
    out.append("static const LevelDef LEVELS[%d] = {%s};" % (len(LEVELS), ", ".join(
        '{"%s", %d, %d, LEVEL%d_ROWS}' % (L.name, L.world, L.w, i) for i, L in enumerate(LEVELS))))
    out.append("static const int NLEVELS = %d, LEVEL_ROWS = %d;" % (len(LEVELS), ROWS))
    open(path, "w").write("\n".join(out) + "\n")
    print("wrote", path)

if __name__ == "__main__":
    export(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "levels.h"))
    if "--show" in __import__("sys").argv:
        for L in LEVELS:
            print(L.name); print("\n".join("".join(r) for r in L.g))
