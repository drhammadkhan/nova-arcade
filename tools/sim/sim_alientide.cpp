#include "../../games/AlienTide/game.h"
#include "sim_driver.h"
int main() {
  setup();
  for (int i = 0; i < 60; i++) simStep(step);
  simShot(draw, "out/at_title.ppm");
  simStep(step, 0, 0, BTN_START);
  int maxWave = 1;
  for (int f = 0; f < 60 * 300 && state != ST_OVER; f++) {
    // AI: dodge shots overhead, otherwise hunt the nearest column with aliens
    float target = px;
    int minC, maxC, maxR; formationBounds(minC, maxC, maxR);
    if (maxC >= 0) { int c = minC + (f / 90) % (maxC - minC + 1); target = fx + c * CW; }
    for (auto& s : eshots) if (s.on && s.y > 130 && fabsf(s.x - (px + 9)) < 14) target = px + (s.x < px + 9 ? 30 : -30);
    float ax = constrain((target - px) / 10.0f, -1.0f, 1.0f);
    simStep(step, ax, 0, (f % 8 == 0) ? BTN_A : 0);
    if (f == 900) simShot(draw, "out/at_play.ppm");
    if (wave > maxWave) { maxWave = wave; if (wave == 2 && stateT == 200) {} }
    if (state == ST_PLAY && wave == 2 && stateT == 600) simShot(draw, "out/at_wave2.ppm");
    if (ufoDir && ufoX > 120 && ufoX < 125) simShot(draw, "out/at_ufo.ppm");
  }
  printf("wave=%d lives=%d score=%u state=%d alive=%d\n", wave, lives, score, state, nAlive);
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/at_over.ppm");
}
