// =====================================================================
//  MAZE MUNCH  -  a maze chase (Nova Arcade)
//  Gobble every glow-dot while four wisps hunt you through the maze.
//  Power crystals turn the tables for a few seconds. Each wisp has its
//  own way of hunting: Blaze chases, Pinkie cuts you off, Frost flanks
//  and Ember wanders off when it gets close.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace mmm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 C */ {72, 1, 79, 1, 76, 1, 79, 1, 84, 1, 79, 1, 76, 1, 74, 1},
  /*1 Am*/ {69, 1, 76, 1, 72, 1, 76, 1, 81, 1, 76, 1, 72, 1, 71, 1},
  /*2 F */ {65, 1, 72, 1, 69, 1, 72, 1, 77, 1, 72, 1, 69, 1, 67, 1},
  /*3 G */ {67, 1, 74, 1, 71, 1, 74, 1, 79, 1, 77, 1, 74, 1, 71, 1},
  /*4 t */ {72, 1, 76, 79, 1, 1, 84, 1, 83, 1, 79, 1, 76, 1, 1, 1},
  /*5 ov*/ {79, 1, 78, 1, 77, 1, 76, 1, 75, 1, 74, 1, 72, 1, 0, 0},
  /*6 cl*/ {72, 76, 79, 84, 1, 1, 88, 1, 91, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.h.S.h.K.h.S.h.", "K.h.S.h.K.hhS.hS", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{C_, 4, 2}, {AM, -1, 2}, {F_, 4, 2}, {G_, -1, 2}};
static const Bar GAME_BARS[] = {{C_, 0, 0}, {AM, 1, 0}, {F_, 2, 0}, {G_, 3, 1}};
static const Bar FRIGHT_BARS[] = {{EM, -1, 1}, {EM, -1, 1}};
static const Bar OVER_BARS[] = {{C_, 5, 3}, {C_, -1, 3}};
static const Bar CLEAR_BARS[] = {{C_, 6, 3}, {C_, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 108, true, false, 0x30},
  {GAME_BARS, 4, 132, true, false, 0},
  {FRIGHT_BARS, 2, 170, true, true, 0},
  {OVER_BARS, 2, 100, false, false, 0},
  {CLEAR_BARS, 2, 140, false, false, 0},
};
}  // namespace mmm
static const audio::Music MUSIC = {audio::STD_CHORDS, mmm::LEADS, mmm::DRUMS, mmm::SONGS, 6};
enum { SONG_NONE, SONG_TITLE, SONG_GAME, SONG_FRIGHT, SONG_OVER, SONG_CLEAR };

// ------------------------------------------------------------ maze
static const int MCOLS = 28, MROWS = 22, TL = 10;
static const int MX = 20, MY = 16;               // screen position of the maze
static const int TUNNEL_ROW = 10;
static const char* const MAZE[MROWS] = {
  "############################",
  "#............##............#",
  "#.####.#####.##.#####.####.#",
  "#o####.#####.##.#####.####o#",
  "#..........................#",
  "#.####.##.########.##.####.#",
  "#......##....##....##......#",
  "######.##### ## #####.######",
  "     #.##          ##.#     ",
  "######.## ###--### ##.######",
  "      .   #      #   .      ",
  "######.## ######## ##.######",
  "     #.##          ##.#     ",
  "######.## ######## ##.######",
  "#............##............#",
  "#.####.#####.##.#####.####.#",
  "#o..##.......  .......##..o#",
  "###.##.##.########.##.##.###",
  "#......##....##....##......#",
  "#.##########.##.##########.#",
  "#..........................#",
  "############################",
};
static uint8_t dots[MROWS][MCOLS];     // 0 none, 1 dot, 2 power crystal
static int dotsLeft = 0, dotsEaten = 0;

static inline bool wallAt(int c, int r, bool allowDoor) {
  if (r == TUNNEL_ROW && (c < 0 || c >= MCOLS)) return false;
  if (c < 0 || c >= MCOLS || r < 0 || r >= MROWS) return true;
  char ch = MAZE[r][c];
  return ch == '#' || (ch == '-' && !allowDoor);
}

// ------------------------------------------------------------ actors
static const int8_t DX[4] = {0, -1, 0, 1}, DY[4] = {-1, 0, 1, 0};   // up, left, down, right
static const int HOME_X = 135, HOME_Y = 85;    // tile just above the door
static const int PEN_Y = 105;

struct Actor { int x, y, dir; float acc; };      // x,y in maze pixels (tile centre = t*10+5)
static Actor pl;
static int want = 1, mouthT = 0;
enum GMode : uint8_t { G_PEN, G_LEAVE, G_ACTIVE, G_EYES, G_ENTER };
struct Wisp { Actor a; GMode mode; int penT; bool scared; uint8_t r, g, b; };
static Wisp wisps[4];
static const char* const WISP_NAMES[4] = {"BLAZE", "PINKIE", "FROST", "EMBER"};
static const uint8_t WISP_COL[4][3] = {{235, 60, 80}, {255, 123, 213}, {62, 198, 224}, {255, 138, 61}};
static const int SCATTER[4][2] = {{25, -2}, {2, -2}, {27, 23}, {0, 23}};

static int level = 1, lives = 3;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 10000;
static bool newHi = false, extraGiven = false;
static int frightT = 0, eatChain = 0, modeT = 0, modeIdx = 0;
static int gemT = 0, freezeT = 0;
enum State { ST_TITLE, ST_READY, ST_PLAY, ST_DYING, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
struct Popup { bool on; int x, y, t; char txt[12]; };
static Popup popups[4];
struct Part { bool on; float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[160];

static void addScore(uint32_t v) {
  score += v;
  if (!extraGiven && score >= 10000) { extraGiven = true; lives++; audio::play(SFX_POWERUP); }
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void popup(int x, int y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 60, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}
static void burst(float x, float y, int n, uint16_t col, float sp) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s, (uint8_t)frange(12, 28), col}; break; }
}

static inline int tileOf(int v) { return (int)floorf(v / (float)TL); }
static inline bool atCentre(const Actor& a) { return ((a.x % TL) + TL) % TL == 5 && ((a.y % TL) + TL) % TL == 5; }
static inline bool canGo(const Actor& a, int d, bool door) { return !wallAt(tileOf(a.x) + DX[d], tileOf(a.y) + DY[d], door); }

// speeds in maze pixels per frame
static float playerSpeed() { return min(1.25f + 0.04f * (level - 1), 1.5f); }
static float wispSpeed(const Wisp& w) {
  if (w.mode == G_EYES || w.mode == G_ENTER) return 2.4f;
  if (w.mode != G_ACTIVE) return 0.8f;
  float s = min(1.1f + 0.06f * (level - 1), 1.45f) * arcade::speed();
  if (w.scared) s *= 0.55f;
  if (tileOf(w.a.y) == TUNNEL_ROW && (w.a.x < 60 || w.a.x > 220)) s *= 0.55f;
  return s;
}
static int frightFrames() { return arcade::frames(max(90, 420 - (level - 1) * 45)); }

static void buildDots() {
  dotsLeft = 0; dotsEaten = 0;
  for (int r = 0; r < MROWS; r++)
    for (int c = 0; c < MCOLS; c++) {
      char ch = MAZE[r][c];
      dots[r][c] = ch == '.' ? 1 : ch == 'o' ? 2 : 0;
      if (dots[r][c]) dotsLeft++;
    }
}

static void placeActors() {
  pl = {135, 165, 1, 0}; want = 1;
  for (int i = 0; i < 4; i++) {
    Wisp& w = wisps[i];
    w.scared = false;
    w.r = WISP_COL[i][0]; w.g = WISP_COL[i][1]; w.b = WISP_COL[i][2];
    if (i == 0) { w.a = {HOME_X, HOME_Y, 1, 0}; w.mode = G_ACTIVE; w.penT = 0; }
    else { w.a = {115 + i * 10, PEN_Y, 0, 0}; w.mode = G_PEN; w.penT = arcade::frames(i * 180); }
  }
  frightT = 0; modeT = 0; modeIdx = 0; freezeT = 0;
}

static void startLevel() {
  buildDots();
  placeActors();
  gemT = 0;
  state = ST_READY; stateT = 0;
  audio::music(SONG_NONE);
}

static void resetGame() {
  score = 0; lives = 3; level = 1; newHi = false; extraGiven = false;
  memset(popups, 0, sizeof(popups)); memset(parts, 0, sizeof(parts));
  startLevel();
}

// odd schedule slots chase, even ones scatter; after the last slot they chase for good
static const int SCHED[] = {7 * 60, 20 * 60, 7 * 60, 20 * 60, 5 * 60, 20 * 60, 5 * 60};
static bool chasing() { return modeIdx >= 7 || (modeIdx & 1); }

static void wispTarget(int i, int& tx, int& ty) {
  Wisp& w = wisps[i];
  int pc = tileOf(pl.x), pr = tileOf(pl.y);
  if (w.mode == G_EYES) { tx = tileOf(HOME_X); ty = tileOf(HOME_Y); return; }
  if (!chasing()) { tx = SCATTER[i][0]; ty = SCATTER[i][1]; return; }
  switch (i) {
    case 0: tx = pc; ty = pr; break;                                              // straight at you
    case 1: tx = pc + DX[pl.dir] * 4; ty = pr + DY[pl.dir] * 4; break;            // four tiles ahead
    case 2: {                                                                    // mirror Blaze through a point ahead of you
      int ax = pc + DX[pl.dir] * 2, ay = pr + DY[pl.dir] * 2;
      tx = 2 * ax - tileOf(wisps[0].a.x); ty = 2 * ay - tileOf(wisps[0].a.y);
      break;
    }
    default: {                                                                   // chases from afar, retreats up close
      int dx = tileOf(w.a.x) - pc, dy = tileOf(w.a.y) - pr;
      if (dx * dx + dy * dy > 64) { tx = pc; ty = pr; } else { tx = SCATTER[3][0]; ty = SCATTER[3][1]; }
      break;
    }
  }
}

static void chooseDir(int i) {
  Wisp& w = wisps[i];
  int tx, ty; wispTarget(i, tx, ty);
  int best = -1; long bestD = 1L << 30;
  int opts[4], n = 0;
  for (int d = 0; d < 4; d++) {
    if (d == (w.a.dir + 2) % 4) continue;               // no reversing
    if (!canGo(w.a, d, w.mode == G_EYES)) continue;
    opts[n++] = d;
    int nx = tileOf(w.a.x) + DX[d], ny = tileOf(w.a.y) + DY[d];
    long dd = (long)(nx - tx) * (nx - tx) + (long)(ny - ty) * (ny - ty);
    if (dd < bestD) { bestD = dd; best = d; }
  }
  if (n == 0) { w.a.dir = (w.a.dir + 2) % 4; return; }
  w.a.dir = (w.scared && w.mode == G_ACTIVE) ? opts[rnd() % n] : best;
}

static void wrapTunnel(Actor& a) {
  // wrap by a whole number of tiles so tile centres stay aligned
  if (a.x < -10) a.x += MCOLS * TL + 2 * TL;
  else if (a.x > MCOLS * TL + TL) a.x -= MCOLS * TL + 2 * TL;
}

static void moveWisp(int i) {
  Wisp& w = wisps[i];
  w.a.acc += wispSpeed(w);
  while (w.a.acc >= 1) {
    w.a.acc -= 1;
    switch (w.mode) {
      case G_PEN:     // bob up and down until released
        w.a.y += DY[w.a.dir];
        if (w.a.y <= PEN_Y - 3) w.a.dir = 2; else if (w.a.y >= PEN_Y + 3) w.a.dir = 0;
        break;
      case G_LEAVE:   // centre on the door, then float up through it
        if (w.a.x != HOME_X) w.a.x += w.a.x < HOME_X ? 1 : -1;
        else if (w.a.y > HOME_Y) w.a.y--;
        else { w.mode = G_ACTIVE; w.a.dir = 1; }
        break;
      case G_ENTER:   // eyes drop back into the pen, then head out again
        if (w.a.y < PEN_Y) w.a.y++;
        else { w.mode = G_LEAVE; w.scared = false; }
        break;
      default:
        if (atCentre(w.a)) {
          if (w.mode == G_EYES && w.a.x == HOME_X && w.a.y == HOME_Y) { w.mode = G_ENTER; break; }
          chooseDir(i);
        }
        w.a.x += DX[w.a.dir]; w.a.y += DY[w.a.dir];
        wrapTunnel(w.a);
        break;
    }
  }
  if (w.mode == G_PEN && --w.penT <= 0 && state == ST_PLAY) w.mode = G_LEAVE;
}

static void eatAt(int c, int r) {
  if (c < 0 || c >= MCOLS || r < 0 || r >= MROWS || !dots[r][c]) return;
  uint8_t d = dots[r][c];
  dots[r][c] = 0; dotsLeft--; dotsEaten++;
  if (d == 2) {
    addScore(50);
    frightT = frightFrames(); eatChain = 0;
    for (auto& w : wisps) if (w.mode == G_ACTIVE || w.mode == G_PEN || w.mode == G_LEAVE) { w.scared = true; if (w.mode == G_ACTIVE) w.a.dir = (w.a.dir + 2) % 4; }
    audio::play(SFX_POWERUP);
    audio::music(SONG_FRIGHT);
    input::rumble(80, 0x50, 0);
  } else {
    addScore(10);
    audio::play(SFX_MOVE);
  }
  if (dotsEaten == 70 || dotsEaten == 170) gemT = 540;
}

static void movePlayer(const Pad& in) {
  if (in.down(BTN_UP)) want = 0; else if (in.down(BTN_DOWN)) want = 2;
  else if (in.down(BTN_LEFT)) want = 1; else if (in.down(BTN_RIGHT)) want = 3;
  if (want == (pl.dir + 2) % 4) pl.dir = want;          // reverse any time
  pl.acc += playerSpeed();
  bool moved = false;
  while (pl.acc >= 1) {
    pl.acc -= 1;
    if (atCentre(pl)) {
      eatAt(tileOf(pl.x), tileOf(pl.y));
      if (canGo(pl, want, false)) pl.dir = want;
      if (!canGo(pl, pl.dir, false)) { pl.acc = 0; break; }
    }
    pl.x += DX[pl.dir]; pl.y += DY[pl.dir];
    wrapTunnel(pl);
    moved = true;
  }
  if (moved) mouthT++;
}

static void loseLife() {
  state = ST_DYING; stateT = 0;
  audio::music(SONG_NONE);
  audio::play(SFX_PLAYER_DIE);
  input::rumble(500, 0xC0, 0xC0);
}

static void checkCollisions() {
  for (int i = 0; i < 4; i++) {
    Wisp& w = wisps[i];
    if (w.mode != G_ACTIVE) continue;
    if (abs(w.a.x - pl.x) < 7 && abs(w.a.y - pl.y) < 7) {
      if (w.scared) {
        uint32_t v = 200u << min(eatChain, 3);
        eatChain++;
        addScore(v);
        popup(MX + w.a.x - 8, MY + w.a.y - 10, v);
        burst(MX + w.a.x, MY + w.a.y, 16, rgbS(w.r, w.g, w.b), 1.8f);
        w.mode = G_EYES; w.scared = false;
        freezeT = 30;
        audio::play(SFX_EXPLODE);
      } else {
        loseLife();
        return;
      }
    }
  }
  if (gemT && abs(pl.x - 135) < 7 && abs(pl.y - 165) < 7) {
    uint32_t v = 500 * level;
    addScore(v); popup(MX + 125, MY + 150, v); gemT = 0;
    burst(MX + 135, MY + 165, 20, rgbS(182, 255, 110), 1.8f);
    audio::play(SFX_SELECT);
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  if (freezeT) { freezeT--; return; }                  // brief pause after eating a wisp
  if (!frightT && modeIdx < 7 && ++modeT >= SCHED[modeIdx]) {
    modeT = 0; modeIdx++;
    for (auto& w : wisps) if (w.mode == G_ACTIVE) w.a.dir = (w.a.dir + 2) % 4;
  }
  if (frightT && --frightT == 0) {
    for (auto& w : wisps) w.scared = false;
    audio::music(SONG_GAME);
  }
  if (gemT) gemT--;
  movePlayer(in);
  checkCollisions();
  if (state != ST_PLAY) return;
  for (int i = 0; i < 4; i++) moveWisp(i);
  checkCollisions();
  if (state == ST_PLAY && dotsLeft == 0) {
    state = ST_CLEAR; stateT = 0;
    addScore(1000 * level);
    audio::music(SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.94f; p.vy *= 0.94f; if (--p.life == 0) p.on = false; }
  for (auto& p : popups) if (p.on) { if ((p.t & 3) == 0) p.y--; if (--p.t <= 0) p.on = false; }
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); }
      break;
    case ST_READY:
      if (in.hit(BTN_START) && stateT > 10) { arcade::pause(); break; }
      for (auto& w : wisps) if (w.mode == G_PEN) { w.a.acc += 0.5f; while (w.a.acc >= 1) { w.a.acc -= 1; w.a.y += DY[w.a.dir]; if (w.a.y <= PEN_Y - 3) w.a.dir = 2; else if (w.a.y >= PEN_Y + 3) w.a.dir = 0; } }
      if (stateT > 120) { state = ST_PLAY; stateT = 0; audio::music(SONG_GAME); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_DYING:
      if (stateT == 60) burst(MX + pl.x, MY + pl.y, 30, rgbS(182, 255, 110), 2.2f);
      if (stateT > 130) {
        lives--;
        if (lives <= 0) { state = ST_OVER; stateT = 0; audio::music(SONG_OVER); if (newHi) arcade::saveHi(hiscore); }
        else { placeActors(); state = ST_READY; stateT = 60; }
      }
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
static void drawBackground() {
  uint16_t bg = rgbS(6, 4, 18);
  for (int i = 0; i < SW * STRIP; i++) B[i] = bg;
}

static void drawMaze() {
  bool flashWhite = state == ST_CLEAR && ((stateT >> 4) & 1);
  uint16_t fillc = rgbS(20, 16, 60), edge = flashWhite ? rgbS(255, 255, 255) : rgbS(122, 61, 184), edgeHi = flashWhite ? rgbS(255, 255, 255) : rgbS(194, 120, 240);
  for (int r = 0; r < MROWS; r++) {
    int y = MY + r * TL;
    if (!rowsVisible(y, TL)) continue;
    for (int c = 0; c < MCOLS; c++) {
      int x = MX + c * TL;
      char ch = MAZE[r][c];
      if (ch == '-') { rectf(x, y + 4, TL, 2, rgbS(255, 123, 213)); continue; }
      if (ch != '#') {
        if (dots[r][c] == 1) rectf(x + 4, y + 4, 2, 2, rgbS(255, 216, 170));
        else if (dots[r][c] == 2 && ((frameNo >> 3) & 1 || state != ST_PLAY)) {
          rectf(x + 3, y + 2, 4, 6, rgbS(182, 255, 110)); rectf(x + 2, y + 3, 6, 4, rgbS(182, 255, 110));
          pset(x + 4, y + 3, rgbS(255, 255, 255));
        }
        continue;
      }
      rectf(x, y, TL, TL, fillc);
      // neon edge wherever the wall meets a corridor
      auto openAny = [&](int cc, int rr) { return cc >= 0 && cc < MCOLS && rr >= 0 && rr < MROWS && MAZE[rr][cc] != '#'; };
      if (openAny(c, r - 1)) rectf(x, y, TL, 2, edgeHi);
      if (openAny(c, r + 1)) rectf(x, y + TL - 2, TL, 2, edge);
      if (openAny(c - 1, r)) rectf(x, y, 2, TL, edgeHi);
      if (openAny(c + 1, r)) rectf(x + TL - 2, y, 2, TL, edge);
    }
  }
  // cover the tunnel mouths so actors slide in and out of darkness
  if (rowsVisible(MY + TUNNEL_ROW * TL, TL)) {
    rectf(0, MY + TUNNEL_ROW * TL, MX, TL, rgbS(6, 4, 18));
    rectf(MX + MCOLS * TL, MY + TUNNEL_ROW * TL, SW - MX - MCOLS * TL, TL, rgbS(6, 4, 18));
  }
}

static void drawPlayer() {
  int x = MX + pl.x, y = MY + pl.y;
  if (!rowsVisible(y - 7, 14)) return;
  uint16_t hi = rgbS(240, 255, 210);
  if (state == ST_DYING) {
    if (stateT >= 60) return;
    int r = 5 - stateT / 12;
    if (r > 0) disc(x, y, r, c565(182, 255, 110));
    return;
  }
  // a round munch-bot: body, a mouth that opens towards its heading, two eyes
  disc(x, y, 6, c565(30, 107, 58));
  disc(x, y, 5, c565(182, 255, 110));
  int open = (mouthT >> 2) % 3;               // 0 closed .. 2 wide
  int dx = DX[pl.dir], dy = DY[pl.dir];
  for (int k = 1; k <= 5; k++)
    for (int s = -open * k / 4; s <= open * k / 4; s++)
      pset(x + dx * k + dy * s, y + dy * k + dx * s, rgbS(6, 4, 18));
  int ex = x + (dx ? -dx : 2), ey = y + (dy ? -dy : -2);
  rectf(ex - 1, ey - 1, 2, 2, rgbS(13, 11, 30));
  pset(x - 3, y - 3, hi);
}

static void drawWisp(const Wisp& w) {
  int x = MX + w.a.x, y = MY + w.a.y;
  if (!rowsVisible(y - 8, 16)) return;
  bool eyesOnly = w.mode == G_EYES || w.mode == G_ENTER;
  if (!eyesOnly) {
    uint16_t col;
    if (w.scared) {
      bool blink = frightT < 120 && ((frightT >> 3) & 1);
      col = blink ? rgbS(255, 255, 255) : rgbS(51, 48, 170);
    } else col = rgbS(w.r, w.g, w.b);
    // flame-shaped body: round base, flickering tips
    rectf(x - 5, y - 2, 11, 7, col);
    rectf(x - 4, y - 4, 9, 2, col);
    rectf(x - 3, y - 5, 7, 1, col);
    int f = (frameNo >> 2) & 1;
    rectf(x - 3 + f, y - 7, 2, 2, col); rectf(x + 1 - f, y - 8, 2, 3, col);
    for (int k = -5; k <= 5; k += 2) pset(x + k + f, y + 5, col);
    if (w.scared) {
      rectf(x - 3, y - 2, 2, 2, rgbS(255, 216, 170)); rectf(x + 2, y - 2, 2, 2, rgbS(255, 216, 170));
      for (int k = -4; k <= 4; k++) pset(x + k, y + 2 + (k & 1), rgbS(255, 216, 170));
      return;
    }
  }
  int ex = DX[w.a.dir], ey = DY[w.a.dir];
  uint16_t white = rgbS(255, 255, 255), pupil = rgbS(20, 30, 120);
  rectf(x - 4, y - 3, 3, 4, white); rectf(x + 2, y - 3, 3, 4, white);
  rectf(x - 3 + ex, y - 2 + ey, 2, 2, pupil); rectf(x + 3 + ex, y - 2 + ey, 2, 2, pupil);
}

static void drawHud() {
  if (Y0 != 0) return;
  rectf(0, 0, SW, 14, rgbS(10, 8, 25));
  textf(4, 3, TFT_WHITE, 1, top_left, "%07lu", (unsigned long)score);
  textf(62, 3, c565(184, 243, 255), 1, top_left, "LV %d", level);
  textf(142, 3, c565(255, 216, 74), 1, top_left, "HI %07lu", (unsigned long)hiscore);
  for (int i = 0; i < min(lives - 1, 5); i++) disc(SW - 10 - i * 12, 7, 4, c565(182, 255, 110));
}

static void drawTitle() {
  blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 34);
  text("CLEAR THE MAZE, OUTRUN THE WISPS", SW / 2, 74, c565(184, 243, 255), 1, top_center);
  textf(SW / 2, 16, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
  // cast list
  for (int i = 0; i < 4; i++) {
    Wisp w = {};
    w.a = {0, 0, 3, 0}; w.mode = G_ACTIVE; w.r = WISP_COL[i][0]; w.g = WISP_COL[i][1]; w.b = WISP_COL[i][2];
    w.a.x = 40 + i * 72 - MX; w.a.y = 100 - MY;
    drawWisp(w);
    text(WISP_NAMES[i], 40 + i * 72, 112, c565(w.r, w.g, w.b), 1, top_center, false);
  }
  bool blink = (frameNo >> 4) & 1;
  if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 134, TFT_WHITE, 2, top_center); }
  else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 138, c565(255, 138, 61), 1, top_center);
  arcade::drawDifficulty(160);
  text("D-PAD MOVE   START PAUSE", SW / 2, 182, c565(180, 194, 220), 1, top_center);
  text("CRYSTALS LET YOU EAT THE WISPS", SW / 2, 196, c565(182, 255, 110), 1, top_center);
  text("SELECT+START: BACK TO MENU", SW / 2, 218, c565(115, 132, 168), 1, top_center);
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) { drawTitle(); return; }
  drawHud();
  drawMaze();
  if (gemT && rowsVisible(MY + 158, 14)) {
    int x = MX + 135, y = MY + 165;
    uint16_t gc = (frameNo & 8) ? rgbS(255, 123, 213) : rgbS(255, 216, 74);
    rectf(x - 1, y - 5, 3, 11, gc); rectf(x - 3, y - 3, 7, 7, gc); rectf(x - 5, y - 1, 11, 3, gc);
  }
  if (state != ST_CLEAR && !(state == ST_DYING && stateT > 40))
    for (auto& w : wisps) drawWisp(w);
  drawPlayer();
  for (auto& p : parts) if (p.on) pset((int)p.x, (int)p.y, p.col);
  for (auto& p : popups) if (p.on) text(p.txt, p.x, p.y, (p.t & 8) ? c565(255, 216, 74) : TFT_WHITE, 1, top_left);
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_READY) text("READY!", MX + 140, MY + 122, c565(255, 216, 74), 1, top_center);
  if (state == ST_READY && level > 1 && stateT < 60) textf(MX + 140, MY + 62, TFT_WHITE, 1, top_center, "LEVEL %d", level);
  if (state == ST_CLEAR && stateT > 40) {
    if (rowsVisible(96, 40)) shade(70, 96, 180, 40);
    text("MAZE CLEAR!", SW / 2, 100, c565(182, 255, 110), 2, top_center);
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
  arcade::begin("mazemunch", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildDots();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
