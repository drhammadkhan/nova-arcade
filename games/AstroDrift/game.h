// =====================================================================
//  ASTRO DRIFT  -  a space-rock shooter (Nova Arcade)
//  Turn, thrust and fire in a wrap-around field. Big rocks split into
//  smaller ones; from wave 2 a hunter saucer drops in to take shots at you.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace adm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 t Am*/ {69, 1, 1, 1, 72, 1, 76, 1, 81, 1, 1, 1, 79, 1, 76, 1},
  /*1 t F */ {77, 1, 1, 1, 76, 1, 72, 1, 69, 1, 1, 1, 72, 1, 1, 1},
  /*2 t G */ {74, 1, 1, 1, 79, 1, 1, 1, 83, 1, 81, 1, 79, 1, 1, 1},
  /*3 t E */ {80, 1, 1, 1, 76, 1, 1, 1, 71, 1, 1, 1, 0, 0, 0, 0},
  /*4 over*/ {76, 1, 72, 1, 69, 1, 68, 1, 69, 1, 1, 1, 1, 1, 0, 0},
  /*5 clear*/{81, 84, 88, 93, 1, 1, 91, 1, 93, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.......K.......", "K...h...K...h.h.", "................"};
static const Bar TITLE_BARS[] = {{AM, 0, 0}, {F_, 1, 0}, {G_, 2, 0}, {E_, 3, 0}};
static const Bar PLAY_BARS[] = {{AM, -1, 1}, {AM, -1, 1}, {F_, -1, 1}, {E_, -1, 1}};
static const Bar OVER_BARS[] = {{AM, 4, 2}, {AM, -1, 2}};
static const Bar CLEAR_BARS[] = {{A_, 5, 2}, {A_, -1, 2}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 92, true, false, 0x30},
  {PLAY_BARS, 4, 84, true, true, 0},
  {OVER_BARS, 2, 90, false, false, 0},
  {CLEAR_BARS, 2, 130, false, false, 0},
};
}  // namespace adm
static const audio::Music MUSIC = {audio::STD_CHORDS, adm::LEADS, adm::DRUMS, adm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_PLAY, SONG_OVER, SONG_CLEAR };

// ------------------------------------------------------------ world
static const int FY = 14, FH = SH - FY;     // playfield rows 14..239, wraps both ways
static const int NV = 10;                   // rock outline vertices
static const float ROCK_R[3] = {21, 12, 6};
static const int ROCK_PTS[3] = {20, 50, 100};

struct Rock { bool on; float x, y, vx, vy, a, va; uint8_t size; int8_t shape[NV]; uint8_t flash; };
static Rock rocks[48];
struct Shot { bool on; float x, y, vx, vy; int16_t life; };
static Shot pshots[6], eshots[6];
struct Part { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[260];
struct Popup { bool on; int x, y, t; char txt[12]; };
static Popup popups[4];

struct Ship { float x, y, vx, vy, a; int invuln, fireCd; bool alive, thrust; } ship;
struct Saucer { bool on; float x, y, vx, baseY, t; int fireT; bool small; } ufo;
static int ufoTimer = 900;

static int lives = 3, wave = 1;
static uint32_t score = 0, hiscore = 0, nextExtra = 10000;
static const uint32_t HI_DEFAULT = 8000;
static bool newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static float shake = 0;
static int shakeX = 0, shakeY = 0;
struct Star { int16_t x, y; uint8_t b; };
static Star stars[90];

static inline void wrap(float& x, float& y) {
  if (x < 0) x += SW; else if (x >= SW) x -= SW;
  if (y < FY) y += FH; else if (y >= SH) y -= FH;
}
// shortest wrapped offset from a to b
static inline float wdx(float a, float b) { float d = b - a; if (d > SW / 2) d -= SW; if (d < -SW / 2) d += SW; return d; }
static inline float wdy(float a, float b) { float d = b - a; if (d > FH / 2) d -= FH; if (d < -FH / 2) d += FH; return d; }
static inline bool hitCircle(float ax, float ay, float bx, float by, float r) {
  float dx = wdx(ax, bx), dy = wdy(ay, by);
  return dx * dx + dy * dy < r * r;
}

static void addScore(uint32_t v) {
  score += v;
  if (score >= nextExtra) { nextExtra += 10000; lives = min(lives + 1, 9); audio::play(SFX_POWERUP); }
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, uint16_t col, float sp) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.2f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s, (uint8_t)frange(14, 36), col}; break; }
}
static void popup(int x, int y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 50, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}

static void spawnRock(float x, float y, uint8_t size, float dirA = -1) {
  for (auto& r : rocks) {
    if (r.on) continue;
    float a = dirA < 0 ? frand() * 6.283f : dirA;
    float sp = frange(0.45f, 1.0f) * (1.0f + size * 0.45f) * (1.0f + min(wave - 1, 8) * 0.07f) * arcade::speed();
    r = {true, x, y, cosf(a) * sp, sinf(a) * sp, frand() * 6.283f, frange(-0.03f, 0.03f), size, {0}, 0};
    for (auto& s : r.shape) s = (int8_t)frange(-28, 12);   // % of radius
    return;
  }
}

static void startWave() {
  memset(rocks, 0, sizeof(rocks));
  int n = min(3 + wave, 10);
  for (int i = 0; i < n; i++) {
    // start around the edges, away from the ship
    float x, y;
    do { x = frand() * SW; y = FY + frand() * FH; } while (hitCircle(x, y, ship.x, ship.y, 80));
    spawnRock(x, y, 0);
  }
  ufo.on = false;
  ufoTimer = arcade::frames(900 + rnd() % 600);
  state = ST_PLAY; stateT = 0;
}

static void resetShip() {
  ship = {};
  ship.x = SW / 2; ship.y = FY + FH / 2; ship.a = -1.5708f;
  ship.alive = true; ship.invuln = 150;
}

static void resetGame() {
  score = 0; lives = 3; wave = 1; nextExtra = 10000; newHi = false;
  memset(parts, 0, sizeof(parts)); memset(popups, 0, sizeof(popups));
  memset(pshots, 0, sizeof(pshots)); memset(eshots, 0, sizeof(eshots));
  resetShip();
  startWave();
  audio::music(SONG_PLAY);
}

static void gameOver() {
  state = ST_OVER; stateT = 0;
  audio::music(SONG_OVER);
  if (newHi) arcade::saveHi(hiscore);
}

static void killShip() {
  ship.alive = false;
  burst(ship.x, ship.y, 50, rgbS(255, 200, 120), 3.0f);
  burst(ship.x, ship.y, 20, rgbS(62, 198, 224), 2.0f);
  shake = 8;
  audio::play(SFX_PLAYER_DIE);
  input::rumble(500, 0xFF, 0xC0);
  lives--;
  if (lives <= 0) gameOver(); else { state = ST_DEAD; stateT = 0; }
}

static void breakRock(Rock& r) {
  r.on = false;
  uint32_t v = ROCK_PTS[r.size];
  addScore(v);
  uint16_t col = r.size == 0 ? rgbS(224, 180, 138) : r.size == 1 ? rgbS(255, 138, 61) : rgbS(255, 216, 74);
  burst(r.x, r.y, 10 + (2 - r.size) * 8, col, 1.5f + (2 - r.size) * 0.4f);
  audio::play(r.size == 0 ? SFX_BIG_EXPLODE : SFX_EXPLODE);
  if (r.size == 0) { shake = max(shake, 4.0f); input::rumble(120, 0x80, 0x40); }
  if (r.size < 2) {
    // copy first: the first spawn can reuse r's own slot
    float x = r.x, y = r.y, base = atan2f(r.vy, r.vx);
    uint8_t size = r.size + 1;
    spawnRock(x, y, size, base + frange(0.4f, 1.0f));
    spawnRock(x, y, size, base - frange(0.4f, 1.0f));
  }
}

static void fireShot(Shot* pool, int n, float x, float y, float a, float sp, float bvx, float bvy, int life) {
  for (int i = 0; i < n; i++)
    if (!pool[i].on) { pool[i] = {true, x, y, cosf(a) * sp + bvx, sinf(a) * sp + bvy, (int16_t)life}; return; }
}

static void updateShip(const Pad& in) {
  if (!ship.alive) return;
  float turn = in.ax;
  if (in.down(BTN_LEFT)) turn = -1;
  if (in.down(BTN_RIGHT)) turn = 1;
  ship.a += turn * 0.085f;
  ship.thrust = in.down(BTN_UP | BTN_B | BTN_L1);
  if (ship.thrust) {
    ship.vx += cosf(ship.a) * 0.09f;
    ship.vy += sinf(ship.a) * 0.09f;
    if ((frameNo & 1) == 0)
      for (auto& p : parts)
        if (!p.on) {
          float a = ship.a + 3.14159f + frange(-0.3f, 0.3f);
          p = {true, ship.x - cosf(ship.a) * 6, ship.y - sinf(ship.a) * 6, cosf(a) * 1.6f + ship.vx, sinf(a) * 1.6f + ship.vy, 12, rgbS(255, 138, 61)};
          break;
        }
  }
  float v = sqrtf(ship.vx * ship.vx + ship.vy * ship.vy);
  if (v > 4.0f) { ship.vx *= 4.0f / v; ship.vy *= 4.0f / v; }
  ship.vx *= 0.992f; ship.vy *= 0.992f;
  ship.x += ship.vx; ship.y += ship.vy;
  wrap(ship.x, ship.y);
  if (ship.invuln) ship.invuln--;
  if (ship.fireCd) ship.fireCd--;
  if (in.hit(BTN_A | BTN_R1 | BTN_R2) && ship.fireCd == 0) {
    fireShot(pshots, 6, ship.x + cosf(ship.a) * 8, ship.y + sinf(ship.a) * 8, ship.a, 5.0f, ship.vx, ship.vy, 48);
    ship.fireCd = 5;
    audio::play(SFX_SHOOT);
  }
  if (in.hit(BTN_X | BTN_Y)) {   // hyperspace: risky jump to a random spot
    burst(ship.x, ship.y, 14, rgbS(184, 243, 255), 1.5f);
    ship.x = frand() * SW; ship.y = FY + frand() * FH; ship.vx = ship.vy = 0;
    burst(ship.x, ship.y, 14, rgbS(184, 243, 255), 1.5f);
    audio::play(SFX_ROTATE);
  }
}

static void updateUfo() {
  if (!ufo.on) {
    if (wave >= 2 && --ufoTimer <= 0) {
      ufo.on = true;
      ufo.small = wave >= 4 && (rnd() % 2);
      int dirn = rnd() % 2 ? 1 : -1;
      ufo.x = dirn > 0 ? -16 : SW + 16;
      ufo.baseY = FY + 30 + frand() * (FH - 60);
      ufo.vx = dirn * (ufo.small ? 1.3f : 0.9f) * arcade::speed();
      ufo.t = 0; ufo.fireT = arcade::frames(60);
    }
    return;
  }
  ufo.t += 1;
  ufo.x += ufo.vx;
  ufo.y = ufo.baseY + 24 * sinf(ufo.t * 0.03f);
  if ((frameNo % 16) == 0) audio::play(SFX_UFO);
  if (--ufo.fireT <= 0 && ship.alive) {
    ufo.fireT = arcade::frames(ufo.small ? 55 : 80);
    float a = ufo.small ? atan2f(wdy(ufo.y, ship.y), wdx(ufo.x, ship.x)) + frange(-0.15f, 0.15f) : frand() * 6.283f;
    fireShot(eshots, 6, ufo.x, ufo.y, a, 2.6f * arcade::speed(), 0, 0, 90);
    audio::play(SFX_SHOOT);
  }
  if (ufo.x < -24 || ufo.x > SW + 24) { ufo.on = false; ufoTimer = arcade::frames(800 + rnd() % 700); }
}

static void killUfo() {
  uint32_t v = ufo.small ? 1000 : 200;
  addScore(v);
  popup((int)ufo.x - 8, (int)ufo.y - 12, v);
  burst(ufo.x, ufo.y, 40, rgbS(255, 123, 213), 2.5f);
  audio::play(SFX_BIG_EXPLODE);
  ufo.on = false; ufoTimer = arcade::frames(900 + rnd() % 700);
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  updateShip(in);
  updateUfo();
  int alive = 0;
  for (auto& r : rocks) {
    if (!r.on) continue;
    alive++;
    r.x += r.vx; r.y += r.vy; r.a += r.va;
    wrap(r.x, r.y);
    if (r.flash) r.flash--;
    if (ship.alive && !ship.invuln && hitCircle(r.x, r.y, ship.x, ship.y, ROCK_R[r.size] * 0.9f + 5)) {
      breakRock(r);
      killShip();
    }
  }
  for (auto& s : pshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy; wrap(s.x, s.y);
    if (--s.life <= 0) { s.on = false; continue; }
    for (auto& r : rocks)
      if (r.on && hitCircle(r.x, r.y, s.x, s.y, ROCK_R[r.size] + 1)) { s.on = false; breakRock(r); break; }
    if (s.on && ufo.on && hitCircle(ufo.x, ufo.y, s.x, s.y, ufo.small ? 7 : 11)) { s.on = false; killUfo(); }
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy; wrap(s.x, s.y);
    if (--s.life <= 0) { s.on = false; continue; }
    if (ship.alive && !ship.invuln && hitCircle(ship.x, ship.y, s.x, s.y, 6)) { s.on = false; killShip(); }
  }
  if (ufo.on && ship.alive && !ship.invuln && hitCircle(ufo.x, ufo.y, ship.x, ship.y, 14)) { killUfo(); killShip(); }
  if (state == ST_PLAY && alive == 0 && !ufo.on) {
    state = ST_CLEAR; stateT = 0;
    addScore(250 * wave);
    audio::music(SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.96f; p.vy *= 0.96f; if (--p.life == 0) p.on = false; }
  for (auto& p : popups) if (p.on) { if ((p.t & 3) == 0) p.y--; if (--p.t <= 0) p.on = false; }
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.86f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      for (auto& r : rocks) if (r.on) { r.x += r.vx; r.y += r.vy; r.a += r.va; wrap(r.x, r.y); }
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_DEAD: {
      // keep the world moving; respawn once the centre is clear
      updatePlay(in);
      bool clear = true;
      for (auto& r : rocks) if (r.on && hitCircle(r.x, r.y, SW / 2, FY + FH / 2, ROCK_R[r.size] + 40)) clear = false;
      if (state == ST_DEAD && stateT > 90 && (clear || stateT > 400)) { resetShip(); state = ST_PLAY; }
      break;
    }
    case ST_CLEAR:
      updateShip(in);
      for (auto& s : pshots) if (s.on) { s.x += s.vx; s.y += s.vy; wrap(s.x, s.y); if (--s.life <= 0) s.on = false; }
      if (stateT > 150) { wave++; startWave(); audio::music(SONG_PLAY); }
      break;
    case ST_OVER:
      for (auto& r : rocks) if (r.on) { r.x += r.vx; r.y += r.vy; r.a += r.va; wrap(r.x, r.y); }
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ draw
static uint16_t bgRow[SH];
static void buildBackground() {
  for (int y = 0; y < SH; y++) {
    float t = (float)y / SH;
    bgRow[y] = t < 0.5f ? lerpS(4, 4, 14, 14, 8, 34, t / 0.5f) : lerpS(14, 8, 34, 6, 14, 30, (t - 0.5f) / 0.5f);
  }
  for (auto& s : stars) { s.x = rnd() % SW; s.y = FY + rnd() % FH; s.b = rnd() % 3; }
}

static void drawBackground() {
  for (int r = 0; r < STRIP; r++) {
    uint16_t c = bgRow[Y0 + r];
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) d[x] = c;
  }
  static const uint16_t sc[3] = {rgbS(60, 60, 110), rgbS(140, 140, 200), rgbS(255, 255, 255)};
  for (auto& s : stars) if (!(s.b == 0 && ((frameNo + s.x * 5) & 63) < 12)) pset(s.x, s.y, sc[s.b]);
}

// draws something at (x,y) and at its wrapped copies when it overlaps an edge
template <typename F>
static void drawWrapped(float x, float y, float r, F fn) {
  int xs[2] = {0, 0}, ys[2] = {0, 0}, nx = 1, ny = 1;
  if (x - r < 0) xs[nx++] = SW; else if (x + r >= SW) xs[nx++] = -SW;
  if (y - r < FY) ys[ny++] = FH; else if (y + r >= SH) ys[ny++] = -FH;
  for (int i = 0; i < nx; i++)
    for (int j = 0; j < ny; j++) fn((int)x + xs[i] + shakeX, (int)y + ys[j] + shakeY);
}

static void drawRock(const Rock& r) {
  float R = ROCK_R[r.size];
  drawWrapped(r.x, r.y, R + 2, [&](int cx, int cy) {
    if (!rowsVisible(cy - (int)R - 3, 2 * (int)R + 6)) return;
    int px[NV], py[NV];
    for (int i = 0; i < NV; i++) {
      float a = r.a + i * 6.2832f / NV, rr = R * (1.0f + r.shape[i] / 100.0f);
      px[i] = cx + (int)(cosf(a) * rr); py[i] = cy + (int)(sinf(a) * rr);
    }
    disc(cx, cy, (int)(R * 0.72f), c565(56, 40, 44));
    disc(cx - (int)(R * 0.2f), cy - (int)(R * 0.2f), (int)(R * 0.35f), c565(82, 62, 60));
    uint16_t oc = r.flash ? c565(255, 255, 255) : r.size == 0 ? c565(224, 180, 138) : r.size == 1 ? c565(255, 170, 110) : c565(255, 216, 74);
    for (int i = 0; i < NV; i++) line(px[i], py[i], px[(i + 1) % NV], py[(i + 1) % NV], oc);
  });
}

static void drawShip() {
  if (!ship.alive || (ship.invuln && (ship.invuln & 4))) return;
  float ca = cosf(ship.a), sa = sinf(ship.a);
  drawWrapped(ship.x, ship.y, 10, [&](int cx, int cy) {
    if (!rowsVisible(cy - 12, 24)) return;
    auto P = [&](float fx, float fy, int& ox, int& oy) { ox = cx + (int)(fx * ca - fy * sa); oy = cy + (int)(fx * sa + fy * ca); };
    int nx, ny, lx, ly, rx, ry, bx, by;
    P(9, 0, nx, ny); P(-6, -6, lx, ly); P(-6, 6, rx, ry); P(-3, 0, bx, by);
    uint16_t hull = c565(184, 243, 255), edge = c565(62, 198, 224);
    line(nx, ny, lx, ly, hull); line(nx, ny, rx, ry, hull);
    line(lx, ly, bx, by, edge); line(rx, ry, bx, by, edge);
    int wx, wy; P(2, 0, wx, wy);
    disc(wx, wy, 1, c565(255, 123, 213));
    if (ship.thrust && (frameNo & 2)) {
      int fx, fy; P(-11 - (int)(frameNo & 1) * 2, 0, fx, fy);
      int f1x, f1y, f2x, f2y; P(-5, -3, f1x, f1y); P(-5, 3, f2x, f2y);
      line(f1x, f1y, fx, fy, c565(255, 216, 74)); line(f2x, f2y, fx, fy, c565(255, 138, 61));
    }
  });
}

static void drawUfo() {
  if (!ufo.on) return;
  int w = ufo.small ? 9 : 14;
  int x = (int)ufo.x + shakeX, y = (int)ufo.y + shakeY;
  if (!rowsVisible(y - 8, 16)) return;
  uint16_t body = ufo.small ? rgbS(255, 123, 213) : rgbS(194, 58, 214), dark = rgbS(74, 29, 107);
  rectf(x - w, y, 2 * w, 3, body);
  rectf(x - w + 2, y + 3, 2 * w - 4, 2, dark);
  rectf(x - w / 2, y - 4, w, 4, rgbS(184, 243, 255));
  rectf(x - w / 2 + 1, y - 5, w - 2, 1, rgbS(184, 243, 255));
  for (int i = -1; i <= 1; i++) pset(x + i * w / 2, y + 1, ((frameNo >> 3) + i) & 1 ? rgbS(255, 216, 74) : rgbS(255, 255, 255));
}

static void drawHud() {
  if (Y0 != 0) return;
  rectf(0, 0, SW, 12, rgbS(6, 6, 20));
  rectf(0, 12, SW, 1, rgbS(51, 48, 122));
  textf(4, 2, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
  textf(66, 2, c565(184, 243, 255), 1, top_left, "WAVE %d", wave);
  textf(142, 2, c565(255, 216, 74), 1, top_left, "HI %07lu", (unsigned long)hiscore);
  for (int i = 0; i < min(lives - 1, 6); i++) {
    int x = SW - 10 - i * 11;
    line(x, 2, x - 4, 10, c565(184, 243, 255)); line(x, 2, x + 4, 10, c565(184, 243, 255));
    line(x - 4, 10, x + 4, 10, c565(62, 198, 224));
  }
}

static void draw() {
  drawBackground();
  for (auto& r : rocks) if (r.on) drawRock(r);
  if (state == ST_TITLE) {
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 36);
    text("SPLIT THE ROCKS, DODGE THE HUNTERS", SW / 2, 74, c565(184, 243, 255), 1, top_center);
    textf(SW / 2, 16, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 106, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 110, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(134);
    if (rowsVisible(152, 56)) shade(40, 152, 240, 56);
    text("\x11\x10 TURN    \x1e/B THRUST", SW / 2, 158, c565(180, 194, 220), 1, top_center);
    text("A FIRE    X HYPERSPACE", SW / 2, 172, c565(180, 194, 220), 1, top_center);
    text("START PAUSE", SW / 2, 186, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 214, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawUfo();
  drawShip();
  for (auto& s : pshots) if (s.on) { rectf((int)s.x - 1 + shakeX, (int)s.y - 1 + shakeY, 2, 2, rgbS(184, 243, 255)); pset((int)s.x + shakeX, (int)s.y + shakeY, rgbS(255, 255, 255)); }
  for (auto& s : eshots) if (s.on) rectf((int)s.x - 1 + shakeX, (int)s.y - 1 + shakeY, 3, 3, (frameNo & 4) ? rgbS(255, 123, 213) : rgbS(255, 216, 74));
  for (auto& p : parts) if (p.on) { pset((int)p.x + shakeX, (int)p.y + shakeY, p.col); if (p.life > 24) pset((int)p.x + 1 + shakeX, (int)p.y + shakeY, p.col); }
  for (auto& p : popups) if (p.on) text(p.txt, p.x, p.y, (p.t & 8) ? c565(255, 216, 74) : TFT_WHITE, 1, top_left);
  drawHud();
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_PLAY && stateT < 90) textf(SW / 2, 96, TFT_WHITE, 2, top_center, "WAVE %d", wave);
  if (state == ST_CLEAR) {
    text("WAVE CLEARED!", SW / 2, 96, c565(182, 255, 110), 2, top_center);
    textf(SW / 2, 118, TFT_WHITE, 1, top_center, "BONUS %d", 250 * wave);
  }
  if (state == ST_OVER) {
    if (rowsVisible(84, 70)) { shade(60, 84, 200, 70); shade(60, 84, 200, 70); }
    text("GAME OVER", SW / 2, 92, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 122, c565(255, 216, 74), 1, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 138, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("astrodrift", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildBackground();
  hiscore = arcade::loadHi(HI_DEFAULT);
  // drifting rocks behind the title
  ship.x = -100; ship.y = -100;
  for (int i = 0; i < 5; i++) spawnRock(frand() * SW, FY + frand() * FH, i % 3);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
