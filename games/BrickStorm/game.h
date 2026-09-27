// =====================================================================
//  BRICK STORM  -  a brick-breaker (Nova Arcade)
//  8 hand-made stages that loop faster, multi-hit / gold / explosive bricks,
//  power-ups: Expand, Multi-ball, Laser, Slow, Catch, extra life.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace bsm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 C */ {72, 1, 76, 79, 84, 1, 79, 76, 77, 1, 76, 1, 74, 1, 72, 1},
  /*1 Am*/ {76, 1, 72, 76, 81, 1, 79, 76, 74, 1, 72, 1, 69, 1, 1, 1},
  /*2 F */ {77, 1, 81, 1, 84, 1, 81, 77, 79, 1, 77, 1, 76, 1, 74, 1},
  /*3 G */ {79, 1, 83, 1, 86, 1, 83, 1, 79, 1, 81, 1, 83, 1, 1, 1},
  /*4 t */ {84, 1, 1, 1, 83, 1, 79, 1, 76, 1, 1, 1, 79, 1, 1, 1},
  /*5 w */ {72, 76, 79, 84, 88, 1, 1, 1, 84, 1, 88, 1, 91, 1, 1, 1},   // stage clear
  /*6 o */ {76, 1, 74, 1, 72, 1, 69, 1, 67, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.h.S.h.K.hKS.hh", "K.S.K.S.K.S.SSSS", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{C_, 4, 2}, {AM, -1, 2}, {F_, 4, 2}, {G_, -1, 2}};
static const Bar GAME_BARS[] = {{C_, 0, 0}, {AM, 1, 0}, {F_, 2, 0}, {G_, 3, 0},
                                {C_, 0, 0}, {AM, 1, 0}, {F_, 2, 0}, {G_, 3, 1}};
static const Bar CLEAR_BARS[] = {{C_, 5, 3}, {C_, -1, 3}};
static const Bar OVER_BARS[] = {{AM, 6, 3}, {AM, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 110, true, false, 0x30},
  {GAME_BARS, 8, 132, true, false, 0},
  {CLEAR_BARS, 2, 140, false, false, 0},
  {OVER_BARS, 2, 96, false, false, 0},
};
}  // namespace bsm
static const audio::Music MUSIC = {audio::STD_CHORDS, bsm::LEADS, bsm::DRUMS, bsm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_GAME, SONG_CLEAR, SONG_OVER };

// ------------------------------------------------------------ stages
static const int BCOLS = 13, BROWS = 12, BW = 22, BH = 10;
static const int FX = 8, FY = 16, FR = 312;        // playfield walls
static const int GX = 17, GY = 34;                 // brick grid origin
static const char* const STAGES[8][BROWS] = {
  {"", "1111111111111", "2222222222222", "3333333333333", "4444444444444", "5555555555555", "6666666666666"},
  {"", "......S......", ".....S1S.....", "....S222S....", "...S33333S...", "..S4444444S..", ".S555555555S.", "S66666666666S"},
  {"", "1.2.3.4.5.6.1", ".2.3.4.5.6.1.", "G.G.G.G.G.G.G", "3.4.5.6.1.2.3", ".4.5.6.1.2.3.", "5.6.1.2.3.4.5"},
  {"", "......6......", ".....656.....", "....65X56....", "...6554556...", "..655X4X556..", "...6554556...", "....65X56....", ".....656.....", "......6......"},
  {"", "SSSSSSSSSSSSS", "1111111111111", "G22222G22222G", "3333333333333", "4444X444X4444", "SSSSSSSSSSSSS"},
  {"", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "S.S.S.S.S.S.S", "X.X.X.X.X.X.X"},
  {"", "GGGG.....GGGG", "G11G.....G11G", "G11GSSSSSG11G", "G22222222222G", "G33X33333X33G", "G44444444444G", "G55555555555G", "GGGGGG.GGGGGG"},
  {"", "X1X2X3X4X5X6X", "SSSSSSSSSSSSS", "6543216543216", "1234561234561", "SSSSSSSSSSSSS", "X6X5X4X3X2X1X", "G...G...G...G"},
};
static const uint8_t BCOL[10][3] = {
  {0, 0, 0}, {235, 60, 80}, {250, 145, 40}, {250, 210, 40}, {80, 215, 95}, {60, 175, 240}, {170, 80, 225},
  {185, 190, 205},   // 7 silver
  {230, 180, 60},    // 8 gold
  {255, 90, 50},     // 9 explosive
};
struct Brick { uint8_t kind, hp, flash; };   // kind: 0 empty, 1-6 colour, 7 silver, 8 gold, 9 explosive
static Brick bricks[BROWS][BCOLS];
static int bricksLeft = 0;
static void buildBackground(int variant);

// ------------------------------------------------------------ entities
struct Ball { bool on; float x, y, vx, vy; bool stuck; float stuckOff; };
static Ball balls[8];
struct Cap { bool on; float x, y; uint8_t type; };
static Cap caps[6];
struct Beam { bool on; float x, y; };
static Beam beams[12];
struct Part { bool on; float x, y, vx, vy; uint8_t life, max; uint16_t col; };
static Part parts[300];

static float padX = 160, padW = 40;
static const int PAD_Y = 222;
static int lives = 3, stage = 0, loopN = 0;
static uint32_t score = 0, hiscore = 0;
static float ballSpeed = 3.0f;
static int combo = 0;
static int laserT = 0, slowT = 0, wideT = 0, catchT = 0;
static int laserCd = 0;
enum State { ST_TITLE, ST_PLAY, ST_CLEAR, ST_OVER, ST_LOST };
static State state = ST_TITLE;
static int stateT = 0;
static bool newHi = false;
static float shake = 0;
static int shakeX = 0, shakeY = 0;
static int padFlash = 0;
enum { CAP_E, CAP_M, CAP_L, CAP_S, CAP_C, CAP_1, NCAP };
static const char CAPCH[NCAP] = {'E', 'M', 'L', 'S', 'C', '+'};
static const uint8_t CAPCOL[NCAP][3] = {{60, 175, 240}, {250, 145, 40}, {235, 60, 80}, {80, 215, 95}, {170, 80, 225}, {255, 255, 255}};

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, uint16_t col, float sp) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) {
        float a = frand() * 6.283f, s = frange(0.3f, 1.0f) * sp;
        p = {true, x, y, cosf(a) * s, sinf(a) * s, (uint8_t)frange(14, 30), 30, col};
        break;
      }
}

static void loadStage(int n) {
  memset(bricks, 0, sizeof(bricks));
  bricksLeft = 0;
  const char* const* rows = STAGES[n % 8];
  for (int r = 0; r < BROWS; r++) {
    const char* s = rows[r];
    if (!s) break;
    for (int c = 0; c < BCOLS && s[c]; c++) {
      char ch = s[c];
      Brick& b = bricks[r][c];
      if (ch >= '1' && ch <= '6') { b.kind = ch - '0'; b.hp = 1; }
      else if (ch == 'S') { b.kind = 7; b.hp = 2 + loopN; }
      else if (ch == 'G') { b.kind = 8; b.hp = 255; }
      else if (ch == 'X') { b.kind = 9; b.hp = 1; }
      if (b.kind && b.kind != 8) bricksLeft++;
    }
  }
}

static void resetBall() {
  memset(balls, 0, sizeof(balls));
  balls[0] = {true, padX, (float)PAD_Y - 4, 0, 0, true, 0};
  memset(caps, 0, sizeof(caps));
  memset(beams, 0, sizeof(beams));
  laserT = slowT = wideT = catchT = 0;
  padW = 40;
  combo = 0;
}

static void startStage() {
  loadStage(stage);
  buildBackground(stage);
  ballSpeed = 3.0f + 0.35f * loopN + 0.05f * (stage % 8);
  resetBall();
  state = ST_PLAY; stateT = 0;
}

static void resetGame() {
  score = 0; lives = 3; stage = 0; loopN = 0; newHi = false; padX = 160;
  memset(parts, 0, sizeof(parts));
  startStage();
}

static void spawnCap(float x, float y) {
  if (rnd() % 100 >= 14) return;
  for (auto& c : caps)
    if (!c.on) {
      int r = rnd() % 100;
      c = {true, x - 8, y, (uint8_t)(r < 22 ? CAP_E : r < 42 ? CAP_M : r < 60 ? CAP_L : r < 75 ? CAP_S : r < 92 ? CAP_C : CAP_1)};
      return;
    }
}

static void hitBrick(int r, int c, bool byBeam = false);
static void explodeAt(int r, int c) {
  addScore(50);
  input::rumble(120, 0x80, 0x40);
  shake = max(shake, 5.0f);
  audio::play(SFX_EXPLODE);
  burst(GX + c * BW + BW / 2, GY + r * BH + BH / 2, 20, rgbS(255, 180, 60), 2.5f);
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      int rr = r + dr, cc = c + dc;
      if ((dr || dc) && rr >= 0 && rr < BROWS && cc >= 0 && cc < BCOLS && bricks[rr][cc].kind && bricks[rr][cc].kind != 8) {
        bricks[rr][cc].hp = 1;
        hitBrick(rr, cc, true);
      }
    }
}

static void hitBrick(int r, int c, bool byBeam) {
  Brick& b = bricks[r][c];
  if (!b.kind) return;
  b.flash = 6;
  if (b.kind == 8) { audio::play(SFX_BOUNCE, 20); return; }
  if (--b.hp > 0) { audio::play(SFX_BRICK, 0); addScore(20); return; }
  uint8_t k = b.kind;
  b.kind = 0;
  bricksLeft--;
  combo++;
  addScore(k == 7 ? 100 : 60 + min(combo, 10) * 10);
  audio::play(SFX_BRICK, min(combo, 20));
  float cx = GX + c * BW + BW / 2, cy = GY + r * BH + BH / 2;
  burst(cx, cy, 10, rgbS(BCOL[k][0], BCOL[k][1], BCOL[k][2]), 1.8f);
  if (k == 9) explodeAt(r, c);
  else if (!byBeam) spawnCap(cx, cy);
}

// brick at pixel? returns true and fills r,c
static bool brickAt(float x, float y, int& r, int& c) {
  if (x < GX || y < GY) return false;
  c = (int)((x - GX) / BW); r = (int)((y - GY) / BH);
  if (c < 0 || c >= BCOLS || r < 0 || r >= BROWS) return false;
  return bricks[r][c].kind != 0;
}

static void launch(Ball& b) {
  float off = constrain(b.stuckOff / (padW / 2), -1.0f, 1.0f);
  float ang = -1.5708f + off * 0.9f;
  if (fabsf(off) < 0.05f) ang = -1.5708f + 0.25f;   // never perfectly vertical
  b.vx = cosf(ang) * ballSpeed; b.vy = sinf(ang) * ballSpeed; b.stuck = false;
  audio::play(SFX_BOUNCE, 0);
}

static void updateBall(Ball& b) {
  if (b.stuck) { b.x = padX + b.stuckOff; b.y = PAD_Y - 4; return; }
  float spd = slowT ? ballSpeed * 0.65f : ballSpeed;
  float len = sqrtf(b.vx * b.vx + b.vy * b.vy);
  if (len > 0.01f) { b.vx = b.vx / len * spd; b.vy = b.vy / len * spd; }
  int sub = 3;
  for (int s = 0; s < sub; s++) {
    float nx = b.x + b.vx / sub;
    int r, c;
    if (brickAt(nx + (b.vx > 0 ? 3 : -3), b.y, r, c)) { b.vx = -b.vx; hitBrick(r, c); }
    else b.x = nx;
    float ny = b.y + b.vy / sub;
    if (brickAt(b.x, ny + (b.vy > 0 ? 3 : -3), r, c)) { b.vy = -b.vy; hitBrick(r, c); }
    else b.y = ny;
    // walls
    if (b.x < FX + 3) { b.x = FX + 3; b.vx = fabsf(b.vx); audio::play(SFX_BOUNCE, 5); }
    if (b.x > FR - 3) { b.x = FR - 3; b.vx = -fabsf(b.vx); audio::play(SFX_BOUNCE, 5); }
    if (b.y < FY + 3) { b.y = FY + 3; b.vy = fabsf(b.vy); audio::play(SFX_BOUNCE, 7); }
    // paddle
    if (b.vy > 0 && b.y + 3 >= PAD_Y && b.y + 3 <= PAD_Y + 7 && b.x > padX - padW / 2 - 3 && b.x < padX + padW / 2 + 3) {
      float off = constrain((b.x - padX) / (padW / 2), -1.0f, 1.0f);
      float ang = -1.5708f + off * 1.05f;
      b.vx = cosf(ang) * spd; b.vy = sinf(ang) * spd;
      b.y = PAD_Y - 3;
      combo = 0;
      padFlash = 6;
      ballSpeed = min(ballSpeed + 0.03f, 5.6f + 0.3f * loopN);
      audio::play(SFX_BOUNCE, 12);
      if (catchT) { b.stuck = true; b.stuckOff = b.x - padX; }
    }
  }
  // stop near-horizontal loops
  if (fabsf(b.vy) < 0.6f) b.vy = b.vy < 0 ? -0.6f : 0.6f;
  if (b.y > SH + 4) b.on = false;
}

static void applyCap(uint8_t t) {
  audio::play(SFX_POWERUP);
  addScore(100);
  switch (t) {
    case CAP_E: wideT = 900; break;
    case CAP_M: {
      Ball* src = nullptr;
      for (auto& b : balls) if (b.on) { src = &b; break; }
      if (!src) break;
      if (src->stuck) launch(*src);
      for (int k = 0; k < 2; k++)
        for (auto& b : balls)
          if (!b.on) {
            float ang = atan2f(src->vy, src->vx) + (k ? 0.5f : -0.5f);
            b = {true, src->x, src->y, cosf(ang) * ballSpeed, sinf(ang) * ballSpeed, false, 0};
            break;
          }
      break;
    }
    case CAP_L: laserT = 720; break;
    case CAP_S: slowT = 600; break;
    case CAP_C: catchT = 900; break;
    case CAP_1: lives = min(lives + 1, 9); break;
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  float target = wideT ? 64 : 40;
  padW += (target - padW) * 0.2f;
  float spd = 5.0f;
  padX += in.ax * spd;
  padX = constrain(padX, FX + padW / 2, FR - padW / 2);
  if (wideT) wideT--;
  if (slowT) slowT--;
  if (catchT) catchT--;
  if (laserT) laserT--;
  if (padFlash) padFlash--;

  bool fire = in.hit(BTN_A | BTN_R1 | BTN_R2 | BTN_X);
  for (auto& b : balls) if (b.on && b.stuck && fire) { launch(b); fire = false; }
  if (laserT && in.down(BTN_A | BTN_R1 | BTN_R2 | BTN_X) && --laserCd <= 0) {
    laserCd = 12;
    for (int s = -1; s <= 1; s += 2)
      for (auto& bm : beams) if (!bm.on) { bm = {true, padX + s * (padW / 2 - 4), (float)PAD_Y - 4}; break; }
    audio::play(SFX_SHOOT);
  }
  for (auto& bm : beams) {
    if (!bm.on) continue;
    bm.y -= 7;
    int r, c;
    if (brickAt(bm.x, bm.y, r, c)) { hitBrick(r, c, true); bm.on = false; }
    else if (bm.y < FY) bm.on = false;
  }
  int alive = 0;
  for (auto& b : balls) if (b.on) { updateBall(b); if (b.on) alive++; }
  for (auto& c : caps) {
    if (!c.on) continue;
    c.y += 1.3f;
    if (c.y + 8 >= PAD_Y && c.y <= PAD_Y + 6 && c.x + 16 > padX - padW / 2 && c.x < padX + padW / 2) { c.on = false; applyCap(c.type); }
    else if (c.y > SH) c.on = false;
  }
  if (!alive) {
    lives--;
    audio::play(SFX_PLAYER_DIE);
    input::rumble(300, 0x90, 0x90);
    burst(padX, PAD_Y, 30, rgbS(62, 198, 224), 2.5f);
    state = lives > 0 ? ST_LOST : ST_OVER; stateT = 0;
    if (state == ST_OVER) { audio::music(SONG_OVER); if (newHi) arcade::saveHi(hiscore); }
  }
  if (bricksLeft <= 0) {
    state = ST_CLEAR; stateT = 0;
    addScore(1000 * (loopN + 1));
    audio::music(SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += 0.06f;
    if (--p.life == 0) p.on = false;
  }
  for (auto& row : bricks) for (auto& b : row) if (b.flash) b.flash--;
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

// title demo ball
static float dbx = 60, dby = 150, dvx = 2.2f, dvy = -1.7f;

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      dbx += dvx; dby += dvy;
      if (dbx < 10 || dbx > SW - 10) dvx = -dvx;
      if (dby < 110 || dby > 230) dvy = -dvy;
      if (in.hit(BTN_LEFT)) arcade::changeVolume(-1);
      if (in.hit(BTN_RIGHT)) arcade::changeVolume(+1);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); audio::music(SONG_GAME); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_LOST:
      if (stateT > 70) { resetBall(); state = ST_PLAY; }
      break;
    case ST_CLEAR:
      if (stateT > 150) {
        stage++;
        if (stage % 8 == 0) loopN++;
        startStage();
        audio::music(SONG_GAME);
      }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ draw
static uint16_t bgRow[SH];
static void buildBackground(int variant) {
  static const uint8_t T[4][6] = {{10, 12, 40, 40, 10, 60}, {30, 8, 40, 70, 20, 40}, {6, 26, 40, 20, 50, 70}, {30, 20, 8, 70, 30, 40}};
  const uint8_t* t = T[variant & 3];
  for (int y = 0; y < SH; y++) bgRow[y] = lerpS(t[0], t[1], t[2], t[3], t[4], t[5], (float)y / SH);
}

static void drawBackground() {
  int v = frameNo / 2;
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t c = bgRow[y];
    uint16_t c2 = sw16((sw16(c) >> 1) & 0x7BEF);
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) d[x] = (((x + y + v) >> 4) & 1) ? c : ((((x + y + v) & 15) == 0) ? c2 : c);
  }
}

static void drawBrick(int r, int c) {
  const Brick& b = bricks[r][c];
  int x = GX + c * BW + shakeX, y = GY + r * BH + shakeY;
  if (!rowsVisible(y, BH)) return;
  uint8_t k = b.kind;
  int R = BCOL[k][0], G = BCOL[k][1], Bc = BCOL[k][2];
  if (b.flash) { R = G = Bc = 255; }
  if (k == 7 && !b.flash) { int d = (2 + loopN - b.hp) * 30; R -= d; G -= d; Bc -= d; R = max(R, 60); G = max(G, 60); Bc = max(Bc, 60); }
  uint16_t base = rgbS(R, G, Bc);
  uint16_t hi = rgbS(min(255, R + 70), min(255, G + 70), min(255, Bc + 70));
  uint16_t lo = rgbS(R / 2, G / 2, Bc / 2);
  rectf(x, y, BW - 1, BH - 1, base);
  rectf(x, y, BW - 1, 1, hi); rectf(x, y, 1, BH - 1, hi);
  rectf(x, y + BH - 2, BW - 1, 1, lo); rectf(x + BW - 2, y, 1, BH - 1, lo);
  if (k == 8) { pset(x + 3, y + 2, rgbS(255, 255, 220)); pset(x + 4, y + 2, rgbS(255, 255, 220)); pset(x + 3, y + 3, rgbS(255, 255, 220)); }
  if (k == 9) { rectf(x + 8, y + 3, 5, 3, (frameNo & 8) ? rgbS(255, 240, 120) : rgbS(120, 20, 10)); }
  if (k != 8 && k != 9 && !b.flash) { pset(x + 2, y + 2, hi); pset(x + 3, y + 2, hi); }
}

static void drawPaddle() {
  int w = (int)padW, x = (int)padX - w / 2 + shakeX, y = PAD_Y + shakeY;
  if (!rowsVisible(y - 2, 10) || state == ST_LOST || state == ST_OVER) return;
  uint16_t body = padFlash ? rgbS(255, 255, 255) : rgbS(180, 194, 220);
  uint16_t cap = laserT ? rgbS(235, 60, 80) : catchT ? rgbS(170, 80, 225) : rgbS(62, 198, 224);
  rectf(x + 6, y, w - 12, 7, body);
  rectf(x + 6, y, w - 12, 2, rgbS(240, 245, 255));
  rectf(x + 6, y + 5, w - 12, 2, rgbS(90, 100, 130));
  rectf(x, y + 1, 7, 5, cap); rectf(x + w - 7, y + 1, 7, 5, cap);
  rectf(x + 1, y, 5, 7, cap); rectf(x + w - 6, y, 5, 7, cap);
  pset(x + 2, y + 1, rgbS(255, 255, 255)); pset(x + w - 5, y + 1, rgbS(255, 255, 255));
  if (laserT) { rectf(x + 3, y - 3, 2, 3, cap); rectf(x + w - 5, y - 3, 2, 3, cap); }
}

static void drawBall(float bx, float by) {
  int x = (int)bx + shakeX, y = (int)by + shakeY;
  if (!rowsVisible(y - 4, 9)) return;
  uint16_t o = rgbS(40, 40, 70), w = rgbS(255, 255, 255), g = rgbS(200, 220, 255);
  rectf(x - 2, y - 3, 5, 7, o); rectf(x - 3, y - 2, 7, 5, o);
  rectf(x - 1, y - 2, 3, 5, g); rectf(x - 2, y - 1, 5, 3, g);
  pset(x - 1, y - 1, w); pset(x, y - 1, w); pset(x - 1, y, w);
}

static void drawHud() {
  if (Y0 == 0) {
    rectf(0, 0, SW, 13, rgbS(10, 8, 25));
    textf(4, 3, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
    textf(142, 3, c565(255, 216, 74), 1, top_left, "HI %07lu", (unsigned long)hiscore);
    for (int i = 0; i < min(lives - 1, 5); i++) { rectf(SW - 16 - i * 14, 5, 12, 4, rgbS(180, 194, 220)); rectf(SW - 16 - i * 14, 5, 2, 4, rgbS(62, 198, 224)); rectf(SW - 6 - i * 14, 5, 2, 4, rgbS(62, 198, 224)); }
    textf(66, 3, c565(184, 243, 255), 1, top_left, "STAGE %d", stage + 1);
  }
  // walls
  uint16_t wc = rgbS(90, 80, 150), wl = rgbS(150, 140, 220);
  rectf(FX - 4, FY - 3, 4, SH, wc); rectf(FR, FY - 3, 4, SH, wc);
  rectf(FX - 4, FY - 3, FR - FX + 8, 3, wc);
  rectf(FX - 1, FY, 1, SH, wl); rectf(FR, FY, 1, SH, wl); rectf(FX - 1, FY - 1, FR - FX + 2, 1, wl);
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 40);
    text("SMASH EVERY BRICK", SW / 2, 76, c565(184, 243, 255), 1, top_center);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 118, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 120, c565(255, 138, 61), 1, top_center);
    textf(SW / 2, 18, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    text("STICK/D-PAD MOVE   A LAUNCH/FIRE", SW / 2, 160, c565(180, 194, 220), 1, top_center);
    // power-up legend
    static const char* names[NCAP] = {"WIDE", "MULTI", "LASER", "SLOW", "CATCH", "LIFE"};
    for (int i = 0; i < NCAP; i++) {
      int x = 22 + i * 48, y = 180;
      if (rowsVisible(y, 10)) {
        rectf(x, y, 16, 8, rgbS(CAPCOL[i][0], CAPCOL[i][1], CAPCOL[i][2]));
        rect(x, y, 16, 8, rgbS(20, 20, 40));
      }
      char s[2] = {CAPCH[i], 0};
      text(s, x + 8, y, c565(20, 20, 40), 1, top_center, false);
      text(names[i], x + 8, y + 12, c565(180, 194, 220), 1, top_center, false);
    }
    text("SELECT+START: BACK TO MENU", SW / 2, 214, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawHud();
  for (int r = 0; r < BROWS; r++)
    for (int c = 0; c < BCOLS; c++) if (bricks[r][c].kind) drawBrick(r, c);
  for (auto& bm : beams) if (bm.on) rectf((int)bm.x, (int)bm.y, 2, 6, rgbS(255, 120, 120));
  for (auto& c : caps) {
    if (!c.on) continue;
    int x = (int)c.x, y = (int)c.y;
    if (rowsVisible(y, 9)) {
      const uint8_t* cc = CAPCOL[c.type];
      rectf(x + 1, y, 14, 8, rgbS(cc[0], cc[1], cc[2]));
      rectf(x, y + 1, 16, 6, rgbS(cc[0], cc[1], cc[2]));
      rectf(x + 2, y + 1, 12, 1, rgbS(255, 255, 255));
      rectf(x + 1, y + 7, 14, 1, rgbS(cc[0] / 2, cc[1] / 2, cc[2] / 2));
    }
    char s[2] = {CAPCH[c.type], 0};
    text(s, x + 8, y, c565(20, 20, 40), 1, top_center, false);
  }
  drawPaddle();
  for (auto& b : balls) if (b.on) drawBall(b.x, b.y);
  for (auto& p : parts) {
    if (!p.on) continue;
    pset((int)p.x, (int)p.y, p.col);
    if (p.life > 18) pset((int)p.x + 1, (int)p.y, p.col);
  }
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_PLAY) {
    for (auto& b : balls)
      if (b.on && b.stuck && stateT < 400 && blink) text("PRESS A TO LAUNCH", SW / 2, 170, TFT_WHITE, 1, top_center);
    if (stateT < 90) {
      textf(SW / 2, 150, TFT_WHITE, 2, top_center, "STAGE %d", stage + 1);
    }
  } else if (state == ST_CLEAR) {
    text("STAGE CLEAR!", SW / 2, 120, c565(255, 216, 74), 2, top_center);
    textf(SW / 2, 142, TFT_WHITE, 1, top_center, "BONUS %d", 1000 * (loopN + 1));
  } else if (state == ST_OVER) {
    text("GAME OVER", SW / 2, 110, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 140, c565(255, 216, 74), 2, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 166, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("brickstorm", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildBackground(0);
  hiscore = arcade::loadHi(15000);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
