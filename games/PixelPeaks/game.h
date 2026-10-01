// =====================================================================
//  PIXEL PEAKS  -  a platformer (Nova Arcade)
//  A little judoka runs, jumps and rolls her way over three worlds of
//  mountain trails: Blossom Hills, Bamboo Grove and Misty Peaks. Stomp
//  the mochi blobs, roll through spiky chestnuts, find each level's three
//  secret scrolls, and bow at the torii gate. Her belt changes colour
//  with every world she clears.
//  Art: tools/make_art.py -> art.h. Levels: tools/make_levels.py -> levels.h
// =====================================================================
#include <ArcadeCore.h>
#include "art.h"
#include "levels.h"

using namespace gfx;

// ------------------------------------------------------------ music (original, pentatonic)
namespace ppm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0  C */ {72, 1, 74, 76, 79, 1, 76, 74, 72, 1, 69, 1, 72, 1, 1, 1},
  /*1  Am*/ {69, 1, 72, 74, 76, 1, 74, 72, 69, 1, 67, 1, 69, 1, 1, 1},
  /*2  F */ {77, 1, 76, 74, 72, 1, 74, 1, 76, 1, 79, 1, 81, 1, 1, 1},
  /*3  G */ {79, 1, 76, 1, 74, 1, 72, 1, 74, 1, 76, 1, 79, 1, 0, 0},
  /*4  Dm*/ {74, 1, 1, 77, 79, 1, 81, 1, 79, 1, 77, 1, 74, 1, 1, 1},
  /*5  Bb*/ {70, 1, 1, 74, 77, 1, 74, 1, 72, 1, 70, 1, 69, 1, 1, 1},
  /*6  C */ {72, 1, 1, 76, 79, 1, 81, 1, 84, 1, 81, 1, 79, 1, 1, 1},
  /*7  Am*/ {81, 1, 79, 1, 76, 1, 74, 1, 72, 1, 69, 1, 74, 1, 0, 0},
  /*8  Em*/ {76, 1, 79, 1, 81, 1, 83, 1, 81, 1, 79, 1, 76, 1, 74, 1},
  /*9  C */ {72, 1, 76, 1, 79, 1, 81, 1, 79, 1, 76, 1, 72, 1, 74, 1},
  /*10 D */ {74, 1, 78, 1, 81, 1, 83, 1, 86, 1, 83, 1, 81, 1, 78, 1},
  /*11 Em*/ {76, 1, 1, 1, 83, 1, 1, 1, 81, 1, 79, 1, 76, 1, 0, 0},
  /*12 t */ {69, 1, 1, 72, 74, 1, 1, 76, 79, 1, 76, 1, 74, 1, 72, 1},
  /*13 t2*/ {76, 1, 1, 74, 72, 1, 1, 69, 67, 1, 69, 1, 72, 1, 1, 1},
  /*14 cl*/ {72, 76, 79, 84, 1, 1, 79, 1, 84, 1, 1, 1, 1, 1, 0, 0},
  /*15 ov*/ {76, 1, 74, 1, 72, 1, 69, 1, 67, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.h.S.h.K.hhS.h.", "K.h.S.hKK.h.S.hS", "K...h...K...h...", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{AM, 12, 3}, {F_, 13, 3}, {G_, 12, 3}, {AM, 13, 3}};
static const Bar W1_BARS[] = {{C_, 0, 0}, {AM, 1, 0}, {F_, 2, 0}, {G_, 3, 1}, {C_, 0, 0}, {AM, 1, 0}, {F_, 2, 0}, {G_, 3, 1}};
static const Bar W2_BARS[] = {{DM, 4, 2}, {BB, 5, 2}, {C_, 6, 2}, {AM, 7, 2}};
static const Bar W3_BARS[] = {{EM, 8, 0}, {C_, 9, 0}, {D_, 10, 0}, {EM, 11, 1}};
static const Bar CLEAR_BARS[] = {{C_, 14, 4}, {C_, -1, 4}};
static const Bar OVER_BARS[] = {{AM, 15, 4}, {AM, -1, 4}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 100, true, false, 0x30},
  {W1_BARS, 8, 132, true, false, 0},
  {W2_BARS, 4, 112, true, false, 0x30},
  {W3_BARS, 4, 144, true, true, 0},
  {CLEAR_BARS, 2, 140, false, false, 0},
  {OVER_BARS, 2, 96, false, false, 0},
};
}  // namespace ppm
static const audio::Music MUSIC = {audio::STD_CHORDS, ppm::LEADS, ppm::DRUMS, ppm::SONGS, 7};
enum { SONG_NONE, SONG_TITLE, SONG_W1, SONG_W2, SONG_W3, SONG_CLEAR, SONG_OVER };

// ------------------------------------------------------------ map
static const int TS = 16, ROWS = 15, MAXW = 224;
enum Cell : uint8_t { C_AIR, C_GROUND, C_BLOCK, C_PLANK, C_SPIKE, C_WATER, C_BOX, C_BOXHEART, C_BOXUSED, C_COIN, C_SCROLL };
static uint8_t cells[ROWS][MAXW];
static uint8_t tileGfx[ROWS][MAXW];   // which TILE_PX to draw for ground (autotiled once per level)
static int mapW = 0, world = 0, levelIdx = 0, loopN = 0;

static inline uint8_t cellAt(int tx, int ty) {
  if (ty < 0) return C_AIR;
  if (ty >= ROWS) return C_AIR;
  if (tx < 0 || tx >= mapW) return C_BLOCK;   // the level edges are walls
  return cells[ty][tx];
}
static inline bool solidCell(uint8_t c) { return c == C_GROUND || c == C_BLOCK || c == C_BOX || c == C_BOXHEART || c == C_BOXUSED; }

// ------------------------------------------------------------ things in the level
enum EType : uint8_t { E_MOCHI, E_CROW, E_CHESTNUT };
struct Enemy { bool on, awake; uint8_t type, state; float x, y, vx, vy, hx, hy; int t; };   // state 0 alive, 1 squashed, 2 knocked away
static Enemy enemies[40];
struct Mover { bool on; float x, x0, y, v, dx; };
static Mover movers[8];
enum DType : uint8_t { D_TREE, D_TORO, D_BUSH, D_FLOWERS, D_ROCK, D_CHECK, D_GOAL };
struct Deco { uint8_t type; int16_t x, y; };   // x = left, y = bottom (ground line)
static Deco decos[64];
static int ndecos = 0;
struct Part { bool on; float x, y, vx, vy; uint8_t life, kind; uint16_t col; };
static Part parts[160];
struct Popup { bool on; int x, y, t; char txt[12]; };
static Popup popups[6];
struct Weather { float x, y, v, ph; };
static Weather weather[36];

// ------------------------------------------------------------ the hero
struct Hero {
  float x, y, vx, vy;         // hitbox top-left, 10 x 22 (14 when rolling)
  bool onGround, facingLeft, alive;
  int coyote, jumpBuf, rollT, rollCd, invuln, hurtT, anim, deadT;
  Mover* riding;
} hero;
static const int HW = 10, HH = 22, RH = 14;
static int hearts = 3, lives = 3, coins = 0, levelCoins = 0, scrollsGot = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 20000;
static bool newHi = false;
static float checkX = 0, checkY = 0, camX = 0;
enum State { ST_TITLE, ST_INTRO, ST_PLAY, ST_DEAD, ST_CLEAR, ST_WIN, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0, clearBonus = 0;
static float shake = 0;
static int shakeX = 0, shakeY = 0;

// belt colours earned world by world: yellow, orange, green, then blue, purple, brown, black
static const uint8_t BELTS[7][2][3] = {
  {{250, 206, 60}, {196, 146, 30}}, {{250, 140, 50}, {196, 90, 30}}, {{80, 190, 100}, {40, 130, 70}},
  {{70, 130, 230}, {40, 80, 170}}, {{160, 90, 210}, {110, 50, 150}}, {{150, 96, 60}, {100, 60, 40}}, {{40, 40, 50}, {10, 10, 20}}};

// per-world colours, converted to screen format when a level loads
static uint16_t skyRow[SH], farCol[7], midCol[7], sunCol;

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void popup(int x, int y, const char* s) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 50, {0}}; strncpy(p.txt, s, sizeof(p.txt) - 1); return; }
}
static void popupNum(int x, int y, uint32_t v) { char b[12]; snprintf(b, sizeof(b), "%lu", (unsigned long)v); popup(x, y, b); }
static void burst(float x, float y, int n, uint16_t col, float sp, uint8_t kind = 0) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s - 0.6f, (uint8_t)frange(14, 30), kind, col}; break; }
}
static void dust(float x, float y, int n) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { p = {true, x + frange(-4, 4), y, frange(-0.8f, 0.8f), frange(-0.6f, -0.1f), (uint8_t)frange(10, 18), 1, rgbS(230, 220, 200)}; break; }
}

// ------------------------------------------------------------ loading a level
static void setBelt() {
  const uint8_t (*b)[3] = BELTS[min(levelIdx + loopN * NLEVELS, 6)];
  pal[PC_BELT] = rgbS(b[0][0], b[0][1], b[0][2]);
  pal[PC_BELT2] = rgbS(b[1][0], b[1][1], b[1][2]);
}

static void setWorldColours(int w) {
  const WorldArt& A = WORLD_ART[w];
  for (int y = 0; y < SH; y++) {
    float t = min(1.0f, y / 200.0f);
    skyRow[y] = lerpS(A.sky[0][0], A.sky[0][1], A.sky[0][2], A.sky[1][0], A.sky[1][1], A.sky[1][2], t);
  }
  for (int i = 0; i < 7; i++) {
    farCol[i] = rgbS(A.far[i][0], A.far[i][1], A.far[i][2]);
    midCol[i] = rgbS(A.mid[i][0], A.mid[i][1], A.mid[i][2]);
  }
  sunCol = c565(A.sun[0], A.sun[1], A.sun[2]);
  for (auto& w : weather) { w.x = frand() * SW; w.y = frand() * SH; w.v = frange(0.4f, 1.0f); w.ph = frand() * 6.28f; }
}

static void autotile() {
  int base = world * T_PER_MAT;
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < mapW; x++) {
      if (cells[y][x] != C_GROUND) continue;
      auto open = [&](int xx, int yy) { uint8_t c = cellAt(xx, yy); return !(c == C_GROUND || (xx < 0 || xx >= mapW)) && !(yy >= ROWS); };
      int m = (open(x, y - 1) ? 1 : 0) | (open(x - 1, y) ? 2 : 0) | (open(x + 1, y) ? 4 : 0) | (open(x, y + 1) ? 8 : 0);
      int t = base + m;
      if (m == 0) { int h = (x * 7 + y * 13) % 5; if (h == 1) t = base + 16; else if (h == 3) t = base + 17; }
      tileGfx[y][x] = t;
    }
}

static void resetHero(float x, float y) {
  hero = {};
  hero.x = x; hero.y = y; hero.alive = true; hero.invuln = 60;
  camX = constrain(x - 140, 0.0f, (float)(mapW * TS - SW));
}

static void loadLevel(int idx) {
  const LevelDef& L = LEVELS[idx];
  levelIdx = idx; world = L.world; mapW = min((int)L.w, MAXW);
  memset(cells, 0, sizeof(cells)); memset(enemies, 0, sizeof(enemies)); memset(movers, 0, sizeof(movers));
  memset(parts, 0, sizeof(parts)); memset(popups, 0, sizeof(popups));
  ndecos = 0; scrollsGot = 0; levelCoins = 0;
  float sx = 32, sy = 150;
  auto deco = [&](uint8_t type, int x, int y) { if (ndecos < 64) decos[ndecos++] = {type, (int16_t)x, (int16_t)y}; };
  auto enemy = [&](uint8_t type, float x, float y) {
    for (auto& e : enemies) if (!e.on) { e = {}; e.on = true; e.type = type; e.x = e.hx = x; e.y = e.hy = y; e.vx = -1; e.t = rnd() % 200; return; }
  };
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < mapW; x++) {
      char ch = L.rows[y][x];
      uint8_t c = C_AIR;
      int px = x * TS, py = y * TS;
      switch (ch) {
        case '#': c = C_GROUND; break;
        case 'X': c = C_BLOCK; break;
        case '=': c = C_PLANK; break;
        case '^': c = C_SPIKE; break;
        case '~': c = C_WATER; break;
        case '?': c = C_BOX; break;
        case '!': c = C_BOXHEART; break;
        case 'o': c = C_COIN; break;
        case 'S': c = C_SCROLL; break;
        case 'P': sx = px + 3; sy = py + TS - HH; break;
        case 'b': enemy(E_MOCHI, px, py + TS - 13); break;
        case 'k': enemy(E_CHESTNUT, px, py); break;
        case 'c': enemy(E_CROW, px, py); break;
        case 'M': for (auto& m : movers) if (!m.on) { m = {true, (float)px, (float)px, (float)py, 0.55f, 0}; break; } break;
        case 'C': deco(D_CHECK, px + 2, py + TS); break;
        case 'G': deco(D_GOAL, px + 8 - SPR_TORII.w / 2, py + TS); break;
        case 't': deco(D_TREE, px + 8, py + TS); break;
        case 'l': deco(D_TORO, px, py + TS); break;
        case 'u': deco(D_BUSH, px - 2, py + TS); break;
        case 'v': deco(D_FLOWERS, px + 4, py + TS); break;
        case 'r': deco(D_ROCK, px + 1, py + TS); break;
      }
      cells[y][x] = c;
    }
  autotile();
  setWorldColours(world);
  setBelt();
  checkX = sx; checkY = sy;
  resetHero(sx, sy);
}

static void startLevel(int idx) {
  loadLevel(idx);
  hearts = 3;
  state = ST_INTRO; stateT = 0;
  audio::music(SONG_NONE);
}

static void resetGame() {
  score = 0; lives = 3; coins = 0; loopN = 0; newHi = false;
  startLevel(0);
}

// ------------------------------------------------------------ collision helpers
static bool boxSolid(float x, float y, float w, float h) {
  int tx0 = (int)floorf(x / TS), tx1 = (int)floorf((x + w - 0.01f) / TS);
  int ty0 = (int)floorf(y / TS), ty1 = (int)floorf((y + h - 0.01f) / TS);
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++)
      if (solidCell(cellAt(tx, ty))) return true;
  return false;
}
static int heroH() { return hero.rollT ? RH : HH; }

static void hurtHero(bool fatal);

static void hitBox(int tx, int ty) {
  if (tx < 0 || tx >= mapW || ty < 0 || ty >= ROWS) { audio::play(SFX_LAND); return; }
  uint8_t c = cells[ty][tx];
  if (c != C_BOX && c != C_BOXHEART) { audio::play(SFX_LAND); return; }
  cells[ty][tx] = C_BOXUSED;
  int px = tx * TS + 8, py = ty * TS;
  if (c == C_BOXHEART) {
    if (hearts < 3) { hearts++; popup(px - 12, py - 10, "HEART"); } else { addScore(1000); popupNum(px - 12, py - 10, 1000); }
    audio::play(SFX_POWERUP);
    burst(px, py - 4, 14, rgbS(214, 52, 52), 1.6f);
  } else {
    coins++; levelCoins++; addScore(100);
    audio::play(SFX_COIN);
    burst(px, py - 6, 10, rgbS(255, 214, 74), 1.5f);
    popupNum(px - 6, py - 12, 100);
  }
  shake = max(shake, 2.0f);
}

// ------------------------------------------------------------ hero update
static void killHero() {
  if (!hero.alive) return;
  hero.alive = false; hero.deadT = 0;
  hero.vy = -5.5f; hero.vx = 0; hero.rollT = 0;
  audio::play(SFX_PLAYER_DIE);
  input::rumble(400, 0xC0, 0xC0);
  shake = 6;
  state = ST_DEAD; stateT = 0;
}

static void hurtHero(bool fatal) {
  if (!hero.alive) return;
  if (fatal) { hearts = 0; killHero(); return; }
  if (hero.invuln || hero.rollT) return;
  hearts--;
  if (hearts <= 0) { killHero(); return; }
  hero.invuln = 100; hero.hurtT = 24;
  hero.vx = hero.facingLeft ? 2.2f : -2.2f; hero.vy = -3.5f;
  audio::play(SFX_HIT);
  input::rumble(180, 0x90, 0x60);
  shake = 3;
}

static void collectAt(int tx, int ty) {
  uint8_t c = cellAt(tx, ty);
  if (c == C_COIN) {
    cells[ty][tx] = C_AIR;
    coins++; levelCoins++; addScore(50);
    audio::play(SFX_COIN);
    burst(tx * TS + 8, ty * TS + 8, 6, rgbS(255, 244, 170), 1.0f);
    if (coins % 100 == 0) { lives++; popup(tx * TS - 4, ty * TS - 8, "1UP"); audio::play(SFX_POWERUP); }
  } else if (c == C_SCROLL) {
    cells[ty][tx] = C_AIR;
    scrollsGot++; addScore(2000);
    popup(tx * TS - 8, ty * TS - 8, "SCROLL!");
    audio::play(SFX_POWERUP);
    burst(tx * TS + 8, ty * TS + 8, 24, rgbS(250, 240, 214), 2.0f);
  }
}

static void updateHero(const Pad& in) {
  Hero& h = hero;
  if (h.invuln) h.invuln--;
  if (h.hurtT) h.hurtT--;
  if (h.rollCd) h.rollCd--;
  // --- input
  float ax = in.ax;
  if (in.down(BTN_LEFT)) ax = -1;
  if (in.down(BTN_RIGHT)) ax = 1;
  if (h.hurtT) ax = 0;
  const float MAXV = 2.3f;
  if (h.rollT) {
    h.rollT--;
    h.vx = (h.facingLeft ? -1 : 1) * 3.4f;
    if (!h.rollT) {   // stand up only if there's headroom, otherwise keep rolling
      if (boxSolid(h.x, h.y - (HH - RH), HW, HH)) h.rollT = 4;
      else { h.y -= HH - RH; h.rollCd = 14; }
    }
  } else {
    float acc = h.onGround ? 0.28f : 0.18f;
    if (ax > 0.2f) { h.vx = min(h.vx + acc * ax, MAXV * ax); h.facingLeft = false; }
    else if (ax < -0.2f) { h.vx = max(h.vx + acc * ax, MAXV * ax); h.facingLeft = true; }
    else { float f = h.onGround ? 0.32f : 0.06f; if (h.vx > f) h.vx -= f; else if (h.vx < -f) h.vx += f; else h.vx = 0; }
  }
  if (in.hit(BTN_A)) h.jumpBuf = 7; else if (h.jumpBuf) h.jumpBuf--;
  if (in.hit(BTN_B | BTN_X | BTN_R1) && h.onGround && !h.rollT && !h.rollCd && !h.hurtT) {   // judo roll
    h.rollT = 26; h.y += HH - RH;
    audio::play(SFX_ROTATE);
    dust(h.x + HW / 2, h.y + RH, 5);
  }
  if (h.jumpBuf && (h.onGround || h.coyote) ) {
    if (h.rollT && boxSolid(h.x, h.y - (HH - RH), HW, HH)) { /* no room to jump out of a roll here */ }
    else {
      if (h.rollT) { h.y -= HH - RH; h.rollT = 0; h.rollCd = 14; }
      h.vy = -7.0f; h.onGround = false;   // rises ~4.3 tiles: 3-tile climbs with room to spare h.coyote = 0; h.jumpBuf = 0; h.riding = nullptr;
      audio::play(SFX_JUMP);
      dust(h.x + HW / 2, h.y + HH, 4);
    }
  }
  if (!in.down(BTN_A) && h.vy < -2.2f) h.vy = -2.2f;   // let go early for a short hop
  // --- gravity
  h.vy = min(h.vy + (h.vy < 0 ? 0.34f : 0.4f), 6.5f);
  // --- ride moving planks
  if (h.riding) h.x += h.riding->dx;
  // --- move X
  int hh = heroH();
  h.x += h.vx;
  if (boxSolid(h.x, h.y, HW, hh)) {
    if (h.vx > 0) h.x = floorf((h.x + HW) / TS) * TS - HW; else if (h.vx < 0) h.x = floorf(h.x / TS) * TS + TS;
    else h.x = roundf(h.x);
    if (boxSolid(h.x, h.y, HW, hh)) h.x -= h.vx;
    h.vx = 0;
  }
  // --- move Y
  float oldBottom = h.y + hh;
  h.y += h.vy;
  bool wasGround = h.onGround;
  h.onGround = false; h.riding = nullptr;
  if (h.vy >= 0) {
    float bottom = h.y + hh;
    int ty = (int)floorf(bottom / TS);
    bool land = false;
    for (int tx = (int)floorf(h.x / TS); tx <= (int)floorf((h.x + HW - 0.01f) / TS); tx++) {
      uint8_t c = cellAt(tx, ty);
      if (solidCell(c) || (c == C_PLANK && oldBottom <= ty * TS + 0.5f)) land = true;
    }
    if (land) { h.y = ty * TS - hh; h.vy = 0; h.onGround = true; }
    for (auto& m : movers) {
      if (!m.on) continue;
      if (h.x + HW > m.x && h.x < m.x + 48 && oldBottom <= m.y + 0.5f && bottom >= m.y) {
        h.y = m.y - hh; h.vy = 0; h.onGround = true; h.riding = &m;
      }
    }
    if (h.onGround && !wasGround && oldBottom < h.y + hh + 0.1f && h.vy == 0) {
      audio::play(SFX_LAND);
      dust(h.x + HW / 2, h.y + hh, 3);
    }
  } else {
    int ty = (int)floorf(h.y / TS);
    int cx = (int)floorf((h.x + HW / 2) / TS);
    for (int tx = (int)floorf(h.x / TS); tx <= (int)floorf((h.x + HW - 0.01f) / TS); tx++) {
      if (solidCell(cellAt(tx, ty))) {
        h.y = (ty + 1) * TS; h.vy = 0.5f;
        int bx = (cellAt(cx, ty) == C_BOX || cellAt(cx, ty) == C_BOXHEART) ? cx : tx;
        hitBox(bx, ty);
        break;
      }
    }
  }
  if (h.onGround) h.coyote = 6; else if (h.coyote) h.coyote--;
  // --- pickups, hazards
  for (int ty = (int)floorf(h.y / TS); ty <= (int)floorf((h.y + hh - 1) / TS); ty++)
    for (int tx = (int)floorf(h.x / TS); tx <= (int)floorf((h.x + HW - 1) / TS); tx++) {
      uint8_t c = cellAt(tx, ty);
      if (c == C_COIN || c == C_SCROLL) collectAt(tx, ty);
      if (c == C_SPIKE && h.y + hh > ty * TS + 8) hurtHero(false);
      if (c == C_WATER && h.y + hh > ty * TS + 6) {
        burst(h.x + HW / 2, ty * TS + 4, 16, rgbS(190, 228, 255), 1.8f);
        audio::play(SFX_LAND);
        hurtHero(true);
      }
    }
  if (h.y > SH + 8) hurtHero(true);
  // --- checkpoints and the goal
  for (int i = 0; i < ndecos; i++) {
    Deco& d = decos[i];
    if (d.type == D_CHECK && h.x + HW > d.x && h.x < d.x + 20 && checkX < d.x) {
      checkX = d.x; checkY = d.y - HH - 1;
      audio::play(SFX_SELECT);
      popup(d.x - 8, d.y - 44, "CHECKPOINT");
      burst(d.x + 10, d.y - 26, 18, rgbS(255, 206, 100), 1.4f);
    }
    if (d.type == D_GOAL && h.x + HW / 2 > d.x + SPR_TORII.w / 2 - 4 && state == ST_PLAY) {
      state = ST_CLEAR; stateT = 0;
      hero.vx = 0; hero.rollT = 0;
      clearBonus = levelCoins * 10 + scrollsGot * 3000 + hearts * 500;
      addScore(clearBonus);
      audio::music(SONG_CLEAR);
    }
  }
  if (h.rollT) h.anim++;
  else if (h.onGround && fabsf(h.vx) > 0.3f) h.anim++;
  else if (h.onGround) h.anim = 0;
}

// ------------------------------------------------------------ enemies and moving planks
static void updateEnemies() {
  float sp = arcade::speed() * (1.0f + 0.12f * loopN);
  for (auto& e : enemies) {
    if (!e.on) continue;
    if (!e.awake) { if (e.x < camX + SW + 32) e.awake = true; else continue; }
    e.t++;
    if (e.state == 2) {                              // knocked away: tumble off screen
      e.x += e.vx; e.y += e.vy; e.vy += 0.35f;
      if (e.y > SH + 20) e.on = false;
      continue;
    }
    if (e.state == 1) { if (e.t > 40) e.on = false; continue; }
    int w = e.type == E_CROW ? 18 : 16, hgt = e.type == E_MOCHI ? 13 : e.type == E_CROW ? 11 : 16;
    if (e.type == E_CROW) {
      // patrols in a lazy figure of eight, and dips towards the hero when she's close below
      float tt = e.t * 0.02f * sp;
      e.x = e.hx + 56 * sinf(tt);
      float dip = (fabsf(hero.x - e.x) < 70 && hero.y > e.y) ? 26 * (1 - fabsf(hero.x - e.x) / 70) : 0;
      e.y = e.hy + 10 * sinf(tt * 2) + dip;
      e.vx = cosf(tt);
    } else {
      float v = (e.type == E_MOCHI ? 0.55f : 0.42f) * sp;
      e.vx = e.vx < 0 ? -v : v;
      float nx = e.x + e.vx;
      int ahead = (int)floorf((e.vx > 0 ? nx + w : nx) / TS);
      int footRow = (int)floorf((e.y + hgt + 1) / TS);
      bool wall = solidCell(cellAt(ahead, (int)floorf((e.y + hgt - 2) / TS)));
      bool ledge = !solidCell(cellAt(ahead, footRow)) && cellAt(ahead, footRow) != C_PLANK;
      if (wall || ledge) e.vx = -e.vx; else e.x = nx;
      // stay on the ground
      e.vy = min(e.vy + 0.4f, 6.0f);
      e.y += e.vy;
      int by = (int)floorf((e.y + hgt) / TS);
      if (solidCell(cellAt((int)floorf((e.x + w / 2) / TS), by)) || cellAt((int)floorf((e.x + w / 2) / TS), by) == C_PLANK) { e.y = by * TS - hgt; e.vy = 0; }
      if (e.y > SH + 20) e.on = false;
    }
    // --- touching the hero
    if (!hero.alive) continue;
    int hh = heroH();
    if (!overlap(hero.x, hero.y, HW, hh, e.x + 1, e.y + 1, w - 2, hgt - 2)) continue;
    float cx = e.x + w / 2, cy = e.y + hgt / 2;
    if (hero.rollT) {                                 // a judo roll bowls anything over
      e.state = 2; e.vy = -4; e.vx = hero.facingLeft ? -2 : 2; e.t = 0;
      addScore(200); popupNum((int)cx - 8, (int)e.y - 10, 200);
      audio::play(SFX_STOMP); burst(cx, cy, 10, rgbS(255, 255, 255), 1.6f);
      shake = max(shake, 2.0f);
    } else if (hero.vy > 0.5f && hero.y + hh - e.y < 10 && e.type != E_CHESTNUT) {   // stomp
      if (e.type == E_MOCHI) { e.state = 1; e.t = 0; } else { e.state = 2; e.vy = -2; e.vx = 0; e.t = 0; }
      hero.vy = input::pad.down(BTN_A) ? -7.2f : -4.8f;
      addScore(100); popupNum((int)cx - 8, (int)e.y - 10, 100);
      audio::play(SFX_STOMP); burst(cx, e.y, 8, rgbS(246, 242, 236), 1.4f);
    } else {
      hurtHero(false);
    }
  }
  for (auto& m : movers) {
    if (!m.on) continue;
    float px = m.x;
    m.x += m.v * sp;
    if (m.x > m.x0 + 4 * TS || m.x < m.x0) m.v = -m.v;
    m.dx = m.x - px;
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += p.kind == 1 ? 0.0f : 0.12f;
    if (p.kind == 1) { p.vx *= 0.92f; p.vy *= 0.92f; }
    if (--p.life == 0) p.on = false;
  }
  for (auto& p : popups) if (p.on) { if ((p.t & 3) == 0) p.y--; if (--p.t <= 0) p.on = false; }
  for (auto& w : weather) {
    w.ph += 0.03f;
    if (world == 0) { w.x += w.v * 0.6f + sinf(w.ph) * 0.3f; w.y += w.v * 0.5f; }         // petals drift down-right
    else if (world == 1) { w.x += sinf(w.ph) * 0.3f; w.y += cosf(w.ph * 0.7f) * 0.2f; }   // fireflies wander
    else { w.x += sinf(w.ph) * 0.4f - 0.2f; w.y += w.v * 0.7f; }                           // snow falls
    if (w.y > SH) w.y -= SH; if (w.y < 0) w.y += SH;
    if (w.x > SW) w.x -= SW; if (w.x < 0) w.x += SW;
  }
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

static void updateCamera() {
  float target = hero.x + HW / 2 - 150 + (hero.facingLeft ? -26 : 26);
  camX += (target - camX) * 0.1f;
  camX = constrain(camX, 0.0f, (float)(mapW * TS - SW));
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      camX += 0.6f;
      if (camX > mapW * TS - SW) camX = 0;
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_INTRO:
      if (stateT > 130 || (stateT > 30 && in.hit(BTN_A | BTN_START))) {
        state = ST_PLAY; stateT = 0;
        audio::music(SONG_W1 + world);
      }
      break;
    case ST_PLAY:
      if (in.hit(BTN_START)) { arcade::pause(); break; }
      updateHero(in);
      if (state == ST_PLAY || state == ST_CLEAR) updateEnemies();
      updateCamera();
      break;
    case ST_DEAD:
      hero.y += hero.vy; hero.vy += 0.3f; hero.deadT++;
      updateEnemies();
      if (stateT > 110) {
        lives--;
        if (lives <= 0) { state = ST_OVER; stateT = 0; audio::music(SONG_OVER); if (newHi) arcade::saveHi(hiscore); }
        else { hearts = 3; resetHero(checkX, checkY); state = ST_PLAY; stateT = 0; }
      }
      break;
    case ST_CLEAR:
      if (!hero.onGround) { Pad none; updateHero(none); }
      hero.vx = 0;
      updateCamera();
      if (stateT > 220) {
        if (levelIdx + 1 < NLEVELS) startLevel(levelIdx + 1);
        else { state = ST_WIN; stateT = 0; audio::music(SONG_CLEAR); }
      }
      break;
    case ST_WIN:
      if (stateT > 240 && in.hit(BTN_A | BTN_START)) { loopN++; startLevel(0); }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; loadLevel(0); audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static const int FAR_Y = 64, MID_Y = 122;

static void drawBackground() {
  int farOff = (int)(camX * 0.18f), midOff = (int)(camX * 0.45f);
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t* d = B + r * SW;
    uint16_t sky = skyRow[y];
    if (y >= MID_Y + MID_H) {                         // below the hills: fill with the hill colour
      uint16_t c = midCol[3];
      for (int x = 0; x < SW; x++) d[x] = c;
      continue;
    }
    for (int x = 0; x < SW; x++) d[x] = sky;
    if (y >= FAR_Y && y < FAR_Y + FAR_H) {
      const uint8_t* row = LAYER_FAR + (y - FAR_Y) * LAYER_W;
      for (int x = 0; x < SW; x++) { uint8_t v = row[(x + farOff) & (LAYER_W - 1)]; if (v) d[x] = farCol[v - 1]; }
    }
    if (y >= MID_Y) {
      const uint8_t* row = LAYER_MID[world] + (y - MID_Y) * LAYER_W;
      for (int x = 0; x < SW; x++) { uint8_t v = row[(x + midOff) & (LAYER_W - 1)]; if (v) d[x] = midCol[v - 1]; }
    }
  }
}

static void drawSky() {
  if (rowsVisible(20, 50)) {
    disc(262, 46, 17, sunCol);
    int cx = (int)(-camX * 0.06f + frameNo * 0.05f);
    blit(SPR_CLOUD0, ((cx + 30) % 420 + 420) % 420 - 60, 30);
    blit(SPR_CLOUD1, ((cx + 230) % 420 + 420) % 420 - 60, 50);
  }
}

static inline void drawTile(const uint8_t* px, int x, int y) {
  int y0 = max(Y0, y), y1 = min(Y0 + STRIP, y + TS);
  int x0 = max(0, x), x1 = min(SW, x + TS);
  for (int yy = y0; yy < y1; yy++) {
    const uint8_t* s = px + (yy - y) * TS;
    uint16_t* d = B + (yy - Y0) * SW;
    for (int xx = x0; xx < x1; xx++) { uint8_t v = s[xx - x]; if (v) d[xx] = pal[v]; }
  }
}

static void drawTiles() {
  int cx = (int)camX - shakeX;
  int tx0 = cx / TS, tx1 = min(mapW - 1, (cx + SW) / TS + 1);
  int ty0 = max(0, (Y0 - shakeY) / TS - 1), ty1 = min(ROWS - 1, (Y0 + STRIP - shakeY) / TS + 1);
  int wave = (frameNo >> 4) & 1;
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++) {
      uint8_t c = cells[ty][tx];
      int x = tx * TS - cx, y = ty * TS + shakeY;
      switch (c) {
        case C_GROUND: drawTile(TILE_PX[tileGfx[ty][tx]], x, y); break;
        case C_BLOCK: drawTile(TILE_PX[T_BLOCK], x, y); break;
        case C_PLANK: drawTile(TILE_PX[T_PLANK], x, y); break;
        case C_SPIKE: drawTile(TILE_PX[T_SPIKE], x, y); break;
        case C_WATER: drawTile(TILE_PX[cellAt(tx, ty - 1) == C_WATER ? T_WATERB : T_WATER0 + wave], x, y); break;
        case C_BOX: case C_BOXHEART: drawTile(TILE_PX[((frameNo >> 3) % 12 == 0) ? T_BOX1 : T_BOX0], x, y); break;
        case C_BOXUSED: drawTile(TILE_PX[T_BOXUSED], x, y); break;
        case C_COIN: blit(*COIN[(frameNo / 6 + tx) & 3], x + 2, y + 2); break;
        case C_SCROLL: {
          int bob = (int)(2 * sinf(frameNo * 0.08f));
          if (rowsVisible(y - 4, 20)) {
            if ((frameNo >> 3) & 1) { pset(x + 1, y + 1 + bob, rgbS(255, 255, 255)); pset(x + 14, y + 12 + bob, rgbS(255, 255, 255)); }
            blit(SPR_SCROLL, x + 2, y + 3 + bob);
          }
          break;
        }
        default: break;
      }
    }
}

static void drawDecos(bool front) {
  int cx = (int)camX - shakeX;
  for (int i = 0; i < ndecos; i++) {
    const Deco& d = decos[i];
    const Sprite* s = nullptr;
    int ox = 0;
    switch (d.type) {
      case D_TREE: s = world == 0 ? &SPR_SAKURA : world == 1 ? &SPR_BAMBOO : &SPR_PINE; ox = -s->w / 2; break;
      case D_TORO: s = &SPR_TORO; break;
      case D_BUSH: s = &SPR_BUSH; break;
      case D_FLOWERS: s = &SPR_FLOWERS; break;
      case D_ROCK: s = &SPR_ROCK; break;
      case D_CHECK: s = checkX >= d.x ? &SPR_LANTERN_ON : &SPR_LANTERN_OFF; break;
      case D_GOAL: s = &SPR_TORII; break;
    }
    if (!s || front != (d.type == D_FLOWERS)) continue;
    int x = d.x + ox - cx, y = d.y - s->h + shakeY;
    if (x > SW || x + s->w < 0 || !rowsVisible(y, s->h)) continue;
    if (d.type == D_CHECK && checkX >= d.x && rowsVisible(y, 30)) {   // soft glow round a lit lantern
      disc(x + 10, y + 11, 11 + ((frameNo >> 3) & 1), c565(120, 90, 40));
    }
    blit(*s, x, y);
  }
}

static void drawMovers() {
  int cx = (int)camX - shakeX;
  for (auto& m : movers) {
    if (!m.on) continue;
    int x = (int)m.x - cx, y = (int)m.y + shakeY;
    for (int k = 0; k < 3; k++) drawTile(TILE_PX[T_PLANK], x + k * TS, y);
  }
}

static void drawEnemies() {
  int cx = (int)camX - shakeX;
  for (auto& e : enemies) {
    if (!e.on || !e.awake) continue;
    int x = (int)e.x - cx, y = (int)e.y + shakeY;
    if (x < -24 || x > SW + 8) continue;
    const Sprite* s;
    bool flip = e.vx > 0;
    if (e.type == E_MOCHI) s = e.state == 1 ? &SPR_MOCHI_FLAT : MOCHI[(e.t >> 4) & 1];
    else if (e.type == E_CROW) { s = CROW[(e.t >> 3) & 1]; flip = e.vx > 0; y -= 2; }
    else s = CHESTNUT[(e.t >> 3) & 1];
    if (!rowsVisible(y - 2, s->h + 4)) continue;
    if (e.state == 2) {   // upside down while tumbling away
      for (int r = 0; r < s->h; r++)
        for (int c = 0; c < s->w; c++) { uint8_t v = s->px[(s->h - 1 - r) * s->w + c]; if (v) pset(x + c, y + r, pal[v]); }
    } else if (flip) blitFlip(*s, x, y);
    else blit(*s, x, y);
  }
}

static void drawHero() {
  if (!hero.alive && state != ST_DEAD) return;
  if (hero.invuln && ((hero.invuln >> 2) & 1) && hero.alive) return;
  int hh = heroH();
  int bx = (int)hero.x - (int)camX + shakeX, by = (int)(hero.y + hh) + shakeY;
  const Sprite* s;
  if (!hero.alive) s = HERO[H_HURT];
  else if (state == ST_CLEAR && stateT > 20) s = HERO[H_BOW];
  else if (hero.rollT) {
    s = ROLL[(hero.anim >> 2) & 3];
    if (hero.facingLeft) s = ROLL[3 - ((hero.anim >> 2) & 3)];
    int x = bx + HW / 2 - s->w / 2, y = by - s->h;
    blit(*s, x, y);
    return;
  } else if (hero.hurtT) s = HERO[H_HURT];
  else if (!hero.onGround) s = HERO[hero.vy < 0 ? H_JUMP : H_FALL];
  else if (fabsf(hero.vx) > 0.3f) s = HERO[H_RUN0 + (hero.anim / 5) % 6];
  else s = HERO[(frameNo >> 5) & 1 ? H_IDLE1 : H_IDLE0];
  int x = bx + HW / 2 - 12, y = by - 26;
  if (!rowsVisible(y, s->h)) return;
  if (hero.facingLeft) blitFlip(*s, x, y); else blit(*s, x, y);
}

static void drawWeather() {
  for (auto& w : weather) {
    int x = (int)w.x, y = (int)w.y;
    if (y < Y0 - 2 || y > Y0 + STRIP + 2) continue;
    if (world == 0) { pset(x, y, rgbS(255, 186, 208)); pset(x + 1, y, rgbS(250, 138, 176)); }
    else if (world == 1) { if (sinf(w.ph * 3) > 0.2f) { pset(x, y, rgbS(255, 240, 140)); pset(x + 1, y, rgbS(255, 200, 80)); } }
    else { pset(x, y, rgbS(255, 255, 255)); if (w.v > 0.75f) pset(x + 1, y, rgbS(220, 230, 250)); }
  }
}

static void drawHud() {
  if (!rowsVisible(0, 16)) return;
  shade(0, 0, SW, 15);
  for (int i = 0; i < 3; i++) blit(i < hearts ? SPR_HEART : SPR_HEART_EMPTY, 4 + i * 11, 4);
  blit(*COIN[0], 42, 2);
  textf(56, 4, TFT_WHITE, 1, top_left, "x%02d", coins % 100);
  for (int i = 0; i < 3; i++) {
    if (i < scrollsGot) blit(SPR_SCROLL, 88 + i * 15, 3);
    else rect(90 + i * 15, 4, 10, 7, rgbS(90, 90, 120));
  }
  textf(140, 4, c565(255, 216, 74), 1, top_left, "%07lu", (unsigned long)score);
  // lives: a tiny face
  rectf(200, 4, 8, 8, rgbS(58, 34, 52)); rectf(202, 6, 6, 5, rgbS(255, 214, 180)); pset(206, 8, rgbS(40, 26, 60));
  textf(211, 4, TFT_WHITE, 1, top_left, "x%d", lives);
  textf(SW - 4, 4, c565(184, 243, 255), 1, top_right, "%d-%d", loopN + 1, levelIdx + 1);
}

// Twice-size blit for the title screen hero
static void blit2x(const Sprite& s, int x, int y) {
  if (!rowsVisible(y, s.h * 2)) return;
  for (int r = 0; r < s.h; r++)
    for (int c = 0; c < s.w; c++) {
      uint8_t v = s.px[r * s.w + c];
      if (v) rectf(x + c * 2, y + r * 2, 2, 2, pal[v]);
    }
}

static void drawWorld() {
  drawBackground();
  drawSky();
  drawDecos(false);
  drawTiles();
  drawMovers();
  drawDecos(true);
  drawEnemies();
  drawHero();
  for (auto& p : parts) if (p.on) {
    int x = (int)p.x - (int)camX, y = (int)p.y;
    pset(x, y, p.col);
    if (p.life > 16) pset(x + 1, y, p.col);
  }
  for (auto& p : popups) if (p.on) text(p.txt, p.x - (int)camX, p.y, (p.t & 8) ? c565(255, 216, 74) : TFT_WHITE, 1, top_left);
  drawWeather();
}

static void draw() {
  drawWorld();
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_TITLE) {
    if (rowsVisible(20, 200)) shade(24, 20, 272, 196);
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 28);
    text("A LITTLE JUDOKA'S MOUNTAIN QUEST", SW / 2, 68, c565(184, 243, 255), 1, top_center);
    int f = (frameNo >> 6) % 4;
    blit2x(*HERO[f == 3 ? H_BOW : f == 2 ? H_IDLE1 : H_IDLE0], 40, 90);
    blit(*MOCHI[(frameNo >> 4) & 1], 250, 130);
    blit(*CHESTNUT[(frameNo >> 3) & 1], 272, 126);
    textf(SW / 2, 84, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2 + 16, 104, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2 + 16, 108, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(132);
    text("A JUMP (HOLD FOR HIGHER)", SW / 2 + 16, 154, c565(180, 194, 220), 1, top_center);
    text("B JUDO ROLL: BOWLS FOES OVER", SW / 2 + 16, 166, c565(180, 194, 220), 1, top_center);
    text("STOMP BLOBS, ROLL THROUGH SPIKES", SW / 2 + 16, 178, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 200, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawHud();
  if (state == ST_INTRO) {
    if (rowsVisible(78, 70)) { shade(0, 78, SW, 70); shade(0, 78, SW, 70); }
    textf(SW / 2, 86, c565(184, 243, 255), 1, top_center, "WORLD %d-%d", loopN + 1, levelIdx + 1);
    text(LEVELS[levelIdx].name, SW / 2, 100, TFT_WHITE, 2, top_center);
    // show the belt she's wearing into this world
    if (rowsVisible(124, 10)) { rectf(120, 124, 80, 6, pal[PC_BELT]); rectf(120, 128, 80, 2, pal[PC_BELT2]); rectf(156, 122, 8, 12, pal[PC_BELT2]); }
    textf(SW / 2, 138, c565(255, 216, 74), 1, top_center, "LIVES %d", lives);
  } else if (state == ST_CLEAR) {
    if (stateT > 30 && rowsVisible(70, 90)) { shade(40, 70, 240, 90); shade(40, 70, 240, 90); }
    if (stateT > 30) {
      text("REI!  STAGE CLEAR", SW / 2, 78, c565(182, 255, 110), 2, top_center);
      textf(SW / 2, 104, TFT_WHITE, 1, top_center, "COINS   %3d x 10", levelCoins);
      textf(SW / 2, 116, TFT_WHITE, 1, top_center, "SCROLLS %d/3 x 3000", scrollsGot);
      textf(SW / 2, 128, TFT_WHITE, 1, top_center, "HEARTS  %d x 500", hearts);
      textf(SW / 2, 144, c565(255, 216, 74), 1, top_center, "BONUS %d", clearBonus);
    }
  } else if (state == ST_WIN) {
    if (rowsVisible(60, 110)) { shade(30, 60, 260, 110); shade(30, 60, 260, 110); }
    text("ALL PEAKS CLEARED!", SW / 2, 70, c565(182, 255, 110), 2, top_center);
    text("A NEW BELT AND A HARDER CLIMB", SW / 2, 96, TFT_WHITE, 1, top_center);
    text("AWAIT ON THE NEXT LAP", SW / 2, 108, TFT_WHITE, 1, top_center);
    blit(*HERO[H_BOW], SW / 2 - 12, 122);
    if (stateT > 240 && blink) text("PRESS A", SW / 2, 156, c565(255, 216, 74), 1, top_center);
  } else if (state == ST_OVER) {
    if (rowsVisible(84, 70)) { shade(60, 84, 200, 70); shade(60, 84, 200, 70); }
    text("GAME OVER", SW / 2, 92, c565(235, 60, 80), 3, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 122, c565(255, 216, 74), 1, top_center);
    if (stateT > 90) text("PRESS START", SW / 2, 138, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("pixelpeaks", &MUSIC);
  setPalette(PP_PAL565, PP_PAL_N);
  loadLevel(0);
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
