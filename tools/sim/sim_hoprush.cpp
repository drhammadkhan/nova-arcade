#include "../../games/HopRush/game.h"
#include "sim_driver.h"
// AI: look a few frames ahead and hop up when the next row will be safe
static bool safeAt(int row, float x, int ahead) {
  float save[NLANES]; memcpy(save, laneOff, sizeof(save));
  for (int i = 0; i < NLANES; i++) laneOff[i] += laneSpd[i] * ahead;
  bool ok;
  if (row == 0) { ok = false; for (int i = 0; i < NDOCK; i++) if (!docks[i] && x + 8 >= dockX(i) + 4 && x + 8 <= dockX(i) + 28) ok = true; }
  else if (isRiver(row)) { int li = platformUnder(x + 8, row); ok = li >= 0; if (ok) { float nx = x + laneSpd[li] * 10; ok = nx > 8 && nx < SW - 24 && platformUnder(nx + 8, row) >= 0; } }
  else if (isRoad(row)) { ok = true; for (int a = -ahead; a <= 14; a += 2) { for (int i = 0; i < NLANES; i++) laneOff[i] = save[i] + laneSpd[i] * (ahead + a); if (hitByTraffic(x, row)) ok = false; } }
  else ok = true;
  memcpy(laneOff, save, sizeof(save));
  return ok;
}
int main() {
  setup();
  for (int i = 0; i < 90; i++) simStep(step);
  simShot(draw, "out/hr_title.ppm");
  simStep(step, 0, 0, BTN_START);
  bool shotPlay = false;
  for (int f = 0; f < 60 * 300 && state != ST_OVER; f++) {
    uint32_t b = 0;
    if (state == ST_PLAY && hopT == 0 && (f & 1)) {
      if (safeAt(prow - 1, px, 7)) b = BTN_UP;
      else if (prow == 1 || prow == 0) {
        // steer sideways along the top river lane towards a free dock
        int best = 99; float tx = px;
        for (int i = 0; i < NDOCK; i++) if (!docks[i] && abs(dockX(i) + 8 - (int)px) < best) { best = abs(dockX(i) + 8 - (int)px); tx = dockX(i) + 8; }
        if (tx > px + 8 && safeAt(prow, px + 16, 7)) b = BTN_RIGHT; else if (tx < px - 8 && safeAt(prow, px - 16, 7)) b = BTN_LEFT;
      } else if (isRoad(prow) && !safeAt(prow, px, 7) && safeAt(prow + 1, px, 7)) b = BTN_DOWN;
    }
    State before = state; int r0 = prow;
    simStep(step, 0, 0, b);
    if (before == ST_PLAY && state == ST_DEAD) printf("death f=%d row=%d now=%d hop=%d kind=%d px=%.1f time=%d\n", f, r0, prow, hopT, deathKind, px, timeLeft);
    if (!shotPlay && state == ST_PLAY && prow == 4) { simShot(draw, "out/hr_play.ppm"); shotPlay = true; }
    if (state == ST_CLEAR && stateT == 20) simShot(draw, "out/hr_clear.ppm");
  }
  int n = 0; for (bool d : docks) n += d;
  printf("level=%d docks=%d lives=%d score=%u state=%d\n", level, n, lives, score, state);
  while (state != ST_OVER) { lives = 1; die(0); for (int i = 0; i < 90; i++) simStep(step); }
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/hr_over.ppm");
}
