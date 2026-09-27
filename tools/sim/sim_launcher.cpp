#include "../../Launcher/Launcher.ino"
#include "sim_driver.h"
int main() {
  setup();
  for (int i = 0; i < 30; i++) simStep(step);
  simShot(draw, "out/ln_menu.ppm");
  simStep(step, 1, 0, 0); for (int i = 0; i < 6; i++) simStep(step);
  simShot(draw, "out/ln_slide.ppm");
  for (int i = 0; i < 30; i++) simStep(step);
  simShot(draw, "out/ln_menu2.ppm");
  simStep(step, 0, 0, BTN_A); simStep(step);
  simShot(draw, "out/ln_install.ppm");
  printf("sel=%d n=%d\n", sel, ngames);
}
