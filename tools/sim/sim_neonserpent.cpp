#include "../../games/NeonSerpent/game.h"
#include "sim_driver.h"
#include <queue>
// BFS towards the food; falls back to any free neighbour
static int aiDir() {
  int hc = cellC(body[0]), hr = cellR(body[0]);
  static int prev[GROWS * GCOLS];
  for (auto& p : prev) p = -1;
  std::queue<int> q; q.push(hr * GCOLS + hc); prev[hr * GCOLS + hc] = hr * GCOLS + hc;
  int target = foodR * GCOLS + foodC;
  while (!q.empty()) {
    int v = q.front(); q.pop();
    if (v == target) break;
    for (int d = 0; d < 4; d++) {
      int c = v % GCOLS + DX[d], r = v / GCOLS + DY[d];
      if (c < 0 || c >= GCOLS || r < 0 || r >= GROWS || prev[r * GCOLS + c] >= 0 || occupied(c, r, true)) continue;
      prev[r * GCOLS + c] = v; q.push(r * GCOLS + c);
    }
  }
  if (prev[target] >= 0) {
    int v = target;
    while (prev[v] != hr * GCOLS + hc) v = prev[v];
    for (int d = 0; d < 4; d++) if (v == (hr + DY[d]) * GCOLS + hc + DX[d]) return d;
  }
  for (int d = 0; d < 4; d++) { int c = hc + DX[d], r = hr + DY[d]; if (c >= 0 && c < GCOLS && r >= 0 && r < GROWS && !occupied(c, r, true)) return d; }
  return dir;
}
int main() {
  setup();
  for (int i = 0; i < 90; i++) simStep(step);
  simShot(draw, "out/ns_title.ppm");
  // difficulty selector: down twice = hard, up = normal
  simStep(step, 0, 0, BTN_DOWN); simStep(step); simStep(step, 0, 0, BTN_DOWN); simStep(step);
  printf("difficulty after 2x down: %s\n", arcade::difficultyName());
  simStep(step, 0, 0, BTN_UP); simStep(step);
  simStep(step, 0, 0, BTN_START);
  int maxLevel = 1, shots = 0;
  static const uint32_t DB[4] = {BTN_RIGHT, BTN_DOWN, BTN_LEFT, BTN_UP};
  for (int f = 0; f < 60 * 300 && state != ST_OVER; f++) {
    uint32_t b = 0;
    if (state == ST_PLAY && moveT == 0 && nQueued == 0) { int d = aiDir(); if (d != dir) b = DB[d]; }
    simStep(step, 0, 0, b);
    if (f == 900) simShot(draw, "out/ns_play.ppm");
    if (level > maxLevel && state == ST_PLAY && stateT == 200) { maxLevel = level; if (level == 3 && !shots++) simShot(draw, "out/ns_level3.ppm"); }
    if (state == ST_CLEAR && stateT == 30 && level == 2) simShot(draw, "out/ns_clear.ppm");
  }
  printf("level=%d len=%d lives=%d score=%u state=%d\n", level, len, lives, score, state);
  simStep(step, 0, 0, BTN_START); for (int i = 0; i < 30; i++) simStep(step);
  simShot(draw, "out/ns_pause.ppm");
  if (state != ST_OVER) { lives = 1; while (state != ST_OVER) { die(); } }
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/ns_over.ppm");
}
