#include "../../games/BrickStorm/game.h"
#include "sim_driver.h"
int main() {
  setup();
  for (int i = 0; i < 60; i++) simStep(step);
  simShot(draw, "out/bs_title.ppm");
  simStep(step, 0, 0, BTN_START);
  int maxStage = 0;
  for (int f = 0; f < 60 * 240 && state != ST_OVER; f++) {
    // AI: track the lowest descending ball, aim slightly off-centre
    float tx = padX; float lowest = -1;
    for (auto& b : balls) if (b.on && b.vy > 0 && b.y > lowest) { lowest = b.y; tx = b.x + ((f / 200) % 2 ? 8 : -8); }
    for (auto& c : caps) if (c.on && lowest < 150 && c.y > 170) tx = c.x + 8;
    float ax = constrain((tx - padX) / 8.0f, -1.0f, 1.0f);
    uint32_t btn = (f % 30 == 0) ? BTN_A : 0;
    simStep(step, ax, 0, btn);
    if (f == 700) simShot(draw, "out/bs_play.ppm");
    if (stage > maxStage) { maxStage = stage; if (stage == 3) simShot(draw, "out/bs_stage4.ppm"); }
    if (state == ST_CLEAR && stateT == 20) simShot(draw, "out/bs_clear.ppm");
  }
  printf("stage=%d lives=%d score=%u state=%d bricksLeft=%d\n", stage, lives, score, state, bricksLeft);
}
