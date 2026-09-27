// =====================================================================
//  Browser entry point: one game compiled to WebAssembly with Emscripten.
//  scripts/build-web.sh builds this once per game with
//  -DGAME_H='"../../games/<Game>/game.h"'. The page (web/play/play.js)
//  feeds in the pad state, calls web_frame() every animation frame, blits
//  the returned RGBA pixels to a canvas and pulls audio from web_audio().
// =====================================================================
#include GAME_H

extern "C" {

EMSCRIPTEN_KEEPALIVE void web_init(uint32_t seed) {
  rngState ^= seed;
  if (!rngState) rngState = 0x9E3779B9;
  setup();
}

// held: BTN_* bitmask, ax/ay: stick (-1..1). The page folds the D-pad into ax/ay
// the same way the hardware does.
EMSCRIPTEN_KEEPALIVE void web_pad(uint32_t held, float ax, float ay) {
  input::simPad = Pad();
  input::simPad.connected = true;
  input::simPad.held = held;
  input::simPad.ax = ax;
  input::simPad.ay = ay;
}

// Runs the game loop once (fixed 60 Hz steps inside) and returns 320x240 RGBA pixels.
EMSCRIPTEN_KEEPALIVE uint32_t* web_frame() {
  static uint32_t rgba[240 * 320];
  loop();
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++) {
      uint16_t c = swp(FB[y][x]);
      uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
      rgba[y * 320 + x] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }
  return rgba;
}

// Renders `frames` mono samples at AUDIO_RATE (22050 Hz) as floats -1..1.
EMSCRIPTEN_KEEPALIVE float* web_audio(int frames) {
  static int16_t st[4096 * 2];
  static float out[4096];
  if (frames > 4096) frames = 4096;
  audio::renderBlock(st, frames);
  for (int i = 0; i < frames; i++) out[i] = st[i * 2] / 32768.0f;
  return out;
}

EMSCRIPTEN_KEEPALIVE int web_audio_rate() { return AUDIO_RATE; }

}
