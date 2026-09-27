// =====================================================================
//  NOVA LANCE  -  a retro side-scrolling shoot-'em-up (Nova Arcade)
//  Built on ArcadeCore: see lib/ArcadeCore.
// =====================================================================
#include <ArcadeCore.h>
#include "assets.h"
#include "music.h"

using namespace gfx;

#define START_LIVES 3
#define START_BOMBS 2
// ------------------------------------------------------------ background
static uint16_t skyPat[SH][4];
static uint16_t sunCol[SH];
static int16_t sunHalf[SH];
static const int SUN_X = 236, SUN_Y = 190, SUN_R = 46;
static uint8_t mountH[512];
static uint8_t cityH[1024], cityOff[1024], cityW[1024], citySeed[1024];
struct Star { float x; uint8_t y, layer, tw; };
static Star stars[90];
static float mountScroll = 0, cityScroll = 0, planetX = 262;

static void buildBackground() {
  struct StopC { int y; uint8_t r, g, b; };
  static const StopC stops[] = {
    {0, 6, 5, 20}, {50, 14, 12, 44}, {100, 28, 20, 74}, {140, 52, 28, 100},
    {172, 96, 36, 118}, {205, 170, 60, 120}, {240, 240, 120, 110}};
  for (int y = 0; y < SH; y++) {
    int i = 0;
    while (i < 5 && y >= stops[i + 1].y) i++;
    const StopC &a = stops[i], &b = stops[i + 1];
    int level = (y - a.y) * 16 / (b.y - a.y);
    uint16_t ca = rgbS(a.r, a.g, a.b), cb = rgbS(b.r, b.g, b.b);
    for (int x = 0; x < 4; x++) skyPat[y][x] = (BAYER[y & 3][x] < level) ? cb : ca;
  }
  // synthwave sun: gradient with widening slats towards the bottom
  for (int y = 0; y < SH; y++) {
    int dy = y - SUN_Y;
    sunHalf[y] = -1;
    if (dy < -SUN_R || dy > SUN_R) continue;
    if (dy > 4 && (dy % 8) < (dy / 9)) continue;
    sunHalf[y] = (int16_t)sqrtf((float)(SUN_R * SUN_R - dy * dy));
    float t = (float)(dy + SUN_R) / (2 * SUN_R);
    uint8_t r = 255, g = (uint8_t)(230 - 150 * t), b = (uint8_t)(90 + 60 * t);
    sunCol[y] = rgbS(r, g, b);
  }
  // mountains: periodic sum of sines so it wraps seamlessly
  for (int x = 0; x < 512; x++) {
    float a = x * 6.2831853f / 512.0f;
    float h = 44 + 16 * sinf(a * 3) + 9 * sinf(a * 7 + 1.3f) + 4 * sinf(a * 19 + 2.1f);
    mountH[x] = (uint8_t)constrain((int)h, 16, 80);
  }
  // city skyline
  int x = 0;
  while (x < 1024) {
    int w = 10 + rnd() % 18;
    int h = 16 + rnd() % 44;
    uint8_t seed = rnd();
    bool antenna = (rnd() % 4) == 0;
    for (int i = 0; i < w && x < 1024; i++, x++) {
      cityH[x] = h; cityOff[x] = i; cityW[x] = w; citySeed[x] = seed;
      if (antenna && i == w / 2) { cityH[x] = h + 9; cityOff[x] = 255; }
    }
    int gap = rnd() % 3;
    for (int i = 0; i < gap && x < 1024; i++, x++) { cityH[x] = 6; cityOff[x] = 0; cityW[x] = 1; citySeed[x] = 0; }
  }
  for (auto& s : stars) {
    s.x = frange(0, SW); s.y = rnd() % 170; s.layer = rnd() % 3; s.tw = rnd();
  }
}

static uint8_t flashTimer = 0;

static void drawBackground() {
  // sky
  if (flashTimer) {
    uint16_t c = (flashTimer & 2) ? rgbS(255, 240, 220) : rgbS(255, 180, 200);
    for (int i = 0; i < SW * STRIP; i++) B[i] = c;
    return;
  }
  for (int r = 0; r < STRIP; r++) {
    const uint16_t* p = skyPat[Y0 + r];
    uint32_t p01 = p[0] | (p[1] << 16), p23 = p[2] | (p[3] << 16);
    uint32_t* d = (uint32_t*)(B + r * SW);
    for (int x = 0; x < SW / 2; x += 2) { d[x] = p01; d[x + 1] = p23; }
  }
  // stars
  static const uint16_t starCol[3] = {rgbS(90, 90, 150), rgbS(170, 170, 220), rgbS(255, 255, 255)};
  for (auto& s : stars) {
    if (s.y < Y0 || s.y >= Y0 + STRIP) continue;
    if (s.layer == 0 && ((frameNo + s.tw) & 63) < 6) continue;  // twinkle
    int x = (int)s.x;
    pset(x, s.y, starCol[s.layer]);
    if (s.layer == 2) pset(x + 1, s.y, starCol[1]);
  }
  // ringed planet
  blit(SPR_PLANET, (int)planetX, 18);
  // sun
  for (int r = 0; r < STRIP; r++) {
    int y = Y0 + r, hw = sunHalf[y];
    if (hw < 0) continue;
    uint16_t c = sunCol[y];
    uint16_t* d = B + r * SW;
    for (int x = max(0, SUN_X - hw); x <= min(SW - 1, SUN_X + hw); x++) d[x] = c;
  }
  // mountains
  const int y1 = Y0 + STRIP - 1;
  static const uint16_t mRim = rgbS(120, 64, 150), mMid = rgbS(76, 38, 108), mBody = rgbS(44, 24, 72),
                        mDeep = rgbS(34, 18, 58);
  int ms = (int)mountScroll;
  for (int x = 0; x < SW; x++) {
    int top = SH - mountH[(x + ms) & 511];
    if (top > y1) continue;
    for (int y = max(top, Y0); y <= y1; y++) {
      int d = y - top;
      uint16_t c = d == 0 ? mRim : d < 3 ? mMid : (d > 30 && BAYER[y & 3][x & 3] < (d - 30) / 2) ? mDeep : mBody;
      B[(y - Y0) * SW + x] = c;
    }
  }
  // city
  static const uint16_t cRim = rgbS(56, 52, 110), cBody = rgbS(16, 13, 38), cEdge = rgbS(26, 22, 58),
                        cWin = rgbS(255, 206, 90), cWin2 = rgbS(90, 210, 240), cWinDim = rgbS(120, 90, 60),
                        cBeacon = rgbS(255, 60, 80);
  int cs = (int)cityScroll;
  for (int x = 0; x < SW; x++) {
    int wx = (x + cs) & 1023;
    int top = SH - cityH[wx];
    if (top > y1) continue;
    uint8_t off = cityOff[wx], bw = cityW[wx], seed = citySeed[wx];
    for (int y = max(top, Y0); y <= y1; y++) {
      int d = y - top;
      uint16_t c;
      if (off == 255) {
        c = (d == 0 && (frameNo & 32)) ? cBeacon : (d < 9 ? cRim : cBody);
      } else if (d == 0) {
        c = cRim;
      } else if (off == 0 || off == bw - 1) {
        c = cEdge;
      } else if (off % 3 == 1 && d >= 3 && (d & 3) == 1 && off < bw - 1) {
        uint8_t h = (seed * 31 + (d >> 2) * 17 + (off / 3) * 7) & 15;
        c = h < 5 ? cWin : h < 7 ? cWin2 : h < 8 ? cWinDim : cBody;
      } else {
        c = cBody;
      }
      B[(y - Y0) * SW + x] = c;
    }
  }
}

static void scrollBackground(float speed) {
  mountScroll += 0.35f * speed;
  cityScroll += 1.1f * speed;
  if (mountScroll >= 512) mountScroll -= 512;
  if (cityScroll >= 1024) cityScroll -= 1024;
  planetX -= 0.04f * speed;
  if (planetX < -90) planetX = SW + 10;
  static const float sp[3] = {0.25f, 0.6f, 1.4f};
  for (auto& s : stars) {
    s.x -= sp[s.layer] * speed;
    if (s.x < 0) { s.x += SW; s.y = rnd() % 170; }
  }
}

// ------------------------------------------------------------ entities
enum EType : uint8_t { E_DRONE, E_DART, E_POD, E_BOSS };
struct Enemy {
  bool on; uint8_t type, flash;
  float x, y, vx, vy, baseY, t, param;
  int16_t hp, timer;
};
struct Shot { bool on; float x, y, vx, vy; uint8_t kind; };
struct Particle { bool on; float x, y, vx, vy; uint8_t life, maxLife, kind, size; };
struct Ring { bool on; float x, y, r, vr; uint8_t life; };
struct Pickup { bool on; float x, y, t; uint8_t type; };
struct Popup { bool on; float x, y; uint8_t life; char txt[8]; };
struct Pending { bool on; int16_t delay; uint8_t type; float y, param; };

static Enemy enemies[32];
static Shot pshots[48], eshots[120];
static Particle parts[320];
static Ring rings[16];
static Pickup pickups[8];
static Popup popups[12];
static Pending pending[32];

struct Player {
  float x, y;
  int lives, bombs, weapon, fireCd, invuln, shield, respawn;
  bool alive;
} pl;

enum State { ST_TITLE, ST_PLAY, ST_OVER };
static State state = ST_TITLE;
static uint32_t score = 0, hiscore = 0;
static int level = 1;
static uint32_t levelFrames = 0;
static int waveTimer = 120;
static int warnTimer = 0, bannerTimer = 0, clearTimer = 0, overTimer = 0;
static bool bossActive = false;
static int bossIdx = -1;
static float shake = 0;
static int shakeX = 0, shakeY = 0;
static bool newHi = false;

static const Sprite& sprOf(const Enemy& e) {
  bool f = (frameNo >> 3) & 1;
  switch (e.type) {
    case E_DRONE: return f ? SPR_DRONE1 : SPR_DRONE0;
    case E_DART: return SPR_DART;
    case E_POD: return (e.timer % 70) < 10 ? SPR_POD1 : SPR_POD0;
    default: return (frameNo >> 2) & 1 ? SPR_BOSS1 : SPR_BOSS0;
  }
}

template <typename T, size_t N>
static T* alloc(T (&arr)[N]) {
  for (auto& a : arr) if (!a.on) { memset(&a, 0, sizeof(T)); a.on = true; return &a; }
  return nullptr;
}

static void addShake(float s) { shake = max(shake, s); }

static void popup(float x, float y, const char* t) {
  Popup* p = alloc(popups);
  if (!p) return;
  p->x = x; p->y = y; p->life = 40;
  strncpy(p->txt, t, 7);
}

static void explode(float x, float y, int n, float speed, bool big) {
  for (int i = 0; i < n; i++) {
    Particle* p = alloc(parts);
    if (!p) break;
    float a = frand() * 6.2831853f, sp = frange(0.2f, 1.0f) * speed;
    p->x = x; p->y = y; p->vx = cosf(a) * sp; p->vy = sinf(a) * sp;
    p->maxLife = p->life = (uint8_t)(frange(16, 36) * (big ? 1.5f : 1.0f));
    p->kind = (rnd() % 5 == 0) ? 1 : 0;
    p->size = (big && rnd() % 3 == 0) ? 2 : 1;
  }
  Ring* r = alloc(rings);
  if (r) { r->x = x; r->y = y; r->r = 2; r->vr = big ? 2.2f : 1.4f; r->life = big ? 22 : 14; }
  addShake(big ? 9 : 2.5f);
}

static void spawnEnemy(uint8_t type, float y, float param) {
  Enemy* e = alloc(enemies);
  if (!e) return;
  e->type = type; e->x = SW + 4; e->y = y; e->baseY = y; e->param = param;
  float lv = level - 1;
  switch (type) {
    case E_DRONE: e->hp = 2; e->vx = -1.3f - lv * 0.12f; e->timer = 60 + rnd() % 120; break;
    case E_DART: e->hp = 1; e->vx = -3.0f - lv * 0.2f; break;
    case E_POD: e->hp = 9 + level * 2; e->vx = -1.6f; e->param = frange(236, 280); e->timer = 0; break;
    case E_BOSS:
      e->hp = 280 + level * 90; e->vx = -0.8f; e->x = SW + 10; e->y = 90; e->baseY = 90;
      bossActive = true; bossIdx = e - enemies;
      break;
  }
}

static void queueSpawn(int delay, uint8_t type, float y, float param = 0) {
  Pending* p = alloc(pending);
  if (!p) return;
  p->delay = delay; p->type = type; p->y = y; p->param = param;
}

static void fireEnemyShot(float x, float y, float angle, float speed) {
  Shot* s = alloc(eshots);
  if (!s) return;
  s->x = x - 3.5f; s->y = y - 3.5f; s->vx = cosf(angle) * speed; s->vy = sinf(angle) * speed;
}
static float aimAt(float x, float y) { return atan2f((pl.y + 8) - y, (pl.x + 16) - x); }
static float bulletSpeed() { return 1.9f + (level - 1) * 0.2f; }

static void spawnWave() {
  int r = rnd() % 100;
  int extra = min(level - 1, 3);
  if (r < 34) {
    float y = frange(30, 170);
    float ph = frand() * 6.28f;
    for (int i = 0; i < 5 + extra; i++) queueSpawn(i * 12, E_DRONE, y, ph);
  } else if (r < 58) {
    float y = frange(30, 185);
    for (int i = 0; i < 3 + extra; i++) queueSpawn(i * 9, E_DART, y + ((i & 1) ? 12 : -12));
  } else if (r < 80 && levelFrames > 60 * 8) {
    queueSpawn(0, E_POD, frange(30, 170));
    if (level > 1) queueSpawn(90, E_POD, frange(30, 170));
  } else {
    for (int i = 0; i < 4 + extra / 2; i++) {
      queueSpawn(i * 14, E_DRONE, 55, 0);
      queueSpawn(i * 14, E_DRONE, 160, 3.14159f);
    }
  }
}

static void resetGame() {
  memset(enemies, 0, sizeof(enemies)); memset(pshots, 0, sizeof(pshots));
  memset(eshots, 0, sizeof(eshots)); memset(parts, 0, sizeof(parts));
  memset(rings, 0, sizeof(rings)); memset(pickups, 0, sizeof(pickups));
  memset(popups, 0, sizeof(popups)); memset(pending, 0, sizeof(pending));
  pl = {};
  pl.x = -32; pl.y = 110; pl.lives = START_LIVES; pl.bombs = START_BOMBS; pl.weapon = 1;
  pl.alive = true; pl.invuln = 120;
  score = 0; level = 1; levelFrames = 0; waveTimer = 90; newHi = false;
  bossActive = false; bossIdx = -1; warnTimer = clearTimer = overTimer = 0; bannerTimer = 150;
}

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}

static void dropPickup(float x, float y, int chancePct) {
  if ((int)(rnd() % 100) >= chancePct) return;
  Pickup* p = alloc(pickups);
  if (!p) return;
  int r = rnd() % 100;
  p->type = r < 58 ? 0 : r < 84 ? 1 : 2;  // P, S, B
  p->x = x; p->y = y;
}

static void killEnemy(Enemy& e) {
  const Sprite& s = sprOf(e);
  float cx = e.x + s.w / 2, cy = e.y + s.h / 2;
  static const uint16_t pts[] = {100, 150, 500, 10000};
  uint32_t v = pts[e.type] * (e.type == E_BOSS ? level : 1);
  addScore(v);
  char buf[8]; snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
  if (e.type == E_BOSS) {
    e.timer = -1;  // enter dying sequence (handled in update)
    e.hp = 0;
    return;
  }
  popup(cx - 6, cy - 8, buf);
  bool big = e.type == E_POD;
  explode(cx, cy, big ? 40 : 18, big ? 2.6f : 1.8f, big);
  audio::play(big ? SFX_BIG_EXPLODE : SFX_EXPLODE);
  dropPickup(cx - 6, cy - 6, e.type == E_POD ? 60 : 9);
  e.on = false;
}

static void damageEnemy(Enemy& e, int dmg) {
  if (e.hp <= 0) return;
  e.hp -= dmg;
  e.flash = 3;
  if (e.hp <= 0) killEnemy(e);
  else audio::play(SFX_HIT);
}

static void playerHit() {
  if (pl.invuln > 0 || !pl.alive) return;
  if (pl.shield > 0) {
    pl.shield = 0; pl.invuln = 60;
    explode(pl.x + 16, pl.y + 8, 14, 1.5f, false);
    audio::play(SFX_HIT);
    input::rumble(150, 0x60, 0x20);
    return;
  }
  pl.alive = false;
  pl.respawn = 100;
  explode(pl.x + 16, pl.y + 8, 60, 3.0f, true);
  audio::play(SFX_PLAYER_DIE);
  input::rumble(500, 0xFF, 0xFF);
  pl.weapon = max(1, pl.weapon - 1);
}

static void useBomb() {
  if (pl.bombs <= 0 || !pl.alive) return;
  pl.bombs--;
  flashTimer = 10;
  addShake(12);
  audio::play(SFX_BOMB);
  input::rumble(600, 0xC0, 0xFF);
  for (auto& s : eshots) if (s.on) { explode(s.x + 3, s.y + 3, 2, 1.0f, false); s.on = false; }
  for (auto& e : enemies) if (e.on) damageEnemy(e, e.type == E_BOSS ? 40 : 30);
  pl.invuln = max(pl.invuln, 60);
}

// ------------------------------------------------------------ update
static void updatePlayer(const Pad& in) {
  if (!pl.alive) {
    if (--pl.respawn <= 0) {
      if (pl.lives <= 0) { state = ST_OVER; overTimer = 0; audio::music(SONG_GAMEOVER);
        if (newHi) { arcade::saveHi(hiscore); }
        return; }
      pl.lives--;
      pl.alive = true; pl.x = -32; pl.y = 110; pl.invuln = 150; pl.bombs = max(pl.bombs, START_BOMBS);
    }
    return;
  }
  const float spd = 2.5f;
  if (pl.x < 20 && pl.invuln > 100) pl.x += 1.5f;  // fly in after respawn
  else { pl.x += in.ax * spd; pl.y += in.ay * spd; }
  pl.x = constrain(pl.x, 0.0f, (float)(SW - 34));
  pl.y = constrain(pl.y, 14.0f, (float)(SH - 18));
  if (pl.invuln > 0) pl.invuln--;

  if (pl.fireCd > 0) pl.fireCd--;
  if (in.down(BTN_A | BTN_R1 | BTN_R2 | BTN_X) && pl.fireCd == 0) {
    pl.fireCd = 7;
    auto shot = [](float x, float y, float vx, float vy, uint8_t k) {
      Shot* s = alloc(pshots);
      if (s) { s->x = x; s->y = y; s->vx = vx; s->vy = vy; s->kind = k; }
    };
    if (pl.weapon == 1) shot(pl.x + 26, pl.y + 5, 7.5f, 0, 0);
    else { shot(pl.x + 24, pl.y + 2, 7.5f, 0, 0); shot(pl.x + 24, pl.y + 9, 7.5f, 0, 0); }
    if (pl.weapon >= 3) { shot(pl.x + 20, pl.y + 4, 6.5f, -1.6f, 1); shot(pl.x + 20, pl.y + 5, 6.5f, 1.6f, 1); }
    audio::play(SFX_SHOOT);
  }
  if (in.hit(BTN_B | BTN_L1 | BTN_Y)) useBomb();

  // engine exhaust particles
  if ((frameNo & 1) == 0) {
    Particle* p = alloc(parts);
    if (p) {
      p->x = pl.x + 1; p->y = pl.y + 7.5f + frange(-1, 1);
      p->vx = -frange(1.0f, 2.2f); p->vy = frange(-0.2f, 0.2f);
      p->maxLife = p->life = 12; p->kind = 2; p->size = 1;
    }
  }
}

static void updateEnemies() {
  float py = pl.y + 8;
  for (auto& e : enemies) {
    if (!e.on) continue;
    if (e.flash) e.flash--;
    e.t += 1;
    const Sprite& s = sprOf(e);
    float cx = e.x + s.w / 2, cy = e.y + s.h / 2;
    switch (e.type) {
      case E_DRONE:
        e.x += e.vx;
        e.y = e.baseY + 26 * sinf(e.t * 0.055f + e.param);
        if (level >= 2 && --e.timer <= 0 && e.x < SW - 20 && e.x > 60) {
          fireEnemyShot(cx, cy, aimAt(cx, cy), bulletSpeed());
          e.timer = 150 + rnd() % 120;
        }
        break;
      case E_DART:
        if (e.t < 45) { e.vy += (py > cy ? 0.05f : -0.05f); e.vy = constrain(e.vy, -0.9f, 0.9f); }
        e.x += e.vx; e.y += e.vy;
        break;
      case E_POD:
        e.timer++;
        if (e.timer < 420) {
          if (e.x > e.param) e.x += e.vx;
          e.y = e.baseY + 8 * sinf(e.t * 0.04f);
          if (e.timer % 70 == 10 && e.x < SW - 10) {
            float a = aimAt(e.x, cy);
            for (int k = -1; k <= 1; k++) fireEnemyShot(e.x + 2, cy, a + k * 0.22f, bulletSpeed());
          }
        } else {
          e.x -= 1.4f;
        }
        break;
      case E_BOSS: {
        if (e.timer < 0) {  // dying: chain of explosions
          e.timer--;
          if ((e.timer & 7) == 0) {
            explode(e.x + frange(10, 74), e.y + frange(8, 54), 16, 2.0f, false);
            audio::play(SFX_EXPLODE);
          }
          if (e.timer < -100) {
            explode(cx, cy, 120, 4.0f, true);
            flashTimer = 8; addShake(16);
            audio::play(SFX_BIG_EXPLODE);
            input::rumble(700, 0xFF, 0xFF);
            e.on = false; bossActive = false; bossIdx = -1;
            clearTimer = 240;
            addScore(5000 * level);
            audio::music(SONG_STAGE);
          }
          break;
        }
        if (e.x > 214) e.x += e.vx;
        e.y = e.baseY + 55 * sinf(e.t * 0.012f);
        e.timer++;
        bool angry = e.hp < (280 + level * 90) / 2;
        int cyc = e.timer % 600;
        float coreX = e.x + 12, coreY = e.y + 32;
        if (cyc < 200) {                       // fans
          if (cyc % (angry ? 50 : 70) == 0)
            for (int k = -5; k <= 5; k++) fireEnemyShot(coreX, coreY, 3.14159f + k * 0.16f, bulletSpeed() * 0.9f);
        } else if (cyc < 380) {                // aimed triples
          if (cyc % (angry ? 16 : 24) == 0) {
            float a = aimAt(coreX, coreY);
            for (int k = -1; k <= 1; k++) fireEnemyShot(coreX, coreY, a + k * 0.12f, bulletSpeed() * 1.2f);
          }
        } else if (cyc < 520) {                // spiral
          if (cyc % (angry ? 4 : 6) == 0) {
            float a = e.timer * 0.21f;
            fireEnemyShot(coreX + 20, coreY, a, bulletSpeed() * 0.8f);
            if (angry) fireEnemyShot(coreX + 20, coreY, a + 3.14159f, bulletSpeed() * 0.8f);
          }
        } else if (cyc == 540) {               // escorts
          queueSpawn(0, E_DRONE, 40, 0); queueSpawn(12, E_DRONE, 40, 0);
          queueSpawn(0, E_DRONE, 190, 3.14f); queueSpawn(12, E_DRONE, 190, 3.14f);
        }
        break;
      }
    }
    if (e.x < -s.w - 10 || e.y > SH + 40 || e.y < -60) e.on = false;
    // body collision with player
    if (e.on && pl.alive && e.hp > 0) {
      float bx = e.x + 2, by = e.y + 2, bw = s.w - 4, bh = s.h - 4;
      if (e.type == E_BOSS) { bx = e.x + 6; by = e.y + 12; bw = s.w - 14; bh = s.h - 24; }
      if (overlap(pl.x + 6, pl.y + 5, 20, 7, bx, by, bw, bh)) {
        playerHit();
        if (e.type != E_BOSS) damageEnemy(e, 50);
      }
    }
  }
}

static void updateShots() {
  for (auto& s : pshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if (s.x > SW || s.y < -10 || s.y > SH) { s.on = false; continue; }
    float w = s.kind ? 8 : 12, h = s.kind ? 8 : 5;
    for (auto& e : enemies) {
      if (!e.on || e.hp <= 0) continue;
      const Sprite& sp = sprOf(e);
      float bx = e.x + 2, by = e.y + 2, bw = sp.w - 4, bh = sp.h - 4;
      if (e.type == E_BOSS) { bx = e.x + 4; by = e.y + 10; bw = sp.w - 10; bh = sp.h - 20; }
      if (overlap(s.x, s.y, w, h, bx, by, bw, bh)) {
        damageEnemy(e, 1);
        for (int i = 0; i < 3; i++) {
          Particle* p = alloc(parts);
          if (p) { p->x = s.x + w; p->y = s.y + h / 2; p->vx = frange(-2, 0.5f); p->vy = frange(-1.2f, 1.2f);
                   p->maxLife = p->life = 8; p->kind = 3; p->size = 1; }
        }
        s.on = false;
        break;
      }
    }
  }
  float hx = pl.x + 17, hy = pl.y + 8;
  for (auto& s : eshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if (s.x < -8 || s.x > SW + 8 || s.y < -8 || s.y > SH + 8) { s.on = false; continue; }
    if (pl.alive) {
      float dx = (s.x + 3.5f) - hx, dy = (s.y + 3.5f) - hy;
      float r = pl.shield ? 13 : 0;
      if (pl.shield && dx * dx + dy * dy < r * r) {
        s.on = false; explode(s.x + 3, s.y + 3, 4, 1.0f, false);
        if (--pl.shield <= 0) pl.shield = 0;
        continue;
      }
      if (fabsf(dx) < 6 && fabsf(dy) < 4) { s.on = false; playerHit(); }
    }
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy;
    p.vx *= 0.96f; p.vy *= 0.96f;
    if (p.kind == 1) p.vy += 0.03f;
    if (--p.life == 0) p.on = false;
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    r.r += r.vr; r.vr *= 0.94f;
    if (--r.life == 0) r.on = false;
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    p.y -= 0.5f;
    if (--p.life == 0) p.on = false;
  }
  for (auto& p : pickups) {
    if (!p.on) continue;
    p.t += 1; p.x -= 0.7f; p.y += sinf(p.t * 0.08f) * 0.5f;
    if (p.x < -16) { p.on = false; continue; }
    if (pl.alive && overlap(pl.x, pl.y, 32, 16, p.x, p.y, 13, 13)) {
      p.on = false;
      audio::play(SFX_POWERUP);
      input::rumble(80, 0x40, 0);
      if (p.type == 0) {
        if (pl.weapon < 3) { pl.weapon++; popup(p.x - 8, p.y - 6, "POWER"); }
        else { addScore(1000); popup(p.x - 8, p.y - 6, "1000"); }
      } else if (p.type == 1) { pl.shield = 3; popup(p.x - 8, p.y - 6, "SHIELD"); }
      else { pl.bombs = min(pl.bombs + 1, 5); popup(p.x - 4, p.y - 6, "BOMB"); }
    }
  }
  if (shake > 0.3f) {
    shakeX = (int)frange(-shake, shake); shakeY = (int)frange(-shake, shake);
    shake *= 0.85f;
  } else { shake = 0; shakeX = shakeY = 0; }
  if (flashTimer) flashTimer--;
}

static void updateDirector() {
  levelFrames++;
  for (auto& p : pending) {
    if (!p.on) continue;
    if (--p.delay <= 0) { spawnEnemy(p.type, p.y, p.param); p.on = false; }
  }
  if (bannerTimer) bannerTimer--;
  if (clearTimer) {
    if (--clearTimer == 0) { level++; levelFrames = 0; bannerTimer = 150; waveTimer = 60; }
    return;
  }
  const uint32_t bossAt = 60 * 55;
  if (!bossActive && levelFrames == bossAt) {
    warnTimer = 180;
    audio::music(SONG_NONE);
    audio::play(SFX_WARNING);
  }
  if (warnTimer) {
    if (--warnTimer == 0) { spawnEnemy(E_BOSS, 90, 0); audio::music(SONG_BOSS); }
    return;
  }
  if (bossActive || levelFrames > bossAt) return;
  if (--waveTimer <= 0) {
    spawnWave();
    waveTimer = max(50, 125 - level * 12 - (int)(levelFrames / 500));
  }
}

// ------------------------------------------------------------ draw
static const uint16_t* fireRamp() {
  static uint16_t r[6];
  static bool init = false;
  if (!init) {
    const uint8_t idx[6] = {7, 14, 13, 12, 11, 20};
    for (int i = 0; i < 6; i++) r[i] = pal[idx[i]];
    init = true;
  }
  return r;
}

static void drawWorld() {
  int ox = shakeX, oy = shakeY;
  bool blink = (frameNo >> 2) & 1;
  for (auto& p : pickups) {
    if (!p.on) continue;
    const Sprite& s = p.type == 0 ? SPR_PICKP : p.type == 1 ? SPR_PICKS : SPR_PICKB;
    int x = (int)p.x + ox, y = (int)p.y + oy;
    blit(s, x, y);
    // orbiting sparkle so pickups catch the eye
    float a = frameNo * 0.15f;
    pset(x + 6 + (int)(9 * cosf(a)), y + 6 + (int)(9 * sinf(a)), pal[7]);
    pset(x + 6 - (int)(9 * cosf(a)), y + 6 - (int)(9 * sinf(a)), pal[14]);
  }
  for (auto& e : enemies) {
    if (!e.on) continue;
    const Sprite& s = sprOf(e);
    blit(s, (int)e.x + ox, (int)e.y + oy, e.flash ? palWhite : pal);
  }
  for (auto& s : pshots) {
    if (!s.on) continue;
    blit(s.kind ? SPR_PSPREAD : SPR_PBULLET, (int)s.x + ox, (int)s.y + oy);
  }
  if (pl.alive && !(pl.invuln && blink && pl.invuln < 140)) {
    int x = (int)pl.x + ox, y = (int)pl.y + oy;
    blit((frameNo & 2) ? SPR_FLAME0 : SPR_FLAME1, x - 9, y + 5);
    blit(SPR_PLAYER, x, y);
    if (pl.shield) {
      int cx = x + 17, cy = y + 8;
      if (rowsVisible(cy - 15, 31)) {
        uint16_t c = (frameNo & 4) ? c565(62, 198, 224) : c565(184, 243, 255);
        S->drawCircle(cx, cy - Y0, 14, c);
        if (pl.shield > 1) S->drawCircle(cx, cy - Y0, 12, c565(31, 111, 139));
      }
    }
  }
  const uint16_t* ramp = fireRamp();
  static const uint16_t trail[4] = {rgbS(184, 243, 255), rgbS(62, 198, 224), rgbS(31, 111, 139), rgbS(27, 31, 74)};
  for (auto& p : parts) {
    if (!p.on) continue;
    int x = (int)p.x + ox, y = (int)p.y + oy;
    if (y < Y0 - 2 || y >= Y0 + STRIP) continue;
    uint16_t c;
    int age = (p.maxLife - p.life) * 6 / p.maxLife;
    if (p.kind == 1) c = pal[(p.life & 4) ? 9 : 8];
    else if (p.kind == 2) c = trail[min(3, age * 4 / 6)];
    else if (p.kind == 3) c = pal[p.life > 4 ? 7 : 14];
    else c = ramp[min(5, age)];
    pset(x, y, c);
    if (p.size > 1) { pset(x + 1, y, c); pset(x, y + 1, c); pset(x + 1, y + 1, c); }
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    int x = (int)r.x + ox, y = (int)r.y + oy, rad = (int)r.r;
    if (!rowsVisible(y - rad - 1, rad * 2 + 3)) continue;
    S->drawCircle(x, y - Y0, rad, r.life > 8 ? c565(255, 240, 200) : c565(255, 138, 61));
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    blit(blink ? SPR_EBULLET1 : SPR_EBULLET0, (int)s.x + ox, (int)s.y + oy);
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    text(p.txt, (int)p.x, (int)p.y, (p.life & 4) ? c565(255, 216, 74) : TFT_WHITE);
  }
}

static void drawHud() {
  if (Y0 == 0) {
    // darken a band behind the HUD for legibility
    for (int i = 0; i < SW * 12; i++) {
      uint16_t c = sw16(B[i]);
      B[i] = sw16((c >> 1) & 0x7BEF);
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
    text(buf, 4, 2, TFT_WHITE);
    snprintf(buf, sizeof(buf), "HI %07lu", (unsigned long)hiscore);
    text(buf, 160, 2, c565(255, 216, 74), 1, top_center);
    for (int i = 0; i < min(pl.lives, 5); i++) blit(SPR_LIFEICON, SW - 14 - i * 13, 2);
    for (int i = 0; i < min(pl.bombs, 5); i++) {
      int x = 60 + i * 9;
      rectf(x, 3, 6, 6, pal[15]); rectf(x + 1, 4, 4, 4, pal[16]); pset(x + 2, 5, pal[7]);
    }
    for (int i = 0; i < pl.weapon; i++) rectf(108 + i * 5, 4, 3, 5, pal[13]);
  }
  if (bossActive && bossIdx >= 0 && rowsVisible(14, 6)) {
    Enemy& b = enemies[bossIdx];
    int maxHp = 280 + level * 90;
    int w = max(0, (int)b.hp) * 200 / maxHp;
    rectf(59, 14, 202, 5, pal[1]);
    rectf(60, 15, w, 3, (frameNo & 8) && b.hp < maxHp / 4 ? pal[14] : pal[12]);
  }
}

static void drawOverlays() {
  bool blink = (frameNo >> 4) & 1;
  const Pad& in = input::pad;
  if (state == ST_TITLE) {
    blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 34);
    text("A RETRO SHOOT-'EM-UP", SW / 2, 74, c565(184, 243, 255), 1, top_center);
    int y = 120 + (int)(6 * sinf(frameNo * 0.05f));
    blit((frameNo & 2) ? SPR_FLAME0 : SPR_FLAME1, 60 - 9, y + 5);
    blit(SPR_PLAYER, 60, y);
    if (in.connected) {
      if (blink) text("PRESS START", SW / 2, 150, TFT_WHITE, 2, top_center);
    } else {
      text("PAIR YOUR CONTROLLER", SW / 2, 146, TFT_WHITE, 1, top_center);
      text("STADIA: HOLD Y + STADIA FOR 2S", SW / 2, 158, blink ? c565(255, 138, 61) : c565(255, 216, 74), 1, top_center);
    }
    text("A/R2 FIRE   B/L1 BOMB   START PAUSE", SW / 2, 186, c565(180, 194, 220), 1, top_center);
    text("SELECT+START: BACK TO MENU", SW / 2, 198, c565(115, 132, 168), 1, top_center);
    char buf[24];
    snprintf(buf, sizeof(buf), "HI-SCORE %07lu", (unsigned long)hiscore);
    text(buf, SW / 2, 20, c565(255, 216, 74), 1, top_center);
  } else if (state == ST_OVER) {
    text("GAME OVER", SW / 2, 80, c565(255, 56, 79), 3, top_center);
    char buf[24];
    snprintf(buf, sizeof(buf), "SCORE %07lu", (unsigned long)score);
    text(buf, SW / 2, 118, TFT_WHITE, 2, top_center);
    if (newHi && blink) text("NEW HI-SCORE!", SW / 2, 142, c565(255, 216, 74), 2, top_center);
    if (overTimer > 90) text("PRESS START", SW / 2, 172, c565(184, 243, 255), 1, top_center);
  }
  if (state == ST_PLAY) {
    if (bannerTimer) {
      char buf[16]; snprintf(buf, sizeof(buf), "STAGE %d", level);
      text(buf, SW / 2, 96, TFT_WHITE, 3, top_center);
      text("GET READY", SW / 2, 126, c565(255, 216, 74), 1, top_center);
    }
    if (warnTimer && ((warnTimer >> 3) & 1)) {
      if (rowsVisible(100, 36)) {
        rectf(0, 100, SW, 36, pal[11]);
        rectf(0, 102, SW, 2, pal[14]); rectf(0, 132, SW, 2, pal[14]);
      }
      text("WARNING", SW / 2, 106, TFT_WHITE, 3, top_center);
    }
    if (clearTimer) {
      text("STAGE CLEAR", SW / 2, 96, c565(182, 255, 110), 3, top_center);
      char buf[24]; snprintf(buf, sizeof(buf), "BONUS %d", 5000 * level);
      text(buf, SW / 2, 128, TFT_WHITE, 2, top_center);
    }
  }
}

static void draw() {
  drawBackground();
  if (state != ST_TITLE) drawWorld();
  if (state == ST_PLAY) drawHud();
  drawOverlays();
}

// ------------------------------------------------------------ main loop
static void step(const Pad& in) {
  switch (state) {
    case ST_TITLE:
      scrollBackground(0.6f);
      if (in.hit(BTN_LEFT)) arcade::changeVolume(-1);
      if (in.hit(BTN_RIGHT)) arcade::changeVolume(+1);
      if (in.hit(BTN_START | BTN_A)) {
        resetGame();
        state = ST_PLAY;
        audio::play(SFX_START);
        audio::music(SONG_STAGE);
      }
      break;
    case ST_PLAY:
      scrollBackground(1.0f);
      updateDirector();
      updatePlayer(in);
      if (state != ST_PLAY) break;
      updateEnemies();
      updateShots();
      updateFx();
      if (in.hit(BTN_START)) arcade::pause();
      break;
    default:
      overTimer++;
      scrollBackground(0.4f);
      updateFx();
      if (overTimer > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; audio::music(SONG_TITLE); }
      break;
  }
}

void setup() {
  arcade::begin("novalance", &MUSIC);
  setPalette(PAL565, PAL_SIZE);
  buildBackground();
  hiscore = arcade::loadHi(20000);
  audio::music(SONG_TITLE);
}

void loop() { arcade::run(step, draw); }
