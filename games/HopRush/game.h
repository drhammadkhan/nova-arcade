// =====================================================================
//  HOP RUSH  -  a road-and-river crossing game (Nova Arcade)
//  Guide the little hop-bot across five lanes of traffic and a river of
//  drifting logs and lily pads, then park it in one of the five docks.
//  Fill every dock to clear the level; each level runs a little faster.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace hrm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 C */ {72, 1, 74, 76, 79, 1, 76, 1, 74, 1, 72, 1, 74, 1, 76, 1},
  /*1 F */ {77, 1, 76, 77, 81, 1, 77, 1, 76, 1, 74, 1, 72, 1, 1, 1},
  /*2 G */ {79, 1, 77, 76, 74, 1, 76, 1, 77, 1, 79, 1, 74, 1, 1, 1},
  /*3 C2*/ {76, 1, 79, 1, 84, 1, 83, 1, 79, 1, 76, 1, 72, 1, 1, 1},
  /*4 t */ {72, 1, 1, 76, 1, 1, 79, 1, 1, 1, 77, 1, 76, 1, 74, 1},
  /*5 ov*/ {72, 1, 71, 1, 69, 1, 67, 1, 65, 1, 64, 1, 60, 1, 0, 0},
  /*6 cl*/ {72, 76, 79, 84, 1, 1, 79, 84, 88, 1, 1, 1, 1, 1, 0, 0},
  /*7 hm*/ {84, 88, 91, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const char* const DRUMS[] = {"K.h.S.h.K.h.S.hh", "K.hhS.h.KKh.S.h.", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{C_, 4, 2}, {F_, -1, 2}, {G_, 4, 2}, {C_, -1, 2}};
static const Bar GAME_BARS[] = {{C_, 0, 0}, {F_, 1, 0}, {G_, 2, 0}, {C_, 3, 1},
                                {AM, 0, 0}, {F_, 1, 0}, {G_, 2, 0}, {C_, 3, 1}};
static const Bar OVER_BARS[] = {{C_, 5, 3}, {C_, -1, 3}};
static const Bar CLEAR_BARS[] = {{C_, 6, 3}, {C_, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 112, true, false, 0x30},
  {GAME_BARS, 8, 140, true, false, 0},
  {OVER_BARS, 2, 100, false, false, 0},
  {CLEAR_BARS, 2, 140, false, false, 0},
};
}  // namespace hrm
static const audio::Music MUSIC = {audio::STD_CHORDS, hrm::LEADS, hrm::DRUMS, hrm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_GAME, SONG_OVER, SONG_CLEAR };

// ------------------------------------------------------------ layout
static const int T = 16;                   // tile size
static const int TOP = 16;                 // screen y of row 0
static const int NROWS = 13;               // 0 docks, 1-5 river, 6 median, 7-11 road, 12 start
static const int START_ROW = 12, MEDIAN_ROW = 6;
static const int NDOCK = 5;
static inline int rowY(int r) { return TOP + r * T; }
static inline int dockX(int i) { return 24 + i * 64; }    // dock inlets are 32 px wide
static inline bool isRiver(int r) { return r >= 1 && r <= 5; }
static inline bool isRoad(int r) { return r >= 7 && r <= 11; }

enum Kind : uint8_t { K_LOG, K_PAD, K_CAR, K_TRUCK, K_DOZER, K_RACER };
struct Lane { uint8_t row; float speed; uint8_t len, gap; Kind kind; uint8_t col; };
static const Lane LANES[] = {
  {1, 0.45f, 4, 3, K_LOG, 0},   {2, -0.70f, 3, 2, K_PAD, 0},  {3, 0.95f, 6, 4, K_LOG, 0},
  {4, 0.55f, 3, 3, K_LOG, 0},   {5, -0.60f, 2, 2, K_PAD, 0},
  {7, -0.85f, 3, 5, K_TRUCK, 0}, {8, 1.50f, 1, 7, K_RACER, 1}, {9, -0.70f, 1, 3, K_CAR, 2},
  {10, 0.55f, 2, 4, K_DOZER, 3}, {11, -0.80f, 1, 4, K_CAR, 4},
};
static const int NLANES = sizeof(LANES) / sizeof(LANES[0]);
static const uint8_t CARCOL[5][3] = {{235, 60, 80}, {255, 216, 74}, {62, 198, 224}, {250, 145, 40}, {194, 58, 214}};
static float laneOff[NLANES];
static float laneSpd[NLANES];

// objects of a lane repeat every `period` px over a loop `span` px wide
static inline int lanePeriod(const Lane& l) { return (l.len + l.gap) * T; }
static inline int laneCount(const Lane& l) { return (SW + l.len * T + lanePeriod(l) - 1) / lanePeriod(l) + 1; }
static inline int laneSpan(const Lane& l) { return laneCount(l) * lanePeriod(l); }
static inline float objX(int li, int k) {
  const Lane& l = LANES[li];
  float span = laneSpan(l);
  float x = fmodf(laneOff[li] + k * lanePeriod(l), span);
  if (x < 0) x += span;
  return x - l.len * T;
}

// ------------------------------------------------------------ state
static float px = 0;                 // player left x
static int prow = START_ROW;
static int hopT = 0, hopDir = 0;     // hop animation
static float hopFromX = 0; static int hopFromRow = 0;
static int bestRow = START_ROW;
static bool docks[NDOCK];
static int gemDock = -1, gemT = 0;
static int lives = 3, level = 1, timeLeft = 0, timeMax = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 3000;
static bool newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_HOME, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static int deathKind = 0;   // 0 squashed, 1 splash
static float camShake = 0;
struct Part { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[200];
struct Popup { bool on; int x, y, t; char txt[12]; };
static Popup popups[4];

// from level 2 the lily pads in lane 1 sink for a moment every five seconds
static const int SINK_LANE = 1;
static int padCycle = 0;
static inline bool padSunk(int li) { return level >= 2 && li == SINK_LANE && padCycle >= 220 && padCycle < 290; }
static inline bool padWarn(int li) { return level >= 2 && li == SINK_LANE && padCycle >= 180 && padCycle < 220; }

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, uint16_t col, float sp, float grav = 0.05f) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s - grav * 20, (uint8_t)frange(14, 30), col}; break; }
}
static void popup(int x, int y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 60, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}

static void setupLanes() {
  float mult = (1.0f + 0.14f * min(level - 1, 8)) * arcade::speed();
  for (int i = 0; i < NLANES; i++) { laneSpd[i] = LANES[i].speed * mult; laneOff[i] = frand() * laneSpan(LANES[i]); }
}

static void startLife() {
  px = SW / 2 - 8; prow = START_ROW; hopT = 0; bestRow = START_ROW;
  timeMax = timeLeft = arcade::frames(60 * 30);
  state = ST_PLAY; stateT = 0;
}

static void startLevel() {
  memset(docks, 0, sizeof(docks));
  gemDock = -1; gemT = 400;
  setupLanes();
  startLife();
  audio::music(SONG_GAME);
}

static void resetGame() {
  score = 0; lives = 3; level = 1; newHi = false;
  memset(parts, 0, sizeof(parts)); memset(popups, 0, sizeof(popups));
  startLevel();
}

static void die(int kind) {
  deathKind = kind;
  float cx = px + 8, cy = rowY(prow) + 8;
  if (kind) { burst(cx, cy, 24, rgbS(184, 243, 255), 1.8f); audio::play(SFX_LAND); }
  else { burst(cx, cy, 30, rgbS(62, 198, 224), 2.4f); audio::play(SFX_PLAYER_DIE); camShake = 6; }
  input::rumble(350, 0xA0, 0x80);
  lives--;
  state = ST_DEAD; stateT = 0;
}

// platform under the player's centre in river lanes; returns lane index or -1
static int platformUnder(float cx, int row) {
  for (int li = 0; li < NLANES; li++) {
    if (LANES[li].row != row) continue;
    if (padSunk(li)) return -1;
    for (int k = 0; k < laneCount(LANES[li]); k++) {
      float x = objX(li, k);
      if (cx >= x + 2 && cx <= x + LANES[li].len * T - 2) return li;
    }
    return -1;
  }
  return -1;
}

static bool hitByTraffic(float x0, int row) {
  for (int li = 0; li < NLANES; li++) {
    if (LANES[li].row != row) continue;
    for (int k = 0; k < laneCount(LANES[li]); k++) {
      float x = objX(li, k);
      if (x0 + 12 > x + 1 && x0 + 4 < x + LANES[li].len * T - 1) return true;
    }
  }
  return false;
}

static void reachHome() {
  float cx = px + 8;
  for (int i = 0; i < NDOCK; i++) {
    if (cx >= dockX(i) + 2 && cx <= dockX(i) + 30) {
      if (docks[i]) break;
      docks[i] = true;
      uint32_t v = 50 + 10 * (timeLeft * 30 / max(1, timeMax));
      if (gemDock == i) { v += 200; gemDock = -1; audio::play(SFX_POWERUP); }
      else audio::play(SFX_SELECT);
      addScore(v);
      popup(dockX(i) + 4, rowY(0) + 20, v);
      burst(dockX(i) + 16, rowY(0) + 8, 20, rgbS(255, 216, 74), 1.6f);
      int n = 0;
      for (bool d : docks) n += d;
      if (n == NDOCK) { addScore(1000 * level); state = ST_CLEAR; audio::music(SONG_CLEAR); }
      else state = ST_HOME;
      stateT = 0;
      return;
    }
  }
  die(0);   // bumped into the bank or an occupied dock
}

static void landed() {
  if (prow < bestRow) { bestRow = prow; addScore(10); }
  if (prow == 0) { reachHome(); return; }
  if (isRiver(prow) && platformUnder(px + 8, prow) < 0) { die(1); return; }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  if (--timeLeft <= 0) { die(0); return; }
  if (hopT == 0) {
    int dx = 0, dy = 0;
    if (in.hit(BTN_UP)) dy = -1;
    else if (in.hit(BTN_DOWN)) dy = 1;
    else if (in.hit(BTN_LEFT)) dx = -1;
    else if (in.hit(BTN_RIGHT)) dx = 1;
    if (dy == 1 && prow == START_ROW) dy = 0;
    if ((dx < 0 && px < 4) || (dx > 0 && px > SW - 20)) dx = 0;
    if (dx || dy) {
      hopFromX = px; hopFromRow = prow; hopDir = dx ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
      prow += dy;
      px += dx * T;
      hopT = 7;
      audio::play(SFX_MOVE);
    }
  }
  if (hopT) { if (--hopT == 0) { landed(); if (state != ST_PLAY) return; } }
  // ride the river
  if (isRiver(prow) && hopT == 0) {
    int li = platformUnder(px + 8, prow);
    if (li < 0) { die(1); return; }
    px += laneSpd[li];
    if (px < -6 || px > SW - 10) { die(1); return; }
  }
  // mid-hop the bot counts as being in the row it's leaving until halfway
  int hitRow = hopT > 3 ? hopFromRow : prow;
  float hitX = hopT > 3 ? hopFromX : px;
  if (isRoad(hitRow) && hitByTraffic(hitX, hitRow)) { die(0); return; }
}

static void updateWorld() {
  for (int i = 0; i < NLANES; i++) laneOff[i] += laneSpd[i];
  padCycle = (padCycle + 1) % 300;
  if (gemDock < 0) {
    if (--gemT <= 0) {
      int d = rnd() % NDOCK;
      if (!docks[d]) { gemDock = d; gemT = arcade::frames(360); } else gemT = 60;
    }
  } else if (--gemT <= 0) { gemDock = -1; gemT = 500; }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vy += 0.05f; if (--p.life == 0) p.on = false; }
  for (auto& p : popups) if (p.on) { if ((p.t & 3) == 0) p.y--; if (--p.t <= 0) p.on = false; }
  if (camShake > 0.3f) camShake *= 0.85f; else camShake = 0;
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      updateWorld();
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_PLAY: updateWorld(); updatePlay(in); break;
    case ST_DEAD:
      updateWorld();
      if (stateT > 80) {
        if (lives <= 0) { state = ST_OVER; stateT = 0; audio::music(SONG_OVER); if (newHi) arcade::saveHi(hiscore); }
        else startLife();
      }
      break;
    case ST_HOME:
      updateWorld();
      if (stateT > 40) startLife();
      break;
    case ST_CLEAR:
      if (stateT > 150) { level++; startLevel(); }
      break;
    case ST_OVER:
      updateWorld();
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); setupLanes(); }
      break;
  }
}

// ------------------------------------------------------------ draw
static void drawBackground() {
  uint16_t water1 = rgbS(24, 60, 130), water2 = rgbS(36, 84, 170), road = rgbS(34, 34, 48), roadHi = rgbS(44, 44, 60);
  uint16_t pave = rgbS(74, 29, 107), paveHi = rgbS(122, 61, 184), grass = rgbS(30, 107, 58), grassHi = rgbS(79, 214, 107);
  uint16_t sky = rgbS(10, 8, 25);
  int wv = frameNo / 3;
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t* d = B + r * SW;
    int row = (y - TOP) / T, ry = (y - TOP) % T;
    if (y < TOP || row >= NROWS) { for (int x = 0; x < SW; x++) d[x] = sky; continue; }
    if (row == 0) {
      for (int x = 0; x < SW; x++) d[x] = (((x >> 2) + (y >> 2)) & 3) == 0 ? grassHi : grass;
      for (int i = 0; i < NDOCK; i++) if (ry >= 3) for (int x = dockX(i); x < dockX(i) + 32; x++) d[x] = ((x + wv + y) & 7) == 0 ? water2 : water1;
    } else if (isRiver(row)) {
      for (int x = 0; x < SW; x++) d[x] = (((x + wv * (row & 1 ? 1 : -1) + ry * 5) & 31) == 0 && (ry == 4 || ry == 11)) ? water2 : water1;
    } else if (row == MEDIAN_ROW || row == START_ROW) {
      for (int x = 0; x < SW; x++) d[x] = (ry == 0 || ((x & 15) == 0) || ry == 8) ? paveHi : pave;
    } else {
      for (int x = 0; x < SW; x++) d[x] = road;
      if (ry == 0 && row > 7) for (int x = 0; x < SW; x++) if ((x & 15) < 8) d[x] = roadHi;
    }
  }
}

static void drawLog(int x, int y, int w) {
  if (!rowsVisible(y, T)) return;
  uint16_t bark = rgbS(122, 74, 58), dark = rgbS(70, 40, 30), light = rgbS(170, 110, 80), ring = rgbS(224, 180, 138);
  rectf(x + 2, y + 2, w - 4, T - 4, bark);
  rectf(x + 3, y + 3, w - 6, 2, light);
  rectf(x + 2, y + T - 4, w - 4, 2, dark);
  for (int k = x + 10; k < x + w - 6; k += 13) rectf(k, y + 7, 5, 1, dark);
  rectf(x, y + 3, 3, T - 6, ring); rectf(x + w - 3, y + 3, 3, T - 6, ring);
}

static void drawPad(int x, int y, int w, bool warn, bool sunk) {
  if (sunk) { for (int k = 0; k < w / T; k++) circle(x + k * T + 8, y + 8, 5, c565(62, 140, 200)); return; }
  for (int k = 0; k < w / T; k++) {
    int cx = x + k * T + 8, cy = y + 8;
    uint16_t c = warn && (frameNo & 4) ? c565(30, 107, 58) : c565(79, 214, 107);
    disc(cx, cy, 7, c565(30, 107, 58));
    disc(cx, cy, 6, c);
    line(cx, cy, cx + 5, cy - 3, c565(30, 107, 58));
    pset(cx - 2, cy - 2, rgbS(182, 255, 110));
  }
}

static void drawVehicle(Kind k, int x, int y, int w, int dir, uint8_t col) {
  if (!rowsVisible(y, T)) return;
  const uint8_t* c = CARCOL[col];
  uint16_t body = rgbS(c[0], c[1], c[2]), dark = rgbS(c[0] / 2, c[1] / 2, c[2] / 2), glass = rgbS(184, 243, 255);
  uint16_t tyre = rgbS(13, 11, 30), light = rgbS(255, 255, 220);
  int front = dir > 0 ? x + w - 3 : x;
  switch (k) {
    case K_TRUCK: {
      uint16_t box = rgbS(180, 194, 220), boxd = rgbS(115, 132, 168);
      int cab = dir > 0 ? x + w - 14 : x;
      int bx = dir > 0 ? x + 1 : x + 15;
      rectf(bx, y + 2, w - 16, T - 4, box); rectf(bx, y + T - 5, w - 16, 2, boxd);
      rectf(cab, y + 3, 13, T - 6, body); rectf(cab + (dir > 0 ? 7 : 2), y + 4, 4, T - 8, glass);
      for (int t = x + 4; t < x + w - 4; t += 12) { rectf(t, y + 1, 5, 2, tyre); rectf(t, y + T - 3, 5, 2, tyre); }
      break;
    }
    case K_DOZER:
      rectf(x + 3, y + 3, w - 6, T - 6, rgbS(255, 216, 74));
      rectf(x + 3, y + 3, w - 6, 2, rgbS(255, 240, 160));
      rectf(dir > 0 ? x + w - 4 : x, y + 1, 4, T - 2, rgbS(180, 194, 220));
      rectf(x + 8, y + 5, 8, T - 10, rgbS(61, 74, 102));
      rectf(x + 3, y + 1, w - 6, 2, tyre); rectf(x + 3, y + T - 3, w - 6, 2, tyre);
      break;
    default:
      rectf(x + 1, y + 3, w - 2, T - 6, body);
      rectf(x + 1, y + T - 5, w - 2, 2, dark);
      rectf(x + (dir > 0 ? 5 : 4), y + 5, 6, T - 10, glass);
      rectf(x + 2, y + 1, 4, 2, tyre); rectf(x + w - 6, y + 1, 4, 2, tyre);
      rectf(x + 2, y + T - 3, 4, 2, tyre); rectf(x + w - 6, y + T - 3, 4, 2, tyre);
      if (k == K_RACER) rectf(x + 1, y + 7, w - 2, 2, rgbS(255, 255, 255));
      rectf(front, y + 4, 3, 2, light); rectf(front, y + T - 6, 3, 2, light);
      break;
  }
}

static void drawBot(int x, int y, int dirn, bool squash) {
  if (!rowsVisible(y, T)) return;
  uint16_t body = rgbS(62, 198, 224), dark = rgbS(31, 111, 139), hi = rgbS(184, 243, 255), eye = rgbS(255, 255, 255), pupil = rgbS(13, 11, 30);
  if (squash) { rectf(x + 1, y + 10, 14, 4, body); rectf(x + 3, y + 9, 10, 1, hi); return; }
  rectf(x + 3, y + 4, 10, 9, body);
  rectf(x + 3, y + 4, 10, 2, hi);
  rectf(x + 3, y + 11, 10, 2, dark);
  rectf(x + 7, y + 1, 2, 3, dark); rectf(x + 6, y, 4, 2, rgbS(255, 123, 213));
  rectf(x + 1, y + 12, 4, 3, dark); rectf(x + 11, y + 12, 4, 3, dark);   // feet
  int ex = dirn == 1 ? 1 : dirn == 3 ? -1 : 0, ey = dirn == 2 ? 1 : 0;
  rectf(x + 4 + ex, y + 6, 3, 3, eye); rectf(x + 9 + ex, y + 6, 3, 3, eye);
  pset(x + 5 + ex + (ex > 0), y + 7 + ey - (dirn == 0), pupil); pset(x + 10 + ex + (ex > 0), y + 7 + ey - (dirn == 0), pupil);
}

static void drawHud() {
  if (Y0 == 0) {
    rectf(0, 0, SW, 14, rgbS(10, 8, 25));
    textf(4, 3, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
    textf(62, 3, c565(184, 243, 255), 1, top_left, "LV %d", level);
    textf(142, 3, c565(255, 216, 74), 1, top_left, "HI %07lu", (unsigned long)hiscore);
  }
  int y = rowY(NROWS) + 2;
  if (!rowsVisible(y, 14)) return;
  for (int i = 0; i < min(lives - 1, 5); i++) {
    int x = 4 + i * 12;
    rectf(x + 1, y + 3, 8, 7, rgbS(62, 198, 224)); rectf(x + 2, y + 5, 2, 2, rgbS(255, 255, 255)); rectf(x + 6, y + 5, 2, 2, rgbS(255, 255, 255));
  }
  text("TIME", 238, y + 3, c565(184, 243, 255), 1, top_right, false);
  int w = timeMax ? 72 * timeLeft / timeMax : 0;
  rect(242, y + 3, 74, 8, rgbS(61, 74, 102));
  uint16_t tc = timeLeft < timeMax / 4 ? ((frameNo & 8) ? rgbS(235, 60, 80) : rgbS(255, 216, 74)) : rgbS(79, 214, 107);
  rectf(243, y + 4, w, 6, tc);
}

static void drawWorld() {
  for (int li = 0; li < NLANES; li++) {
    const Lane& l = LANES[li];
    int y = rowY(l.row);
    if (!rowsVisible(y, T)) continue;
    int w = l.len * T, dir = laneSpd[li] > 0 ? 1 : -1;
    for (int k = 0; k < laneCount(l); k++) {
      int x = (int)objX(li, k);
      if (x > SW || x + w < 0) continue;
      if (l.kind == K_LOG) drawLog(x, y, w);
      else if (l.kind == K_PAD) drawPad(x, y, w, padWarn(li), padSunk(li));
      else drawVehicle(l.kind, x, y, w, dir, l.col);
    }
  }
  // docks: parked bots and the bonus gem
  for (int i = 0; i < NDOCK; i++) {
    if (docks[i]) drawBot(dockX(i) + 8, rowY(0) + 1, 2, false);
    else if (gemDock == i && rowsVisible(rowY(0), T)) {
      int cx = dockX(i) + 16, cy = rowY(0) + 9;
      uint16_t gc = (frameNo & 8) ? rgbS(255, 123, 213) : rgbS(255, 216, 74);
      rectf(cx - 1, cy - 5, 3, 11, gc); rectf(cx - 3, cy - 3, 7, 7, gc); rectf(cx - 5, cy - 1, 11, 3, gc);
      pset(cx - 1, cy - 2, rgbS(255, 255, 255));
    }
  }
}

static void draw() {
  drawBackground();
  drawWorld();
  if (state == ST_TITLE) {
    if (rowsVisible(30, 180)) shade(20, 30, 280, 180);
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 40);
    text("CROSS THE ROAD, RIDE THE RIVER", SW / 2, 78, c565(184, 243, 255), 1, top_center);
    textf(SW / 2, 20, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 106, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 110, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(134);
    text("D-PAD HOP   START PAUSE", SW / 2, 160, c565(180, 194, 220), 1, top_center);
    text("FILL ALL FIVE DOCKS", SW / 2, 174, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 196, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawHud();
  int cs = camShake > 0.3f ? (int)frange(-camShake, camShake) : 0;
  if (state == ST_PLAY || state == ST_HOME) {
    int x = (int)px, y = rowY(prow);
    if (hopT) {
      float t = 1.0f - hopT / 7.0f;
      x = (int)(hopFromX + (px - hopFromX) * t);
      y = (int)(rowY(hopFromRow) + (rowY(prow) - rowY(hopFromRow)) * t - sinf(t * 3.14159f) * 4);
    }
    if (state == ST_PLAY) drawBot(x + cs, y, hopDir, false);
  } else if (state == ST_DEAD && stateT < 60) {
    if (deathKind == 0) drawBot((int)px + cs, rowY(prow), hopDir, true);
    else if (stateT < 24) circle((int)px + 8, rowY(prow) + 8, 2 + stateT / 3, c565(184, 243, 255));
  }
  for (auto& p : parts) if (p.on) pset((int)p.x, (int)p.y, p.col);
  for (auto& p : popups) if (p.on) text(p.txt, p.x, p.y, (p.t & 8) ? c565(255, 216, 74) : TFT_WHITE, 1, top_left);
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_PLAY && stateT < 60 && prow == START_ROW) textf(SW / 2, 104, TFT_WHITE, 2, top_center, "LEVEL %d", level);
  if (state == ST_CLEAR) {
    if (rowsVisible(96, 40)) shade(50, 96, 220, 40);
    text("LEVEL CLEAR!", SW / 2, 100, c565(182, 255, 110), 2, top_center);
    textf(SW / 2, 122, TFT_WHITE, 1, top_center, "BONUS %d", 1000 * level);
  }
  if (state == ST_OVER) {
    if (rowsVisible(84, 70)) { shade(60, 84, 200, 70); shade(60, 84, 200, 70); }
    text("GAME OVER", SW / 2, 92, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 122, c565(255, 216, 74), 1, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 138, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("hoprush", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  setupLanes();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
