#pragma once
// =====================================================================
//  ArcadeCore system layer: start-up, fixed 60 Hz game loop, shared volume,
//  a standard pause menu (Resume / Volume / Quit to menu), difficulty levels,
//  hi-score storage and returning to the launcher.
// =====================================================================
#include "Render.h"
#include "Audio.h"
#include "Input.h"
#ifndef ARCADE_SIM
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#else
#include "ArcadeSimSystem.h"
#endif

namespace arcade {

static Preferences prefs;    // this game's own storage (hi-score, options)
static Preferences shared;   // settings shared by every game ("arcade")
static int volume = DEFAULT_VOLUME;
static int volShow = 0;
static bool launcherMode = false;
static bool isPaused = false;
static int pauseSel = 0;
static char toastMsg[40];
static int toastT = 0;
static void (*userDraw)() = nullptr;

// ------------------------------------------------------------ difficulty
// Each game saves its own choice. Games scale their hazards with speed():
// slower is easier. Each difficulty keeps its own hi-score.
enum Difficulty { DIFF_EASY, DIFF_NORMAL, DIFF_HARD, DIFF_COUNT };
static int difficulty = DIFF_NORMAL;
static const char* const DIFF_NAMES[DIFF_COUNT] = {"EASY", "NORMAL", "HARD"};

inline float speed() {
  static const float S[DIFF_COUNT] = {0.7f, 1.0f, 1.25f};
  return S[difficulty];
}
// Stretch (easy) or shorten (hard) a frame interval such as a fire cooldown.
inline int frames(int f) { return max(1, (int)(f / speed() + 0.5f)); }
inline const char* difficultyName() { return DIFF_NAMES[difficulty]; }
inline void setDifficulty(int d) {
  difficulty = constrain(d, 0, DIFF_COUNT - 1);
  prefs.putUChar("diff", (uint8_t)difficulty);
}

inline void toast(const char* msg, int frames = 120) {
  strncpy(toastMsg, msg, sizeof(toastMsg) - 1);
  toastT = frames;
}

inline void setVolume(int v) {
  v = constrain(v, 0, 10);
  volShow = 90;
  if (v == volume) return;
  volume = v;
  audio::setVolume(volume);
  shared.putUChar("vol", (uint8_t)volume);
  audio::play(SFX_BLIP);
}
inline void changeVolume(int d) { setVolume(volume + d); }

// Make any reset (or power cycle) come back to the launcher.
inline void armReturnToLauncher() {
#ifndef ARCADE_SIM
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (run && run->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY) {
    const esp_partition_t* f = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
    if (f) esp_ota_set_boot_partition(f);
  }
#endif
}

inline void exitToLauncher() {
  audio::music(0);
  audio::engine(0, 0);
#ifndef ARCADE_SIM
  for (int b = 200; b >= 0; b -= 20) { gfx::lcd.setBrightness(b); delay(15); }
  gfx::lcd.fillScreen(0);
  armReturnToLauncher();
  esp_restart();
#elif defined(ARCADE_WEB)
  arcadeWebExit();   // the browser player goes back to its game list
#endif
}

// Normal keeps the original "hi" key so existing hi-scores carry over.
static inline const char* hiKey() {
  return difficulty == DIFF_EASY ? "hiE" : difficulty == DIFF_HARD ? "hiH" : "hi";
}
inline uint32_t loadHi(uint32_t def) { return prefs.getUInt(hiKey(), def); }
inline void saveHi(uint32_t v) { prefs.putUInt(hiKey(), v); }

// Title-screen controls shared by every game: UP/DOWN pick the difficulty,
// LEFT/RIGHT change the volume. Returns true when the difficulty changed,
// so the game can reload its hi-score with loadHi().
inline bool titleInput(const Pad& in) {
  if (in.down(BTN_SELECT)) return false;
  if (in.hit(BTN_LEFT)) changeVolume(-1);
  if (in.hit(BTN_RIGHT)) changeVolume(+1);
  int d = in.hit(BTN_DOWN) ? 1 : in.hit(BTN_UP) ? -1 : 0;
  if (!d || difficulty + d < 0 || difficulty + d >= DIFF_COUNT) return false;
  setDifficulty(difficulty + d);
  audio::play(SFX_MOVE);
  return true;
}

inline void begin(const char* gameId, const audio::Music* music, bool launcher = false) {
  Serial.begin(115200);
  launcherMode = launcher;
  gfx::begin();
#ifndef ARCADE_SIM
  rngState ^= esp_random();
#endif
  prefs.begin(gameId, false);
  shared.begin("arcade", false);
  volume = constrain((int)shared.getUChar("vol", DEFAULT_VOLUME), 0, 10);
  difficulty = constrain((int)prefs.getUChar("diff", DIFF_NORMAL), 0, DIFF_COUNT - 1);
  audio::begin();
  audio::setMusic(music);
  audio::setVolume(volume);
  input::begin();
  if (!launcher) armReturnToLauncher();
}

inline void pause() { isPaused = true; pauseSel = 0; audio::play(SFX_BLIP); }
inline bool paused() { return isPaused; }

// Handles system-wide controls; returns false if the game should not step.
static inline bool systemInput(Pad& in) {
  // SELECT + UP/DOWN: volume, anywhere
  if (in.down(BTN_SELECT) && in.hit(BTN_UP)) { changeVolume(+1); in.pressed &= ~BTN_UP; }
  if (in.down(BTN_SELECT) && in.hit(BTN_DOWN)) { changeVolume(-1); in.pressed &= ~BTN_DOWN; }
  if (!launcherMode && (in.hit(BTN_SYSTEM) || (in.down(BTN_SELECT) && in.hit(BTN_START)))) {
    exitToLauncher();
    return false;
  }
  if (!isPaused) return true;
  // pause menu
  if (in.hit(BTN_UP)) { pauseSel = (pauseSel + 2) % 3; audio::play(SFX_MOVE); }
  if (in.hit(BTN_DOWN)) { pauseSel = (pauseSel + 1) % 3; audio::play(SFX_MOVE); }
  if (pauseSel == 1 && in.hit(BTN_LEFT)) changeVolume(-1);
  if (pauseSel == 1 && in.hit(BTN_RIGHT)) changeVolume(+1);
  if (in.hit(BTN_START) || (in.hit(BTN_A) && pauseSel == 0) || in.hit(BTN_B)) {
    isPaused = false; audio::play(SFX_BLIP);
  } else if (in.hit(BTN_A) && pauseSel == 2) {
    exitToLauncher();
  }
  return false;
}

static inline void drawVolumeBar(int y) {
  using namespace gfx;
  if (!rowsVisible(y - 3, 18)) return;
  shade(86, y - 3, 148, 16);
  rect(86, y - 3, 148, 16, rgbS(90, 80, 150));
  text(volume ? "VOL" : "MUTE", 92, y + 1, TFT_WHITE, 1, top_left, false);
  for (int i = 0; i < 10; i++) {
    int x = 124 + i * 10, h = 3 + i;
    uint16_t c = i < volume ? (i < 7 ? rgbS(62, 198, 224) : i < 9 ? rgbS(255, 216, 74) : rgbS(232, 56, 79))
                            : rgbS(61, 74, 102);
    rectf(x, y + 10 - h, 7, h, c);
  }
}

static const uint8_t DIFF_RGB[DIFF_COUNT][3] = {{79, 214, 107}, {62, 198, 224}, {235, 60, 80}};

// Difficulty selector for title screens, centred on row y.
static inline void drawDifficulty(int y) {
  using namespace gfx;
  if (!rowsVisible(y - 3, 16)) return;
  const uint8_t* c = DIFF_RGB[difficulty];
  shade(96, y - 3, 128, 15);
  rect(96, y - 3, 128, 15, rgbS(c[0] / 2, c[1] / 2, c[2] / 2));
  for (int i = 0; i < DIFF_COUNT; i++)
    rectf(104 + i * 5, y + 7 - i * 2, 3, 2 + i * 2, i <= difficulty ? rgbS(c[0], c[1], c[2]) : rgbS(61, 74, 102));
  text(difficultyName(), SW / 2 + 6, y + 1, c565(c[0], c[1], c[2]), 1, top_center, false);
  text("\x1e\x1f", 206, y + 1, c565(150, 140, 200), 1, top_center, false);
}

static inline void drawOverlay() {
  using namespace gfx;
  if (isPaused) {
    if (rowsVisible(62, 118)) {
      shade(70, 62, 180, 118);
      shade(70, 62, 180, 118);
      rect(70, 62, 180, 118, rgbS(122, 61, 184));
      rect(71, 63, 178, 116, rgbS(51, 48, 122));
    }
    text("PAUSED", SW / 2, 72, TFT_WHITE, 2, top_center);
    static const char* items[3] = {"RESUME", "VOLUME", "QUIT TO MENU"};
    for (int i = 0; i < 3; i++) {
      int y = 100 + i * 22;
      bool sel = i == pauseSel;
      if (sel && rowsVisible(y - 4, 16)) rectf(82, y - 4, 156, 16, rgbS(122, 61, 184));
      if (i == 1) {
        text(sel ? "< VOLUME >" : "VOLUME", 92, y, sel ? TFT_WHITE : c565(180, 194, 220));
        for (int k = 0; k < 10; k++)
          rectf(172 + k * 6, y + 7 - k * 7 / 9, 4, 1 + k * 7 / 9,
                k < volume ? rgbS(62, 198, 224) : rgbS(61, 74, 102));
      } else {
        text(items[i], 92, y, sel ? TFT_WHITE : c565(180, 194, 220));
      }
    }
    const uint8_t* dc = DIFF_RGB[difficulty];
    text(difficultyName(), SW / 2, 89, c565(dc[0], dc[1], dc[2]), 1, top_center, false);
    text("SELECT+START: MENU ANYTIME", SW / 2, 166, c565(115, 132, 168), 1, top_center, false);
  } else if (volShow) {
    drawVolumeBar(216);
  }
  if (toastT) {
    if (rowsVisible(20, 14)) { shade(0, 20, SW, 14); }
    text(toastMsg, SW / 2, 23, c565(255, 216, 74), 1, top_center);
  }
}

static inline void drawAll() {
  if (userDraw) userDraw();
  drawOverlay();
}

// Call from loop(): reads input, runs step() at a fixed 60 Hz, then draws a frame.
inline void run(void (*step)(const Pad&), void (*draw)()) {
  static uint32_t last = micros(), acc = 0, carry = 0;
  const uint32_t STEP_US = 16667;
  Pad in = input::update();
  in.pressed |= carry;   // a press on a frame that ran no step is kept for the next one
  bool live = systemInput(in);
  uint32_t now = micros();
  acc += now - last;
  last = now;
  if (acc > STEP_US * 4) acc = STEP_US * 4;
  bool first = true;
  carry = (acc < STEP_US && live && !isPaused) ? in.pressed : 0;
  while (acc >= STEP_US) {
    Pad p = in;
    if (!first) { p.pressed = 0; p.released = 0; }
    gfx::frameNo++;
    if (live && !isPaused) step(p);
    if (volShow) volShow--;
    if (toastT) toastT--;
    first = false;
    acc -= STEP_US;
  }
  if (isPaused) audio::engine(0, 0);   // racing games set it again on the next step
  userDraw = draw;
  gfx::render(drawAll);
}

}  // namespace arcade
