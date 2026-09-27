# Making a Nova Arcade game

A game is an ordinary Arduino sketch that includes `ArcadeCore.h`. The engine handles the screen, sound, controller, pause menu, volume, hi-score saving and returning to the launcher. You write two functions: `step()`, which runs 60 times a second, and `draw()`.

## Minimal game

```cpp
#include <ArcadeCore.h>
using namespace gfx;

static float x = 160, y = 120;

static void step(const Pad& in) {
  x += in.ax * 2;  y += in.ay * 2;                 // stick or d-pad, -1..1
  if (in.hit(BTN_A)) audio::play(SFX_SHOOT);       // pressed this frame
  if (in.hit(BTN_START)) arcade::pause();          // standard pause menu
}

static void draw() {                               // called once per 48-row strip
  fill(rgbS(10, 10, 40));
  rectf((int)x - 4, (int)y - 4, 8, 8, rgbS(62, 198, 224));
  text("HELLO", SW / 2, 20, TFT_WHITE, 2, top_center);
}

static const audio::SongDef SONGS[] = {{nullptr, 0, 120, false, false, 0}};
static const audio::Music MUSIC = {audio::STD_CHORDS, nullptr, nullptr, SONGS, 1};

void setup() { arcade::begin("hello", &MUSIC); }  // "hello" = save namespace (max 15 chars)
void loop()  { arcade::run(step, draw); }
```

> **Tip:** once your game defines its own `struct`s, put the code in a `game.h` next to the sketch and make the `.ino` just `#include "game.h"`, as the bundled games do. The Arduino IDE auto-generates function prototypes at the top of `.ino` files, before your types exist, which breaks the build; it leaves headers alone.

## Drawing

The 320×240 screen is drawn as five horizontal strips of 48 rows, so `draw()` runs five times per frame. Each time, `Y0` is the screen row the current strip starts at. The helpers in `arcade/Render.h` handle the clipping for you, so you can always use full-screen coordinates:

| Function | |
|---|---|
| `fill(c)`, `rectf(x,y,w,h,c)`, `rect(...)`, `pset(x,y,c)` | shapes. Colours are byte-swapped RGB565: use `rgbS(r,g,b)` |
| `blit(sprite, x, y, palette)` | palette-indexed sprite, index 0 is transparent |
| `text(str, x, y, colour565, size, datum)` / `textf(...)` | 6×8 pixel font. Colour is plain RGB565 (`c565(r,g,b)`) |
| `shade(x,y,w,h)` | halves the brightness, for translucent panels |
| `circle`, `disc`, `line` | via LovyanGFX |
| `rowsVisible(y, h)` | skip work for things outside the current strip |

Sprites are `struct Sprite { w, h, const uint8_t* px }`. The easiest way to make them is with Python and Pillow. See `tools/art/*.py` and `games/NovaLance/tools/make_assets.py`.

## Sound

- `audio::play(SFX_x, param)` plays one of the shared sound effects: shoot, hit, explode, power-up, line clear, bounce, brick, march, UFO and more.
- `audio::music(n)` plays song `n` from your `Music` table. Songs are bars of `{chord, lead pattern, drum pattern}`. Lead patterns are 16 steps of MIDI notes, where 0 is a rest and 1 holds the previous note. See any game's music block for an example.

## Input

`Pad` has `ax`/`ay` (analog, -1 to 1, with the D-pad overriding the stick) and three button bitmasks: `held`, `pressed` and `released`. Use `in.down(BTN_A)` and `in.hit(BTN_B | BTN_X)`. `input::Repeat` gives menu-style auto-repeat.

## Difficulty

Every game offers Easy, Normal and Hard, and the engine does most of the work:

- On the title screen, call `if (arcade::titleInput(in)) hiscore = arcade::loadHi(DEFAULT);`. It handles UP/DOWN for difficulty and LEFT/RIGHT for volume, and returns true when the difficulty changes, because each difficulty keeps its own hi-score.
- Draw the selector with `arcade::drawDifficulty(y)`.
- Scale your hazards with `arcade::speed()`: 0.7 on Easy, 1.0 on Normal and 1.25 on Hard. Stretch intervals such as fire cooldowns with `arcade::frames(n)`. Slower is easier. Leave the player's own movement alone so the controls feel the same at every level.

The choice is saved per game, and the pause menu shows it.

## Saving

`arcade::loadHi(default)` and `arcade::saveHi(v)` store a hi-score for the current difficulty. `arcade::prefs` is a `Preferences` namespace for anything else you want to keep.

## Test on your PC

`tools/sim` compiles any game for the desktop and saves screenshots, with no board needed:

```bash
cd tools/sim
g++ -std=gnu++17 -DARCADE_SIM -I. -I../../lib/ArcadeCore/src -x c++ sim_blockfall.cpp -o sim && ./sim
```

Copy one of the `sim_*.cpp` files for your game and script the inputs.

## Ship it

Build with the same settings as the other games: board `esp32_bluepad32 → ESP32S3 Dev Module`, Flash 16MB, PSRAM OPI. Then copy these to `/arcade` on the SD card:

- `MyGame.bin`: the sketch binary (*Sketch → Export Compiled Binary*, the file **without** "merged" or "bootloader" in its name)
- `MyGame.png`: a 160×120 thumbnail
- `MyGame.txt`: title, a one-line description and the save id, on three lines

To include it in the official pack, add it to `games/` and to `GAMES` in `scripts/build.sh`.
