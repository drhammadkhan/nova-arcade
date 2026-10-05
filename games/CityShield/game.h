// =====================================================================
//  CITY SHIELD  -  a missile-defence game (Nova Arcade)
//  Enemy warheads rain down on six cities. Steer the crosshair and launch
//  interceptors from three bases: X fires from the left, A from the centre
//  (the fastest) and B from the right. Each interceptor bursts into a
//  fireball that takes out anything that flies into it, and anything it
//  destroys bursts too, so a well-placed shot can set off a chain.
//  Later waves bring splitting warheads, bombers and satellites.
//  Art: tools/make_art.py -> art.h
// =====================================================================
#include <ArcadeCore.h>
#include "art.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace csm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 t Am*/ {69, 1, 1, 1, 76, 1, 1, 1, 74, 1, 72, 1, 71, 1, 72, 1},
  /*1 t F */ {77, 1, 1, 1, 76, 1, 1, 1, 72, 1, 1, 1, 69, 1, 1, 1},
  /*2 t G */ {74, 1, 1, 1, 79, 1, 1, 1, 77, 1, 76, 1, 74, 1, 71, 1},
  /*3 t E */ {76, 1, 1, 1, 1, 1, 1, 1, 68, 1, 71, 1, 74, 1, 0, 0},
  /*4 p Am*/ {81, 0, 81, 0, 79, 1, 76, 1, 0, 0, 76, 79, 81, 1, 0, 0},
  /*5 p F */ {84, 1, 83, 1, 81, 1, 77, 1, 0, 0, 77, 1, 79, 1, 81, 1},
  /*6 p G */ {83, 0, 83, 0, 81, 1, 79, 1, 0, 0, 74, 77, 79, 1, 0, 0},
  /*7 p E */ {80, 1, 1, 1, 83, 1, 1, 1, 86, 1, 84, 1, 83, 1, 80, 1},
  /*8 clr*/  {69, 72, 76, 81, 1, 1, 79, 1, 81, 1, 1, 1, 1, 1, 0, 0},
  /*9 ovr*/  {76, 1, 1, 1, 75, 1, 1, 1, 74, 1, 1, 1, 73, 1, 1, 1},
  /*10 ovr2*/{72, 1, 1, 1, 71, 1, 1, 1, 69, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.......K.....h.", "K.hhS.hhK.hhS.hS", "KKh.S.h.KKh.S.hS", "................", "K...K...K...K..."};
static const Bar TITLE_BARS[] = {{AM, 0, 0}, {F_, 1, 0}, {G_, 2, 0}, {E_, 3, 4}};
static const Bar PLAY_BARS[] = {{AM, -1, 1}, {AM, -1, 1}, {F_, -1, 1}, {E_, -1, 2},
                                {AM, 4, 1}, {F_, 5, 1}, {G_, 6, 1}, {E_, 7, 2}};
static const Bar CLEAR_BARS[] = {{A_, 8, 3}, {A_, -1, 3}};
static const Bar OVER_BARS[] = {{AM, 9, 3}, {DM, 10, 3}, {AM, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 84, true, false, 0x30},
  {PLAY_BARS, 8, 132, true, true, 0x40},
  {CLEAR_BARS, 2, 140, false, false, 0},
  {OVER_BARS, 3, 70, false, false, 0},
};
}  // namespace csm
static const audio::Music MUSIC = {audio::STD_CHORDS, csm::LEADS, csm::DRUMS, csm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_PLAY, SONG_CLEAR, SONG_OVER };

// ------------------------------------------------------------ world
static const int AMMO_PER_BASE = 10, LAUNCH_Y = 198;
static const float SHOT_SPEED[3] = {4.2f, 6.2f, 4.2f};   // the centre base is the fastest

struct Warhead { bool on, targeted, mirv; float x0, y0, x, y, vx, vy; int8_t target; uint8_t splitY; };
static Warhead warheads[40];
struct Shot { bool on; float x, y, tx, ty, vx, vy; };
static Shot shots[12];
struct Blast { bool on; float x, y; int t; float maxR; bool big; };
static Blast blasts[40];
struct Flyer { bool on; bool sat; float x, y, vx; int dropT, frame; };
static Flyer flyers[2];
struct Smoke { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Smoke smoke[160];
struct Popup { bool on; float x, y; int t; char txt[12]; };
static Popup popups[8];
struct Star { int16_t x, y; uint8_t b; };
static Star stars[70];

static bool cityAlive[6];
static bool baseAlive[3];
static int ammo[3];
static float crossX = 160, crossY = 110;
static int wave = 1, toSpawn = 0, spawnT = 0, bonusCities = 0, flyerT = 0;
static uint32_t score = 0, hiscore = 0, nextBonus = 10000;
static const uint32_t HI_DEFAULT = 7500;
static bool newHi = false;
static int shake = 0, flash = 0;

enum State { ST_TITLE, ST_INTRO, ST_PLAY, ST_BONUS, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static int bonusAmmo = 0, bonusCity = 0, bonusStep = 0;   // the end-of-wave tally

static inline int mult() { return min(6, (wave + 1) / 2); }
static inline float cityCX(int i) { return CITY_X[i] + CITY_W / 2; }
static inline float groundAt(float x) { return GROUND_Y + GROUND_TOP[constrain((int)x, 0, 319)]; }
static int citiesLeft() { int n = 0; for (bool c : cityAlive) n += c; return n; }

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
  if (score >= nextBonus) { bonusCities++; nextBonus += 10000; arcade::toast("BONUS CITY!"); audio::play(SFX_POWERUP); }
}
static void popup(float x, float y, const char* s) {
  for (auto& p : popups) if (!p.on) { p.on = true; p.x = x; p.y = y; p.t = 50; strncpy(p.txt, s, 11); p.txt[11] = 0; return; }
}
static void puff(float x, float y, int n, uint16_t col, float sp, float rise) {
  for (auto& p : smoke) {
    if (p.on) continue;
    float a = frand() * 6.2832f, v = frange(0.2f, sp);
    p = {true, x, y, cosf(a) * v, sinf(a) * v - rise, (uint8_t)(30 + rnd() % 40), col};
    if (--n <= 0) break;
  }
}
static void blast(float x, float y, float r, bool big = false) {
  for (auto& b : blasts) if (!b.on) { b = {true, x, y, 0, r, big}; return; }
}
static inline float blastR(const Blast& b) {
  if (b.t < 16) return b.maxR * b.t / 16.0f;
  if (b.t < 30) return b.maxR;
  return b.maxR * (46 - b.t) / 16.0f;
}

// ------------------------------------------------------------ spawning
static void targetPoint(int8_t t, float* tx, float* ty) {
  if (t < 6) { *tx = cityCX(t) + frange(-6, 6); *ty = FLAT_Y - 4; }
  else if (t < 9) { *tx = BASE_X[t - 6]; *ty = LAUNCH_Y + 2; }
  else { *tx = frange(10, 310); *ty = groundAt(*tx); }
}
static int8_t pickTarget() {
  for (int tries = 0; tries < 12; tries++) {
    int8_t t = rnd() % 10;
    if (t < 6 && cityAlive[t]) return t;
    if (t >= 6 && t < 9 && baseAlive[t - 6]) return t;
  }
  return 9;
}
static float warheadSpeed() { return min(1.7f, 0.30f + wave * 0.055f) * arcade::speed(); }

static void launchWarhead(float x, float y, int8_t target, bool canMirv) {
  for (auto& w : warheads) {
    if (w.on) continue;
    float tx, ty; targetPoint(target, &tx, &ty);
    float dx = tx - x, dy = ty - y, d = sqrtf(dx * dx + dy * dy);
    float v = warheadSpeed() * frange(0.85f, 1.15f);
    w = {true, false, canMirv && wave >= 3 && rnd() % 100 < (uint32_t)min(40, wave * 5), x, y, x, y, dx / d * v, dy / d * v, target, (uint8_t)(70 + rnd() % 50)};
    return;
  }
}

static void buildSky();
static void startWave() {
  buildSky();
  memset(warheads, 0, sizeof(warheads)); memset(shots, 0, sizeof(shots)); memset(blasts, 0, sizeof(blasts));
  memset(flyers, 0, sizeof(flyers));
  for (int i = 0; i < 3; i++) { baseAlive[i] = true; ammo[i] = AMMO_PER_BASE; }
  // spend banked bonus cities on the ruins
  for (int i = 0; i < 6 && bonusCities; i++) if (!cityAlive[i]) { cityAlive[i] = true; bonusCities--; }
  toSpawn = 10 + wave * 3;
  spawnT = 90;
  flyerT = 400 + rnd() % 300;
  state = ST_INTRO; stateT = 0;
  audio::music(SONG_NONE);
  audio::play(SFX_WARNING);
}

static void resetGame() {
  score = 0; newHi = false; wave = 1; bonusCities = 0; nextBonus = 10000;
  for (bool& c : cityAlive) c = true;
  crossX = 160; crossY = 110;
  hiscore = arcade::loadHi(HI_DEFAULT);
  startWave();
}

// ------------------------------------------------------------ firing
static bool fire(int base, float tx, float ty) {
  // fall back to the nearest base that can still fire
  if (!baseAlive[base] || !ammo[base]) {
    int best = -1; float bd = 1e9;
    for (int i = 0; i < 3; i++) if (baseAlive[i] && ammo[i] && fabsf(BASE_X[i] - tx) < bd) { bd = fabsf(BASE_X[i] - tx); best = i; }
    if (best < 0) { audio::play(SFX_MOVE); return false; }
    base = best;
  }
  for (auto& s : shots) {
    if (s.on) continue;
    float x = BASE_X[base], y = LAUNCH_Y - 8;
    float dx = tx - x, dy = ty - y, d = max(1.0f, sqrtf(dx * dx + dy * dy));
    s = {true, x, y, tx, ty, dx / d * SHOT_SPEED[base], dy / d * SHOT_SPEED[base]};
    ammo[base]--;
    audio::play(SFX_LAUNCH);
    return true;
  }
  return false;
}

// Attract-mode / test autopilot: lead each warhead and fire from the best base.
static void autopilot() {
  for (auto& w : warheads) {
    if (!w.on || w.targeted || w.y < 40) continue;
    int base = 1; float bd = 1e9;
    for (int i = 0; i < 3; i++) if (baseAlive[i] && ammo[i] && fabsf(BASE_X[i] - w.x) < bd) { bd = fabsf(BASE_X[i] - w.x); base = i; }
    float px = w.x, py = w.y;
    for (int it = 0; it < 3; it++) {
      float t = sqrtf((px - BASE_X[base]) * (px - BASE_X[base]) + (py - LAUNCH_Y) * (py - LAUNCH_Y)) / SHOT_SPEED[base] + 8;
      px = w.x + w.vx * t; py = w.y + w.vy * t;
    }
    if (py > 185) continue;
    if (fire(base, px, py)) w.targeted = true;
    return;   // one shot per frame
  }
}

// ------------------------------------------------------------ update
static void destroyWarhead(Warhead& w, bool scored) {
  w.on = false;
  blast(w.x, w.y, 13);
  if (scored) {
    addScore(25 * mult());
    char b[12]; snprintf(b, sizeof(b), "%d", 25 * mult());
    popup(w.x, w.y - 8, b);
  }
}

static void hitGround(Warhead& w) {
  w.on = false;
  blast(w.x, w.y, 16, true);
  shake = 10;
  for (int i = 0; i < 6; i++)
    if (cityAlive[i] && fabsf(w.x - cityCX(i)) < CITY_W / 2 + 3) {
      cityAlive[i] = false;
      flash = 6; shake = 18;
      audio::play(SFX_BIG_EXPLODE);
      puff(cityCX(i), FLAT_Y - 8, 40, rgbS(255, 150, 60), 2.5f, 0.6f);
      puff(cityCX(i), FLAT_Y - 6, 30, rgbS(90, 80, 100), 1.0f, 0.4f);
      return;
    }
  for (int i = 0; i < 3; i++)
    if (baseAlive[i] && fabsf(w.x - BASE_X[i]) < 16) {
      baseAlive[i] = false; ammo[i] = 0;
      flash = 4; shake = 16;
      audio::play(SFX_BIG_EXPLODE);
      puff(BASE_X[i], LAUNCH_Y, 40, rgbS(255, 150, 60), 2.5f, 0.6f);
      return;
    }
  audio::play(SFX_EXPLODE);
}

static void updateWorld(bool demo) {
  // spawning
  if (toSpawn > 0 && --spawnT <= 0) {
    int salvo = min(toSpawn, 1 + (int)(rnd() % (2 + wave / 2)));
    for (int i = 0; i < salvo; i++) launchWarhead(frange(10, 310), 14, pickTarget(), true);
    toSpawn -= salvo;
    spawnT = arcade::frames(max(40, 170 - wave * 10)) + rnd() % 60;
  }
  if (wave >= 2 && toSpawn > 0 && --flyerT <= 0) {
    for (auto& f : flyers) if (!f.on) {
      bool left = rnd() & 1;
      f = {true, wave >= 4 && (rnd() & 1), left ? -24.0f : 330.0f, frange(60, 110), (left ? 1 : -1) * 0.7f * arcade::speed(), 60, 0};
      break;
    }
    flyerT = 500 + rnd() % 400;
  }
  for (auto& f : flyers) {
    if (!f.on) continue;
    f.x += f.vx; f.frame++;
    if (--f.dropT <= 0 && toSpawn > 0 && f.x > 20 && f.x < 300) {
      launchWarhead(f.x, f.y + 6, pickTarget(), false); toSpawn--;
      f.dropT = arcade::frames(110) + rnd() % 60;
    }
    if (f.x < -30 || f.x > 340) f.on = false;
  }
  // warheads
  for (auto& w : warheads) {
    if (!w.on) continue;
    w.x += w.vx; w.y += w.vy;
    if (w.mirv && w.y >= w.splitY) {
      w.mirv = false;
      int n = 2 + (rnd() % 2 == 0);
      for (int i = 0; i < n; i++) launchWarhead(w.x, w.y, pickTarget(), false);
      w.x0 = w.x; w.y0 = w.y;   // the original carries on, with a fresh trail
    }
    if (w.y >= groundAt(w.x) - 1 || (w.target < 6 && w.y >= FLAT_Y - 4) || (w.target >= 6 && w.target < 9 && w.y >= LAUNCH_Y + 2)) hitGround(w);
  }
  // interceptors
  for (auto& s : shots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if ((s.vy <= 0 && s.y <= s.ty) || (s.vy > 0 && s.y >= s.ty)) { s.on = false; blast(s.tx, s.ty, 19); audio::play(SFX_EXPLODE); }
  }
  // fireballs
  for (auto& b : blasts) {
    if (!b.on) continue;
    if (++b.t >= 46) { b.on = false; continue; }
    float r = blastR(b);
    for (auto& w : warheads) {
      if (!w.on) continue;
      float dx = w.x - b.x, dy = w.y - b.y;
      if (dx * dx + dy * dy <= r * r) destroyWarhead(w, !demo);
    }
    for (auto& f : flyers) {
      if (!f.on) continue;
      float dx = f.x - b.x, dy = f.y - b.y;
      if (dx * dx + dy * dy <= (r + 8) * (r + 8)) {
        f.on = false;
        blast(f.x, f.y, 20, true);
        puff(f.x, f.y, 24, rgbS(200, 210, 230), 2.0f, 0.0f);
        audio::play(SFX_BIG_EXPLODE);
        if (!demo) { addScore(100 * mult()); char t[12]; snprintf(t, sizeof(t), "%d", 100 * mult()); popup(f.x, f.y - 10, t); }
      }
    }
  }
  for (auto& p : smoke) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vx *= 0.97f; p.vy = p.vy * 0.97f - 0.01f;
    if (!--p.life) p.on = false;
  }
  // burning ruins smoulder
  if ((frameNo & 7) == 0)
    for (int i = 0; i < 6; i++) if (!cityAlive[i]) puff(CITY_X[i] + 4 + rnd() % 20, FLAT_Y - 4, 1, rgbS(70, 64, 84), 0.2f, 0.35f);
  for (auto& p : popups) if (p.on) { p.y -= 0.4f; if (--p.t <= 0) p.on = false; }
  if (shake) shake--;
  if (flash) flash--;
}

static bool waveDone() {
  if (toSpawn > 0) return false;
  for (auto& w : warheads) if (w.on) return false;
  for (auto& b : blasts) if (b.on) return false;
  for (auto& s : shots) if (s.on) return false;
  for (auto& f : flyers) if (f.on) return false;
  return true;
}

static void step(const Pad& in) {
  stateT++;
  switch (state) {
    case ST_TITLE:
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { audio::play(SFX_START); resetGame(); break; }
      // attract mode: the cities defend themselves
      if (waveDone()) { for (int i = 0; i < 3; i++) { baseAlive[i] = true; ammo[i] = 99; } for (bool& c : cityAlive) c = true; toSpawn = 8; spawnT = 60; }
      autopilot();
      updateWorld(true);
      break;
    case ST_INTRO:
      updateWorld(false);
      if (stateT > 150 || (stateT > 40 && in.hit(BTN_A | BTN_START))) { state = ST_PLAY; stateT = 0; audio::music(SONG_PLAY); }
      break;
    case ST_PLAY: {
      if (in.hit(BTN_START)) { arcade::pause(); break; }
      float sp = 3.6f;
      crossX = constrain(crossX + in.ax * sp, 4.0f, 316.0f);
      crossY = constrain(crossY + in.ay * sp, 16.0f, 188.0f);
      if (in.hit(BTN_X)) fire(0, crossX, crossY);
      if (in.hit(BTN_A | BTN_Y)) fire(1, crossX, crossY);
      if (in.hit(BTN_B)) fire(2, crossX, crossY);
      updateWorld(false);
      if (stateT > 60 && waveDone()) {
        state = ST_BONUS; stateT = 0; bonusStep = 0;
        bonusAmmo = ammo[0] + ammo[1] + ammo[2]; bonusCity = citiesLeft();
        audio::music(SONG_CLEAR);
      }
      break;
    }
    case ST_BONUS:
      updateWorld(false);
      // tally: each spare interceptor, then each city
      if (stateT > 60 && stateT % 5 == 0) {
        if (bonusStep < bonusAmmo) { bonusStep++; addScore(5 * mult()); audio::play(SFX_MOVE); }
        else if (bonusStep < bonusAmmo + bonusCity && stateT % 15 == 0) { bonusStep++; addScore(100 * mult()); audio::play(SFX_COIN); }
      }
      if (bonusStep >= bonusAmmo + bonusCity && stateT > 120 + bonusAmmo * 5 + bonusCity * 15 + 60) {
        if (citiesLeft() == 0 && bonusCities == 0) {
          state = ST_OVER; stateT = 0; audio::music(SONG_OVER);
          if (newHi) arcade::saveHi(hiscore);
        } else { wave++; startWave(); }
      }
      break;
    case ST_OVER:
      updateWorld(true);
      if (stateT % 40 == 0 && stateT < 300) { blast(frange(40, 280), frange(60, 180), 26, true); audio::play(SFX_BIG_EXPLODE); shake = 8; }
      if (stateT > 150 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; for (bool& c : cityAlive) c = true; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static uint16_t skyRow[SH];
static uint16_t ridgeCol[2], skylineCol, trailCol565, trailHead565;
static const uint8_t SKIES[4][3][3] = {
  {{4, 6, 22}, {18, 22, 60}, {60, 40, 90}},       // midnight
  {{14, 4, 30}, {60, 20, 80}, {190, 70, 110}},     // violet dusk
  {{6, 10, 26}, {20, 50, 70}, {40, 140, 120}},     // aurora green
  {{20, 4, 10}, {80, 16, 30}, {230, 110, 60}},     // red dawn
};
static void buildSky() {
  const auto& s = SKIES[(wave - 1) % 4];
  for (int y = 0; y < SH; y++) {
    float t = (float)y / GROUND_Y;
    if (t > 1) t = 1;
    skyRow[y] = t < 0.55f ? lerpS(s[0][0], s[0][1], s[0][2], s[1][0], s[1][1], s[1][2], t / 0.55f)
                          : lerpS(s[1][0], s[1][1], s[1][2], s[2][0], s[2][1], s[2][2], (t - 0.55f) / 0.45f);
  }
  ridgeCol[0] = rgbS(s[1][0] / 2 + 6, s[1][1] / 2 + 6, s[1][2] / 2 + 14);
  ridgeCol[1] = rgbS(s[1][0] / 3 + 4, s[1][1] / 3 + 4, s[1][2] / 3 + 10);
  skylineCol = rgbS(s[0][0] + 6, s[0][1] + 8, s[0][2] + 20);
  static const uint8_t TR[4][3] = {{255, 80, 110}, {255, 120, 60}, {255, 90, 200}, {255, 230, 90}};
  trailCol565 = c565(TR[(wave - 1) % 4][0], TR[(wave - 1) % 4][1], TR[(wave - 1) % 4][2]);
  trailHead565 = c565(255, 255, 255);
}

static void drawSky() {
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t c = flash ? rgbS(120 + flash * 18, 90 + flash * 14, 90 + flash * 10) : skyRow[y];
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) d[x] = c;
  }
  if (flash) return;
  static const uint16_t sc[3] = {rgbS(70, 70, 120), rgbS(150, 150, 210), rgbS(255, 255, 255)};
  for (auto& s : stars) if (!(s.b == 2 && ((frameNo + s.x * 7) & 127) < 10)) pset(s.x, s.y, sc[s.b]);
  if (wave % 4 == 3 || state == ST_TITLE) {
    // aurora curtains: soft vertical streaks blended into the sky
    for (int x = 0; x < SW; x++) {
      float ph = x * 0.03f + frameNo * 0.01f;
      int top = 26 + (int)(12 * sinf(ph) + 6 * sinf(x * 0.07f - frameNo * 0.017f));
      int len = 30 + (int)(14 * sinf(x * 0.05f + frameNo * 0.013f));
      float str = 0.5f + 0.5f * sinf(x * 0.21f + sinf(ph * 2) * 2);   // streakiness
      if (str < 0.25f) continue;
      for (int y = max(top, Y0); y < min(top + len, Y0 + STRIP); y++) {
        int k = (int)(str * 3 * (1 - (float)(y - top) / len) + BAYER[y & 3][x & 3] / 16.0f);   // fades towards the bottom
        if (k <= 0) continue;
        uint16_t c = sw16(B[(y - Y0) * SW + x]);
        int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
        g = min(63, g + k * 7); b = min(31, b + k * 2); r = min(31, r + k);
        B[(y - Y0) * SW + x] = sw16((r << 11) | (g << 5) | b);
      }
    }
  }
  if (state != ST_TITLE) blit(SPR_MOON, 262, 24);
  // distant ridge and the far city behind the defended ones
  for (int x = 0; x < SW; x++) {
    int top = GROUND_Y + 12 - RIDGE_H[x];
    for (int y = max(top, Y0); y < min(GROUND_Y + 14, Y0 + STRIP); y++) B[(y - Y0) * SW + x] = y < top + 2 ? ridgeCol[0] : ridgeCol[1];
    int st = GROUND_Y + 16 - SKYLINE_H[x];
    for (int y = max(st, Y0); y < min(GROUND_Y + 16, Y0 + STRIP); y++) {
      bool lit = ((x * 7 + y * 13) % 17 == 0) && y > st + 1;
      B[(y - Y0) * SW + x] = lit ? pal[CS_WIN0] : skylineCol;
    }
  }
}

static void drawGround() {
  blit(SPR_GROUND, 0, GROUND_Y);
  for (int i = 0; i < 6; i++) blit(cityAlive[i] ? *CITY[i] : *RUBBLE[i], CITY_X[i], FLAT_Y - CITY_H + 1);
  for (int i = 0; i < 3; i++) {
    int top = (int)groundAt(BASE_X[i]);
    blit(baseAlive[i] ? SPR_BASE : SPR_BASE_RUIN, BASE_X[i] - 15, top - 12);
    // ammo rack below the base
    if (state == ST_TITLE) continue;
    int ay = 230;
    if (baseAlive[i] && ammo[i]) for (int k = 0; k < ammo[i]; k++) blit(SPR_AMMO, BASE_X[i] - 15 + k * 3, ay);
    else if (state == ST_PLAY || state == ST_INTRO) text("OUT", BASE_X[i], ay - 1, c565(255, 90, 90), 1, top_center, false);
  }
}

static void drawAction() {
  // enemy trails
  for (auto& w : warheads) {
    if (!w.on) continue;
    line((int)w.x0, (int)w.y0, (int)w.x, (int)w.y, trailCol565);
    uint16_t hc = ((frameNo >> 2) & 1) ? rgbS(255, 255, 255) : rgbS(255, 220, 120);
    rectf((int)w.x - 1, (int)w.y - 1, 2, 2, hc);
    if (w.mirv && ((frameNo >> 3) & 1)) pset((int)w.x, (int)w.y - 3, rgbS(255, 90, 90));
  }
  // flyers
  for (auto& f : flyers) {
    if (!f.on) continue;
    const Sprite& s = f.sat ? *SAT[(f.frame >> 4) & 1] : *BOMBER[(f.frame >> 3) & 1];
    if (f.vx < 0 || f.sat) blit(s, (int)f.x - s.w / 2, (int)f.y - s.h / 2);
    else blitFlip(s, (int)f.x - s.w / 2, (int)f.y - s.h / 2);
  }
  // interceptors
  for (auto& s : shots) {
    if (!s.on) continue;
    float d = sqrtf(s.vx * s.vx + s.vy * s.vy);
    line((int)s.x, (int)s.y, (int)(s.x - s.vx / d * 14), (int)(s.y - s.vy / d * 14), c565(90, 200, 255));
    pset((int)s.x, (int)s.y, rgbS(255, 255, 255));
    if ((frameNo >> 2) & 1) blit(SPR_MARK, (int)s.tx - 2, (int)s.ty - 2);
  }
  // fireballs: hot core, colour-cycling shell
  static const uint16_t RING[6] = {c565(255, 255, 255), c565(255, 230, 90), c565(255, 140, 40), c565(255, 70, 110), c565(170, 90, 255), c565(80, 200, 255)};
  for (auto& b : blasts) {
    if (!b.on) continue;
    int r = (int)blastR(b);
    if (r < 1) continue;
    int k = (b.t / 2 + (int)b.x) % 6;
    disc((int)b.x, (int)b.y, r, RING[k]);
    if (r > 4) disc((int)b.x, (int)b.y, r * 3 / 5, b.big ? RING[(k + 2) % 6] : RING[(k + 3) % 6]);
    if (r > 8 && b.t < 30) disc((int)b.x, (int)b.y, r / 4, RING[0]);
  }
  for (auto& p : smoke) if (p.on) { pset((int)p.x, (int)p.y, p.col); if (p.life > 40) pset((int)p.x + 1, (int)p.y, p.col); }
  for (auto& p : popups) if (p.on) text(p.txt, (int)p.x, (int)p.y, (p.t & 4) ? TFT_WHITE : c565(255, 216, 74), 1, top_center);
}

static void drawHud() {
  if (!rowsVisible(0, 12)) return;
  shade(0, 0, SW, 11);
  textf(4, 2, c565(255, 216, 74), 1, top_left, "%07lu", (unsigned long)score);
  textf(SW / 2, 2, c565(184, 200, 255), 1, top_center, "HI %07lu", (unsigned long)hiscore);
  textf(SW - 4, 2, TFT_WHITE, 1, top_right, "WAVE %d", wave);
}

static void draw() {
  drawSky();
  drawGround();
  drawAction();
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_TITLE) {
    if (rowsVisible(28, 150)) { shade(16, 28, 288, 150); }
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 36);
    text("DEFEND THE SIX CITIES", SW / 2, 76, c565(184, 220, 255), 1, top_center);
    textf(SW / 2, 90, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 104, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 108, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(128);
    text("STICK AIMS   X / A / B FIRE", SW / 2, 146, c565(180, 194, 220), 1, top_center);
    text("FROM THE LEFT / CENTRE / RIGHT", SW / 2, 158, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 168 + 2, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawHud();
  if (state == ST_PLAY || state == ST_INTRO) blit(SPR_CROSS, (int)crossX - 6, (int)crossY - 6);
  if (state == ST_INTRO) {
    if (rowsVisible(70, 64)) { shade(50, 70, 220, 64); shade(50, 70, 220, 64); }
    textf(SW / 2, 78, TFT_WHITE, 2, top_center, "WAVE %d", wave);
    textf(SW / 2, 100, c565(255, 216, 74), 1, top_center, "%d X POINTS", mult());
    if (blink) text("DEFEND CITIES", SW / 2, 116, c565(255, 90, 110), 1, top_center);
  } else if (state == ST_BONUS) {
    if (rowsVisible(60, 90)) { shade(40, 60, 240, 90); shade(40, 60, 240, 90); }
    textf(SW / 2, 68, c565(130, 255, 190), 2, top_center, "WAVE %d CLEAR", wave);
    if (stateT > 60) {
      int a = min(bonusStep, bonusAmmo), c = max(0, bonusStep - bonusAmmo);
      for (int k = 0; k < a && k < 30; k++) blit(SPR_AMMO, 70 + k * 6, 96);
      textf(SW - 54, 96, TFT_WHITE, 1, top_right, "%d", a * 5 * mult());
      for (int k = 0; k < c; k++) blit(*CITY[k % 6], 52 + k * 30, 108);
      textf(SW - 54, 122, TFT_WHITE, 1, top_right, "%d", c * 100 * mult());
      if (bonusCities) textf(SW / 2, 136, c565(255, 216, 74), 1, top_center, "BONUS CITIES: %d", bonusCities);
    }
  } else if (state == ST_OVER) {
    if (rowsVisible(76, 76)) { shade(40, 76, 240, 76); shade(40, 76, 240, 76); }
    text("THE END", SW / 2, 84, c565(255, 90, 110), 3, top_center);
    textf(SW / 2, 114, TFT_WHITE, 1, top_center, "REACHED WAVE %d", wave);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 126, c565(255, 216, 74), 1, top_center);
    if (stateT > 150) text("PRESS START", SW / 2, 138, c565(184, 220, 255), 1, top_center);
  }
}

void setup() {
  arcade::begin("cityshield", &MUSIC);
  setPalette(CS_PAL565, CS_PAL_N);
  for (auto& s : stars) { s.x = rnd() % SW; s.y = 12 + rnd() % 160; s.b = rnd() % 3; }
  for (bool& c : cityAlive) c = true;
  for (int i = 0; i < 3; i++) { baseAlive[i] = true; ammo[i] = 99; }
  buildSky();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
