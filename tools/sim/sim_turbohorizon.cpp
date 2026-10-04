#include "../../games/TurboHorizon/game.h"
#include "sim_driver.h"
// Bot: the game's own autopilot, fed through the real controls.
int main() {
  setup();
  for (int i = 0; i < 300; i++) simStep(step);
  simShot(draw, "out/th_title.ppm");
  simStep(step, 0, 0, BTN_START);
  for (int i = 0; i < 70; i++) simStep(step);
  simShot(draw, "out/th_count.ppm");
  int lastStage = -1, crashes = 0;
  for (int f = 0; f < 60 * 60 * 4 && state != ST_OVER; f++) {
    float steer; bool gas, brake;
    autopilot(&steer, &gas, &brake);
    if (state != ST_RACE) { gas = false; brake = false; steer = 0; }
    simStep(step, steer, 0, (gas ? BTN_A : 0) | (brake ? BTN_B : 0));
    if (crashT == 49) crashes++;
    if (stage != lastStage && state == ST_RACE) { lastStage = stage; printf("stage %d at f=%d time=%.1f score=%u\n", stage, f, timeLeft, score); }
    if (state == ST_RACE && stateT == 600) simShot(draw, "out/th_s1a.ppm");
    if (state == ST_RACE && stateT == 1300) simShot(draw, "out/th_s1b.ppm");
    static int s2n = 0; if (stage == 1 && checkT == 0 && (f % 400) == 0 && s2n++ < 2) simShot(draw, s2n == 1 ? "out/th_s2.ppm" : "out/th_s2b.ppm");
    if (stage == 2 && fmodf(position, 50000) < 200 && f % 3 == 0) simShot(draw, "out/th_s3.ppm");
    if (checkT == 120) simShot(draw, "out/th_check.ppm");
  }
  printf("end stage=%d score=%u passes=%d crashes=%d state=%d time=%.1f\n", stage, score, passes, crashes, state, timeLeft);
  timeLeft = 0.01f;
  for (int i = 0; i < 200; i++) simStep(step);
  simShot(draw, "out/th_over.ppm");
}
