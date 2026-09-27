// =====================================================================
//  ALIEN TIDE  -  hold back the descending waves (Nova Arcade)
//  A marching formation that speeds up as it thins out, destructible
//  shields, a bonus saucer, and waves that start lower each time.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"
#include "art.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace atm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 t Em*/ {76, 1, 1, 1, 79, 1, 1, 1, 83, 1, 81, 1, 79, 1, 1, 1},
  /*1 t C */ {79, 1, 1, 1, 76, 1, 1, 1, 72, 1, 74, 1, 76, 1, 1, 1},
  /*2 t D */ {78, 1, 1, 1, 74, 1, 1, 1, 81, 1, 1, 1, 78, 1, 1, 1},
  /*3 t B */ {75, 1, 1, 1, 78, 1, 1, 1, 83, 1, 1, 1, 0, 0, 0, 0},
  /*4 over*/ {71, 1, 67, 1, 64, 1, 63, 1, 64, 1, 1, 1, 1, 1, 0, 0},
  /*5 clear*/{76, 79, 83, 88, 1, 1, 86, 1, 88, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"..h...h...h...h.", "K.......K.......", "................"};
static const Chord CH[] = {
  {40, {55, 59, 64, 59}},  // Em
  {48, {55, 60, 64, 60}},  // C
  {50, {54, 57, 62, 57}},  // D
  {47, {54, 59, 63, 59}},  // B
};
static const Bar TITLE_BARS[] = {{0, 0, 0}, {1, 1, 0}, {2, 2, 0}, {3, 3, 1}};
static const Bar PLAY_BARS[] = {{0, -1, 2}, {1, -1, 2}, {0, -1, 2}, {3, -1, 2}};
static const Bar OVER_BARS[] = {{0, 4, 2}, {0, -1, 2}};
static const Bar CLEAR_BARS[] = {{0, 5, 2}, {0, -1, 2}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 96, true, false, 0x30},
  {PLAY_BARS, 4, 80, true, false, 0},
  {OVER_BARS, 2, 90, false, false, 0},
  {CLEAR_BARS, 2, 130, false, false, 0},
};
}  // namespace atm
static const audio::Music MUSIC = {atm::CH, atm::LEADS, atm::DRUMS, atm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_PLAY, SONG_OVER, SONG_CLEAR };

// ------------------------------------------------------------ constants
static const int AROWS = 5, ACOLS = 10, CW = 22, CH_ = 18;
static const int CANNON_Y = 206, GROUND_Y = 220, SHIELD_Y = 176;
static const int NSH = 4, SHW = 26, SHH = 14;

// ------------------------------------------------------------ state
static bool alive[AROWS][ACOLS];
static int nAlive = 0;
static float fx = 50, fy = 36;
static int fdir = 1, stepT = 0, marchNote = 0, animF = 0;
static uint8_t shield[NSH][SHH][SHW];
struct Shot { bool on; float x, y, vy; uint8_t kind; };
static Shot pshot;
static Shot eshots[8];
struct PopFx { bool on; int x, y, t; uint16_t col; };
static PopFx pops[10];
struct Part { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[200];
struct Popup { bool on; int x, y, t; char txt[8]; };
static Popup popups[4];
static float px = 150;
static int lives = 3, wave = 1;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 5000;
static bool extraGiven = false, newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static float ufoX = 0; static int ufoDir = 0, ufoT = 1200, ufoScore = 0;
static float shake = 0;
static int shakeX = 0, shakeY = 0;

struct Star { int16_t x, y; uint8_t b; };
static Star stars[80];
static uint8_t groundH[SW];

static const Sprite* alienSpr(int row, int f) {
  if (row == 0) return f ? &SPR_ORB1 : &SPR_ORB0;
  if (row <= 2) return f ? &SPR_MANTIS1 : &SPR_MANTIS0;
  return f ? &SPR_JELLY1 : &SPR_JELLY0;
}
static int alienPts(int row) { return row == 0 ? 30 : row <= 2 ? 20 : 10; }
static uint16_t alienCol(int row) { return row == 0 ? rgbS(255, 123, 213) : row <= 2 ? rgbS(62, 198, 224) : rgbS(79, 214, 107); }

static void addScore(uint32_t v) {
  score += v;
  if (!extraGiven && score >= 10000) { extraGiven = true; lives++; audio::play(SFX_POWERUP); }
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, uint16_t col, float sp) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s, (uint8_t)frange(12, 30), col}; break; }
}

static void buildShields() {
  for (int s = 0; s < NSH; s++)
    for (int y = 0; y < SHH; y++)
      for (int x = 0; x < SHW; x++) {
        bool on = true;
        if (y < 4 && (x < 4 - y || x > SHW - 5 + y)) on = false;          // rounded top corners
        if (y >= 9 && x >= 8 && x < SHW - 8) on = false;                   // arch
        if (y >= 7 && y < 9 && x >= 10 && x < SHW - 10) on = false;
        shield[s][y][x] = on;
      }
}
static int shieldX(int s) { return 34 + s * 76; }

// erode a shield near (x,y); returns true if something was hit
static bool shieldHit(int x, int y, int radius) {
  for (int s = 0; s < NSH; s++) {
    int lx = x - shieldX(s), ly = y - SHIELD_Y;
    if (lx < 0 || lx >= SHW || ly < 0 || ly >= SHH || !shield[s][ly][lx]) continue;
    for (int dy = -radius; dy <= radius; dy++)
      for (int dx = -radius; dx <= radius; dx++) {
        int xx = lx + dx, yy = ly + dy;
        if (xx >= 0 && xx < SHW && yy >= 0 && yy < SHH && dx * dx + dy * dy <= radius * radius + (int)(rnd() % 3))
          shield[s][yy][xx] = 0;
      }
    return true;
  }
  return false;
}

static void startWave() {
  for (auto& r : alive) for (auto& a : r) a = true;
  nAlive = AROWS * ACOLS;
  fx = 50; fy = 36 + min(wave - 1, 5) * 8; fdir = 1; stepT = 0;
  memset(eshots, 0, sizeof(eshots)); pshot.on = false;
  ufoDir = 0; ufoT = 900 + rnd() % 900;
  buildShields();
  state = ST_PLAY; stateT = 0;
  audio::music(SONG_PLAY);
}

static void resetGame() {
  score = 0; lives = 3; wave = 1; extraGiven = false; newHi = false; px = 150;
  memset(parts, 0, sizeof(parts)); memset(pops, 0, sizeof(pops)); memset(popups, 0, sizeof(popups));
  startWave();
}

static void gameOver() {
  state = ST_OVER; stateT = 0;
  audio::music(SONG_OVER);
  if (newHi) arcade::saveHi(hiscore);
}

static void killPlayer() {
  burst(px + 9, CANNON_Y + 5, 50, rgbS(255, 200, 120), 3.0f);
  shake = 8;
  audio::play(SFX_PLAYER_DIE);
  input::rumble(500, 0xFF, 0xC0);
  lives--;
  memset(eshots, 0, sizeof(eshots));
  if (lives <= 0) gameOver(); else { state = ST_DEAD; stateT = 0; }
}

static void formationBounds(int& minC, int& maxC, int& maxR) {
  minC = ACOLS; maxC = -1; maxR = -1;
  for (int r = 0; r < AROWS; r++)
    for (int c = 0; c < ACOLS; c++)
      if (alive[r][c]) { minC = min(minC, c); maxC = max(maxC, c); maxR = max(maxR, r); }
}

static void updateFormation() {
  int interval = arcade::frames(max(1, 2 + nAlive * 30 / 50 - min(wave - 1, 4)));
  if (++stepT < interval) return;
  stepT = 0;
  int minC, maxC, maxR;
  formationBounds(minC, maxC, maxR);
  if (maxC < 0) return;
  float left = fx + minC * CW, right = fx + maxC * CW + 16;
  if ((fdir > 0 && right + 3 > SW - 6) || (fdir < 0 && left - 3 < 6)) { fy += 8; fdir = -fdir; }
  else fx += fdir * 3;
  animF ^= 1;
  audio::play(SFX_MARCH, marchNote);
  marchNote = (marchNote + 1) & 3;
  // aliens chew through shields they touch
  float bottom = fy + maxR * CH_ + 12;
  if (bottom >= SHIELD_Y)
    for (int r = 0; r < AROWS; r++)
      for (int c = 0; c < ACOLS; c++)
        if (alive[r][c]) {
          int ax = (int)fx + c * CW, ay = (int)fy + r * CH_;
          for (int s = 0; s < NSH; s++)
            for (int y = 0; y < SHH; y++)
              for (int x = 0; x < SHW; x++)
                if (shield[s][y][x] && shieldX(s) + x >= ax && shieldX(s) + x < ax + 16 && SHIELD_Y + y >= ay && SHIELD_Y + y < ay + 12)
                  shield[s][y][x] = 0;
        }
  if (bottom >= CANNON_Y) { killPlayer(); if (state != ST_OVER) gameOver(); }
}

static void alienFire() {
  int active = 0;
  for (auto& s : eshots) if (s.on) active++;
  int maxShots = min(2 + wave, 6);
  if (active >= maxShots || rnd() % arcade::frames(max(8, 40 - wave * 4)) != 0) return;
  int col;
  if (rnd() % 2) col = constrain((int)((px + 9 - fx) / CW), 0, ACOLS - 1);
  else col = rnd() % ACOLS;
  for (int k = 0; k < ACOLS; k++) {
    int c = (col + k) % ACOLS;
    for (int r = AROWS - 1; r >= 0; r--)
      if (alive[r][c]) {
        for (auto& s : eshots)
          if (!s.on) {
            uint8_t kind = rnd() % 3 == 0 ? 1 : 0;
            s = {true, fx + c * CW + 7, fy + r * CH_ + 12, (kind ? 2.8f + wave * 0.1f : 1.6f + wave * 0.08f) * arcade::speed(), kind};
            return;
          }
        return;
      }
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  px = constrain(px + in.ax * 2.3f, 6.0f, (float)SW - 25);
  if (in.hit(BTN_A | BTN_R1 | BTN_R2 | BTN_X | BTN_B) && !pshot.on) {
    pshot = {true, px + 8, (float)CANNON_Y - 4, -6.0f, 0};
    audio::play(SFX_SHOOT);
  }
  updateFormation();
  if (state != ST_PLAY) return;
  alienFire();

  // player shot
  if (pshot.on) {
    pshot.y += pshot.vy;
    if (pshot.y < 14) { pshot.on = false; burst(pshot.x, 16, 4, rgbS(184, 243, 255), 1); }
    else if (shieldHit((int)pshot.x, (int)pshot.y, 2)) pshot.on = false;
    else {
      if (ufoDir && pshot.y < 32 && pshot.x > ufoX && pshot.x < ufoX + 28) {
        static const int US[4] = {50, 100, 150, 300};
        ufoScore = US[rnd() % 4];
        addScore(ufoScore);
        for (auto& p : popups) if (!p.on) { p = {true, (int)ufoX + 4, 20, 90, {0}}; snprintf(p.txt, 8, "%d", ufoScore); break; }
        burst(ufoX + 14, 26, 40, rgbS(255, 138, 61), 2.5f);
        audio::play(SFX_BIG_EXPLODE);
        ufoDir = 0; ufoT = 1500 + rnd() % 900;
        pshot.on = false;
      }
      for (int r = 0; r < AROWS && pshot.on; r++)
        for (int c = 0; c < ACOLS; c++) {
          if (!alive[r][c]) continue;
          float ax = fx + c * CW, ay = fy + r * CH_;
          if (pshot.x >= ax && pshot.x < ax + 16 && pshot.y >= ay && pshot.y < ay + 12) {
            alive[r][c] = false; nAlive--;
            addScore(alienPts(r));
            for (auto& p : pops) if (!p.on) { p = {true, (int)ax, (int)ay, 12, alienCol(r)}; break; }
            burst(ax + 8, ay + 6, 10, alienCol(r), 1.6f);
            audio::play(SFX_EXPLODE);
            pshot.on = false;
            break;
          }
        }
    }
  }
  // alien shots
  for (auto& s : eshots) {
    if (!s.on) continue;
    s.y += s.vy;
    if (s.y > GROUND_Y - 6) { s.on = false; burst(s.x, GROUND_Y - 2, 5, rgbS(255, 216, 74), 1); continue; }
    if (shieldHit((int)s.x + 1, (int)s.y + 7, 3)) { s.on = false; continue; }
    if (pshot.on && fabsf(pshot.x - s.x - 1) < 3 && fabsf(pshot.y - s.y - 4) < 6) {   // shots collide
      pshot.on = false; s.on = false; burst(s.x, s.y + 4, 6, rgbS(255, 255, 255), 1.2f); continue;
    }
    if (s.x + 3 > px + 1 && s.x < px + 18 && s.y + 7 > CANNON_Y + 2 && s.y < CANNON_Y + 10) { s.on = false; killPlayer(); return; }
  }
  // saucer
  if (ufoDir) {
    ufoX += ufoDir * 1.2f * arcade::speed();
    if ((frameNo % 16) == 0) audio::play(SFX_UFO);
    if (ufoX < -30 || ufoX > SW + 2) { ufoDir = 0; ufoT = 1500 + rnd() % 900; }
  } else if (--ufoT <= 0 && nAlive > 8) {
    ufoDir = rnd() % 2 ? 1 : -1; ufoX = ufoDir > 0 ? -28 : SW;
  }
  if (nAlive == 0) {
    state = ST_CLEAR; stateT = 0;
    addScore(500 * wave);
    audio::music(SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.96f; p.vy *= 0.96f; if (--p.life == 0) p.on = false; }
  for (auto& p : pops) if (p.on && --p.t <= 0) p.on = false;
  for (auto& p : popups) if (p.on && --p.t <= 0) p.on = false;
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.86f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_DEAD:
      if (stateT > 100) { state = ST_PLAY; px = 150; }
      break;
    case ST_CLEAR:
      if (stateT > 150) { wave++; startWave(); }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ draw
static uint16_t bgRow[SH];
static void buildBackground() {
  for (int y = 0; y < SH; y++) {
    float t = (float)y / SH;
    bgRow[y] = t < 0.7f ? lerpS(4, 4, 16, 24, 10, 50, t / 0.7f) : lerpS(24, 10, 50, 50, 16, 60, (t - 0.7f) / 0.3f);
  }
  for (auto& s : stars) { s.x = rnd() % SW; s.y = 14 + rnd() % (GROUND_Y - 20); s.b = rnd() % 3; }
  for (int x = 0; x < SW; x++) groundH[x] = 3 + (int)(3 * sinf(x * 0.07f) + 2 * sinf(x * 0.19f + 1));
}

static void drawBackground() {
  for (int r = 0; r < STRIP; r++) {
    uint16_t c = bgRow[Y0 + r];
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) d[x] = c;
  }
  static const uint16_t sc[3] = {rgbS(70, 70, 120), rgbS(150, 150, 210), rgbS(255, 255, 255)};
  for (auto& s : stars) if (!(s.b == 0 && ((frameNo + s.x * 7) & 63) < 10)) pset(s.x, s.y, sc[s.b]);
  // ground
  if (Y0 + STRIP > GROUND_Y) {
    rectf(0, GROUND_Y, SW, 1, rgbS(79, 214, 107));
    for (int x = 0; x < SW; x++) {
      int h = groundH[x];
      for (int y = GROUND_Y + 1; y < SH; y++) {
        uint16_t c = (y - GROUND_Y < h) ? rgbS(30, 60, 50) : rgbS(18, 30, 30);
        if (BAYER[y & 3][x & 3] < 3) c = rgbS(40, 80, 60);
        pset(x, y, c);
      }
    }
  }
}

static void drawShields() {
  uint16_t c1 = rgbS(79, 214, 107), c2 = rgbS(182, 255, 110);
  for (int s = 0; s < NSH; s++) {
    int x0 = shieldX(s);
    if (!rowsVisible(SHIELD_Y, SHH)) continue;
    for (int y = 0; y < SHH; y++)
      for (int x = 0; x < SHW; x++)
        if (shield[s][y][x]) pset(x0 + x, SHIELD_Y + y, (y == 0 || !shield[s][y - 1][x]) ? c2 : c1);
  }
}

static void drawHud() {
  if (Y0 == 0) {
    rectf(0, 0, SW, 12, rgbS(6, 6, 20));
    textf(4, 2, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
    textf(66, 2, c565(184, 243, 255), 1, top_left, "WAVE %d", wave);
    textf(142, 2, c565(255, 216, 74), 1, top_left, "HI %07lu", (unsigned long)hiscore);
  }
  if (Y0 + STRIP > GROUND_Y) {
    for (int i = 0; i < min(lives - 1, 5); i++) blit(SPR_CANNON, 6 + i * 22, 227);
  }
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 28);
    text("HOLD BACK THE DESCENDING WAVES", SW / 2, 68, c565(184, 243, 255), 1, top_center);
    int f = (frameNo >> 5) & 1;
    const Sprite* tbl[3] = {alienSpr(0, f), alienSpr(1, f), alienSpr(3, f)};
    const char* pts[3] = {"= 30 PTS", "= 20 PTS", "= 10 PTS"};
    for (int i = 0; i < 3; i++) {
      blit(*tbl[i], 116, 88 + i * 18);
      text(pts[i], 140, 90 + i * 18, TFT_WHITE, 1, top_left, false);
    }
    blit(f ? SPR_SAUCER1 : SPR_SAUCER0, 104, 142);
    text("= ???", 140, 144, c565(255, 138, 61), 1, top_left, false);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 168, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 170, c565(255, 138, 61), 1, top_center);
    textf(SW / 2, 12, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    arcade::drawDifficulty(191);
    text("\x11\x10 MOVE   A FIRE   START PAUSE", SW / 2, 208, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 222, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawHud();
  drawShields();
  int ox = shakeX, oy = shakeY;
  // formation
  for (int r = 0; r < AROWS; r++) {
    int ay = (int)fy + r * CH_ + oy;
    if (!rowsVisible(ay, 12)) continue;
    for (int c = 0; c < ACOLS; c++)
      if (alive[r][c]) blit(*alienSpr(r, animF), (int)fx + c * CW + ox, ay);
  }
  for (auto& p : pops) if (p.on) blit(SPR_POP, p.x + ox, p.y + oy);
  if (ufoDir) blit((frameNo >> 3) & 1 ? SPR_SAUCER1 : SPR_SAUCER0, (int)ufoX, 18);
  // player
  if (state == ST_PLAY || state == ST_CLEAR || (state == ST_DEAD && stateT > 60 && (stateT & 4)))
    blit(SPR_CANNON, (int)px + ox, CANNON_Y + oy);
  if (pshot.on && rowsVisible((int)pshot.y, 6)) {
    rectf((int)pshot.x, (int)pshot.y, 2, 6, rgbS(184, 243, 255));
    rectf((int)pshot.x, (int)pshot.y, 2, 2, rgbS(255, 255, 255));
  }
  for (auto& s : eshots) if (s.on) blit(s.kind ? SPR_BOLT : ((frameNo >> 2) & 1 ? SPR_ZIG1 : SPR_ZIG0), (int)s.x, (int)s.y);
  for (auto& p : parts) if (p.on) { pset((int)p.x, (int)p.y, p.col); if (p.life > 20) pset((int)p.x + 1, (int)p.y, p.col); }
  for (auto& p : popups) if (p.on) text(p.txt, p.x, p.y, (p.t & 8) ? c565(255, 216, 74) : TFT_WHITE, 1, top_left);
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_PLAY && stateT < 100) textf(SW / 2, 150, TFT_WHITE, 2, top_center, "WAVE %d", wave);
  if (state == ST_CLEAR) {
    text("WAVE CLEARED!", SW / 2, 100, c565(182, 255, 110), 2, top_center);
    textf(SW / 2, 122, TFT_WHITE, 1, top_center, "BONUS %d", 500 * wave);
  }
  if (state == ST_OVER) {
    if (rowsVisible(90, 70)) { shade(60, 90, 200, 70); shade(60, 90, 200, 70); }
    text("GAME OVER", SW / 2, 98, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 128, c565(255, 216, 74), 1, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 142, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("alientide", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildBackground();
  buildShields();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
