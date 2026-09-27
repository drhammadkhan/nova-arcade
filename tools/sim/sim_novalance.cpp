#include "../../games/NovaLance/NovaLance.ino"
#include "sim_driver.h"
int main() {
  setup();
  for (int i = 0; i < 40; i++) simStep(step);
  simShot(draw, "out/nl_title.ppm");
  simStep(step, 0, 0, BTN_START);
  for (int i = 0; i < 900; i++) simStep(step, 0, sinf(i * 0.03f), BTN_A);
  simShot(draw, "out/nl_play.ppm");
  simStep(step, 0, 0, BTN_START);  // pause
  simStep(step, 0, 0, 0);
  simShot(draw, "out/nl_pause.ppm");
  printf("paused=%d score=%u\n", arcade::isPaused, score);
}
