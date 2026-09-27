#include "../../games/MazeMunch/game.h"
#include "sim_driver.h"
#include <queue>
// AI: BFS to the nearest dot, avoiding tiles near hunting wisps
static int aiDir() {
  int sc = tileOf(pl.x), sr = tileOf(pl.y);
  if (sc < 0 || sc >= MCOLS) return pl.dir;
  static int prev[MROWS * MCOLS];
  for (auto& p : prev) p = -1;
  auto danger = [&](int c, int r) {
    for (auto& w : wisps) if (w.mode == G_ACTIVE && !w.scared && abs(tileOf(w.a.x) - c) + abs(tileOf(w.a.y) - r) <= 2) return true;
    return false;
  };
  std::queue<int> q; q.push(sr * MCOLS + sc); prev[sr * MCOLS + sc] = sr * MCOLS + sc;
  int goal = -1;
  while (!q.empty()) {
    int v = q.front(); q.pop();
    int c = v % MCOLS, r = v / MCOLS;
    if (dots[r][c] && v != sr * MCOLS + sc) { goal = v; break; }
    for (int d = 0; d < 4; d++) {
      int nc = c + DX[d], nr = r + DY[d];
      if (nc < 0 || nc >= MCOLS || nr < 0 || nr >= MROWS || wallAt(nc, nr, false) || prev[nr * MCOLS + nc] >= 0 || danger(nc, nr)) continue;
      prev[nr * MCOLS + nc] = v; q.push(nr * MCOLS + nc);
    }
  }
  if (goal < 0) return (pl.dir + 2) % 4;
  int v = goal;
  while (prev[v] != sr * MCOLS + sc) v = prev[v];
  for (int d = 0; d < 4; d++) if (v == (sr + DY[d]) * MCOLS + sc + DX[d]) return d;
  return pl.dir;
}
int main() {
  setup();
  for (int i = 0; i < 90; i++) simStep(step);
  simShot(draw, "out/mm_title.ppm");
  simStep(step, 0, 0, BTN_START);
  static const uint32_t DB[4] = {BTN_UP, BTN_LEFT, BTN_DOWN, BTN_RIGHT};
  bool shotPlay = false, shotFright = false;
  int d = 1;
  for (int f = 0; f < 60 * 400 && state != ST_OVER; f++) {
    if (state == ST_PLAY && atCentre(pl)) d = aiDir();
    simStep(step, 0, 0, state == ST_PLAY ? DB[d] : 0);
    if (!shotPlay && state == ST_PLAY && dotsEaten > 60) { simShot(draw, "out/mm_play.ppm"); shotPlay = true; }
    if (!shotFright && frightT > 200 && frightT < 260) { simShot(draw, "out/mm_fright.ppm"); shotFright = true; }
    if (state == ST_CLEAR && stateT == 60) simShot(draw, "out/mm_clear.ppm");
  }
  printf("level=%d dotsLeft=%d lives=%d score=%u state=%d\n", level, dotsLeft, lives, score, state);
  while (state != ST_OVER) { lives = 1; if (state == ST_PLAY) loseLife(); simStep(step); }
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/mm_over.ppm");
}
