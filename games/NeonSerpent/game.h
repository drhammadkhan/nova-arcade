// =====================================================================
//  NEON SERPENT  -  a snake game (Nova Arcade)
//  Eat the glowing orbs, grow longer, don't bite yourself. Every level
//  brings a new wall layout; bonus stars appear for a short time.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace nsm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 Em*/ {76, 1, 79, 1, 83, 1, 1, 79, 81, 1, 79, 1, 76, 1, 74, 1},
  /*1 C */ {72, 1, 76, 1, 79, 1, 1, 76, 77, 1, 76, 1, 72, 1, 1, 1},
  /*2 D */ {74, 1, 78, 1, 81, 1, 1, 78, 79, 1, 81, 1, 83, 1, 1, 1},
  /*3 Am*/ {81, 1, 1, 1, 79, 1, 76, 1, 72, 1, 1, 1, 71, 1, 1, 1},
  /*4 t1*/ {64, 1, 1, 1, 67, 1, 1, 1, 71, 1, 1, 1, 69, 1, 67, 1},
  /*5 t2*/ {64, 1, 1, 1, 60, 1, 1, 1, 62, 1, 1, 1, 0, 0, 0, 0},
  /*6 ov*/ {71, 1, 69, 1, 67, 1, 64, 1, 63, 1, 1, 1, 64, 1, 0, 0},
  /*7 cl*/ {76, 79, 83, 88, 1, 1, 83, 1, 88, 1, 91, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.h.S.hhK.h.S.h.", "K...S...K...S.SS", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{EM, 4, 2}, {C_, 5, 2}, {EM, 4, 2}, {D_, 5, 2}};
static const Bar GAME_BARS[] = {{EM, 0, 0}, {C_, 1, 0}, {D_, 2, 0}, {AM, 3, 0},
                                {EM, 0, 0}, {C_, 1, 0}, {D_, 2, 0}, {EM, 3, 1}};
static const Bar OVER_BARS[] = {{EM, 6, 3}, {EM, -1, 3}};
static const Bar CLEAR_BARS[] = {{EM, 7, 3}, {EM, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 100, true, false, 0x30},
  {GAME_BARS, 8, 128, true, false, 0},
  {OVER_BARS, 2, 92, false, false, 0},
  {CLEAR_BARS, 2, 140, false, false, 0},
};
}  // namespace nsm
static const audio::Music MUSIC = {audio::STD_CHORDS, nsm::LEADS, nsm::DRUMS, nsm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_GAME, SONG_OVER, SONG_CLEAR };

// ------------------------------------------------------------ board
static const int CELL = 10, GCOLS = 30, GROWS = 21;
static const int GX0 = 10, GY0 = 20;               // screen position of cell (0,0)
static const int MAXLEN = GCOLS * GROWS;
static const int FOOD_PER_LEVEL = 10;
static const int8_t DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};   // right, down, left, up

static uint8_t wall[GROWS][GCOLS];
static uint16_t body[MAXLEN];                       // body[0] is the head, cell = r * GCOLS + c
static int len = 0, grow = 0;
static int dir = 0, queued[2], nQueued = 0;
static int moveT = 0;
static int foodC = 0, foodR = 0;
static int starC = -1, starR = 0, starT = 0;
static int eaten = 0, level = 1, lives = 3;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 2000;
static bool newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static float shake = 0;
static int shakeX = 0, shakeY = 0;

struct Part { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[220];
struct Popup { bool on; int x, y, t; char txt[12]; };
static Popup popups[4];

static inline int cellC(uint16_t v) { return v % GCOLS; }
static inline int cellR(uint16_t v) { return v / GCOLS; }
static inline int cellX(int c) { return GX0 + c * CELL; }
static inline int cellY(int r) { return GY0 + r * CELL; }

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, uint16_t col, float sp) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s, (uint8_t)frange(14, 32), col}; break; }
}
static void popup(int x, int y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 50, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}

// ------------------------------------------------------------ levels
static void wallRect(int c, int r, int w, int h) {
  for (int y = r; y < r + h; y++)
    for (int x = c; x < c + w; x++)
      if (x >= 0 && x < GCOLS && y >= 0 && y < GROWS) wall[y][x] = 1;
}
// The snake starts on row 10 heading right from column 3, so layouts keep
// columns 2-13 of that row clear.
static void buildWalls(int lvl) {
  memset(wall, 0, sizeof(wall));
  switch ((lvl - 1) % 6) {
    case 0: break;
    case 1: wallRect(7, 4, 16, 1); wallRect(7, 16, 16, 1); break;
    case 2: wallRect(15, 2, 1, 7); wallRect(15, 12, 1, 7); wallRect(19, 10, 8, 1); break;
    case 3:
      wallRect(4, 3, 6, 1); wallRect(4, 3, 1, 4); wallRect(20, 3, 6, 1); wallRect(25, 3, 1, 4);
      wallRect(4, 17, 6, 1); wallRect(4, 14, 1, 4); wallRect(20, 17, 6, 1); wallRect(25, 14, 1, 4);
      wallRect(15, 7, 1, 7);
      break;
    case 4:
      wallRect(8, 0, 1, 7); wallRect(15, 14, 1, 7); wallRect(22, 0, 1, 7);
      wallRect(8, 14, 1, 7); wallRect(22, 14, 1, 7); wallRect(15, 0, 1, 7);
      break;
    case 5:
      wallRect(5, 5, 20, 1); wallRect(5, 15, 20, 1); wallRect(24, 6, 1, 4); wallRect(5, 11, 1, 4);
      wallRect(14, 8, 3, 1); wallRect(14, 12, 3, 1);
      break;
  }
}

static bool occupied(int c, int r, bool ignoreTail) {
  if (wall[r][c]) return true;
  int n = ignoreTail && !grow ? len - 1 : len;
  for (int i = 0; i < n; i++) if (body[i] == r * GCOLS + c) return true;
  return false;
}

static void placeFood() {
  int hc = cellC(body[0]), hr = cellR(body[0]);
  for (int tries = 0; tries < 2000; tries++) {
    int c = rnd() % GCOLS, r = rnd() % GROWS;
    if (occupied(c, r, false) || abs(c - hc) + abs(r - hr) < 4) continue;
    if (c == starC && r == starR) continue;
    foodC = c; foodR = r;
    return;
  }
}
static void placeStar() {
  for (int tries = 0; tries < 2000; tries++) {
    int c = rnd() % GCOLS, r = rnd() % GROWS;
    if (occupied(c, r, false) || (c == foodC && r == foodR)) continue;
    starC = c; starR = r; starT = arcade::frames(420);
    return;
  }
}

static void startLife() {
  len = 4; grow = 0; dir = 0; nQueued = 0; moveT = 0;
  for (int i = 0; i < len; i++) body[i] = 10 * GCOLS + (6 - i);
  starC = -1;
  placeFood();
  state = ST_PLAY; stateT = 0;
}

static void startLevel() {
  buildWalls(level);
  eaten = 0;
  startLife();
  audio::music(SONG_GAME);
}

static void resetGame() {
  score = 0; lives = 3; level = 1; newHi = false;
  memset(parts, 0, sizeof(parts)); memset(popups, 0, sizeof(popups));
  startLevel();
}

static int moveInterval() {
  int base = max(4, 9 - (level - 1) / 2 - len / 24);
  return arcade::frames(base);
}

static void die() {
  audio::play(SFX_PLAYER_DIE);
  input::rumble(400, 0xA0, 0xA0);
  shake = 7;
  for (int i = 0; i < len; i += 2)
    burst(cellX(cellC(body[i])) + CELL / 2, cellY(cellR(body[i])) + CELL / 2, 3, lerpS(62, 198, 224, 194, 58, 214, (float)i / len), 1.8f);
  lives--;
  state = lives > 0 ? ST_DEAD : ST_OVER;
  stateT = 0;
  if (state == ST_OVER) { audio::music(SONG_OVER); if (newHi) arcade::saveHi(hiscore); }
}

static void moveSnake() {
  if (nQueued) {
    int d = queued[0];
    queued[0] = queued[1]; nQueued--;
    if ((d + 2) % 4 != dir) dir = d;
  }
  int c = cellC(body[0]) + DX[dir], r = cellR(body[0]) + DY[dir];
  if (c < 0 || c >= GCOLS || r < 0 || r >= GROWS || occupied(c, r, true)) { die(); return; }
  if (grow) { grow--; if (len < MAXLEN) len++; }
  memmove(body + 1, body, (len - 1) * sizeof(body[0]));
  body[0] = r * GCOLS + c;
  int px = cellX(c) + CELL / 2, py = cellY(r) + CELL / 2;
  if (c == foodC && r == foodR) {
    grow += 3;
    eaten++;
    uint32_t v = 10 * level;
    addScore(v);
    popup(px - 6, py - 12, v);
    burst(px, py, 12, rgbS(255, 216, 74), 1.6f);
    audio::play(SFX_BRICK, min(eaten * 2, 20));
    input::rumble(60, 0x40, 0);
    if (eaten >= FOOD_PER_LEVEL) {
      addScore(500 * level);
      state = ST_CLEAR; stateT = 0;
      audio::music(SONG_CLEAR);
      return;
    }
    placeFood();
    if (eaten % 4 == 0 && starC < 0) placeStar();
  }
  if (c == starC && r == starR) {
    uint32_t v = 50 * level + starT / 4;
    addScore(v);
    popup(px - 8, py - 12, v);
    burst(px, py, 24, rgbS(255, 123, 213), 2.2f);
    audio::play(SFX_POWERUP);
    starC = -1;
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  static const uint32_t DB[4] = {BTN_RIGHT, BTN_DOWN, BTN_LEFT, BTN_UP};
  for (int d = 0; d < 4; d++)
    if (in.hit(DB[d]) && nQueued < 2) {
      int last = nQueued ? queued[nQueued - 1] : dir;
      if (d != last && (d + 2) % 4 != last) queued[nQueued++] = d;
    }
  if (starC >= 0 && --starT <= 0) { starC = -1; }
  if (++moveT >= moveInterval()) { moveT = 0; moveSnake(); }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.95f; p.vy *= 0.95f; if (--p.life == 0) p.on = false; }
  for (auto& p : popups) if (p.on) { if ((p.t & 3) == 0) p.y--; if (--p.t <= 0) p.on = false; }
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

// title demo: a snake wandering over the title screen
static float demoA = 0;

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      demoA += 0.02f;
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_PLAY:
      if (stateT > 50) updatePlay(in);
      else if (in.hit(BTN_START)) arcade::pause();
      break;
    case ST_DEAD:
      if (stateT > 100) startLife();
      break;
    case ST_CLEAR:
      if (stateT > 150) { level++; startLevel(); }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ draw
static uint16_t bgRow[SH];
static void buildBackground() {
  for (int y = 0; y < SH; y++) bgRow[y] = lerpS(6, 10, 26, 22, 8, 40, (float)y / SH);
}

static void drawBackground() {
  uint16_t dot = rgbS(36, 40, 80);
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t c = bgRow[y];
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) d[x] = c;
    if (state != ST_TITLE && y >= GY0 && y < GY0 + GROWS * CELL && ((y - GY0) % CELL) == CELL / 2)
      for (int x = GX0 + CELL / 2; x < GX0 + GCOLS * CELL; x += CELL) d[x] = dot;
  }
}

static void drawWalls() {
  uint16_t base = rgbS(122, 61, 184), hi = rgbS(194, 120, 240), lo = rgbS(60, 28, 100);
  for (int r = 0; r < GROWS; r++) {
    int y = cellY(r) + shakeY;
    if (!rowsVisible(y, CELL)) continue;
    for (int c = 0; c < GCOLS; c++) {
      if (!wall[r][c]) continue;
      int x = cellX(c) + shakeX;
      rectf(x, y, CELL, CELL, base);
      if (r == 0 || !wall[r - 1][c]) rectf(x, y, CELL, 2, hi);
      if (r == GROWS - 1 || !wall[r + 1][c]) rectf(x, y + CELL - 2, CELL, 2, lo);
      if (c == 0 || !wall[r][c - 1]) rectf(x, y, 1, CELL, hi);
      if (c == GCOLS - 1 || !wall[r][c + 1]) rectf(x + CELL - 1, y, 1, CELL, lo);
    }
  }
  // border
  uint16_t bc = rgbS(62, 198, 224), bd = rgbS(31, 111, 139);
  int x0 = GX0 - 3 + shakeX, y0 = GY0 - 3 + shakeY, w = GCOLS * CELL + 6, h = GROWS * CELL + 6;
  rect(x0, y0, w, h, bd);
  rect(x0 + 1, y0 + 1, w - 2, h - 2, bc);
}

static void drawSegment(int i) {
  int c = cellC(body[i]), r = cellR(body[i]);
  int x = cellX(c) + shakeX, y = cellY(r) + shakeY;
  if (!rowsVisible(y - 1, CELL + 2)) return;
  float t = len > 1 ? (float)i / (len - 1) : 0;
  uint16_t col = lerpS(62, 198, 224, 194, 58, 214, t);
  uint16_t hi = lerpS(184, 243, 255, 255, 123, 213, t);
  rectf(x + 1, y + 1, CELL - 2, CELL - 2, col);
  rectf(x + 2, y + 2, CELL - 5, 1, hi);
  // bridge to the next segment so the body reads as one piece
  if (i + 1 < len) {
    int nc = cellC(body[i + 1]), nr = cellR(body[i + 1]);
    if (nc == c + 1) rectf(x + CELL - 1, y + 2, 2, CELL - 4, col);
    else if (nc == c - 1) rectf(x - 1, y + 2, 2, CELL - 4, col);
    else if (nr == r + 1) rectf(x + 2, y + CELL - 1, CELL - 4, 2, col);
    else if (nr == r - 1) rectf(x + 2, y - 1, CELL - 4, 2, col);
  }
  if (i == 0) {
    uint16_t eye = rgbS(255, 255, 255), pupil = rgbS(13, 11, 30);
    int ex = DX[dir], ey = DY[dir];
    int ax = x + 5 + ex * 2 - ey * 2, ay = y + 5 + ey * 2 - ex * 2;
    int bx = x + 5 + ex * 2 + ey * 2, by = y + 5 + ey * 2 + ex * 2;
    rectf(ax - 1, ay - 1, 2, 2, eye); rectf(bx - 1, by - 1, 2, 2, eye);
    pset(ax - 1 + (ex > 0), ay - 1 + (ey > 0), pupil); pset(bx - 1 + (ex > 0), by - 1 + (ey > 0), pupil);
  }
}

static void drawFood() {
  int x = cellX(foodC) + CELL / 2 + shakeX, y = cellY(foodR) + CELL / 2 + shakeY;
  int r = 3 + ((frameNo >> 3) & 1);
  disc(x, y, r + 1, c565(138, 21, 56));
  disc(x, y, r, c565(255, 216, 74));
  pset(x - 1, y - 2, rgbS(255, 255, 255));
  if (starC >= 0 && (starT > 90 || (starT & 4))) {
    int sx = cellX(starC) + CELL / 2 + shakeX, sy = cellY(starR) + CELL / 2 + shakeY;
    uint16_t sc = (frameNo & 4) ? rgbS(255, 123, 213) : rgbS(255, 255, 255);
    if (rowsVisible(sy - 5, 11)) {
      rectf(sx - 1, sy - 5, 3, 11, sc);
      rectf(sx - 5, sy - 1, 11, 3, sc);
      rectf(sx - 2, sy - 2, 5, 5, sc);
    }
  }
}

static void drawHud() {
  if (Y0 != 0) return;
  rectf(0, 0, SW, 13, rgbS(10, 8, 25));
  textf(4, 3, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
  textf(62, 3, c565(184, 243, 255), 1, top_left, "LV %d", level);
  textf(142, 3, c565(255, 216, 74), 1, top_left, "HI %07lu", (unsigned long)hiscore);
  for (int i = 0; i < min(lives - 1, 5); i++) {
    int x = SW - 12 - i * 11;
    rectf(x, 4, 8, 6, rgbS(62, 198, 224));
    pset(x + 5, 5, rgbS(255, 255, 255));
  }
  // orbs left this level
  for (int i = 0; i < FOOD_PER_LEVEL; i++)
    rectf(96 + i * 4, 5, 3, 4, i < eaten ? rgbS(255, 216, 74) : rgbS(61, 74, 102));
}

static void drawTitle() {
  // a wandering demo snake behind the logo
  for (int i = 0; i < 24; i++) {
    float a = demoA - i * 0.09f;
    int x = 160 + (int)(120 * sinf(a * 1.3f)), y = 150 + (int)(60 * sinf(a * 2.1f));
    uint16_t col = lerpS(62, 198, 224, 194, 58, 214, i / 23.0f);
    if (rowsVisible(y - 4, 9)) rectf(x - 4, y - 4, 8, 8, col);
  }
  blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 36);
  text("EAT, GROW, DON'T BITE YOURSELF", SW / 2, 76, c565(184, 243, 255), 1, top_center);
  textf(SW / 2, 16, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
  bool blink = (frameNo >> 4) & 1;
  if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 112, TFT_WHITE, 2, top_center); }
  else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 116, c565(255, 138, 61), 1, top_center);
  arcade::drawDifficulty(140);
  text("D-PAD STEER   START PAUSE", SW / 2, 196, c565(180, 194, 220), 1, top_center);
  text("SELECT+START: BACK TO MENU", SW / 2, 212, c565(115, 132, 168), 1, top_center);
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) { drawTitle(); return; }
  drawHud();
  drawWalls();
  if (state != ST_CLEAR) drawFood();
  bool showSnake = state == ST_PLAY || state == ST_CLEAR || (state == ST_DEAD && stateT < 30 && (stateT & 4));
  if (showSnake) for (int i = len - 1; i >= 0; i--) drawSegment(i);
  for (auto& p : parts) if (p.on) { pset((int)p.x, (int)p.y, p.col); if (p.life > 20) pset((int)p.x + 1, (int)p.y, p.col); }
  for (auto& p : popups) if (p.on) text(p.txt, p.x, p.y, (p.t & 8) ? c565(255, 216, 74) : TFT_WHITE, 1, top_left);
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_PLAY && stateT < 50) textf(SW / 2, 96, TFT_WHITE, 2, top_center, "LEVEL %d", level);
  if (state == ST_CLEAR) {
    text("LEVEL CLEAR!", SW / 2, 96, c565(182, 255, 110), 2, top_center);
    textf(SW / 2, 118, TFT_WHITE, 1, top_center, "BONUS %d", 500 * level);
  }
  if (state == ST_OVER) {
    if (rowsVisible(80, 76)) { shade(60, 80, 200, 76); shade(60, 80, 200, 76); }
    text("GAME OVER", SW / 2, 90, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 120, c565(255, 216, 74), 1, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 138, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("neonserpent", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildBackground();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
