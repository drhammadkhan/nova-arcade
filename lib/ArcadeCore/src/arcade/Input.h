#pragma once
// =====================================================================
//  ArcadeCore input: BLE gamepads via Bluepad32 (Stadia in Bluetooth mode,
//  Xbox Wireless fw v5+, ...). Buttons are exposed as bitmasks with
//  held / pressed (this frame) / released states, plus an analog stick.
// =====================================================================
#ifndef ARCADE_SIM
#include <Bluepad32.h>
#endif
#include "Config.h"

enum : uint32_t {
  BTN_UP = 1u << 0, BTN_DOWN = 1u << 1, BTN_LEFT = 1u << 2, BTN_RIGHT = 1u << 3,
  BTN_A = 1u << 4, BTN_B = 1u << 5, BTN_X = 1u << 6, BTN_Y = 1u << 7,
  BTN_L1 = 1u << 8, BTN_R1 = 1u << 9, BTN_L2 = 1u << 10, BTN_R2 = 1u << 11,
  BTN_START = 1u << 12, BTN_SELECT = 1u << 13, BTN_SYSTEM = 1u << 14,
};

struct Pad {
  float ax = 0, ay = 0;        // -1..1, d-pad overrides the stick
  uint32_t held = 0, pressed = 0, released = 0;
  bool connected = false;
  bool down(uint32_t m) const { return held & m; }
  bool hit(uint32_t m) const { return pressed & m; }
};

namespace input {

static Pad pad;
static uint32_t prevHeld = 0;

#ifndef ARCADE_SIM
static ControllerPtr ctl = nullptr;

static inline void onConnected(ControllerPtr c) {
  if (!ctl) { ctl = c; c->setPlayerLEDs(1); }
  else c->disconnect();
}
static inline void onDisconnected(ControllerPtr c) { if (c == ctl) ctl = nullptr; }

inline void begin() {
  BP32.setup(&onConnected, &onDisconnected);
  BP32.enableVirtualDevice(false);
}

static inline float deadzone(int v) {
  float f = v / 512.0f;
  if (f > -0.18f && f < 0.18f) return 0;
  f = f > 0 ? (f - 0.18f) / 0.82f : (f + 0.18f) / 0.82f;
  return constrain(f, -1.0f, 1.0f);
}

inline void rumble(uint16_t ms, uint8_t weak, uint8_t strong) {
#if RUMBLE
  if (ctl && ctl->isConnected()) ctl->playDualRumble(0, ms, weak, strong);
#endif
}
#else
static Pad simPad;   // the simulator writes this
inline void begin() {}
inline void rumble(uint16_t, uint8_t, uint8_t) {}
#endif

inline const Pad& update() {
  Pad p;
#ifndef ARCADE_SIM
  BP32.update();
  if (ctl && ctl->isConnected() && ctl->isGamepad()) {
    p.connected = true;
    uint8_t d = ctl->dpad();
    float dx = ((d & DPAD_RIGHT) ? 1 : 0) - ((d & DPAD_LEFT) ? 1 : 0);
    float dy = ((d & DPAD_DOWN) ? 1 : 0) - ((d & DPAD_UP) ? 1 : 0);
    float sx = deadzone(ctl->axisX()), sy = deadzone(ctl->axisY());
    p.ax = dx != 0 ? dx : sx;
    p.ay = dy != 0 ? dy : sy;
    uint32_t h = 0;
    if (ctl->a()) h |= BTN_A;
    if (ctl->b()) h |= BTN_B;
    if (ctl->x()) h |= BTN_X;
    if (ctl->y()) h |= BTN_Y;
    if (ctl->l1()) h |= BTN_L1;
    if (ctl->r1()) h |= BTN_R1;
    if (ctl->l2()) h |= BTN_L2;
    if (ctl->r2()) h |= BTN_R2;
    if (ctl->miscStart()) h |= BTN_START;
    if (ctl->miscSelect()) h |= BTN_SELECT;
    if (ctl->miscSystem()) h |= BTN_SYSTEM;
    p.held = h;
  }
#else
  p = simPad;
  p.pressed = p.released = 0;
#endif
  if (p.ax < -0.5f) p.held |= BTN_LEFT;
  if (p.ax > 0.5f) p.held |= BTN_RIGHT;
  if (p.ay < -0.5f) p.held |= BTN_UP;
  if (p.ay > 0.5f) p.held |= BTN_DOWN;
  p.pressed = p.held & ~prevHeld;
  p.released = prevHeld & ~p.held;
  prevHeld = p.held;
  pad = p;
  return pad;
}

// Auto-repeat helper for menus / piece movement: true on press, then every
// `rate` frames after `delay` frames of holding.
struct Repeat {
  uint16_t t = 0;
  bool tick(bool held, int delay = 16, int rate = 5) {
    if (!held) { t = 0; return false; }
    t++;
    if (t == 1) return true;
    return t > delay && ((t - delay) % rate) == 0;
  }
};

}  // namespace input
