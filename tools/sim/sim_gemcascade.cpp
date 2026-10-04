#include "../../games/GemCascade/game.h"
#include "sim_driver.h"
// Bot: picks the swap that clears the most gems, walks the cursor there, presses A and a direction.
static int tr = -1, tc = -1, tdr = 0, tdc = 0;
static int scoreSwap(int r0, int c0, int r1, int c1) {
  if (!swapWorks(r0, c0, r1, c1)) return -1;
  if (board[r0][c0].type == T_NOVA || board[r1][c1].type == T_NOVA) return 50;
  swapCells(r0, c0, r1, c1); findRuns(); int n = 0; for (int i = 0; i < nruns; i++) n += runs[i].len * runs[i].len; swapCells(r0, c0, r1, c1);
  return n + (r0 > 4 ? 2 : 0);
}
static void plan() {
  int best = -1;
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
    int s;
    if (c + 1 < N && (s = scoreSwap(r, c, r, c + 1)) > best) { best = s; tr = r; tc = c; tdr = 0; tdc = 1; }
    if (r + 1 < N && (s = scoreSwap(r, c, r + 1, c)) > best) { best = s; tr = r; tc = c; tdr = 1; tdc = 0; }
  }
}
int main() {
  setup();
  for (int i = 0; i < 120; i++) simStep(step);
  simShot(draw, "out/gc_title.ppm");
  simStep(step, 0, 0, BTN_START);
  bool shotSpecial = false, shotLevel = false, shotCascade = false;
  int moves = 0;
  for (int f = 0; f < 60 * 60 * 8 && state == ST_PLAY; f++) {
    uint32_t b = 0; float ax = 0, ay = 0;
    if (phase == PH_IDLE && (f & 3) == 0) {
      if (tr < 0) plan();
      if (tr >= 0) {
        if (curR != tr || curC != tc) { if (selected) b = BTN_B; else if (curR < tr) ay = 1; else if (curR > tr) ay = -1; else if (curC < tc) ax = 1; else ax = -1; }
        else if (!selected) b = BTN_A;
        else { ax = tdc; ay = tdr; tr = -1; moves++; }
      }
    }
    simStep(step, ax, ay, b);
    if (f == 900) simShot(draw, "out/gc_play.ppm");
    if (!shotSpecial && phase == PH_IDLE) {
      int sp = 0; for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) sp += board[r][c].sp != SP_NONE;
      if (sp >= 2) { simShot(draw, "out/gc_special.ppm"); shotSpecial = true; }
    }
    if (!shotCascade && combo >= 2 && phase == PH_CLEAR && phaseT == 4) { for (auto& p : popups) if (p.on) printf("popup %s y=%.1f t=%d\n", p.txt, p.y, p.t); simShot(draw, "out/gc_cascade.ppm"); shotCascade = true; }
    if (!shotLevel && phase == PH_LEVELUP && phaseT == 70) { simShot(draw, "out/gc_level.ppm"); shotLevel = true; }
  }
  printf("level=%d score=%u moves=%d bestCombo=%d state=%d bar=%.2f\n", level, score, moves, bestCombo, state, bar);
  // let the clock run out
  for (int f = 0; f < 60 * 120 && state == ST_PLAY; f++) simStep(step);
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/gc_over.ppm");
  printf("final state=%d\n", state);
}
