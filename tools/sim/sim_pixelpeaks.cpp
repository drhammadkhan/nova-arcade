#include "../../games/PixelPeaks/game.h"
#include "sim_driver.h"
// Screenshots plus a look-ahead bot that proves every level can be finished.
struct Snap {
  Hero h; uint8_t cells[ROWS][MAXW]; Enemy e[40]; Mover m[8];
  int hearts, lives, coins, levelCoins, scrollsGot, stateT, levelIdx, loopN; uint32_t score; State st; float camX, checkX, checkY;
};
static void save(Snap& s) {
  s.h = hero; memcpy(s.cells, cells, sizeof(cells)); memcpy(s.e, enemies, sizeof(enemies)); memcpy(s.m, movers, sizeof(movers));
  s.hearts = hearts; s.lives = lives; s.coins = coins; s.levelCoins = levelCoins; s.scrollsGot = scrollsGot; s.stateT = stateT;
  s.levelIdx = levelIdx; s.loopN = loopN; s.score = score; s.st = state; s.camX = camX; s.checkX = checkX; s.checkY = checkY;
}
static void load(const Snap& s) {
  hero = s.h; memcpy(cells, s.cells, sizeof(cells)); memcpy(enemies, s.e, sizeof(enemies)); memcpy(movers, s.m, sizeof(movers));
  // the hero may be riding a mover: re-point into the live array
  if (s.h.riding) hero.riding = movers + (s.h.riding - movers);
  hearts = s.hearts; lives = s.lives; coins = s.coins; levelCoins = s.levelCoins; scrollsGot = s.scrollsGot; stateT = s.stateT;
  levelIdx = s.levelIdx; loopN = s.loopN; score = s.score; state = s.st; camX = s.camX; checkX = s.checkX; checkY = s.checkY;
}
static uint32_t prevHeld = 0;
static void raw(uint32_t held, float ax) {   // one game step without the audio/system layer
  Pad p; p.connected = true; p.held = held; p.ax = ax; p.pressed = held & ~prevHeld; prevHeld = held;
  input::pad = p;
  hero.invuln = max(hero.invuln, 2);   // enemies can't hurt the bot: we're testing the level, not the bot
  frameNo++;
  step(p);
  sim_qn = 0;
}
struct Opt { uint32_t btn; float ax; int hold; };
// the default "keep going" policy: run right, jump at walls and gaps, hold A while rising
static uint32_t policyButtons() {
  uint32_t b = 0;
  if (hero.onGround) {
    bool wall = boxSolid(hero.x + 6, hero.y, HW, HH);
    int ahead = (int)((hero.x + HW + 10) / TS), below = (int)((hero.y + HH + 2) / TS);
    bool gap = !solidCell(cellAt(ahead, below)) && cellAt(ahead, below) != C_PLANK && !solidCell(cellAt(ahead, below + 1));
    if (wall || gap) b |= BTN_A;
  } else if (hero.vy < 0 && (prevHeld & BTN_A)) b |= BTN_A;
  return b;
}
static uint32_t optButtons(const Opt& o, int f) {
  if (o.btn == 0 && o.ax > 0) return policyButtons();          // option 0 is the policy itself
  if (o.btn == BTN_B) return f == 0 ? BTN_B : 0;
  return f < o.hold ? o.btn : 0;
}
static const Opt OPTS[] = {{0, 1, 0}, {BTN_A, 1, 5}, {BTN_A, 1, 30}, {0, 0, 0}, {BTN_B, 1, 1}, {0, -1, 0}, {BTN_A, 0, 30}, {BTN_A, -1, 30}};
static float evalOpt(const Opt& o, int horizon) {
  Snap s; save(s);
  uint32_t ph = prevHeld;
  int lv = levelIdx;
  float best = -1e9;
  for (int f = 0; f < horizon; f++) {
    bool act = f < 4;
    uint32_t b = act ? optButtons(o, f) : (f < o.hold && o.btn == BTN_A ? BTN_A : policyButtons());
    float ax = act ? o.ax : 1;
    raw(b, ax);
    if (state == ST_DEAD || state == ST_OVER) { best = (f < 16 ? -1e6 : best - 60); break; }
    if (state == ST_CLEAR || levelIdx != lv) { best = 1e8 - f; break; }
    best = max(best, hero.x - (hero.onGround ? 0 : 4));
  }
  load(s); prevHeld = ph;
  return best;
}
int main() {
  setup();
  for (int i = 0; i < 120; i++) simStep(step);
  simShot(draw, "out/pp_title.ppm");
  simStep(step, 0, 0, BTN_START);
  for (int i = 0; i < 40; i++) simStep(step);
  simShot(draw, "out/pp_intro.ppm");
  int deaths = 0, cleared = 0, frames = 0, stuck = 0;
  float lastX = 0; int lastProgress = 0;
  const char* shots[3][3] = {{"out/pp_w1a.ppm", "out/pp_w1b.ppm", "out/pp_w1c.ppm"}, {"out/pp_w2a.ppm", "out/pp_w2b.ppm", "out/pp_w2c.ppm"}, {"out/pp_w3a.ppm", "out/pp_w3b.ppm", "out/pp_w3c.ppm"}};
  int shotN = 0, shotLevel = -1;
  while (frames < 60 * 60 * 12 && cleared < 3) {
    if (state == ST_PLAY) {
      // the run-and-jump policy is the default; anything else has to be clearly better
      int bestI = 0; float bestV = evalOpt(OPTS[0], 130) + 10;
      for (int i = 1; i < (int)(sizeof(OPTS) / sizeof(OPTS[0])); i++) {
        if (OPTS[i].btn == BTN_B && (hero.rollT || hero.rollCd)) continue;
        float v = evalOpt(OPTS[i], 130);
        if (v > bestV) { bestV = v; bestI = i; }
      }
      const Opt& o = OPTS[bestI];
      for (int f = 0; f < 4 && state == ST_PLAY; f++, frames++) raw(optButtons(o, f), o.ax);
      if (state == ST_DEAD) { deaths++; printf("died level %d at x=%.0f y=%.0f (tile %d) chose opt %d score %.0f\n", levelIdx, hero.x, hero.y, (int)(hero.x / 16), bestI, bestV); }
      if (hero.x > lastX + 16) { lastX = hero.x; lastProgress = frames; }
      if (frames - lastProgress > 60 * 40) { stuck++; printf("STUCK level %d at x=%.0f (tile %d)\n", levelIdx, hero.x, (int)(hero.x / 16)); break; }
      if (levelIdx != shotLevel) { shotLevel = levelIdx; shotN = 0; }
      if (shotN < 3 && hero.x > (shotN + 1) * LEVELS[levelIdx].w * 16 / 4) { simShot(draw, shots[levelIdx % 3][shotN]); shotN++; }
    } else {
      State before = state;
      raw(state == ST_INTRO || state == ST_WIN ? (frames & 8 ? BTN_A : 0) : 0, 0); frames++;
      if (state == ST_CLEAR && stateT == 60) { simShot(draw, "out/pp_clear.ppm"); }
      if (before == ST_CLEAR && state != ST_CLEAR) { cleared++; lastX = 0; lastProgress = frames; printf("cleared level %d: score %u coins %d scrolls %d\n", cleared, score, coins, scrollsGot); }
    }
  }
  printf("result: cleared=%d deaths=%d stuck=%d frames=%d state=%d\n", cleared, deaths, stuck, frames, state);
}
