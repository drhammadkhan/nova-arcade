#pragma once
// Include AFTER the game source. Provides frame stepping and PNG capture.
#include <functional>
static void simSave(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) {
    uint16_t c = swp(FB[y][x]);
    unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
// One fixed 60 Hz step with the given pad state (edges computed automatically)
static void simStep(void (*stepFn)(const Pad&), float ax = 0, float ay = 0, uint32_t held = 0) {
  input::simPad = Pad(); input::simPad.connected = true; input::simPad.ax = ax; input::simPad.ay = ay; input::simPad.held = held;
  Pad in = input::update();
  bool live = arcade::systemInput(in);
  gfx::frameNo++;
  if (live && !arcade::isPaused) stepFn(in);
  if (arcade::volShow) arcade::volShow--;
  if (arcade::toastT) arcade::toastT--;
  while (sim_qn) { uint16_t m; xQueueReceive(0, &m, 0); }   // drain audio queue
}
static void simShot(void (*drawFn)(), const char* name) {
  arcade::userDraw = drawFn;
  gfx::render(arcade::drawAll);
  simSave(name);
}
