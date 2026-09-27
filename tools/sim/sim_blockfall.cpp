#include "../../games/Blockfall/Blockfall.ino"
#include "sim_driver.h"
// simple placement AI for the simulation
static double evalBoard() {
  int h[COLS] = {0}, holes = 0, agg = 0, bump = 0, full = 0;
  for (int c = 0; c < COLS; c++) { bool seen = false; for (int r = 0; r < ROWS; r++) { if (board[r][c]) { if (!seen) h[c] = ROWS - r; seen = true; } else if (seen) holes++; } }
  for (int r = 0; r < ROWS; r++) { bool f = true; for (int c = 0; c < COLS; c++) if (!board[r][c]) f = false; if (f) full++; }
  for (int c = 0; c < COLS; c++) { agg += h[c]; if (c) bump += abs(h[c] - h[c - 1]); }
  return -0.51 * agg + 0.76 * full - 0.36 * holes - 0.18 * bump;
}
static void aiPlace() {
  double best = -1e9; int br = 0, bx = cur.x;
  uint8_t save[ROWS][COLS];
  for (int r = 0; r < 4; r++) for (int x = -2; x < COLS; x++) {
    if (!fits(cur.type, r, x, cur.y)) continue;
    int y = cur.y; while (fits(cur.type, r, x, y + 1)) y++;
    memcpy(save, board, sizeof(board));
    for (int k = 0; k < 4; k++) { int cy = y + cells[cur.type][r][k][1]; if (cy >= 0) board[cy][x + cells[cur.type][r][k][0]] = 1; }
    double v = evalBoard();
    memcpy(board, save, sizeof(board));
    if (v > best) { best = v; br = r; bx = x; }
  }
  cur.rot = br; cur.x = bx;
}
int main() {
  setup();
  for (int i = 0; i < 60; i++) simStep(step);
  simShot(draw, "out/bf_title.ppm");
  simStep(step, 0, 0, BTN_START);
  int pieces = 0;
  for (int f = 0; f < 6000 && state != ST_OVER; f++) {
    if (state == ST_PLAY && cur.y <= HIDDEN && (f % 2 == 0)) { aiPlace(); simStep(step, 0, 0, BTN_UP); pieces++; }
    else simStep(step);
    if (f == 1500) simShot(draw, "out/bf_play.ppm");
    if (state == ST_CLEAR && nClear >= 2 && clearT == 4) simShot(draw, "out/bf_clear.ppm");
  }
  printf("pieces=%d lines=%d level=%d score=%u state=%d\n", pieces, lines, level, score, state);
  // force a game over for the screenshot
  for (int r = 4; r < ROWS; r++) for (int c = 0; c < COLS; c++) if ((r + c) % 3) board[r][c] = 1 + (r + c) % 7;
  gameOver();
  for (int i = 0; i < 120; i++) simStep(step);
  simShot(draw, "out/bf_over.ppm");
}
