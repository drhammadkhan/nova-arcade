// =====================================================================
//  GEM CASCADE  -  a match-three puzzle (Nova Arcade)
//  Swap neighbouring gems to line up three or more of a kind. Matches of
//  four make a flame gem (blows up its 3x3 block), an L or T makes a star
//  gem (clears its row and column) and five in a row makes a nova that
//  wipes out every gem of the colour you swap it with. Matches fill the
//  level bar; it drains while you think. Fill it to reach the next level.
//  Art: tools/make_art.py -> art.h
// =====================================================================
#include <ArcadeCore.h>
#include "art.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace gcm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 t F */ {77, 1, 76, 1, 72, 1, 1, 1, 69, 1, 72, 1, 76, 1, 1, 1},
  /*1 t C */ {79, 1, 76, 1, 72, 1, 1, 1, 76, 1, 79, 1, 84, 1, 1, 1},
  /*2 t G */ {83, 1, 1, 1, 79, 1, 74, 1, 79, 1, 1, 1, 81, 1, 83, 1},
  /*3 t Am*/ {84, 1, 1, 1, 81, 1, 1, 1, 76, 1, 1, 1, 0, 0, 0, 0},
  /*4 p Am*/ {76, 0, 76, 79, 81, 1, 79, 76, 74, 1, 76, 1, 0, 0, 72, 74},
  /*5 p F */ {77, 1, 76, 74, 72, 1, 1, 74, 76, 1, 1, 1, 0, 0, 0, 0},
  /*6 p C */ {72, 0, 72, 76, 79, 1, 76, 79, 84, 1, 83, 1, 81, 1, 79, 1},
  /*7 p G */ {79, 1, 1, 81, 83, 1, 81, 79, 74, 1, 1, 1, 0, 0, 0, 0},
  /*8 p Dm*/ {74, 1, 77, 1, 81, 1, 77, 1, 86, 1, 84, 1, 81, 1, 77, 1},
  /*9 p E */ {80, 1, 1, 1, 83, 1, 1, 1, 88, 1, 86, 1, 83, 1, 80, 1},
  /*10 lv*/  {72, 76, 79, 84, 79, 84, 88, 1, 91, 1, 1, 1, 1, 1, 0, 0},
  /*11 ov*/  {81, 1, 1, 79, 76, 1, 1, 74, 72, 1, 1, 1, 69, 1, 0, 0},
};
static const char* const DRUMS[] = {"K...h.h.S...h.h.", "K.hhS.h.K.hhS.hS", "K.h.S.h.K.h.S.h.", "................", "K...............",};
static const Bar TITLE_BARS[] = {{F_, 0, 4}, {C_, 1, 4}, {G_, 2, 4}, {AM, 3, 4}};
static const Bar PLAY_BARS[] = {{AM, 4, 0}, {F_, 5, 0}, {C_, 6, 0}, {G_, 7, 0}, {AM, 4, 2}, {F_, 5, 2}, {DM, 8, 1}, {E_, 9, 1}};
static const Bar LEVEL_BARS[] = {{C_, 10, 3}, {C_, -1, 3}};
static const Bar OVER_BARS[] = {{AM, 11, 3}, {AM, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 96, true, false, 0x30},
  {PLAY_BARS, 8, 118, true, false, 0x40},
  {LEVEL_BARS, 2, 150, false, false, 0},
  {OVER_BARS, 2, 90, false, false, 0},
};
}  // namespace gcm
static const audio::Music MUSIC = {audio::STD_CHORDS, gcm::LEADS, gcm::DRUMS, gcm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_PLAY, SONG_LEVEL, SONG_OVER };

// ------------------------------------------------------------ board
static const int N = 8, CELL = 26, BX = 8, BY = 17, NTYPES = 7, T_NOVA = 7;
enum Special : uint8_t { SP_NONE, SP_FLAME, SP_STAR, SP_NOVA };
struct Gem { int8_t type; uint8_t sp; float yoff, vy; int8_t clearT; uint8_t delay; };
static Gem board[N][N];
static bool mk[N][N], fired[N][N];

enum Phase { PH_IDLE, PH_SWAP, PH_BACK, PH_CLEAR, PH_FALL, PH_LEVELUP, PH_OVER };
static Phase phase = PH_IDLE;
static int phaseT = 0;
static int curR = 3, curC = 3, swR0, swC0, swR1, swC1;
static bool selected = false;
static input::Repeat repU, repD, repL, repR;

static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 15000;
static bool newHi = false;
static int level = 1, combo = 0, idleT = 0, hintR = -1, hintC = -1, bestCombo = 0;
static float bar = 0.5f, barShown = 0.5f;
static int shake = 0;

enum State { ST_TITLE, ST_PLAY, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

// ------------------------------------------------------------ effects
struct Spark { bool on; float x, y, vx, vy; uint8_t life, kind; uint16_t col; };
static Spark sparks[200];
struct Popup { bool on; float x, y; int t; char txt[16]; uint8_t big; };
static Popup popups[6];
struct Mote { float x, y, v; uint8_t b; };
static Mote motes[28];
struct Beam { bool on; int r, c, t; bool row; };   // star-gem lightning
static Beam beams[8];

static void burst(float x, float y, int n, uint16_t col, float sp) {
  for (auto& p : sparks) {
    if (p.on) continue;
    float a = frand() * 6.2832f, v = frange(0.3f, sp);
    p = {true, x, y, cosf(a) * v, sinf(a) * v - 0.8f, (uint8_t)(18 + rnd() % 18), (uint8_t)(rnd() % 4 == 0), col};
    if (--n <= 0) break;
  }
}
static void popup(float x, float y, const char* s, uint8_t big = 0) {
  if (big) for (auto& p : popups) if (p.big) p.on = false;   // one banner at a time
  for (auto& p : popups) if (!p.on) { p.on = true; p.x = x; p.y = y; p.t = big ? 80 : 50; p.big = big; strncpy(p.txt, s, 15); p.txt[15] = 0; return; }
}

static inline int cellX(int c) { return BX + c * CELL + 1; }
static inline int cellY(int r) { return BY + r * CELL + 1; }
static inline int8_t typeAt(int r, int c) {
  if (r < 0 || r >= N || c < 0 || c >= N) return -1;
  return board[r][c].type;
}

// ------------------------------------------------------------ match finding
struct Run { int8_t r, c, len; bool horiz; };
static Run runs[40];
static int nruns = 0;

static void findRuns() {
  nruns = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N;) {
      int8_t t = board[r][c].type; int len = 1;
      while (c + len < N && t >= 0 && t < NTYPES && board[r][c + len].type == t && !board[r][c + len].clearT) len++;
      if (t >= 0 && t < NTYPES && len >= 3 && nruns < 40) runs[nruns++] = {(int8_t)r, (int8_t)c, (int8_t)len, true};
      c += len;
    }
  for (int c = 0; c < N; c++)
    for (int r = 0; r < N;) {
      int8_t t = board[r][c].type; int len = 1;
      while (r + len < N && t >= 0 && t < NTYPES && board[r + len][c].type == t && !board[r + len][c].clearT) len++;
      if (t >= 0 && t < NTYPES && len >= 3 && nruns < 40) runs[nruns++] = {(int8_t)r, (int8_t)c, (int8_t)len, false};
      r += len;
    }
}

// Would the cell (r,c) be part of a line of three?
static bool lineAt(int r, int c) {
  int8_t t = board[r][c].type;
  if (t < 0 || t >= NTYPES) return false;
  int h = 1, v = 1;
  for (int k = c - 1; k >= 0 && board[r][k].type == t; k--) h++;
  for (int k = c + 1; k < N && board[r][k].type == t; k++) h++;
  for (int k = r - 1; k >= 0 && board[k][c].type == t; k--) v++;
  for (int k = r + 1; k < N && board[k][c].type == t; k++) v++;
  return h >= 3 || v >= 3;
}

static void swapCells(int r0, int c0, int r1, int c1) { Gem t = board[r0][c0]; board[r0][c0] = board[r1][c1]; board[r1][c1] = t; }

static bool swapWorks(int r0, int c0, int r1, int c1) {
  if (board[r0][c0].type == T_NOVA || board[r1][c1].type == T_NOVA) return true;
  swapCells(r0, c0, r1, c1);
  bool ok = lineAt(r0, c0) || lineAt(r1, c1);
  swapCells(r0, c0, r1, c1);
  return ok;
}

static bool findMove(int* hr, int* hc) {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      if (c + 1 < N && swapWorks(r, c, r, c + 1)) { if (hr) { *hr = r; *hc = c; } return true; }
      if (r + 1 < N && swapWorks(r, c, r + 1, c)) { if (hr) { *hr = r; *hc = c; } return true; }
    }
  return false;
}

// Fill the board with no ready-made lines and at least one move. Gems drop in from above.
static void newBoard() {
  do {
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++) {
        Gem& g = board[r][c];
        g = {0, SP_NONE, 0, 0, 0, 0};
        do { g.type = rnd() % NTYPES; } while (lineAt(r, c));
      }
  } while (!findMove(nullptr, nullptr));
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) { board[r][c].yoff = (N - r) * CELL + 40 + (N - 1 - r) * 10 + c * 6; board[r][c].vy = 0; }
}

static void shuffleBoard() {
  for (int tries = 0; tries < 200; tries++) {
    for (int i = N * N - 1; i > 0; i--) { int j = rnd() % (i + 1); swapCells(i / N, i % N, j / N, j % N); }
    bool lines = false;
    for (int r = 0; r < N && !lines; r++) for (int c = 0; c < N; c++) if (lineAt(r, c)) { lines = true; break; }
    if (!lines && findMove(nullptr, nullptr)) break;
  }
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { board[r][c].yoff = (N - r) * CELL + 30 + c * 5; board[r][c].vy = 0; }
}

// ------------------------------------------------------------ clearing
static int markedCount() { int n = 0; for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) n += mk[r][c]; return n; }

static void fireSpecials() {
  bool again = true;
  while (again) {
    again = false;
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++) {
        if (!mk[r][c] || fired[r][c] || board[r][c].sp == SP_NONE) continue;
        fired[r][c] = true; again = true;
        Gem& g = board[r][c];
        if (g.sp == SP_FLAME) {
          for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++)
            if (r + dr >= 0 && r + dr < N && c + dc >= 0 && c + dc < N) mk[r + dr][c + dc] = true;
          burst(cellX(c) + 12, cellY(r) + 12, 30, rgbS(255, 170, 40), 3.5f);
          audio::play(SFX_EXPLODE); shake = 10;
        } else if (g.sp == SP_STAR) {
          for (int k = 0; k < N; k++) { mk[r][k] = true; mk[k][c] = true; }
          for (auto& b : beams) if (!b.on) { b = {true, r, c, 18, true}; break; }
          for (auto& b : beams) if (!b.on) { b = {true, r, c, 18, false}; break; }
          audio::play(SFX_BIG_EXPLODE); shake = 8;
        } else if (g.sp == SP_NOVA) {
          int8_t t = rnd() % NTYPES;
          for (int rr = 0; rr < N; rr++) for (int cc = 0; cc < N; cc++) if (board[rr][cc].type == t) mk[rr][cc] = true;
          audio::play(SFX_BOMB); shake = 12;
        }
      }
  }
}

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}

// Clears everything marked in mk[], scores it and starts the clear animation.
// keep[] cells (new specials) survive with their new powers.
struct NewSp { int8_t r, c, type; uint8_t sp; };
static void startClear(NewSp* made, int nmade, int runsUsed) {
  fireSpecials();
  for (int i = 0; i < nmade; i++) mk[made[i].r][made[i].c] = false;
  int n = markedCount();
  float sx = 0, sy = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      if (!mk[r][c]) continue;
      Gem& g = board[r][c];
      g.clearT = 16; g.delay = 0;
      sx += cellX(c) + 12; sy += cellY(r) + 12;
      int t = g.type < NTYPES ? g.type : rnd() % NTYPES;
      burst(cellX(c) + 12, cellY(r) + 12, 7, rgbS(GEM_RGB[t][0], GEM_RGB[t][1], GEM_RGB[t][2]), 2.4f);
    }
  for (int i = 0; i < nmade; i++) {
    Gem& g = board[made[i].r][made[i].c];
    g.type = made[i].type; g.sp = made[i].sp;
    burst(cellX(made[i].c) + 12, cellY(made[i].r) + 12, 16, rgbS(255, 255, 255), 2.0f);
    audio::play(SFX_POWERUP);
  }
  uint32_t pts = (uint32_t)n * 10 * (combo + 1) * (level + 1) / 2 + nmade * 100;
  addScore(pts);
  bar += n * (0.011f / (1 + (level - 1) * 0.22f)) * (1 + combo * 0.25f);
  if (n) {
    char b[24]; snprintf(b, sizeof(b), "%lu", (unsigned long)pts);
    popup(sx / n, sy / n - 6, b);
  }
  if (combo >= 1) {
    char b[24]; snprintf(b, sizeof(b), "CASCADE x%d", min(combo + 1, 99));
    popup(BX + N * CELL / 2, BY + N * CELL / 2 - 10, b, 1);
  }
  if (combo + 1 > bestCombo) bestCombo = combo + 1;
  if (n >= 12 && combo == 0) popup(BX + N * CELL / 2, BY + 40, "DAZZLING!", 1);
  audio::play(SFX_MATCH, combo);
  (void)runsUsed;
  phase = PH_CLEAR; phaseT = 0;
}

// Resolve the runs found by findRuns(). The pivot cells (the two swapped
// gems) are where new specials appear; in a cascade the middle of the run is used.
static void resolveRuns(int pr0, int pc0, int pr1, int pc1) {
  memset(mk, 0, sizeof(mk)); memset(fired, 0, sizeof(fired));
  static bool inH[N][N], inV[N][N];
  memset(inH, 0, sizeof(inH)); memset(inV, 0, sizeof(inV));
  NewSp made[16]; int nmade = 0;
  auto isMade = [&](int r, int c) { for (int i = 0; i < nmade; i++) if (made[i].r == r && made[i].c == c) return true; return false; };
  for (int i = 0; i < nruns; i++) {
    Run& u = runs[i];
    int pr = -1, pc = -1;
    for (int k = 0; k < u.len; k++) {
      int r = u.r + (u.horiz ? 0 : k), c = u.c + (u.horiz ? k : 0);
      mk[r][c] = true;
      (u.horiz ? inH : inV)[r][c] = true;
      if ((r == pr0 && c == pc0) || (r == pr1 && c == pc1)) { pr = r; pc = c; }
    }
    if (pr < 0) { pr = u.r + (u.horiz ? 0 : u.len / 2); pc = u.c + (u.horiz ? u.len / 2 : 0); }
    if (u.len >= 4 && nmade < 16 && !isMade(pr, pc) && board[pr][pc].sp == SP_NONE)
      made[nmade++] = {(int8_t)pr, (int8_t)pc, u.len >= 5 ? (int8_t)T_NOVA : board[pr][pc].type, u.len >= 5 ? (uint8_t)SP_NOVA : (uint8_t)SP_FLAME};
  }
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (inH[r][c] && inV[r][c] && !isMade(r, c) && board[r][c].sp == SP_NONE && nmade < 16)
        made[nmade++] = {(int8_t)r, (int8_t)c, board[r][c].type, SP_STAR};
  startClear(made, nmade, nruns);
}

// A nova swapped with a gem: every gem of that colour goes (two novas clear the board)
static void novaBlast(int nr, int nc, int orr, int oc) {
  memset(mk, 0, sizeof(mk)); memset(fired, 0, sizeof(fired));
  int8_t t = board[orr][oc].type;
  mk[nr][nc] = true; fired[nr][nc] = true;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (t == T_NOVA || board[r][c].type == t) {
        mk[r][c] = true;
        if (board[r][c].type == T_NOVA) fired[r][c] = true;
        for (auto& b : beams) if (!b.on) { b = {true, r, c, 14, false}; b.c = c; b.r = r; b.row = (r + c) & 1; break; }
      }
  audio::play(SFX_BOMB); shake = 16;
  popup(BX + N * CELL / 2, BY + 60, t == T_NOVA ? "SUPERNOVA!" : "NOVA!", 1);
  startClear(nullptr, 0, 0);
}

static void collapse() {
  for (int c = 0; c < N; c++) {
    int dst = N - 1;
    for (int r = N - 1; r >= 0; r--) {
      if (board[r][c].type < 0) continue;
      if (dst != r) { board[dst][c] = board[r][c]; board[dst][c].yoff += (dst - r) * CELL; board[r][c].type = -1; }
      dst--;
    }
    int k = 0;
    for (int r = dst; r >= 0; r--, k++) {
      Gem& g = board[r][c];
      g = {(int8_t)(rnd() % NTYPES), SP_NONE, (float)((dst + 1) * CELL + k * 6 + 8), 0, 0, 0};
    }
  }
  phase = PH_FALL; phaseT = 0;
}

static bool updateFall() {
  bool moving = false;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      Gem& g = board[r][c];
      if (g.yoff <= 0) continue;
      g.vy = min(g.vy + 0.55f, 11.0f);
      g.yoff -= g.vy;
      if (g.yoff <= 0) { g.yoff = 0; g.vy = 0; }
      moving = true;
    }
  return moving;
}

// ------------------------------------------------------------ game flow
static void startGame() {
  score = 0; newHi = false; level = 1; combo = 0; bar = barShown = 0.5f; bestCombo = 0;
  curR = curC = 3; selected = false; idleT = 0; hintR = -1;
  hiscore = arcade::loadHi(HI_DEFAULT);
  newBoard();
  phase = PH_FALL; phaseT = 0;
  state = ST_PLAY; stateT = 0;
  audio::music(SONG_PLAY);
}

static void gameOver() {
  state = ST_OVER; stateT = 0; phase = PH_OVER; phaseT = 0;
  if (newHi) arcade::saveHi(hiscore);
  audio::music(SONG_OVER);
  audio::play(SFX_PLAYER_DIE);
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { board[r][c].delay = (uint8_t)((N - 1 - r) * 6 + rnd() % 8); board[r][c].vy = -frange(1, 3); }
}

static void startLevelUp() {
  phase = PH_LEVELUP; phaseT = 0;
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { board[r][c].clearT = 16; board[r][c].delay = (uint8_t)((r + c) * 3); }
  audio::music(SONG_LEVEL);
  addScore(1000 * level);
}

static void buildBackground();

static void trySwap(int dr, int dc) {
  int r1 = curR + dr, c1 = curC + dc;
  selected = false;
  if (r1 < 0 || r1 >= N || c1 < 0 || c1 >= N) return;
  swR0 = curR; swC0 = curC; swR1 = r1; swC1 = c1;
  phase = PH_SWAP; phaseT = 0;
  curR = r1; curC = c1;
  hintR = -1; idleT = 0;
  audio::play(SFX_SWAP);
}

static void stepBoard(const Pad& in) {
  phaseT++;
  switch (phase) {
    case PH_IDLE: {
      idleT++;
      bar -= (0.00011f + (level - 1) * 0.00004f) * arcade::speed();
      if (bar <= 0) { bar = 0; gameOver(); return; }
      if (idleT == arcade::frames(420)) findMove(&hintR, &hintC);
      bool aHeld = in.down(BTN_A);
      int dr = 0, dc = 0;
      if (repU.tick(in.down(BTN_UP), 14, 5)) dr = -1;
      else if (repD.tick(in.down(BTN_DOWN), 14, 5)) dr = 1;
      else if (repL.tick(in.down(BTN_LEFT), 14, 5)) dc = -1;
      else if (repR.tick(in.down(BTN_RIGHT), 14, 5)) dc = 1;
      if (dr || dc) {
        if (selected || (aHeld && !in.hit(BTN_A))) { trySwap(dr, dc); break; }
        curR = constrain(curR + dr, 0, N - 1); curC = constrain(curC + dc, 0, N - 1);
        audio::play(SFX_MOVE);
      }
      if (in.hit(BTN_A)) { selected = !selected; audio::play(selected ? SFX_ROTATE : SFX_MOVE); }
      if (in.hit(BTN_B)) selected = false;
      break;
    }
    case PH_SWAP:
      if (phaseT >= 9) {
        swapCells(swR0, swC0, swR1, swC1);
        combo = 0;
        Gem &a = board[swR1][swC1], &b = board[swR0][swC0];   // a = the gem the player moved
        if (a.type == T_NOVA) { novaBlast(swR1, swC1, swR0, swC0); break; }
        if (b.type == T_NOVA) { novaBlast(swR0, swC0, swR1, swC1); break; }
        findRuns();
        if (nruns) resolveRuns(swR1, swC1, swR0, swC0);
        else { swapCells(swR0, swC0, swR1, swC1); phase = PH_BACK; phaseT = 0; audio::play(SFX_HIT); curR = swR0; curC = swC0; }
      }
      break;
    case PH_BACK:
      if (phaseT >= 9) phase = PH_IDLE;
      break;
    case PH_CLEAR: {
      bool any = false;
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          Gem& g = board[r][c];
          if (!g.clearT) continue;
          if (--g.clearT == 0) { g.type = -1; g.sp = SP_NONE; }
          else any = true;
        }
      if (!any) collapse();
      break;
    }
    case PH_FALL:
      if (!updateFall()) {
        findRuns();
        if (nruns) { combo++; resolveRuns(-1, -1, -1, -1); }
        else if (bar >= 1.0f) startLevelUp();
        else {
          phase = PH_IDLE; combo = 0; idleT = 0;
          if (!findMove(nullptr, nullptr)) { popup(BX + N * CELL / 2, BY + 90, "NO MOVES - SHUFFLE", 1); shuffleBoard(); phase = PH_FALL; audio::play(SFX_WARNING); }
        }
      }
      break;
    case PH_LEVELUP: {
      bool any = false;
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          Gem& g = board[r][c];
          if (!g.clearT) continue;
          any = true;
          if (g.delay) { g.delay--; continue; }
          if (g.clearT == 16) {
            int t = g.type < NTYPES ? g.type : 0;
            burst(cellX(c) + 12, cellY(r) + 12, 5, rgbS(GEM_RGB[t][0], GEM_RGB[t][1], GEM_RGB[t][2]), 3.0f);
            if (((r + c) & 3) == 0) audio::play(SFX_BRICK, r + c);
          }
          if (--g.clearT == 0) g.type = -1;
        }
      if (!any && phaseT > 150) {
        level++; bar = barShown = 0.5f;
        buildBackground();
        newBoard();
        phase = PH_FALL; phaseT = 0;
        audio::music(SONG_PLAY);
      }
      break;
    }
    case PH_OVER:
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          Gem& g = board[r][c];
          if (g.delay) { g.delay--; continue; }
          g.vy += 0.4f; g.yoff -= g.vy;
        }
      break;
  }
}

static void updateFx() {
  for (auto& p : sparks) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += 0.12f; p.vx *= 0.97f;
    if (!--p.life) p.on = false;
  }
  for (auto& p : popups) if (p.on) { p.y -= p.big ? 0.25f : 0.6f; if (--p.t <= 0) p.on = false; }
  for (auto& m : motes) { m.y -= m.v; if (m.y < -2) { m.y = SH + 2; m.x = rnd() % SW; } }
  for (auto& b : beams) if (b.on && --b.t <= 0) b.on = false;
  barShown += (constrain(bar, 0.0f, 1.0f) - barShown) * 0.12f;
  if (shake) shake--;
}

static void step(const Pad& in) {
  stateT++;
  updateFx();
  switch (state) {
    case ST_TITLE:
      if (arcade::titleInput(in)) hiscore = arcade::loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { audio::play(SFX_START); startGame(); }
      // demo: gems rain behind the title
      if (!updateFall()) {
        if (stateT % 200 == 0) { newBoard(); }
      }
      break;
    case ST_PLAY:
      if (in.hit(BTN_START)) { arcade::pause(); break; }
      stepBoard(in);
      break;
    case ST_OVER:
      stepBoard(in);
      if (stateT > 120 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; newBoard(); audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static uint16_t bgRow[SH];
static uint8_t bgLite[3];
static const uint8_t THEMES[6][3][3] = {
  {{18, 8, 44}, {70, 20, 90}, {150, 90, 220}},     // amethyst dusk
  {{4, 18, 40}, {10, 70, 110}, {80, 200, 240}},     // sapphire deep
  {{4, 30, 24}, {16, 90, 70}, {110, 230, 150}},     // emerald cave
  {{40, 10, 16}, {120, 30, 50}, {250, 120, 140}},   // ruby glow
  {{30, 18, 6}, {110, 64, 20}, {255, 200, 90}},     // amber hall
  {{10, 10, 30}, {40, 44, 90}, {200, 210, 255}},    // pearl night
};
static void buildBackground() {
  const auto& th = THEMES[(level - 1) % 6];
  for (int y = 0; y < SH; y++) {
    float t = (float)y / SH;
    bgRow[y] = t < 0.6f ? lerpS(th[0][0], th[0][1], th[0][2], th[1][0], th[1][1], th[1][2], t / 0.6f)
                        : lerpS(th[1][0], th[1][1], th[1][2], th[0][0], th[0][1], th[0][2], (t - 0.6f) / 0.4f);
  }
  memcpy(bgLite, th[2], 3);
}

static void drawBackground() {
  // diagonal crystal lattice that slowly drifts, over a vertical gradient
  int sh = frameNo >> 1;
  uint16_t lite = rgbS(bgLite[0] / 3, bgLite[1] / 3, bgLite[2] / 3);
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r;
    uint16_t c = bgRow[y];
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) {
      int a = (x + y + sh) & 63, b = (x - y + 512 - sh / 2) & 63;
      d[x] = (a == 0 || b == 0) ? lite : c;
    }
  }
  uint16_t mc[3] = {rgbS(bgLite[0] / 2, bgLite[1] / 2, bgLite[2] / 2), rgbS(bgLite[0], bgLite[1], bgLite[2]), rgbS(255, 255, 255)};
  for (auto& m : motes) pset((int)m.x, (int)m.y, mc[m.b]);
}

// Nearest-neighbour scaled blit centred on (cx,cy)
static void blitScaled(const Sprite& s, int cx, int cy, float k, const uint16_t* p = pal) {
  int w = (int)(s.w * k), h = (int)(s.h * k);
  if (w <= 0 || h <= 0) return;
  int x0 = cx - w / 2, y0 = cy - h / 2;
  if (!rowsVisible(y0, h)) return;
  for (int y = max(y0, Y0); y < min(y0 + h, Y0 + STRIP); y++) {
    const uint8_t* row = s.px + ((y - y0) * s.h / h) * s.w;
    for (int x = max(0, x0); x < min(SW, x0 + w); x++) {
      uint8_t v = row[(x - x0) * s.w / w];
      if (v) B[(y - Y0) * SW + x] = p[v];
    }
  }
}

static void drawGem(const Gem& g, int x, int y, bool sel) {
  if (g.type < 0) return;
  const Sprite* s = g.type == T_NOVA ? NOVA[(frameNo >> 3) & 3] : GEM[g.type];
  if (g.sp == SP_FLAME) blit(*AURA[(frameNo >> 2) % 3], x - 4, y - 4);
  if (g.clearT) {
    if (g.delay) { blit(*s, x, y); return; }
    if (g.clearT > 11) blit(*s, x, y, palWhite);
    else blitScaled(*s, x + 12, y + 12, g.clearT / 11.0f);
    return;
  }
  if (sel) y += ((frameNo >> 3) & 1) ? -1 : 0;
  blit(*s, x, y);
  if (g.sp == SP_STAR) blit(*STARO[(frameNo >> 3) & 1], x, y);
}

static void drawBoard() {
  int ox = shake ? (int)(rnd() % 5) - 2 : 0, oy = shake ? (int)(rnd() % 3) - 1 : 0;
  int x0 = BX + ox, y0 = BY + oy, w = N * CELL;
  if (rowsVisible(y0 - 4, w + 8)) {
    // dark glass well with a two-tone checker
    shade(x0, y0, w, w); shade(x0, y0, w, w);
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++)
        if ((r + c) & 1) shade(x0 + c * CELL, y0 + r * CELL, CELL, CELL);
    // bevelled metal frame
    rect(x0 - 1, y0 - 1, w + 2, w + 2, pal[GC_METAL0 + 2]);
    rect(x0 - 2, y0 - 2, w + 4, w + 4, pal[GC_METAL0 + 4]);
    rect(x0 - 3, y0 - 3, w + 6, w + 6, pal[GC_METAL0 + 1]);
    rect(x0 - 4, y0 - 4, w + 8, w + 8, pal[GC_METAL0]);
  }
  // corner rivets
  for (int i = 0; i < 4; i++) {
    int sx = i & 1 ? x0 + w + 1 : x0 - 6, sy = i & 2 ? y0 + w + 1 : y0 - 6;
    if (!rowsVisible(sy, 6)) continue;
    rectf(sx, sy, 5, 5, pal[GC_METAL0 + 1]);
    rectf(sx + 1, sy + 1, 3, 3, pal[GC_METAL0 + 3]);
    pset(sx + 1, sy + 1, pal[GC_WHITE]);
  }
  // star lightning beams under the gems
  for (auto& b : beams) {
    if (!b.on) continue;
    uint16_t c1 = rgbS(150, 240, 255), c2 = rgbS(255, 255, 255);
    int w2 = b.t > 8 ? 3 : 1;
    if (b.row) { int yy = y0 + b.r * CELL + CELL / 2; rectf(x0, yy - w2, w, w2 * 2 + 1, c1); rectf(x0, yy, w, 1, c2); }
    else { int xx = x0 + b.c * CELL + CELL / 2; rectf(xx - w2, y0, w2 * 2 + 1, w, c1); rectf(xx, y0, 1, w, c2); }
  }
  // gems
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      Gem& g = board[r][c];
      int x = x0 + c * CELL + 1, y = y0 + r * CELL + 1 - (int)g.yoff;
      if (phase == PH_SWAP || phase == PH_BACK) {
        float t = min(1.0f, phaseT / 9.0f);
        if (phase == PH_BACK) t = 1 - t;
        t = t * t * (3 - 2 * t);
        if (r == swR0 && c == swC0) { x += (int)((swC1 - swC0) * CELL * t); y += (int)((swR1 - swR0) * CELL * t); }
        else if (r == swR1 && c == swC1) { x += (int)((swC0 - swC1) * CELL * t); y += (int)((swR0 - swR1) * CELL * t); }
      }
      if (y + 24 < y0 - 2) continue;             // still above the well
      if (!rowsVisible(y - 4, 32)) continue;
      // keep entering gems out of the frame: draw only rows inside the well
      if (y < y0 && state != ST_OVER) {
        int cut = y0 - y;
        if (cut >= 24) continue;
        const Sprite* s = g.type == T_NOVA ? NOVA[(frameNo >> 3) & 3] : (g.type >= 0 ? GEM[g.type] : nullptr);
        if (!s) continue;
        Sprite part = {s->w, (uint16_t)(s->h - cut), s->px + cut * s->w};
        blit(part, x, y0);
        continue;
      }
      if (state == ST_OVER) { if (g.type >= 0) blit(*GEM[g.type < NTYPES ? g.type : 0], x, y, palDark); continue; }
      drawGem(g, x, y, selected && r == curR && c == curC);
    }
  // hint sparkle
  if (hintR >= 0 && phase == PH_IDLE && state == ST_PLAY) {
    int f = (frameNo >> 3) % 3;
    blit(*SPARKLE[f], x0 + hintC * CELL + 2, y0 + hintR * CELL + 2);
    blit(*SPARKLE[(f + 1) % 3], x0 + hintC * CELL + 17, y0 + hintR * CELL + 16);
  }
  // cursor
  if (state == ST_PLAY && (phase == PH_IDLE || phase == PH_FALL || phase == PH_CLEAR)) {
    int cx = x0 + curC * CELL - 1, cy = y0 + curR * CELL - 1;
    if (selected) {
      uint16_t c = ((frameNo >> 2) & 1) ? rgbS(255, 230, 120) : rgbS(255, 160, 40);
      rect(cx, cy, 28, 28, c); rect(cx + 1, cy + 1, 26, 26, c);
    } else {
      int pulse = ((frameNo >> 4) & 1);
      if (pulse) blit(SPR_CURSOR, cx, cy); else blitScaled(SPR_CURSOR, cx + 14, cy + 14, 1.08f);
    }
  }
}

static void drawPanel() {
  const int px = 228, pw = 66;
  if (rowsVisible(13, 216)) {
    shade(px - 4, 13, pw + 8, 216); shade(px - 4, 13, pw + 8, 216);
    rect(px - 4, 13, pw + 8, 216, pal[GC_METAL0 + 1]);
  }
  text("LEVEL", px + pw / 2, 20, c565(184, 200, 255), 1, top_center);
  textf(px + pw / 2, 31, TFT_WHITE, 3, top_center, "%d", level);
  text("SCORE", px + pw / 2, 64, c565(184, 200, 255), 1, top_center);
  textf(px + pw / 2, 75, c565(255, 216, 74), 1, top_center, "%lu", (unsigned long)score);
  text("BEST", px + pw / 2, 92, c565(184, 200, 255), 1, top_center);
  textf(px + pw / 2, 103, TFT_WHITE, 1, top_center, "%lu", (unsigned long)hiscore);
  // rotating showcase
  blit(*NOVA[(frameNo >> 3) & 3], px + pw / 2 - 12, 122);
  const uint8_t* dc = arcade::DIFF_RGB[arcade::difficulty];
  text(arcade::difficultyName(), px + pw / 2, 154, c565(dc[0], dc[1], dc[2]), 1, top_center);
  if (bestCombo > 1) textf(px + pw / 2, 166, c565(150, 240, 255), 1, top_center, "BEST x%d", bestCombo);
  // level bar
  const int bx = 300, by = 16, bh = 210;
  if (rowsVisible(by, bh + 2)) {
    rectf(bx, by, 14, bh, rgbS(16, 10, 30));
    rect(bx - 1, by - 1, 16, bh + 2, pal[GC_METAL0 + 3]);
    int fh = (int)(barShown * (bh - 2));
    bool low = bar < 0.18f && state == ST_PLAY;
    for (int y = by + bh - 1 - fh; y < by + bh - 1; y++) {
      if (y < Y0 || y >= Y0 + STRIP) continue;
      float t = (float)(y - by) / bh;
      uint16_t c = low && ((frameNo >> 3) & 1) ? rgbS(255, 60, 70)
                 : lerpS(255, 120, 220, 80, 200, 255, t);
      if (((y + (frameNo >> 1)) & 15) == 0) c = rgbS(255, 255, 255);
      rectf(bx + 1, y, 12, 1, c);
    }
    rectf(bx + 2, by + 1, 2, bh - 2, rgbS(255, 255, 255) & 0xE7FF);
  }
  if (rowsVisible(180, 40)) {
    text("A: PICK", px + pw / 2, 196, c565(140, 150, 190), 1, top_center, false);
    text("+DIR SWAP", px + pw / 2, 207, c565(140, 150, 190), 1, top_center, false);
  }
}

static void drawFx() {
  for (auto& p : sparks) {
    if (!p.on) continue;
    if (p.kind && p.life > 10) blit(*SPARKLE[(p.life >> 2) % 3], (int)p.x - 3, (int)p.y - 3);
    else { pset((int)p.x, (int)p.y, p.col); if (p.life > 14) pset((int)p.x + 1, (int)p.y, p.col); }
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    if (p.big) {
      int y = (int)p.y;
      if (rowsVisible(y - 3, 22)) shade(BX, y - 3, N * CELL, 22);
      text(p.txt, (int)p.x, y, (p.t & 4) ? c565(255, 230, 120) : c565(255, 140, 220), 2, top_center);
    } else text(p.txt, (int)p.x, (int)p.y, (p.t & 4) ? TFT_WHITE : c565(255, 216, 74), 1, top_center);
  }
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    drawBoard();
    if (rowsVisible(14, 212)) { shade(0, 14, SW, 212); shade(20, 24, 280, 192); }
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 34);
    text("MATCH THREE - CHAIN THE CASCADES", SW / 2, 74, c565(184, 220, 255), 1, top_center);
    for (int i = 0; i < 7; i++) {
      int bob = (int)(sinf(frameNo * 0.08f + i * 0.9f) * 3);
      blit(*GEM[i], 40 + i * 34, 90 + bob);
    }
    textf(SW / 2, 124, c565(255, 216, 74), 1, top_center, "HI-SCORE %lu", (unsigned long)hiscore);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 140, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 144, c565(255, 138, 61), 1, top_center);
    arcade::drawDifficulty(166);
    text("A PICKS A GEM, D-PAD SWAPS IT", SW / 2, 186, c565(180, 194, 220), 1, top_center);
    text("4 = FLAME   L/T = STAR   5 = NOVA", SW / 2, 198, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 214, c565(115, 132, 168), 1, top_center);
    drawFx();
    return;
  }
  drawBoard();
  drawPanel();
  drawFx();
  if (phase == PH_LEVELUP && phaseT > 40) {
    if (rowsVisible(84, 60)) { shade(BX, 84, N * CELL, 60); shade(BX, 84, N * CELL, 60); }
    textf(BX + N * CELL / 2, 92, c565(255, 230, 120), 2, top_center, "LEVEL %d", level + 1);
    textf(BX + N * CELL / 2, 116, TFT_WHITE, 1, top_center, "BONUS %d", 1000 * level);
    text("NEW GEMS INCOMING", BX + N * CELL / 2, 130, c565(150, 240, 255), 1, top_center);
  }
  if (state == ST_OVER && stateT > 40) {
    if (rowsVisible(80, 80)) { shade(BX, 80, N * CELL, 80); shade(BX, 80, N * CELL, 80); }
    text("GAME OVER", BX + N * CELL / 2, 88, c565(255, 90, 120), 3, top_center);
    textf(BX + N * CELL / 2, 118, TFT_WHITE, 1, top_center, "SCORE %lu", (unsigned long)score);
    if (newHi && ((frameNo >> 4) & 1)) text("NEW HI-SCORE!", BX + N * CELL / 2, 130, c565(255, 216, 74), 1, top_center);
    if (stateT > 120) text("PRESS START", BX + N * CELL / 2, 144, c565(184, 220, 255), 1, top_center);
  }
}

void setup() {
  arcade::begin("gemcascade", &MUSIC);
  setPalette(GC_PAL565, GC_PAL_N);
  for (auto& m : motes) { m.x = rnd() % SW; m.y = rnd() % SH; m.v = frange(0.1f, 0.5f); m.b = rnd() % 3; }
  buildBackground();
  newBoard();
  hiscore = arcade::loadHi(HI_DEFAULT);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
