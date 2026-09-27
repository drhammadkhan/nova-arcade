#include "../../games/AstroDrift/game.h"
#include "sim_driver.h"
int main() {
  setup();
  for (int i = 0; i < 90; i++) simStep(step);
  simShot(draw, "out/ad_title.ppm");
  simStep(step, 0, 0, BTN_START);
  int maxWave = 1;
  for (int f = 0; f < 60 * 240 && state != ST_OVER; f++) {
    // AI: turn towards the nearest rock (or saucer) and fire when lined up
    float best = 1e9, ta = ship.a;
    for (auto& r : rocks) if (r.on) { float dx = wdx(ship.x, r.x), dy = wdy(ship.y, r.y), d = dx * dx + dy * dy; if (d < best) { best = d; ta = atan2f(dy, dx); } }
    if (ufo.on) ta = atan2f(wdy(ship.y, ufo.y), wdx(ship.x, ufo.x));
    float da = remainderf(ta - ship.a, 6.2832f);
    float ax = da > 0.05f ? 1 : da < -0.05f ? -1 : 0;
    uint32_t b = (fabsf(da) < 0.2f && (f % 6) == 0) ? BTN_A : 0;
    if (best < 45 * 45 && (f % 90) < 20) b |= BTN_UP;   // dodge a little
    simStep(step, ax, 0, b);
    if (f == 600) simShot(draw, "out/ad_play.ppm");
    if (ufo.on && ufo.x > 60 && ufo.x < 260 && maxWave < 100) { simShot(draw, "out/ad_ufo.ppm"); maxWave = 100; }
    if (state == ST_CLEAR && stateT == 20 && wave == 1) simShot(draw, "out/ad_clear.ppm");
  }
  printf("wave=%d lives=%d score=%u state=%d\n", wave, lives, score, state);
  while (state != ST_OVER) { lives = 1; ship.alive = true; ship.invuln = 0; killShip(); }
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/ad_over.ppm");
}
