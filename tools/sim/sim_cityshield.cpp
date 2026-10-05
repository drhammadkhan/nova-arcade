#include "../../games/CityShield/game.h"
#include "sim_driver.h"
// Bot: the game's own autopilot, driven through the real fire buttons by steering the crosshair.
int main() {
  setup();
  for (int i = 0; i < 400; i++) simStep(step);
  simShot(draw, "out/cs_title.ppm");
  simStep(step, 0, 0, BTN_START);
  for (int i = 0; i < 60; i++) simStep(step);
  simShot(draw, "out/cs_intro.ppm");
  bool shotPlay = false, shotBonus = false, shotFlyer = false, shotWave3 = false;
  for (int f = 0; f < 60 * 60 * 10 && state != ST_OVER; f++) {
    float ax = 0, ay = 0; uint32_t b = 0;
    if (state == ST_PLAY) {
      // aim at the most dangerous warhead's predicted point
      Warhead* tgt = nullptr; float ty = -1;
      for (auto& w : warheads) if (w.on && !w.targeted && w.y > 30 && w.y > ty) { ty = w.y; tgt = &w; }
      if (tgt) {
        int base = tgt->x < 107 ? 0 : tgt->x > 213 ? 2 : 1;
        float px = tgt->x, py = tgt->y;
        for (int it = 0; it < 3; it++) { float t = hypotf(px - BASE_X[base], py - LAUNCH_Y) / SHOT_SPEED[base] + 8; px = tgt->x + tgt->vx * t; py = tgt->y + tgt->vy * t; }
        float dx = px - crossX, dy = py - crossY;
        ax = constrain(dx / 3.6f, -1.0f, 1.0f); ay = constrain(dy / 3.6f, -1.0f, 1.0f);
        if (fabsf(dx) < 4 && fabsf(dy) < 4 && py < 185) { b = base == 0 ? BTN_X : base == 1 ? BTN_A : BTN_B; tgt->targeted = true; }
      }
    }
    simStep(step, ax, ay, b);
    if (!shotPlay && state == ST_PLAY && stateT == 600) { simShot(draw, "out/cs_play.ppm"); shotPlay = true; }
    if (!shotFlyer && state == ST_PLAY && flyers[0].on && flyers[0].x > 80 && flyers[0].x < 240) { simShot(draw, "out/cs_flyer.ppm"); shotFlyer = true; }
    if (!shotBonus && state == ST_BONUS && stateT == 200) { simShot(draw, "out/cs_bonus.ppm"); shotBonus = true; }
    if (!shotWave3 && wave == 3 && state == ST_PLAY && stateT == 900) { simShot(draw, "out/cs_wave3.ppm"); shotWave3 = true; }
    if (state == ST_INTRO && stateT == 1) printf("wave %d cities=%d score=%u\n", wave, citiesLeft(), score);
  }
  printf("end wave=%d score=%u state=%d\n", wave, score, state);
  while (state != ST_OVER) { for (bool& c : cityAlive) c = false; bonusCities = 0; simStep(step); }
  for (int i = 0; i < 200; i++) simStep(step);
  simShot(draw, "out/cs_over.ppm");
}
