#include "../../games/VoltRally/game.h"
#include "sim_driver.h"
int main() {
  setup();
  for (int i = 0; i < 90; i++) simStep(step);
  simShot(draw, "out/vr_title.ppm");
  simStep(step, 0, 0, BTN_START);
  bool shot = false;
  for (int f = 0; f < 60 * 400 && state != ST_OVER; f++) {
    // AI: follow the ball with a little lag, smash now and then
    float target = (bvx < 0 ? by + BS / 2 : (CT + CB) / 2) - PH / 2 + ((f / 300) % 3 - 1) * 10;
    float ay = constrain((target - py) / 10.0f, -1.0f, 1.0f);
    uint32_t b = (bx < 60 && (f / 120) % 2) ? BTN_A : 0;
    simStep(step, 0, ay, b);
    if (!shot && state == ST_PLAY && rally >= 3 && bx > 100 && bx < 200) { simShot(draw, "out/vr_play.ppm"); shot = true; }
    if (state == ST_SERVE && stateT == 30 && rival == 1) simShot(draw, "out/vr_rival.ppm");
    if (state == ST_WON && stateT == 30 && rival == 0) simShot(draw, "out/vr_won.ppm");
  }
  printf("rival=%d pts=%d-%d score=%u state=%d\n", rival, ptsP, ptsC, score, state);
  for (int i = 0; i < 100; i++) simStep(step);
  simShot(draw, "out/vr_over.ppm");
}
