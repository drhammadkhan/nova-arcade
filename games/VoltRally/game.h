// =====================================================================
//  VOLT RALLY  -  paddle tennis against a ladder of CPU rivals (Nova Arcade)
//  First to 5 wins the match. Hold A as the ball meets your paddle for a
//  power smash. Every rival returns faster and reads the ball better.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace vrm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 Am*/ {69, 1, 72, 76, 81, 1, 76, 72, 79, 1, 76, 1, 72, 1, 74, 1},
  /*1 F */ {77, 1, 72, 77, 81, 1, 77, 72, 76, 1, 72, 1, 69, 1, 72, 1},
  /*2 G */ {79, 1, 74, 79, 83, 1, 79, 74, 81, 1, 79, 1, 74, 1, 1, 1},
  /*3 E */ {76, 1, 80, 1, 83, 1, 88, 1, 86, 1, 83, 1, 80, 1, 1, 1},
  /*4 t */ {81, 1, 1, 1, 1, 1, 84, 1, 83, 1, 1, 1, 79, 1, 1, 1},
  /*5 ov*/ {76, 1, 74, 1, 72, 1, 71, 1, 69, 1, 1, 1, 1, 1, 0, 0},
  /*6 win*/{81, 84, 88, 93, 1, 1, 88, 1, 93, 1, 1, 1, 1, 1, 0, 0},
  /*7 pt */ {88, 1, 93, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const char* const DRUMS[] = {"K.hhS.hhK.hhS.hS", "K.h.K.h.S.h.KKS.", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{AM, 4, 2}, {F_, -1, 2}, {G_, 4, 2}, {E_, -1, 2}};
static const Bar GAME_BARS[] = {{AM, 0, 0}, {F_, 1, 0}, {G_, 2, 0}, {E_, 3, 1},
                                {AM, 0, 0}, {F_, 1, 0}, {G_, 2, 0}, {AM, -1, 1}};
static const Bar OVER_BARS[] = {{AM, 5, 3}, {AM, -1, 3}};
static const Bar WIN_BARS[] = {{A_, 6, 3}, {A_, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 116, true, false, 0x30},
  {GAME_BARS, 8, 150, true, true, 0},
  {OVER_BARS, 2, 96, false, false, 0},
  {WIN_BARS, 2, 140, false, false, 0},
};
}  // namespace vrm
static const audio::Music MUSIC = {audio::STD_CHORDS, vrm::LEADS, vrm::DRUMS, vrm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_GAME, SONG_OVER, SONG_WIN };

// ------------------------------------------------------------ court
static const int CT = 30, CB = 232;          // court top / bottom (inside the walls)
static const int PW = 6, PH = 34;            // paddle size
static const int PLX = 14, CPX = SW - 14 - PW;
static const int BS = 6;                     // ball size
static const int WIN_POINTS = 5;
static const int NRIVALS = 6;
static const char* const RIVALS[NRIVALS] = {"SPARKY", "FLUX", "SURGE", "ARCLIGHT", "DYNAMO", "OVERLOAD"};
static const uint8_t RIVCOL[NRIVALS][3] = {{255, 216, 74}, {62, 198, 224}, {79, 214, 107}, {255, 123, 213}, {255, 138, 61}, {235, 60, 80}};

static float py = 0, cy = 0, cpuV = 0;
static float bx = 0, by = 0, bvx = 0, bvy = 0, bspd = 0;
static bool smash = false;
static int ptsP = 0, ptsC = 0, rival = 0, rally = 0, serveDir = 1;
static float cpuErr = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 5000;
static bool newHi = false;
enum State { ST_TITLE, ST_SERVE, ST_PLAY, ST_POINT, ST_WON, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0, lastWinner = 0;
static float shake = 0;
static int shakeX = 0, shakeY = 0, flashP = 0, flashC = 0;
struct Trail { int16_t x, y; };
static Trail trail[10];
struct Part { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[220];

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, uint16_t col, float sp, float dirx = 0) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s + dirx, sinf(a) * s, (uint8_t)frange(12, 28), col}; break; }
}

// rival stats scale with the ladder position and the difficulty
static float cpuMaxSpeed() { return (2.0f + 0.35f * min(rival, 8)) * arcade::speed(); }
static int cpuReactX() { return max(90, 200 - rival * 20); }
static float baseBallSpeed() { return (2.8f + 0.15f * min(rival, 8)) * arcade::speed(); }

static void serve() {
  bx = SW / 2 - BS / 2; by = (CT + CB) / 2 - BS / 2;
  bspd = baseBallSpeed();
  float a = frange(-0.5f, 0.5f);
  bvx = cosf(a) * bspd * serveDir; bvy = sinf(a) * bspd;
  smash = false; rally = 0;
  cpuErr = frange(-1, 1) * max(2, 16 - rival * 2);
  for (auto& t : trail) { t.x = (int16_t)bx; t.y = (int16_t)by; }
  state = ST_PLAY; stateT = 0;
  audio::play(SFX_BLIP);
}

static void startMatch() {
  ptsP = ptsC = 0;
  py = cy = (CT + CB) / 2 - PH / 2;
  serveDir = -1;
  state = ST_SERVE; stateT = 0;
  audio::music(SONG_GAME);
}

static void resetGame() {
  score = 0; rival = 0; newHi = false;
  memset(parts, 0, sizeof(parts));
  startMatch();
}

static void pointTo(int who) {   // 1 player, 2 cpu
  lastWinner = who;
  shake = 6;
  float gx = who == 1 ? SW - 4 : 4;
  burst(gx, by + BS / 2, 40, who == 1 ? rgbS(62, 198, 224) : rgbS(235, 60, 80), 3.0f, who == 1 ? -1.5f : 1.5f);
  if (who == 1) {
    ptsP++;
    addScore(100 * (rival + 1) + rally * 10);
    audio::play(SFX_POWERUP);
  } else {
    ptsC++;
    audio::play(SFX_EXPLODE);
    input::rumble(250, 0x80, 0x80);
  }
  serveDir = who == 1 ? 1 : -1;
  if (ptsP >= WIN_POINTS) {
    addScore(1000 * (rival + 1));
    state = ST_WON; stateT = 0;
    audio::music(SONG_WIN);
  } else if (ptsC >= WIN_POINTS) {
    state = ST_OVER; stateT = 0;
    audio::music(SONG_OVER);
    if (newHi) arcade::saveHi(hiscore);
  } else {
    state = ST_POINT; stateT = 0;
  }
}

static void hitPaddle(bool player, float padY) {
  float off = ((by + BS / 2) - (padY + PH / 2)) / (PH / 2);   // -1..1
  off = constrain(off, -1.0f, 1.0f);
  rally++;
  bspd = min(bspd + 0.18f * arcade::speed(), 7.5f * arcade::speed());
  float spd = bspd;
  smash = false;
  if (player && input::pad.down(BTN_A | BTN_R1 | BTN_R2)) { smash = true; spd *= 1.45f; addScore(20); }
  float a = off * 1.0f;
  bvx = cosf(a) * spd * (player ? 1 : -1);
  bvy = sinf(a) * spd;
  if (player) { addScore(10); flashP = 8; } else { flashC = 8; cpuErr = frange(-1, 1) * max(2, 16 - rival * 2); }
  float hx = player ? PLX + PW : CPX;
  burst(hx, by + BS / 2, smash ? 26 : 10, smash ? rgbS(255, 216, 74) : rgbS(184, 243, 255), smash ? 2.6f : 1.4f, player ? 1.0f : -1.0f);
  audio::play(smash ? SFX_BRICK : SFX_BOUNCE, smash ? 20 : min(rally, 16));
  if (smash) { shake = 4; input::rumble(90, 0x60, 0x30); }
}

static void updateCpu() {
  float target = (CT + CB) / 2 - PH / 2;
  if (bvx > 0 && bx > cpuReactX()) {
    // predict where the ball reaches the CPU paddle, bouncing off the walls
    float t = (CPX - (bx + BS)) / bvx, yy = by + bvy * t;
    float span = CB - CT - BS;
    float rel = fmodf(yy - CT, 2 * span);
    if (rel < 0) rel += 2 * span;
    yy = CT + (rel > span ? 2 * span - rel : rel);
    target = yy + BS / 2 - PH / 2 + cpuErr;
  }
  float d = target - cy, mx = cpuMaxSpeed();
  cpuV = constrain(d * 0.25f, -mx, mx);
  cy = constrain(cy + cpuV, (float)CT, (float)(CB - PH));
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  py = constrain(py + in.ay * 4.2f, (float)CT, (float)(CB - PH));
  updateCpu();
  if (flashP) flashP--;
  if (flashC) flashC--;
  memmove(trail + 1, trail, sizeof(trail) - sizeof(trail[0]));
  trail[0] = {(int16_t)bx, (int16_t)by};
  int n = (int)ceilf(fabsf(bvx) / 2.0f) + 1;
  for (int k = 0; k < n && state == ST_PLAY; k++) {
    bx += bvx / n; by += bvy / n;
    if (by < CT) { by = CT; bvy = fabsf(bvy); audio::play(SFX_BOUNCE, 3); }
    if (by > CB - BS) { by = CB - BS; bvy = -fabsf(bvy); audio::play(SFX_BOUNCE, 3); }
    if (bvx < 0 && bx <= PLX + PW && bx + BS >= PLX && by + BS >= py && by <= py + PH) { bx = PLX + PW; hitPaddle(true, py); }
    else if (bvx > 0 && bx + BS >= CPX && bx <= CPX + PW && by + BS >= cy && by <= cy + PH) { bx = CPX - BS; hitPaddle(false, cy); }
    if (bx < -BS) pointTo(2);
    else if (bx > SW) pointTo(1);
  }
  if (smash && (frameNo & 1)) for (auto& p : parts) if (!p.on) { p = {true, bx + BS / 2, by + BS / 2, 0, 0, 10, rgbS(255, 216, 74)}; break; }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.94f; p.vy *= 0.94f; if (--p.life == 0) p.on = false; }
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

// title demo rally
static float dbx = 100, dby = 150, dvx = 3.0f, dvy = 1.7f;

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      dbx += dvx; dby += dvy;
      if (dbx < 30 || dbx > SW - 36) dvx = -dvx;
      if (dby < 104 || dby > 226) dvy = -dvy;
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_SERVE:
    case ST_POINT:
      if (in.hit(BTN_START)) { arcade::pause(); break; }
      py = constrain(py + in.ay * 4.2f, (float)CT, (float)(CB - PH));
      if (stateT > (state == ST_SERVE ? 90 : 70)) serve();
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_WON:
      if (stateT > 180) { rival++; startMatch(); }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ draw
static uint16_t bgRow[SH];
static void buildBackground() {
  for (int y = 0; y < SH; y++) bgRow[y] = lerpS(8, 6, 28, 20, 8, 44, (float)y / SH);
}

static void drawCourt() {
  uint16_t grid = rgbS(28, 26, 70);
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t c = bgRow[y];
    uint16_t* d = B + r * SW;
    bool gl = y > CT && y < CB && ((y - CT) % 20) == 0;
    for (int x = 0; x < SW; x++) d[x] = (gl || (y > CT && y < CB && (x % 20) == 0)) ? grid : c;
  }
  if (state == ST_TITLE) return;
  uint16_t wall = rgbS(122, 61, 184), wallHi = rgbS(194, 120, 240);
  rectf(0, CT - 4, SW, 4, wall); rectf(0, CT - 1, SW, 1, wallHi);
  rectf(0, CB, SW, 4, wall); rectf(0, CB, SW, 1, wallHi);
  for (int y = CT + 2; y < CB; y += 12) rectf(SW / 2 - 1, y, 2, 7, rgbS(90, 80, 150));
  circle(SW / 2, (CT + CB) / 2, 26, c565(60, 50, 120));
}

static void drawPaddle(int x, float y, const uint8_t* c, int flash) {
  int yy = (int)y + shakeY, xx = x + shakeX;
  if (!rowsVisible(yy - 2, PH + 4)) return;
  uint16_t body = flash ? rgbS(255, 255, 255) : rgbS(c[0], c[1], c[2]);
  uint16_t glow = rgbS(c[0] / 3, c[1] / 3, c[2] / 3);
  rectf(xx - 2, yy - 2, PW + 4, PH + 4, glow);
  rectf(xx, yy, PW, PH, body);
  rectf(xx + 1, yy + 1, 2, PH - 2, rgbS(min(255, c[0] + 90), min(255, c[1] + 90), min(255, c[2] + 90)));
  rectf(xx, yy + PH / 2 - 2, PW, 4, rgbS(c[0] / 2, c[1] / 2, c[2] / 2));
}

static void drawBall(int x, int y) {
  if (!rowsVisible(y - 1, BS + 2)) return;
  uint16_t c = smash ? ((frameNo & 2) ? rgbS(255, 216, 74) : rgbS(255, 255, 255)) : rgbS(255, 255, 255);
  rectf(x + 1, y, BS - 2, BS, c); rectf(x, y + 1, BS, BS - 2, c);
  pset(x + 1, y + 1, rgbS(184, 243, 255));
}

static void drawScore() {
  if (Y0 != 0) return;
  rectf(0, 0, SW, CT - 4, rgbS(10, 8, 25));
  textf(4, 3, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
  textf(SW - 4, 3, c565(255, 216, 74), 1, top_right, "HI %07lu", (unsigned long)hiscore);
  const uint8_t* rc = RIVCOL[rival % NRIVALS];
  textf(SW / 2, 3, c565(rc[0], rc[1], rc[2]), 1, top_center, "VS %s", RIVALS[rival % NRIVALS]);
  // points as pips
  for (int i = 0; i < WIN_POINTS; i++) {
    rectf(SW / 2 - 18 - i * 8, 15, 6, 6, i < ptsP ? rgbS(62, 198, 224) : rgbS(51, 48, 122));
    rectf(SW / 2 + 12 + i * 8, 15, 6, 6, i < ptsC ? rgbS(rc[0], rc[1], rc[2]) : rgbS(51, 48, 122));
  }
  textf(SW / 2, 14, TFT_WHITE, 1, top_center, "%d", rival + 1);
}

static void draw() {
  drawCourt();
  if (state == ST_TITLE) {
    // demo rally in the background
    float lp = constrain(dby - 17, 104.0f, 196.0f), rp = constrain(dby - 17 + 10 * sinf(frameNo * 0.05f), 104.0f, 196.0f);
    drawPaddle(20, lp, RIVCOL[1], 0);
    drawPaddle(SW - 26, rp, RIVCOL[5], 0);
    drawBall((int)dbx, (int)dby);
    if (rowsVisible(96, 80)) shade(50, 96, 220, 80);
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 36);
    text("PADDLE TENNIS WITH A SPARK", SW / 2, 76, c565(184, 243, 255), 1, top_center);
    textf(SW / 2, 16, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 104, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 108, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(132);
    text("\x1e\x1f MOVE   HOLD A: SMASH", SW / 2, 152, c565(180, 194, 220), 1, top_center);
    text("FIRST TO 5 BEATS EACH RIVAL", SW / 2, 164, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 226, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawScore();
  const uint8_t* rc = RIVCOL[rival % NRIVALS];
  static const uint8_t PCOL[3] = {62, 198, 224};
  drawPaddle(PLX, py, PCOL, flashP);
  drawPaddle(CPX, cy, rc, flashC);
  if (state == ST_PLAY) {
    for (int i = 9; i >= 1; i -= 2) {
      int x = trail[i].x + shakeX, y = trail[i].y + shakeY;
      if (rowsVisible(y, BS)) rectf(x + 1, y + 1, BS - 2, BS - 2, smash ? lerpS(255, 216, 74, 40, 20, 60, i / 10.0f) : lerpS(184, 243, 255, 30, 30, 70, i / 10.0f));
    }
    drawBall((int)bx + shakeX, (int)by + shakeY);
  }
  for (auto& p : parts) if (p.on) { pset((int)p.x, (int)p.y, p.col); if (p.life > 18) pset((int)p.x, (int)p.y + 1, p.col); }
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_SERVE) {
    if (rowsVisible(88, 60)) shade(40, 88, 240, 60);
    textf(SW / 2, 96, c565(rc[0], rc[1], rc[2]), 2, top_center, "RIVAL %d", rival + 1);
    text(RIVALS[rival % NRIVALS], SW / 2, 118, TFT_WHITE, 2, top_center);
  } else if (state == ST_POINT) {
    text(lastWinner == 1 ? "POINT!" : "MISSED", SW / 2, 110, lastWinner == 1 ? c565(62, 198, 224) : c565(235, 60, 80), 2, top_center);
  } else if (state == ST_WON) {
    if (rowsVisible(88, 60)) shade(40, 88, 240, 60);
    text("MATCH WON!", SW / 2, 96, c565(182, 255, 110), 2, top_center);
    textf(SW / 2, 120, TFT_WHITE, 1, top_center, "BONUS %d", 1000 * (rival + 1));
    text("NEXT RIVAL...", SW / 2, 134, c565(184, 243, 255), 1, top_center);
  } else if (state == ST_OVER) {
    if (rowsVisible(84, 70)) { shade(60, 84, 200, 70); shade(60, 84, 200, 70); }
    text("GAME OVER", SW / 2, 92, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 122, c565(255, 216, 74), 1, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 138, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("voltrally", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildBackground();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
