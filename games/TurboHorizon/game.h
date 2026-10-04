// =====================================================================
//  TURBO HORIZON  -  a pseudo-3D road racer (Nova Arcade)
//  Race the clock through three stages: Sunset Coast, Canyon Run and
//  Neon Night. Reach each checkpoint before the timer runs out to earn
//  more time. Weave through traffic, stay on the tarmac (the verge slows
//  you down and roadside scenery stops you dead) and keep your foot down.
//  The road is drawn the classic way: track segments are projected from
//  the camera every frame, then each screen row is filled with grass,
//  rumble strips, tarmac and lane lines. Scenery and cars are scaled sprites.
//  Art: tools/make_art.py -> art.h
// =====================================================================
#include <ArcadeCore.h>
#include "art.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace thm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 t C */ {72, 1, 1, 76, 79, 1, 1, 1, 84, 1, 83, 1, 79, 1, 1, 1},
  /*1 t G */ {79, 1, 1, 81, 83, 1, 1, 1, 86, 1, 84, 1, 83, 1, 1, 1},
  /*2 t Am*/ {81, 1, 1, 79, 76, 1, 1, 1, 72, 1, 74, 1, 76, 1, 1, 1},
  /*3 t F */ {77, 1, 76, 1, 74, 1, 72, 1, 74, 1, 1, 1, 0, 0, 0, 0},
  /*4 r C */ {84, 0, 84, 0, 83, 1, 84, 1, 79, 1, 1, 1, 76, 1, 79, 1},
  /*5 r G */ {83, 0, 83, 0, 81, 1, 83, 1, 86, 1, 1, 1, 83, 1, 1, 1},
  /*6 r Am*/ {81, 0, 81, 0, 79, 1, 81, 1, 76, 1, 1, 1, 72, 1, 76, 1},
  /*7 r F */ {77, 1, 79, 1, 81, 1, 84, 1, 86, 1, 84, 1, 81, 1, 79, 1},
  /*8 r Dm*/ {74, 1, 77, 1, 81, 1, 1, 1, 79, 1, 77, 1, 74, 1, 77, 1},
  /*9 r E */ {76, 1, 80, 1, 83, 1, 1, 1, 88, 1, 1, 1, 86, 1, 83, 1},
  /*10 ck*/  {84, 88, 91, 96, 1, 1, 91, 1, 96, 1, 1, 1, 1, 1, 0, 0},
  /*11 ov*/  {79, 1, 1, 77, 76, 1, 1, 74, 72, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K...h...K...h.h.", "K.hhS.hhK.hhS.hh", "K.hhS.hKK.hhS.SS", "................"};
static const Bar TITLE_BARS[] = {{C_, 0, 0}, {G_, 1, 0}, {AM, 2, 0}, {F_, 3, 0}};
static const Bar RACE_BARS[] = {{C_, 4, 1}, {G_, 5, 1}, {AM, 6, 1}, {F_, 7, 2},
                                {C_, 4, 1}, {G_, 5, 1}, {DM, 8, 1}, {E_, 9, 2}};
static const Bar CHECK_BARS[] = {{C_, 10, 3}};
static const Bar OVER_BARS[] = {{F_, 11, 3}, {C_, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 112, true, false, 0x30},
  {RACE_BARS, 8, 150, true, true, 0x40},
  {CHECK_BARS, 1, 150, false, false, 0},
  {OVER_BARS, 2, 90, false, false, 0},
};
}  // namespace thm
static const audio::Music MUSIC = {audio::STD_CHORDS, thm::LEADS, thm::DRUMS, thm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_RACE, SONG_CHECK, SONG_OVER };

// ------------------------------------------------------------ road geometry
static const float SEGL = 200, ROADW = 1400, CAMH = 1000, CAMD = 0.84f, PLAYERZ = CAMH * 0.84f;
static const float U = 6.25f;                 // world units per sprite pixel
static const float MAXSPD = SEGL;             // world units per frame at top speed
static const int DRAWN = 120, STRIPE = 3, MAXSEG = 2200;

struct Seg { float curve, y; uint8_t spr; int8_t off; };   // off: sprite offset x 0.1 road half-widths
static Seg segs[MAXSEG];
static int nseg = 0;
static float trackLen = 0;

static inline float easeIn(float a, float b, float t) { return a + (b - a) * t * t; }
static inline float easeInOut(float a, float b, float t) { return a + (b - a) * ((-cosf(t * 3.14159f) / 2) + 0.5f); }
static inline float lastY() { return nseg ? segs[nseg - 1].y : 0; }
static void addSeg(float curve, float y) { if (nseg < MAXSEG) segs[nseg++] = {curve, y, 0, 0}; }
static void addRoad(int enter, int hold, int leave, float curve, float hill) {
  float y0 = lastY(), y1 = y0 + hill * SEGL;
  int total = enter + hold + leave;
  for (int i = 0; i < enter; i++) addSeg(easeIn(0, curve, (float)i / enter), easeInOut(y0, y1, (float)i / total));
  for (int i = 0; i < hold; i++) addSeg(curve, easeInOut(y0, y1, (float)(enter + i) / total));
  for (int i = 0; i < leave; i++) addSeg(easeInOut(curve, 0, (float)i / leave), easeInOut(y0, y1, (float)(enter + hold + i) / total));
}

// Each stage is a list of road pieces: enter, hold, leave, curve, hill.
struct Piece { int16_t e, h, l; int8_t curve; int8_t hill; };
static const Piece STAGE0[] = {   // Sunset Coast: sweeping bends and gentle rises
  {0, 80, 0, 0, 0}, {30, 60, 30, 2, 10}, {30, 80, 30, -3, -10}, {25, 60, 25, 0, 20}, {30, 90, 30, 4, 0},
  {25, 50, 25, -2, -20}, {25, 60, 25, 0, 15}, {30, 70, 30, -4, 0}, {20, 40, 20, 3, -15}, {25, 70, 25, 0, 25},
  {30, 60, 30, 5, -10}, {25, 60, 25, -3, 0}, {20, 60, 20, 0, -15},
};
static const Piece STAGE1[] = {   // Canyon Run: big crests and tighter turns
  {0, 80, 0, 0, 0}, {25, 50, 25, 0, 40}, {25, 60, 25, 5, -20}, {20, 50, 20, -5, -20}, {30, 40, 30, 0, 35},
  {25, 70, 25, 3, -35}, {20, 40, 20, -6, 0}, {20, 50, 20, 6, 20}, {25, 60, 25, 0, -30}, {25, 50, 25, -4, 25},
  {25, 70, 25, 2, -10}, {20, 40, 20, 5, 0},
};
static const Piece STAGE2[] = {   // Neon Night: city S-bends
  {0, 80, 0, 0, 0}, {20, 40, 20, 4, 0}, {20, 40, 20, -4, 10}, {20, 40, 20, 5, -10}, {25, 60, 25, 0, 20},
  {20, 30, 20, -6, 0}, {20, 30, 20, 6, -20}, {25, 80, 25, -2, 0}, {20, 40, 20, 5, 15}, {20, 40, 20, -5, -15},
  {25, 60, 25, 0, 25}, {20, 50, 20, 4, -10}, {20, 40, 20, -3, 0},
};
struct StageDef { const char* name; const Piece* pieces; int npieces; float time; };
static const StageDef STAGES[3] = {
  {"SUNSET COAST", STAGE0, sizeof(STAGE0) / sizeof(Piece), 44},
  {"CANYON RUN", STAGE1, sizeof(STAGE1) / sizeof(Piece), 32},
  {"NEON NIGHT", STAGE2, sizeof(STAGE2) / sizeof(Piece), 30},
};

// Stage colours (rgb): sky top, sky bottom, fog, grass L/D, rumble L/D, road L/D, lane, far layer, near layer
struct Theme { uint8_t c[12][3]; };
static const Theme THEMES[3] = {
  {{{40, 20, 90}, {255, 120, 110}, {255, 150, 130}, {232, 196, 120}, {214, 174, 100}, {255, 255, 255}, {220, 40, 60},
    {120, 112, 128}, {112, 104, 120}, {255, 255, 255}, {120, 60, 130}, {70, 120, 170}}},
  {{{40, 110, 210}, {190, 230, 250}, {220, 210, 190}, {220, 130, 70}, {200, 112, 58}, {255, 255, 255}, {60, 60, 70},
    {130, 120, 116}, {122, 112, 108}, {255, 230, 120}, {200, 96, 60}, {160, 70, 50}}},
  {{{6, 4, 24}, {60, 20, 90}, {50, 20, 80}, {24, 20, 50}, {18, 14, 40}, {255, 60, 190}, {60, 230, 255},
    {44, 42, 62}, {38, 36, 56}, {255, 220, 120}, {30, 22, 60}, {16, 12, 36}}},
};
enum { TC_SKY0, TC_SKY1, TC_FOG, TC_GRASS0, TC_GRASS1, TC_RUMB0, TC_RUMB1, TC_ROAD0, TC_ROAD1, TC_LANE, TC_FAR, TC_NEAR };

// ------------------------------------------------------------ cars and player
struct Car { float z, off, target, speed; uint8_t type; int8_t ahead; };
static Car cars[24];
static int ncars = 0;
static float position = 0, speed = 0, playerX = 0, playerY = 0, bgFar = 0, bgNear = 0;
static float timeLeft = 0;
static int stage = 0, lap = 0, crashT = 0, offroadT = 0, steerDir = 0, checkT = 0, passes = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 30000;
static bool newHi = false, braking = false;
static float scoreAcc = 0;

enum State { ST_TITLE, ST_COUNT, ST_RACE, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

struct Puff { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Puff puffs[60];
struct Popup { bool on; int x, y, t; char txt[14]; };
static Popup popups[4];

static void popup(const char* s) {
  for (auto& p : popups) if (p.on) p.y -= 12;
  for (auto& p : popups) if (!p.on) { p.on = true; p.x = SW / 2; p.y = 96; p.t = 70; strncpy(p.txt, s, 13); p.txt[13] = 0; return; }
}
static void addScore(uint32_t v) { score += v; if (score > hiscore) { hiscore = score; newHi = true; } }

static inline int segIndex(float z) { int i = (int)floorf(z / SEGL) % nseg; return i < 0 ? i + nseg : i; }
static inline float wrapDz(float dz) { while (dz > trackLen / 2) dz -= trackLen; while (dz < -trackLen / 2) dz += trackLen; return dz; }

static uint16_t col16[12], fogTab[7][16];
static uint8_t farH[512], nearH[512];

static void buildStage(int st) {
  st %= 3;
  const StageDef& sd = STAGES[st];
  nseg = 0;
  for (int i = 0; i < sd.npieces; i++) { const Piece& p = sd.pieces[i]; addRoad(p.e, p.h, p.l, p.curve, p.hill); }
  addRoad(40, 40, 40, 0, -lastY() / SEGL);    // level out so stages join up
  addRoad(0, 140, 0, 0, 0);                   // run-out to the checkpoint
  trackLen = nseg * SEGL;
  // scenery
  for (int i = 0; i < nseg; i++) {
    Seg& s = segs[i];
    int side = (i / 2) & 1 ? 1 : -1;
    uint32_t h = (uint32_t)i * 2654435761u >> 16;
    if (fabsf(s.curve) > 2.5f && i % 6 == 0) { s.spr = SC_CHEVRON + 1; s.off = (int8_t)(s.curve > 0 ? -13 : 13); continue; }
    if (st == 0) {
      if (i % 9 == 0) { s.spr = (h & 1 ? SC_PALM0 : SC_PALM1) + 1; s.off = (int8_t)(side * (14 + h % 8)); }
      else if (i % 97 == 40) { s.spr = SC_BILL0 + 1 + (i / 97) % 3; s.off = (int8_t)(side * 18); }
      else if (i % 9 == 5 && h % 3 == 0) { s.spr = SC_BUSH + 1; s.off = (int8_t)(-side * (13 + h % 10)); }
    } else if (st == 1) {
      if (i % 11 == 0) { s.spr = SC_CACTUS + 1; s.off = (int8_t)(side * (13 + h % 12)); }
      else if (i % 23 == 7) { s.spr = SC_ROCK + 1; s.off = (int8_t)(side * (15 + h % 10)); }
      else if (i % 61 == 30) { s.spr = SC_MESA + 1; s.off = (int8_t)(side * (30 + h % 20)); }
      else if (i % 131 == 70) { s.spr = SC_BILL0 + 1 + (i / 131) % 3; s.off = (int8_t)(side * 18); }
    } else {
      if (i % 8 == 0) { s.spr = SC_LAMP + 1; s.off = (int8_t)(side * 12); }
      else if (i % 14 == 4) { s.spr = (h & 1 ? SC_TOWER0 : SC_TOWER1) + 1; s.off = (int8_t)(side * (24 + h % 16)); }
      else if (i % 89 == 45) { s.spr = SC_BILL0 + 1 + (i / 89) % 3; s.off = (int8_t)(side * 18); }
    }
  }
  segs[nseg - 20].spr = SC_BANNERC + 1;
  // colours, with 16 fog levels for the road colours
  const Theme& th = THEMES[st];
  for (int k = 0; k < 12; k++) col16[k] = rgbS(th.c[k][0], th.c[k][1], th.c[k][2]);
  for (int k = 0; k < 7; k++)
    for (int f = 0; f < 16; f++) {
      const uint8_t* a = th.c[TC_GRASS0 + k]; const uint8_t* fg = th.c[TC_FOG];
      float t = f / 15.0f;
      fogTab[k][f] = lerpS(a[0], a[1], a[2], fg[0], fg[1], fg[2], t);
    }
  // background layers (heights, 512 wide, wrap round)
  for (int x = 0; x < 512; x++) {
    float a = x * 6.2832f / 512;
    if (st == 0) { farH[x] = (uint8_t)(34 + 16 * sinf(a * 3) + 9 * sinf(a * 7 + 1) + 4 * sinf(a * 17)); nearH[x] = (uint8_t)(6 + 3 * sinf(a * 9)); }
    else if (st == 1) {
      float m = sinf(a * 4 + 0.5f); farH[x] = (uint8_t)(m > 0.3f ? 46 + 4 * sinf(a * 30) : 18 + 8 * sinf(a * 11));
      nearH[x] = (uint8_t)(14 + 10 * fabsf(sinf(a * 6)) + 3 * sinf(a * 23));
    } else {
      uint32_t hh = ((x / 9) * 2654435761u) >> 24; farH[x] = (uint8_t)(20 + hh % 50);
      uint32_t h2 = ((x / 14 + 7) * 2246822519u) >> 24; nearH[x] = (uint8_t)(8 + h2 % 26);
    }
  }
}

static void spawnCars() {
  int want = arcade::difficulty == 0 ? 12 : arcade::difficulty == 1 ? 16 : 22;
  ncars = min(24, want + lap * 2);
  for (int i = 0; i < ncars; i++) {
    Car& c = cars[i];
    c.z = fmodf(position + PLAYERZ + 3000 + (trackLen - 6000) * i / ncars + frange(0, 800), trackLen);
    static const float LANES[3] = {-0.66f, 0, 0.66f};
    c.off = c.target = LANES[rnd() % 3];
    c.speed = MAXSPD * frange(0.28f, 0.55f);
    c.type = rnd() % 4;
    c.ahead = 1;
  }
}

static void startRace() {
  stage = 0; lap = 0; position = 0; speed = 0; playerX = 0; score = 0; newHi = false; scoreAcc = 0; passes = 0;
  crashT = 0; checkT = 0;
  buildStage(0);
  segs[10].spr = SC_BANNERS + 1;
  spawnCars();
  timeLeft = STAGES[0].time / arcade::speed();
  hiscore = arcade::loadHi(HI_DEFAULT);
  state = ST_COUNT; stateT = 0;
  audio::music(SONG_NONE);
}

static void reachCheckpoint() {
  float oldLen = trackLen;
  stage++;
  if (stage % 3 == 0) lap++;
  buildStage(stage);
  position -= oldLen;
  spawnCars();
  float bonus = STAGES[stage % 3].time / arcade::speed() * (lap ? 0.9f : 1.0f);
  timeLeft += bonus;
  checkT = 150;
  addScore(5000 + 1000 * stage);
  char b[14]; snprintf(b, sizeof(b), "+%d SEC", (int)bonus);
  popup(b);
  audio::play(SFX_POWERUP);
}

// ------------------------------------------------------------ driving
static int sprWidthPx(int type) { return SCENERY[type]->w; }

static void crash() {
#ifdef TH_DEBUG
  printf("crash x=%.2f seg=%d\n", playerX, segIndex(position + PLAYERZ));
#endif
  crashT = 50;
  speed *= 0.15f;
  audio::play(SFX_EXPLODE); audio::play(SFX_SKID);
  input::rumble(300, 200, 255);
}

// Autopilot used by the title screen and the test bot: choose the safest lane, follow the bends.
static void autopilot(float* steer, bool* gas, bool* brake) {
  static const float LANES[3] = {-0.66f, 0, 0.66f};
  float pz = position + PLAYERZ, best = 1e9, tgt = 0;
  for (float l : LANES) {
    float danger = fabsf(l - playerX) * 0.6f;
    for (int i = 0; i < ncars; i++) {
      float dz = wrapDz(cars[i].z - pz);
      if (dz < -100 || dz > 4500) continue;
      if (fabsf(cars[i].off - l) < 0.45f) danger += 4.0f * (1 - dz / 4500);
    }
    if (danger < best) { best = danger; tgt = l; }
  }
  const Seg& s = segs[segIndex(pz)];
  float push = (speed / MAXSPD) * s.curve * 0.12f;
  *steer = constrain((tgt - playerX) * 4.0f + push, -1.0f, 1.0f);   // feed-forward for the bend
  *gas = true; *brake = false;
  for (int i = 0; i < ncars; i++) {
    float dz = wrapDz(cars[i].z - pz);
    if (dz > 0 && dz < 900 && fabsf(cars[i].off - playerX) < 0.4f && speed > cars[i].speed) { *gas = false; *brake = dz < 500; }
  }
}

static void drive(float steer, bool gas, bool brake) {
  float sp = speed / MAXSPD;
  int pseg = segIndex(position + PLAYERZ);
  const Seg& s = segs[pseg];
  float dx = 2.0f / 60 * sp;
  if (crashT) { crashT--; steer = 0; gas = false; playerX += (playerX > 0 ? -1 : 1) * 0.02f * (fabsf(playerX) > 0.8f); }
  playerX += dx * steer;
  playerX -= dx * sp * s.curve * 0.12f;          // the bend pushes you outwards
  steerDir = steer > 0.3f ? 1 : steer < -0.3f ? -1 : 0;
  braking = brake;
  if (gas) speed += MAXSPD / 330 * (1.15f - sp * 0.5f);
  else if (brake) speed -= MAXSPD / 70;
  else speed -= MAXSPD / 500;
  bool off = fabsf(playerX) > 1.0f;
  if (off) {
    offroadT++;
    if (speed > MAXSPD / 3) speed -= MAXSPD / 90;
    if ((frameNo & 3) == 0 && speed > 10) {
      for (auto& p : puffs) if (!p.on) { p = {true, (float)(SW / 2 + (rnd() % 60) - 30), 232, frange(-1, 1), frange(-1.5f, -0.4f), 24, col16[TC_GRASS1]}; break; }
    }
  } else offroadT = 0;
  playerX = constrain(playerX, -2.6f, 2.6f);
  speed = constrain(speed, 0.0f, MAXSPD);
  // tyre squeal on hard cornering
  if (fabsf(s.curve) > 3.5f && sp > 0.8f && fabsf(steer) > 0.6f && (frameNo % 20) == 0) {
    audio::play(SFX_SKID);
    for (int k = 0; k < 2; k++) for (auto& p : puffs) if (!p.on) { p = {true, (float)(SW / 2 + (k ? 30 : -30)), 230, frange(-0.6f, 0.6f), -0.6f, 30, rgbS(220, 220, 230)}; break; }
  }
  // collisions with roadside scenery
  if (off && !crashT) {
    for (int n = 0; n < 2; n++) {
      const Seg& q = segs[(pseg + n) % nseg];
      if (!q.spr || q.spr - 1 == SC_BUSH || q.spr - 1 >= SC_BANNERC) continue;
      float so = q.off / 10.0f, half = sprWidthPx(q.spr - 1) * U / ROADW / 2;
      if (q.spr - 1 == SC_MESA) half *= 0.8f;
      if (fabsf(so - playerX) < half + 0.25f) { crash(); break; }
    }
  }
  // cars
  float pz = position + PLAYERZ;
  for (int i = 0; i < ncars; i++) {
    Car& c = cars[i];
    float dz = wrapDz(c.z - pz);
    if (!crashT && dz > 0 && dz < 140 && speed > c.speed && fabsf(c.off - playerX) < 0.62f) {
      speed = c.speed * 0.6f;
      position = c.z - PLAYERZ - 160;
      audio::play(SFX_HIT); audio::play(SFX_LAND);
      input::rumble(150, 150, 150);
      offroadT = 8;
    }
    int8_t ahead = dz > 0 ? 1 : -1;
    if (c.ahead == 1 && ahead == -1 && fabsf(dz) < 2000 && state == ST_RACE) { passes++; addScore(200); }
    c.ahead = ahead;
  }
  // move on
  position += speed;
  float pz2 = position + PLAYERZ;
  if (state == ST_RACE && pz2 >= trackLen) reachCheckpoint();
  else if (pz2 >= trackLen) position -= trackLen;
  int ps = segIndex(position + PLAYERZ);
  float pct = fmodf(position + PLAYERZ, SEGL) / SEGL; if (pct < 0) pct += 1;
  playerY = segs[ps].y + (segs[(ps + 1) % nseg].y - segs[ps].y) * pct;
  bgFar += segs[ps].curve * sp * 0.25f;
  bgNear += segs[ps].curve * sp * 0.6f;
  if (state == ST_RACE) {
    scoreAcc += speed * 0.02f;
    if (scoreAcc >= 1) { addScore((uint32_t)scoreAcc); scoreAcc -= (int)scoreAcc; }
  }
}

static void updateCars() {
  static const float LANES[3] = {-0.66f, 0, 0.66f};
  for (int i = 0; i < ncars; i++) {
    Car& c = cars[i];
    c.z += c.speed;
    if (c.z >= trackLen) c.z -= trackLen;
    if (rnd() % 600 == 0) c.target = LANES[rnd() % 3];
    c.off += constrain(c.target - c.off, -0.006f, 0.006f);
  }
}

static void step(const Pad& in) {
  stateT++;
  for (auto& p : puffs) if (p.on) { p.x += p.vx; p.y += p.vy; if (!--p.life) p.on = false; }
  for (auto& p : popups) if (p.on && --p.t <= 0) p.on = false;
  if (checkT) checkT--;
  float steer = 0; bool gas = false, brake = false;
  switch (state) {
    case ST_TITLE:
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { audio::play(SFX_START); startRace(); break; }
      autopilot(&steer, &gas, &brake);
      if (speed > MAXSPD * 0.7f) gas = false;
      drive(steer, gas, brake);
      updateCars();
      audio::engine(0, 0);
      break;
    case ST_COUNT:
      if (stateT % 60 == 0 && stateT < 240) audio::play(stateT == 180 ? SFX_SELECT : SFX_BLIP);
      audio::engine(55 + (in.down(BTN_A) ? 50 : 0), 0.45f);
      if (stateT >= 180) { state = ST_RACE; stateT = 0; audio::music(SONG_RACE); }
      break;
    case ST_RACE: {
      if (in.hit(BTN_START)) { arcade::pause(); break; }
      steer = in.ax;
      gas = in.down(BTN_A | BTN_R2 | BTN_R1) || (in.ay < -0.5f && !in.down(BTN_SELECT));
      brake = in.down(BTN_B | BTN_L2 | BTN_L1) || in.ay > 0.5f;
      if (gas && brake) gas = false;
      drive(steer, gas, brake);
      updateCars();
      int prev = (int)ceilf(timeLeft);
      timeLeft -= 1.0f / 60;
      if (timeLeft < 10 && (int)ceilf(timeLeft) != prev && timeLeft > 0) audio::play(SFX_WARNING);
      if (timeLeft <= 0) {
        timeLeft = 0; state = ST_OVER; stateT = 0;
        audio::music(SONG_OVER);
        if (newHi) arcade::saveHi(hiscore);
      }
      float sp = speed / MAXSPD;
      int gear = min(4, (int)(sp * 5));
      float rev = sp * 5 - gear;
      audio::engine(48 + gear * 16 + rev * 75 + (offroadT ? (frameNo & 2) * 6 : 0), 0.5f);
      break;
    }
    case ST_OVER:
      drive(0, false, true);
      updateCars();
      audio::engine(45 + speed / MAXSPD * 120, speed > 2 ? 0.4f : 0.0f);
      if (stateT > 150 && in.hit(BTN_START | BTN_A)) {
        state = ST_TITLE; stateT = 0; stage = 0; lap = 0; buildStage(0); spawnCars(); audio::music(SONG_TITLE);
      }
      break;
  }
}

// ------------------------------------------------------------ projection (once per frame)
static int16_t rowSeg[SH];       // -1 = background
static float rowX[SH], rowW[SH];
static uint8_t rowFog[SH], rowAlt[SH];
static int horizon = 120;
static int16_t farTop[SW], nearTop[SW];
struct Cmd { const Sprite* s; float z; int16_t x, y, w, h, clip; bool flip; };
static Cmd cmds[96];
static int ncmds = 0;

static void addCmd(const Sprite* s, float z, float cx, float yb, float w, float h, int clip, bool flip) {
  if (ncmds >= 96 || w < 1 || h < 1) return;
  if (cx + w / 2 < 0 || cx - w / 2 > SW || yb - h > clip) return;
  cmds[ncmds++] = {s, z, (int16_t)(cx - w / 2), (int16_t)yb, (int16_t)w, (int16_t)h, (int16_t)clip, flip};
}

static void project() {
  for (int y = 0; y < SH; y++) rowSeg[y] = -1;
  ncmds = 0;
  int base = segIndex(position);
  float baseZ = floorf(position / SEGL) * SEGL;
  float basePct = (position - baseZ) / SEGL;
  float camX = playerX * ROADW, camY = playerY + CAMH;
  float x = 0, dx = -segs[base].curve * basePct;
  float maxy = SH;
  static float pX[DRAWN + 1], pY[DRAWN + 1], pS[DRAWN + 1], pClip[DRAWN + 1];
  static bool pOk[DRAWN + 1];
  for (int n = 0; n < DRAWN; n++) {
    int i = (base + n) % nseg, j = (i + 1) % nseg;
    float z1 = baseZ + n * SEGL - position, z2 = z1 + SEGL;
    pOk[n] = false;
    pClip[n] = maxy;
    if (z1 <= CAMD) { x += dx; dx += segs[i].curve; continue; }
    float s1 = CAMD / z1, s2 = CAMD / z2;
    float X1 = 160 + s1 * (x - camX) * 160, X2 = 160 + s2 * (x + dx - camX) * 160;
    float Y1 = 120 - s1 * (segs[i].y - camY) * 120, Y2 = 120 - s2 * (segs[j].y - camY) * 120;
    float W1 = s1 * ROADW * 160, W2 = s2 * ROADW * 160;
    pX[n] = X1; pY[n] = Y1; pS[n] = s1; pOk[n] = true;
    pX[n + 1] = X2; pY[n + 1] = Y2; pS[n + 1] = s2;
    x += dx; dx += segs[i].curve;
    if (Y2 >= Y1 || Y2 >= maxy) continue;
    float keep = expf(-(float)(n * n) / (DRAWN * DRAWN) * 4.0f);
    uint8_t fog = (uint8_t)constrain((int)((1 - keep) * 15.99f), 0, 15);
    uint8_t alt = ((base + n) / STRIPE) & 1;
    int ya = max(0, (int)ceilf(Y2)), yb = min((int)ceilf(maxy), (int)ceilf(Y1));
    for (int y = ya; y < yb && y < SH; y++) {
      float t = (Y1 - y) / (Y1 - Y2);
      rowSeg[y] = (int16_t)n; rowX[y] = X1 + (X2 - X1) * t; rowW[y] = W1 + (W2 - W1) * t;
      rowFog[y] = fog; rowAlt[y] = alt;
    }
    maxy = Y2;
  }
  horizon = constrain((int)maxy, 40, 200);
  // scenery, far to near
  for (int n = DRAWN - 1; n >= 1; n--) {
    if (!pOk[n]) continue;
    const Seg& sg = segs[(base + n) % nseg];
    if (!sg.spr) continue;
    int t = sg.spr - 1;
    float k = pS[n] * 160;
    if (t == SC_BANNERC || t == SC_BANNERS) {
      float postH = 900 * k, postW = 70 * k, span = 1.15f * ROADW * k, cx = pX[n];
      int clip = (int)pClip[n];
      addCmd(&SPR_POST, n * SEGL, cx - span, pY[n], postW, postH, clip, false);
      addCmd(&SPR_POST, n * SEGL, cx + span, pY[n], postW, postH, clip, false);
      float bw = 2 * span + postW, bh = bw * SCENERY[t]->h / SCENERY[t]->w;
      addCmd(SCENERY[t], n * SEGL - 1, cx, pY[n] - postH + bh * 0.9f, bw, bh, clip, false);
      continue;
    }
    const Sprite* s = SCENERY[t];
    float sx = pX[n] + pS[n] * (sg.off / 10.0f) * ROADW * 160;
    bool flip = (t == SC_LAMP && sg.off > 0) || (t == SC_CHEVRON && sg.off < 0);
    addCmd(s, n * SEGL, sx, pY[n], s->w * U * k, s->h * U * k, (int)pClip[n], flip);
  }
  // traffic
  for (int c = 0; c < ncars; c++) {
    float dz = cars[c].z - baseZ;
    if (dz < 0) dz += trackLen;
    int n = (int)(dz / SEGL);
    if (n < 1 || n >= DRAWN - 1 || !pOk[n]) continue;
    float pct = fmodf(dz, SEGL) / SEGL;
    float s = pS[n] + (pS[n + 1] - pS[n]) * pct;
    float cx = pX[n] + (pX[n + 1] - pX[n]) * pct + s * cars[c].off * ROADW * 160;
    float cy = pY[n] + (pY[n + 1] - pY[n]) * pct;
    const Sprite* sp = RIVAL[cars[c].type];
    addCmd(sp, dz, cx, cy, sp->w * U * s * 160, sp->h * U * s * 160, (int)pClip[n], false);
  }
  // sort far to near
  for (int a = 1; a < ncmds; a++) {
    Cmd t = cmds[a]; int b = a - 1;
    while (b >= 0 && cmds[b].z < t.z) { cmds[b + 1] = cmds[b]; b--; }
    cmds[b + 1] = t;
  }
  // background silhouettes
  for (int xx = 0; xx < SW; xx++) {
    farTop[xx] = horizon - farH[(xx + (int)bgFar) & 511];
    nearTop[xx] = horizon - nearH[(xx + (int)bgNear) & 511];
  }
}

// ------------------------------------------------------------ drawing
// Scaled sprite: left x, bottom y, destination size, rows at or below `clip` hidden (hills).
static void blitScaledClip(const Sprite& s, int x0, int yb, int w, int h, int clip, bool flip) {
  int y0 = yb - h, y1 = min(yb, clip);
  int ra = max(y0, Y0), rb = min(y1, Y0 + STRIP);
  if (ra >= rb) return;
  int xa = max(0, x0), xb = min(SW, x0 + w);
  if (xa >= xb) return;
  uint32_t stepX = ((uint32_t)s.w << 16) / w;
  for (int y = ra; y < rb; y++) {
    const uint8_t* row = s.px + ((y - y0) * s.h / h) * s.w;
    uint16_t* d = B + (y - Y0) * SW;
    uint32_t fx = (uint32_t)(xa - x0) * stepX;
    for (int x = xa; x < xb; x++, fx += stepX) {
      int sx = fx >> 16;
      uint8_t v = row[flip ? s.w - 1 - sx : sx];
      if (v) d[x] = pal[v];
    }
  }
}

static void drawRoadAndSky() {
  int st = stage % 3;
  const Theme& th = THEMES[st];
  int sunX = 230 - ((int)(bgFar * 0.5f) % 640 + 640) % 640, sunY = horizon - (st == 0 ? 44 : st == 1 ? 100 : 80);
  if (sunX < -60) sunX += 640;
  int sunR = st == 0 ? 36 : st == 1 ? 12 : 10;
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t* d = B + r * SW;
    int n = rowSeg[y];
    if (n < 0) {
      // sky gradient, sun / moon, far and near silhouettes
      float t = constrain((float)y / max(1, horizon), 0.0f, 1.0f);
      uint16_t sky = lerpS(th.c[0][0], th.c[0][1], th.c[0][2], th.c[1][0], th.c[1][1], th.c[1][2], t * t);
      int dy = y - sunY, sx0 = SW, sx1 = -1;
      uint16_t sunc = 0;
      if (dy * dy <= sunR * sunR) {
        int hw = (int)sqrtf((float)(sunR * sunR - dy * dy));
        sx0 = sunX - hw; sx1 = sunX + hw;
        float u = (float)(dy + sunR) / (2 * sunR);
        if (st == 0) { sunc = lerpS(255, 240, 120, 255, 70, 140, u); if (dy > 0 && ((dy + (int)(frameNo >> 3)) % 7) < (dy / 6 + 1)) sx1 = -1; }
        else if (st == 1) sunc = rgbS(255, 252, 230);
        else sunc = (dy * dy + (sunX - sx0) > 0) ? rgbS(230, 230, 250) : 0;
      }
      uint16_t farc = col16[TC_FAR], nearc = col16[TC_NEAR];
      for (int x = 0; x < SW; x++) {
        if (y >= nearTop[x]) d[x] = (st == 2 && ((x * 7 + y * 3) % 23 == 0) && y < horizon - 2) ? rgbS(255, 210, 110) : nearc;
        else if (y >= farTop[x]) {
          bool lit = st == 2 && ((x + (int)bgFar) % 4 == 1) && (y % 4 == 1) && (((x + (int)bgFar) * 13 + y) % 5 < 2);
          d[x] = lit ? rgbS(150, 220, 255) : (y < farTop[x] + 2 && st != 2 ? rgbS(th.c[TC_FAR][0] + 40, th.c[TC_FAR][1] + 30, th.c[TC_FAR][2] + 30) : farc);
        }
        else if (x >= sx0 && x <= sx1) d[x] = sunc;
        else d[x] = sky;
      }
      if (st == 2) {   // stars
        for (int k = 0; k < 6; k++) { int sx = ((y * 37 + k * 71) * 13) % SW; if (((sx * 7 + y) % 11) == 0 && y < horizon - 50) d[sx] = rgbS(200, 200, 255); }
      }
      continue;
    }
    int f = rowFog[y], alt = rowAlt[y];
    uint16_t g = fogTab[alt][f], rm = fogTab[2 + alt][f], rd = fogTab[4 + alt][f], ln = fogTab[6][f];
    float cx = rowX[y], w = rowW[y], rw = w / 6, lw = max(1.0f, w / 28);
    for (int x = 0; x < SW; x++) d[x] = g;
    int a = max(0, (int)(cx - w - rw)), b = min(SW, (int)(cx + w + rw));
    for (int x = a; x < b; x++) d[x] = rm;
    a = max(0, (int)(cx - w)); b = min(SW, (int)(cx + w));
    for (int x = a; x < b; x++) d[x] = rd;
    if (!alt) {
      for (int l = 1; l < 3; l++) {
        int lx = (int)(cx - w + 2 * w * l / 3 - lw / 2);
        for (int x = max(0, lx); x < min(SW, lx + (int)lw + 1); x++) d[x] = ln;
      }
    }
    // edge lines
    int e0 = (int)(cx - w), e1 = (int)(cx + w);
    if (st != 1 && w > 30) { if (e0 >= 0 && e0 < SW) d[e0] = ln; if (e1 >= 0 && e1 < SW) d[e1 - 1 < 0 ? 0 : e1 - 1] = ln; }
  }
}

static void drawSprites() {
  for (int i = 0; i < ncmds; i++) {
    const Cmd& c = cmds[i];
    if (!rowsVisible(c.y - c.h, c.h)) continue;
    blitScaledClip(*c.s, c.x, c.y, c.w, c.h, c.clip, c.flip);
  }
}

static void drawPlayer() {
  int sp = (int)(speed / MAXSPD * 100);
  int bounce = (offroadT && speed > 5) ? (int)(rnd() % 3) : ((sp > 10 && ((frameNo >> 2) & 1)) ? 1 : 0);
  int frame = steerDir ? (speed > MAXSPD * 0.5f ? 2 : 1) : 0;
  if (crashT) frame = ((crashT >> 2) & 1) ? 2 : 1;
  bool flip = crashT ? ((crashT >> 3) & 1) : steerDir < 0;
  const Sprite& s = *PCAR[frame + (braking ? 3 : 0)];
  int x = SW / 2 - s.w / 2, y = SH - s.h - 6 + bounce;
  if (rowsVisible(y + s.h - 4, 8)) { shade(x + 4, y + s.h - 3, s.w - 8, 5); }
  if (rowsVisible(y, s.h)) { if (flip) blitFlip(s, x, y); else blit(s, x, y); }
  for (auto& p : puffs) if (p.on) { int r = p.life > 16 ? 2 : 1; if (rowsVisible((int)p.y - 2, 5)) rectf((int)p.x - r, (int)p.y - r, r * 2, r * 2, p.col); }
}

static void drawHud() {
  if (rowsVisible(0, 40)) {
    text("TIME", SW / 2, 3, c565(255, 216, 74), 1, top_center);
    bool low = timeLeft < 10 && state == ST_RACE;
    uint16_t tc = low && ((frameNo >> 3) & 1) ? c565(255, 60, 70) : TFT_WHITE;
    textf(SW / 2, 13, tc, 3, top_center, "%d", (int)ceilf(timeLeft));
    text("SCORE", 6, 3, c565(184, 220, 255), 1, top_left);
    textf(6, 13, TFT_WHITE, 1, top_left, "%lu", (unsigned long)score);
    textf(SW - 6, 3, c565(184, 220, 255), 1, top_right, "STAGE %d", stage + 1);
    // progress bar to the checkpoint
    float prog = constrain((position + PLAYERZ) / trackLen, 0.0f, 1.0f);
    if (rowsVisible(14, 6)) {
      rectf(SW - 70, 15, 64, 4, rgbS(30, 30, 50));
      rectf(SW - 70, 15, (int)(64 * prog), 4, rgbS(255, 200, 60));
      rectf(SW - 70 + (int)(62 * prog), 13, 3, 8, rgbS(255, 255, 255));
    }
  }
  // speedometer
  if (rowsVisible(SH - 26, 26)) {
    int kmh = (int)(speed / MAXSPD * 293);
    textf(SW - 8, SH - 22, TFT_WHITE, 2, top_right, "%d", kmh);
    text("KM/H", SW - 8, SH - 8, c565(184, 220, 255), 1, top_right, false);
    for (int i = 0; i < 12; i++) {
      bool on = i < kmh * 12 / 293 + (kmh > 0);
      uint16_t c = on ? (i < 7 ? rgbS(90, 230, 120) : i < 10 ? rgbS(255, 210, 60) : rgbS(255, 70, 70)) : rgbS(40, 40, 60);
      rectf(8 + i * 6, SH - 8 - i, 4, 4 + i, c);
    }
  }
}

static void draw() {
  if (Y0 == 0) project();
  drawRoadAndSky();
  drawSprites();
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_TITLE) {
    drawPlayer();
    if (rowsVisible(20, 136)) shade(20, 20, 280, 136);
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 30);
    text("BEAT THE CLOCK - REACH EVERY CHECKPOINT", SW / 2, 62, c565(255, 220, 160), 1, top_center);
    textf(SW / 2, 76, c565(255, 216, 74), 1, top_center, "HI-SCORE %lu", (unsigned long)hiscore);
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 90, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 94, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(114);
    text("A ACCELERATE  B BRAKE  STEER", SW / 2, 132, c565(220, 220, 240), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 144, c565(160, 170, 200), 1, top_center);
    return;
  }
  drawPlayer();
  drawHud();
  if (state == ST_COUNT) {
    int n = 3 - stateT / 60;
    if (rowsVisible(60, 70)) {
      for (int i = 0; i < 3; i++) {
        bool lit = i < 3 - n || stateT >= 180;
        uint16_t c = stateT >= 180 ? c565(80, 255, 120) : lit ? c565(255, 60, 60) : c565(60, 30, 40);
        disc(SW / 2 - 40 + i * 40, 76, 12, c);
      }
    }
    text(STAGES[0].name, SW / 2, 46, TFT_WHITE, 2, top_center);
  }
  if (state == ST_RACE && stateT < 60) text("GO!", SW / 2, 90, c565(80, 255, 120), 4, top_center);
  if (checkT) {
    if (rowsVisible(60, 30)) shade(0, 60, SW, 30);
    if (blink || checkT < 100) text("CHECKPOINT!", SW / 2, 64, c565(255, 216, 74), 2, top_center);
    text(STAGES[stage % 3].name, SW / 2, 80, TFT_WHITE, 1, top_center);
  }
  for (auto& p : popups) if (p.on) text(p.txt, p.x, p.y + 30, (p.t & 4) ? c565(130, 255, 160) : TFT_WHITE, 2, top_center);
  if (state == ST_OVER) {
    if (rowsVisible(70, 84)) { shade(40, 70, 240, 84); shade(40, 70, 240, 84); }
    text("TIME UP", SW / 2, 78, c565(255, 90, 90), 3, top_center);
    textf(SW / 2, 106, TFT_WHITE, 1, top_center, "SCORE %lu   STAGE %d", (unsigned long)score, stage + 1);
    textf(SW / 2, 118, c565(184, 220, 255), 1, top_center, "CARS PASSED %d", passes);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 130, c565(255, 216, 74), 1, top_center);
    if (stateT > 150) text("PRESS START", SW / 2, 142, c565(184, 220, 255), 1, top_center);
  }
}

void setup() {
  arcade::begin("turbohorizon", &MUSIC);
  setPalette(TH_PAL565, TH_PAL_N);
  buildStage(0);
  spawnCars();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
