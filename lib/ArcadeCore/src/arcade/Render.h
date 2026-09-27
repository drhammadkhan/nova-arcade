#pragma once
// =====================================================================
//  ArcadeCore renderer. The 320x240 frame is drawn in five 48-row strips,
//  double-buffered in internal RAM and sent to the panel with DMA while the
//  next strip is drawn. Draw code writes into the current strip `B` whose
//  first screen row is `Y0`; every helper here clips to the strip for you.
//  Pixels are byte-swapped RGB565 (use rgbS()/pal[] values, not raw 565).
// =====================================================================
#ifdef ARCADE_SIM
#include "ArcadeSimDisplay.h"
#else
#include "Display.h"
#endif

#ifndef ARCADE_SPRITE_T
#define ARCADE_SPRITE_T
struct Sprite { uint16_t w, h; const uint8_t* px; };   // palette-indexed, 0 = transparent
#endif

namespace gfx {

static const int SW = 320, SH = 240, STRIP = 48, NSTRIP = SH / STRIP;
static LGFX lcd;
static LGFX_Sprite strips[2] = {LGFX_Sprite(&lcd), LGFX_Sprite(&lcd)};
static uint16_t* B = nullptr;   // current strip buffer
static int Y0 = 0;              // screen row of the strip's first line
static LGFX_Sprite* S = nullptr;
static uint32_t frameNo = 0;

static inline uint16_t sw16(uint16_t c) { return (c >> 8) | (c << 8); }
static inline uint16_t c565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static inline uint16_t rgbS(uint8_t r, uint8_t g, uint8_t b) { return sw16(c565(r, g, b)); }
static inline uint16_t lerpS(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, float t) {
  return rgbS(r0 + (r1 - r0) * t, g0 + (g1 - g0) * t, b0 + (b1 - b0) * t);
}

// Palettes for indexed sprites (swapped), filled by setPalette()
static uint16_t pal[64], palWhite[64], palDark[64];
inline void setPalette(const uint16_t* pal565, int n) {
  for (int i = 0; i < n && i < 64; i++) {
    pal[i] = sw16(pal565[i]);
    palWhite[i] = sw16(0xFFFF);
    palDark[i] = sw16((pal565[i] >> 1) & 0x7BEF);
  }
}

static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static inline bool rowsVisible(int y, int h) { return y + h > Y0 && y < Y0 + STRIP; }
static inline void pset(int x, int y, uint16_t c) {
  if ((unsigned)x < (unsigned)SW && y >= Y0 && y < Y0 + STRIP) B[(y - Y0) * SW + x] = c;
}
static inline void rectf(int x, int y, int w, int h, uint16_t c) {
  int x0 = max(0, x), x1 = min(SW, x + w);
  int y0 = max(Y0, y), y1 = min(Y0 + STRIP, y + h);
  for (int yy = y0; yy < y1; yy++) {
    uint16_t* d = B + (yy - Y0) * SW;
    for (int xx = x0; xx < x1; xx++) d[xx] = c;
  }
}
static inline void rect(int x, int y, int w, int h, uint16_t c) {
  rectf(x, y, w, 1, c); rectf(x, y + h - 1, w, 1, c);
  rectf(x, y, 1, h, c); rectf(x + w - 1, y, 1, h, c);
}
// Darken (halve) a rectangle: handy for translucent panels
static inline void shade(int x, int y, int w, int h) {
  int x0 = max(0, x), x1 = min(SW, x + w);
  int y0 = max(Y0, y), y1 = min(Y0 + STRIP, y + h);
  for (int yy = y0; yy < y1; yy++) {
    uint16_t* d = B + (yy - Y0) * SW;
    for (int xx = x0; xx < x1; xx++) d[xx] = sw16((sw16(d[xx]) >> 1) & 0x7BEF);
  }
}
static inline void fill(uint16_t c) { for (int i = 0; i < SW * STRIP; i++) B[i] = c; }

static inline void blit(const Sprite& s, int x, int y, const uint16_t* p = pal) {
  int sy0 = max(0, Y0 - y), sy1 = min((int)s.h, Y0 + STRIP - y);
  if (sy0 >= sy1) return;
  int sx0 = max(0, -x), sx1 = min((int)s.w, SW - x);
  if (sx0 >= sx1) return;
  for (int sy = sy0; sy < sy1; sy++) {
    const uint8_t* row = s.px + sy * s.w;
    uint16_t* d = B + (y + sy - Y0) * SW + x;
    for (int sx = sx0; sx < sx1; sx++) {
      uint8_t c = row[sx];
      if (c) d[sx] = p[c];
    }
  }
}
// Horizontally mirrored blit
static inline void blitFlip(const Sprite& s, int x, int y, const uint16_t* p = pal) {
  int sy0 = max(0, Y0 - y), sy1 = min((int)s.h, Y0 + STRIP - y);
  if (sy0 >= sy1) return;
  for (int sy = sy0; sy < sy1; sy++) {
    const uint8_t* row = s.px + sy * s.w;
    for (int sx = 0; sx < s.w; sx++) {
      uint8_t c = row[s.w - 1 - sx];
      int xx = x + sx;
      if (c && (unsigned)xx < (unsigned)SW) B[(y + sy - Y0) * SW + xx] = p[c];
    }
  }
}
// Copy a block of already-swapped RGB565 pixels (e.g. a decoded PNG), optional scale-down
static inline void blitRGB(const uint16_t* src, int sw, int sh, int x, int y, int step = 1) {
  int dw = sw / step, dh = sh / step;
  int y0 = max(Y0, y), y1 = min(Y0 + STRIP, y + dh);
  int x0 = max(0, x), x1 = min(SW, x + dw);
  for (int yy = y0; yy < y1; yy++) {
    const uint16_t* s = src + (yy - y) * step * sw;
    uint16_t* d = B + (yy - Y0) * SW;
    for (int xx = x0; xx < x1; xx++) d[xx] = s[(xx - x) * step];
  }
}

static inline void text(const char* str, int x, int y, uint16_t col, int size = 1,
                 textdatum_t datum = top_left, bool shadow = true) {
  int top = y - (datum == middle_center ? 4 * size : 0);
  if (!rowsVisible(top, 9 * size)) return;
  S->setFont(&fonts::Font0);
  S->setTextSize(size);
  S->setTextDatum(datum);
  if (shadow) {
    S->setTextColor(c565(20, 10, 40));
    S->drawString(str, x + size, y - Y0 + size);
  }
  S->setTextColor(col);
  S->drawString(str, x, y - Y0);
}
static inline void textf(int x, int y, uint16_t col, int size, textdatum_t datum, const char* fmt, ...) {
  char buf[64];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  text(buf, x, y, col, size, datum);
}
static inline void circle(int cx, int cy, int r, uint16_t col565) {
  if (!rowsVisible(cy - r - 1, 2 * r + 3)) return;
  S->drawCircle(cx, cy - Y0, r, col565);
}
static inline void disc(int cx, int cy, int r, uint16_t col565) {
  if (!rowsVisible(cy - r - 1, 2 * r + 3)) return;
  S->fillCircle(cx, cy - Y0, r, col565);
}
static inline void line(int x0, int y0, int x1, int y1, uint16_t col565) {
  if (!rowsVisible(min(y0, y1) - 1, abs(y1 - y0) + 3)) return;
  S->drawLine(x0, y0 - Y0, x1, y1 - Y0, col565);
}

inline void begin() {
  lcd.init();
  lcd.setRotation(TFT_ROTATION);
  lcd.setBrightness(200);
  lcd.fillScreen(0);
  for (auto& st : strips) {
    st.setColorDepth(16);
    st.setPsram(false);   // DMA needs internal RAM
    st.createSprite(SW, STRIP);
  }
}

// Draws one full frame: calls draw() once per strip.
inline void render(void (*draw)()) {
  lcd.startWrite();
  for (int i = 0; i < NSTRIP; i++) {
    S = &strips[i & 1];
    B = (uint16_t*)S->getBuffer();
    Y0 = i * STRIP;
    draw();
    lcd.pushImageDMA(0, Y0, SW, STRIP, (lgfx::swap565_t*)B);
  }
  lcd.waitDMA();
  lcd.endWrite();
}

}  // namespace gfx

// ------------------------------------------------------------ small utilities
static uint32_t rngState = 0x9E3779B9;
static inline uint32_t rnd() {
  rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
  return rngState;
}
static inline float frand() { return (rnd() & 0xFFFFFF) / 16777216.0f; }
static inline float frange(float a, float b) { return a + (b - a) * frand(); }
static inline bool overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}
