// =====================================================================
//  BLOCKFALL  -  a falling-blocks puzzle game (Nova Arcade)
//  7-bag randomiser, SRS rotation with wall kicks, hold, ghost piece,
//  3-piece preview, lock delay, DAS. Built on ArcadeCore.
// =====================================================================
#include <ArcadeCore.h>
#include "logo.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace bm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  /*0 Dm */ {74, 1, 77, 1, 81, 1, 79, 77, 76, 1, 74, 1, 72, 1, 74, 1},
  /*1 Bb */ {77, 1, 1, 74, 70, 1, 74, 1, 77, 1, 79, 1, 77, 1, 74, 1},
  /*2 C  */ {76, 1, 79, 1, 84, 1, 83, 79, 76, 1, 79, 1, 72, 1, 1, 1},
  /*3 Am */ {81, 1, 1, 1, 79, 1, 76, 1, 72, 1, 76, 1, 81, 1, 79, 1},
  /*4 Dm2*/ {86, 1, 84, 1, 81, 1, 77, 1, 79, 1, 81, 1, 74, 1, 1, 1},
  /*5 end*/ {76, 1, 1, 1, 73, 1, 1, 1, 69, 1, 1, 1, 0, 0, 0, 0},
  /*6 t1 */ {74, 1, 1, 1, 81, 1, 1, 1, 79, 1, 77, 1, 76, 1, 1, 1},
  /*7 t2 */ {77, 1, 1, 1, 76, 1, 74, 1, 72, 1, 1, 1, 74, 1, 1, 1},
  /*8 over*/{81, 1, 77, 1, 74, 1, 70, 1, 69, 1, 1, 1, 1, 1, 0, 0},
};
static const char* const DRUMS[] = {"K.h.S.hKK.h.S.h.", "K.h.S.h.K.h.SSSS", "..h...h...h...h.", "................"};
static const Bar TITLE_BARS[] = {{DM, 6, 2}, {BB, 7, 2}, {C_, -1, 2}, {AM, -1, 2}};
static const Bar GAME_BARS[] = {{DM, 0, 0}, {BB, 1, 0}, {C_, 2, 0}, {AM, 3, 0},
                                {DM, 4, 0}, {BB, 1, 0}, {C_, 2, 0}, {A_, 5, 1}};
static const Bar OVER_BARS[] = {{DM, 8, 3}, {DM, -1, 3}};
static const SongDef SONGS[] = {
  {nullptr, 0, 120, false, false, 0},
  {TITLE_BARS, 4, 104, true, false, 0x30},
  {GAME_BARS, 8, 138, true, false, 0},
  {GAME_BARS, 8, 168, true, true, 0x60},   // "danger" version when the stack is high
  {OVER_BARS, 2, 90, false, false, 0},
};
}  // namespace bm
static const audio::Music MUSIC = {audio::STD_CHORDS, bm::LEADS, bm::DRUMS, bm::SONGS, 5};
enum { SONG_NONE, SONG_TITLE, SONG_GAME, SONG_FAST, SONG_OVER };

// ------------------------------------------------------------ pieces
static const int COLS = 10, ROWS = 22, HIDDEN = 2, CELL = 11;
static const int BX = 105, BY = 10;   // board origin on screen (first visible row)
enum { P_I, P_O, P_T, P_S, P_Z, P_J, P_L };
static const int8_t SHAPES[7][4][2] = {
  {{0, 1}, {1, 1}, {2, 1}, {3, 1}},   // I (4x4 box)
  {{1, 0}, {2, 0}, {1, 1}, {2, 1}},   // O
  {{1, 0}, {0, 1}, {1, 1}, {2, 1}},   // T
  {{1, 0}, {2, 0}, {0, 1}, {1, 1}},   // S
  {{0, 0}, {1, 0}, {1, 1}, {2, 1}},   // Z
  {{0, 0}, {0, 1}, {1, 1}, {2, 1}},   // J
  {{2, 0}, {0, 1}, {1, 1}, {2, 1}},   // L
};
static int8_t cells[7][4][4][2];   // [piece][rotation][block][x,y]

// SRS kicks (y down). Index: 0 0->R,1 R->0,2 R->2,3 2->R,4 2->L,5 L->2,6 L->0,7 0->L
static const int8_t KICK_JLSTZ[8][5][2] = {
  {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}, {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
  {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},   {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
  {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
  {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}};
static const int8_t KICK_I[8][5][2] = {
  {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}}, {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
  {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}, {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
  {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}}, {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
  {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}}, {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}};

static const uint8_t PCOL[10][3] = {
  {0, 0, 0}, {40, 210, 230}, {250, 210, 40}, {170, 80, 225}, {80, 215, 95},
  {235, 60, 80}, {60, 115, 240}, {250, 145, 40}, {105, 105, 130}, {255, 255, 255}};
static uint16_t tiles[10][CELL * CELL];   // big tiles, swapped RGB565
static uint16_t mini[10][8 * 8];          // preview tiles
static uint16_t ghostCol[10];

static void buildTiles() {
  for (int c = 1; c < 10; c++) {
    int r = PCOL[c][0], g = PCOL[c][1], b = PCOL[c][2];
    auto mix = [&](float k, int tr, int tg, int tb) {
      return rgbS(r + (tr - r) * k, g + (tg - g) * k, b + (tb - b) * k);
    };
    for (int n = 0; n < 2; n++) {
      int S = n ? 8 : CELL;
      uint16_t* t = n ? mini[c] : tiles[c];
      for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
          uint16_t v;
          float grad = (float)y / S * 0.25f;
          if (x == S - 1 || y == S - 1) v = mix(0.55f, 0, 0, 0);
          else if (x == 0 || y == 0) v = mix(0.45f, 255, 255, 255);
          else v = mix(grad, 0, 0, 0);
          if (!n && ((x == 2 && y >= 2 && y <= 3) || (y == 2 && x == 3))) v = mix(0.7f, 255, 255, 255);
          t[y * S + x] = v;
        }
    }
    ghostCol[c] = rgbS(r / 2, g / 2, b / 2);
  }
  for (int p = 0; p < 7; p++)
    for (int rot = 0; rot < 4; rot++)
      for (int k = 0; k < 4; k++) {
        int x = SHAPES[p][k][0], y = SHAPES[p][k][1];
        int n = p == P_I ? 4 : 3;
        for (int i = 0; i < rot && p != P_O; i++) { int nx = n - 1 - y; y = x; x = nx; }
        cells[p][rot][k][0] = x; cells[p][rot][k][1] = y;
      }
}

static void drawTile(const uint16_t* t, int S, int x, int y) {
  int y0 = max(Y0, y), y1 = min(Y0 + STRIP, y + S);
  for (int yy = y0; yy < y1; yy++) {
    const uint16_t* s = t + (yy - y) * S;
    uint16_t* d = B + (yy - Y0) * SW;
    for (int i = 0; i < S; i++) if ((unsigned)(x + i) < (unsigned)SW) d[x + i] = s[i];
  }
}

// ------------------------------------------------------------ game state
static uint8_t board[ROWS][COLS];
struct Piece { int type, rot, x, y; } cur;
static int hold = -1;
static bool holdUsed = false;
static uint8_t bag[7], bagN = 0, queue[3];
static uint32_t score = 0, hiscore = 0;
static int level = 1, lines = 0, combo = -1;
static int gravT = 0, lockT = 0, lockResets = 0;
static input::Repeat repL, repR;
enum State { ST_TITLE, ST_PLAY, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int clearRows[4], nClear = 0, clearT = 0;
static int overT = 0;
static bool newHi = false;
static int bannerT = 0;
static char banner[20];
static float shake = 0;
static int shakeX = 0, shakeY = 0;
static int dangerSong = 0;

struct Particle { bool on; float x, y, vx, vy; uint8_t life, max, col; };
static Particle parts[260];
static void spark(float x, float y, float vx, float vy, int life, int col) {
  for (auto& p : parts) if (!p.on) { p = {true, x, y, vx, vy, (uint8_t)life, (uint8_t)life, (uint8_t)col}; return; }
}

// background dust
struct Dust { float x, y, v; uint8_t s; };
static Dust dust[40];
struct Demo { float y; int x, type, rot; float v; };
static Demo demo[7];

static uint8_t nextFromBag() {
  if (bagN == 0) {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) { int j = rnd() % (i + 1); uint8_t t = bag[i]; bag[i] = bag[j]; bag[j] = t; }
    bagN = 7;
  }
  return bag[--bagN];
}

static bool fits(int type, int rot, int x, int y) {
  for (int k = 0; k < 4; k++) {
    int cx = x + cells[type][rot][k][0], cy = y + cells[type][rot][k][1];
    if (cx < 0 || cx >= COLS || cy >= ROWS) return false;
    if (cy >= 0 && board[cy][cx]) return false;
  }
  return true;
}

static void gameOver() {
  state = ST_OVER; overT = 0;
  audio::music(SONG_OVER);
  audio::play(SFX_PLAYER_DIE);
  input::rumble(400, 0xA0, 0xA0);
  if (newHi) arcade::saveHi(hiscore);
}

static void spawn(int type) {
  cur.type = type; cur.rot = 0; cur.x = 3; cur.y = HIDDEN - 1 - (type == P_I ? 1 : 0);
  gravT = 0; lockT = 0; lockResets = 0;
  if (!fits(cur.type, cur.rot, cur.x, cur.y)) gameOver();
}

static void nextPiece() {
  int t = queue[0];
  queue[0] = queue[1]; queue[1] = queue[2]; queue[2] = nextFromBag();
  spawn(t);
  holdUsed = false;
}

static void resetGame() {
  memset(board, 0, sizeof(board));
  memset(parts, 0, sizeof(parts));
  bagN = 0;
  for (auto& q : queue) q = nextFromBag();
  hold = -1; score = 0; level = 1; lines = 0; combo = -1; newHi = false; nClear = 0;
  dangerSong = 0;
  state = ST_PLAY;
  snprintf(banner, sizeof(banner), "LEVEL 1"); bannerT = 90;
  nextPiece();
}

static int gravityFrames() {
  static const uint8_t G[] = {48, 43, 38, 33, 28, 23, 18, 13, 9, 7, 6, 5, 5, 4, 4, 4, 3, 3, 3, 2};
  return level <= 20 ? G[level - 1] : 1;
}

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}

static int ghostY() {
  int y = cur.y;
  while (fits(cur.type, cur.rot, cur.x, y + 1)) y++;
  return y;
}

static void lockPiece() {
  for (int k = 0; k < 4; k++) {
    int cx = cur.x + cells[cur.type][cur.rot][k][0], cy = cur.y + cells[cur.type][cur.rot][k][1];
    if (cy >= 0) board[cy][cx] = cur.type + 1;
  }
  // entirely above the visible area -> game over
  bool visible = false;
  for (int k = 0; k < 4; k++) if (cur.y + cells[cur.type][cur.rot][k][1] >= HIDDEN) visible = true;
  if (!visible) { gameOver(); return; }

  nClear = 0;
  for (int r = 0; r < ROWS; r++) {
    bool full = true;
    for (int c = 0; c < COLS; c++) if (!board[r][c]) { full = false; break; }
    if (full) clearRows[nClear++] = r;
  }
  if (nClear) {
    static const int PTS[5] = {0, 100, 300, 500, 800};
    combo++;
    addScore(PTS[nClear] * level + (combo > 0 ? 50 * combo * level : 0));
    static const char* NAMES[5] = {"", "", "DOUBLE!", "TRIPLE!", "QUAD!"};
    if (nClear >= 2) { strcpy(banner, NAMES[nClear]); bannerT = 60; }
    else if (combo > 0) { snprintf(banner, sizeof(banner), "COMBO x%d", combo); bannerT = 50; }
    audio::play(SFX_LINE, nClear);
    if (nClear == 4) { shake = 6; input::rumble(250, 0x80, 0x60); }
    for (int i = 0; i < nClear; i++)
      for (int c = 0; c < COLS; c++)
        for (int k = 0; k < 3; k++)
          spark(BX + c * CELL + 5, BY + (clearRows[i] - HIDDEN) * CELL + 5, frange(-2.5f, 2.5f), frange(-3.0f, 0.5f),
                (int)frange(20, 40), board[clearRows[i]][c]);
    state = ST_CLEAR; clearT = 0;
  } else {
    combo = -1;
    audio::play(SFX_LAND);
    nextPiece();
  }
}

static void collapseRows() {
  for (int i = 0; i < nClear; i++) {
    int r = clearRows[i];
    for (int y = r; y > 0; y--) memcpy(board[y], board[y - 1], COLS);
    memset(board[0], 0, COLS);
  }
  int before = lines / 10;
  lines += nClear;
  if (lines / 10 > before) {
    level++;
    snprintf(banner, sizeof(banner), "LEVEL %d", level); bannerT = 90;
    audio::play(SFX_POWERUP);
  }
  nClear = 0;
  state = ST_PLAY;
  nextPiece();
}

static bool tryMove(int dx, int dy) {
  if (!fits(cur.type, cur.rot, cur.x + dx, cur.y + dy)) return false;
  cur.x += dx; cur.y += dy;
  return true;
}

static void touchedGround() {   // reset lock delay after a successful move while grounded
  if (!fits(cur.type, cur.rot, cur.x, cur.y + 1) && lockResets < 15) { lockT = 0; lockResets++; }
}

static void rotate(int dir) {   // dir +1 CW, -1 CCW
  if (cur.type == P_O) return;
  int to = (cur.rot + (dir > 0 ? 1 : 3)) & 3;
  static const int CW_IDX[4] = {0, 2, 4, 6}, CCW_IDX[4] = {7, 1, 3, 5};
  int idx = dir > 0 ? CW_IDX[cur.rot] : CCW_IDX[cur.rot];
  const int8_t (*kicks)[2] = cur.type == P_I ? KICK_I[idx] : KICK_JLSTZ[idx];
  for (int k = 0; k < 5; k++) {
    int nx = cur.x + kicks[k][0], ny = cur.y + kicks[k][1];
    if (fits(cur.type, to, nx, ny)) {
      cur.x = nx; cur.y = ny; cur.rot = to;
      audio::play(SFX_ROTATE);
      touchedGround();
      return;
    }
  }
}

static int stackHeight() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) if (board[r][c]) return ROWS - r;
  return 0;
}

// ------------------------------------------------------------ update
static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += 0.12f; p.vx *= 0.98f;
    if (--p.life == 0 || p.y > SH) p.on = false;
  }
  for (auto& d : dust) {
    d.y += d.v;
    if (d.y > SH) { d.y = -4; d.x = rnd() % SW; }
  }
  if (bannerT) bannerT--;
  if (shake > 0.3f) { shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { arcade::pause(); return; }
  // hold
  if (in.hit(BTN_X | BTN_Y | BTN_L1 | BTN_R1) && !holdUsed) {
    int t = cur.type;
    if (hold < 0) { hold = t; nextPiece(); }
    else { int h = hold; hold = t; spawn(h); }
    holdUsed = true;
    audio::play(SFX_BLIP);
    return;
  }
  if (in.hit(BTN_A)) rotate(+1);
  if (in.hit(BTN_B)) rotate(-1);
  if (repL.tick(in.down(BTN_LEFT), 10, 2) && tryMove(-1, 0)) { audio::play(SFX_MOVE); touchedGround(); }
  if (repR.tick(in.down(BTN_RIGHT), 10, 2) && tryMove(1, 0)) { audio::play(SFX_MOVE); touchedGround(); }
  if (in.hit(BTN_UP)) {   // hard drop
    int d = 0;
    int x0 = cur.x;
    while (tryMove(0, 1)) d++;
    addScore(d * 2);
    for (int k = 0; k < 4; k++) {
      int cx = BX + (x0 + cells[cur.type][cur.rot][k][0]) * CELL;
      for (int j = 0; j < 3; j++)
        spark(cx + frange(0, CELL), BY + (cur.y + cells[cur.type][cur.rot][k][1] - HIDDEN) * CELL, 0, frange(-2, -0.5f), 14, 9);
    }
    shake = max(shake, 2.0f);
    lockPiece();
    return;
  }
  bool soft = in.down(BTN_DOWN);
  int g = soft ? min(2, gravityFrames()) : gravityFrames();
  if (++gravT >= g) {
    gravT = 0;
    if (tryMove(0, 1)) { if (soft) addScore(1); lockT = 0; }
  }
  if (!fits(cur.type, cur.rot, cur.x, cur.y + 1)) {
    if (++lockT >= 30) lockPiece();
  }
  // music intensity follows the stack
  int want = stackHeight() > 14 ? 1 : 0;
  if (want != dangerSong) { dangerSong = want; audio::music(want ? SONG_FAST : SONG_GAME); }
}

static void step(const Pad& in) {
  updateFx();
  switch (state) {
    case ST_TITLE:
      for (auto& d : demo) {
        d.y += d.v;
        if (d.y > SH + 10) { d.y = -40 - (rnd() % 60); d.x = rnd() % (SW - 40); d.type = rnd() % 7; d.rot = rnd() % 4; }
      }
      if (in.hit(BTN_LEFT)) arcade::changeVolume(-1);
      if (in.hit(BTN_RIGHT)) arcade::changeVolume(+1);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); audio::play(SFX_START); audio::music(SONG_GAME); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_CLEAR:
      if (++clearT >= 18) collapseRows();
      break;
    case ST_OVER:
      overT++;
      // grey-out animation, bottom to top
      if (overT < ROWS * 3 && overT % 3 == 0) {
        int r = ROWS - 1 - overT / 3;
        for (int c = 0; c < COLS; c++) if (board[r][c]) board[r][c] = 8;
      }
      if (overT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ draw
static uint16_t bgRow[SH];
static void buildBackground() {
  for (int y = 0; y < SH; y++) bgRow[y] = lerpS(8, 10, 30, 30, 12, 50, (float)y / SH);
  for (auto& d : dust) { d.x = rnd() % SW; d.y = rnd() % SH; d.v = frange(0.2f, 0.8f); d.s = d.v > 0.6f ? 2 : 1; }
  for (int i = 0; i < 7; i++) demo[i] = {-(float)(rnd() % 240), (int)(rnd() % (SW - 40)), (int)(rnd() % 7), (int)(rnd() % 4), frange(0.3f, 0.9f)};
}

static void drawBackground() {
  for (int r = 0; r < STRIP; r++) {
    uint16_t c = bgRow[Y0 + r];
    uint16_t* d = B + r * SW;
    for (int x = 0; x < SW; x++) d[x] = c;
  }
  uint16_t dc1 = rgbS(50, 45, 100), dc2 = rgbS(90, 80, 160);
  for (auto& d : dust) {
    int x = (int)d.x, y = (int)d.y;
    if (d.s > 1) { rectf(x, y, 2, 2, dc2); } else pset(x, y, dc1);
  }
}

static void drawPieceMini(int type, int cx, int cy, bool big) {
  int S = big ? 10 : 8;
  // centre the piece in its box
  int minx = 9, maxx = -9, miny = 9, maxy = -9;
  for (int k = 0; k < 4; k++) {
    minx = min(minx, (int)cells[type][0][k][0]); maxx = max(maxx, (int)cells[type][0][k][0]);
    miny = min(miny, (int)cells[type][0][k][1]); maxy = max(maxy, (int)cells[type][0][k][1]);
  }
  int w = (maxx - minx + 1) * S, h = (maxy - miny + 1) * S;
  for (int k = 0; k < 4; k++) {
    int x = cx - w / 2 + (cells[type][0][k][0] - minx) * S;
    int y = cy - h / 2 + (cells[type][0][k][1] - miny) * S;
    if (big) drawTile(tiles[type + 1], CELL, x, y);
    else drawTile(mini[type + 1], 8, x, y);
  }
}

static void box(int x, int y, int w, int h, const char* label) {
  if (rowsVisible(y, h)) {
    shade(x, y, w, h);
    rect(x, y, w, h, rgbS(70, 60, 140));
    rectf(x, y, w, 11, rgbS(40, 30, 90));
  }
  text(label, x + w / 2, y + 2, c565(184, 243, 255), 1, top_center, false);
}

static void drawBoard() {
  int ox = BX + shakeX, oy = BY + shakeY;
  // well
  if (rowsVisible(oy - 3, 20 * CELL + 6)) {
    shade(ox, oy, COLS * CELL, 20 * CELL);
    shade(ox, oy, COLS * CELL, 20 * CELL);
    uint16_t glow = lerpS(62, 198, 224, 170, 80, 225, 0.5f + 0.5f * sinf(frameNo * 0.03f));
    rect(ox - 2, oy - 2, COLS * CELL + 4, 20 * CELL + 4, glow);
    rect(ox - 3, oy - 3, COLS * CELL + 6, 20 * CELL + 6, rgbS(30, 25, 70));
    uint16_t dot = rgbS(40, 40, 80);
    for (int r = 1; r < 20; r++)
      for (int c = 1; c < COLS; c++) pset(ox + c * CELL - 1, oy + r * CELL - 1, dot);
  }
  // settled blocks
  for (int r = HIDDEN; r < ROWS; r++) {
    int y = oy + (r - HIDDEN) * CELL;
    if (!rowsVisible(y, CELL)) continue;
    bool flash = false;
    if (state == ST_CLEAR) for (int i = 0; i < nClear; i++) if (clearRows[i] == r) flash = true;
    for (int c = 0; c < COLS; c++) {
      uint8_t v = board[r][c];
      if (!v) continue;
      if (flash) {
        if ((clearT & 4) == 0) drawTile(tiles[9], CELL, ox + c * CELL, y);
        else {
          // shrink towards the centre
          int s = CELL - clearT / 2;
          if (s > 0) rectf(ox + c * CELL + (CELL - s) / 2, y + (CELL - s) / 2, s, s, rgbS(255, 255, 255));
        }
      } else drawTile(tiles[v], CELL, ox + c * CELL, y);
    }
  }
  if (state == ST_PLAY) {
    // ghost
    int gy = ghostY();
    for (int k = 0; k < 4; k++) {
      int cx = cur.x + cells[cur.type][cur.rot][k][0], cy = gy + cells[cur.type][cur.rot][k][1];
      if (cy < HIDDEN) continue;
      int x = ox + cx * CELL, y = oy + (cy - HIDDEN) * CELL;
      rect(x, y, CELL - 1, CELL - 1, ghostCol[cur.type + 1]);
    }
    // active piece
    for (int k = 0; k < 4; k++) {
      int cx = cur.x + cells[cur.type][cur.rot][k][0], cy = cur.y + cells[cur.type][cur.rot][k][1];
      if (cy < HIDDEN) continue;
      drawTile(tiles[cur.type + 1], CELL, ox + cx * CELL, oy + (cy - HIDDEN) * CELL);
    }
  }
}

static void drawPanels() {
  box(10, 10, 84, 52, "HOLD");
  if (hold >= 0) drawPieceMini(hold, 52, 40, true);
  box(10, 72, 84, 150, "SCORE");
  textf(52, 88, TFT_WHITE, 1, top_center, "%07lu", (unsigned long)score);
  text("LEVEL", 52, 108, c565(184, 243, 255), 1, top_center, false);
  textf(52, 120, TFT_WHITE, 2, top_center, "%d", level);
  text("LINES", 52, 144, c565(184, 243, 255), 1, top_center, false);
  textf(52, 156, TFT_WHITE, 2, top_center, "%d", lines);
  text("HI", 52, 182, c565(255, 216, 74), 1, top_center, false);
  textf(52, 194, c565(255, 216, 74), 1, top_center, "%07lu", (unsigned long)hiscore);

  box(226, 10, 84, 128, "NEXT");
  drawPieceMini(queue[0], 268, 42, true);
  drawPieceMini(queue[1], 268, 82, false);
  drawPieceMini(queue[2], 268, 114, false);
  box(226, 148, 84, 74, "KEYS");
  text("\x1e DROP  \x1f SOFT", 268, 164, c565(150, 140, 200), 1, top_center, false);
  text("A/B ROTATE", 268, 178, c565(150, 140, 200), 1, top_center, false);
  text("X/Y HOLD", 268, 192, c565(150, 140, 200), 1, top_center, false);
  text("START PAUSE", 268, 206, c565(150, 140, 200), 1, top_center, false);
}

static void drawParticles() {
  for (auto& p : parts) {
    if (!p.on) continue;
    uint8_t c = p.col;
    float t = (float)p.life / p.max;
    uint16_t col = lerpS(255, 255, 255, PCOL[c][0], PCOL[c][1], PCOL[c][2], 1 - t);
    int x = (int)p.x, y = (int)p.y;
    pset(x, y, col);
    if (t > 0.5f) { pset(x + 1, y, col); pset(x, y + 1, col); pset(x + 1, y + 1, col); }
  }
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    for (auto& d : demo)
      for (int k = 0; k < 4; k++) {
        int x = d.x + cells[d.type][d.rot][k][0] * CELL, y = (int)d.y + cells[d.type][d.rot][k][1] * CELL;
        if (rowsVisible(y, CELL)) { drawTile(tiles[d.type + 1], CELL, x, y); shade(x, y, CELL, CELL); }
      }
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 46);
    text("A FALLING BLOCKS PUZZLER", SW / 2, 88, c565(184, 243, 255), 1, top_center);
    bool blink = (frameNo >> 4) & 1;
    if (input::pad.connected) { if (blink) text("PRESS START", SW / 2, 128, TFT_WHITE, 2, top_center); }
    else text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 130, c565(255, 138, 61), 1, top_center);
    textf(SW / 2, 22, c565(255, 216, 74), 1, top_center, "HI-SCORE %07lu", (unsigned long)hiscore);
    text("\x11\x10 MOVE  \x1e DROP  \x1f SOFT DROP", SW / 2, 176, c565(180, 194, 220), 1, top_center);
    text("A/B ROTATE   X/Y HOLD   START PAUSE", SW / 2, 190, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 210, c565(115, 132, 168), 1, top_center);
    return;
  }
  drawPanels();
  drawBoard();
  drawParticles();
  if (bannerT && state != ST_OVER) {
    int y = 100;
    int c = (bannerT >> 2) & 1;
    text(banner, BX + COLS * CELL / 2, y, c ? c565(255, 216, 74) : TFT_WHITE, 2, top_center);
  }
  if (state == ST_OVER && overT > 40) {
    if (rowsVisible(80, 80)) { shade(BX - 4, 80, COLS * CELL + 8, 80); shade(BX - 4, 80, COLS * CELL + 8, 80); }
    text("GAME", BX + 55, 88, c565(235, 60, 80), 3, top_center);
    text("OVER", BX + 55, 114, c565(235, 60, 80), 3, top_center);
    if (newHi && ((frameNo >> 4) & 1)) text("NEW HI-SCORE!", BX + 55, 142, c565(255, 216, 74), 1, top_center);
    if (overT > 90) text("PRESS START", BX + 55, 152, TFT_WHITE, 1, top_center);
  }
}

void setup() {
  arcade::begin("blockfall", &MUSIC);
  setPalette(LOGO_PAL565, 25);
  buildTiles();
  buildBackground();
  hiscore = arcade::loadHi(10000);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
