// =====================================================================
//  NOVA ARCADE LAUNCHER
//  Lives in the factory partition. Lists the games in /arcade on the SD
//  card, installs the chosen .bin into the game slot (ota_0) and boots it.
//  Games return here on reset or with SELECT+START / the Stadia button.
//
//  SD card layout:
//    /arcade/NovaLance.bin   game firmware (required)
//    /arcade/NovaLance.png   160x120 thumbnail (optional)
//    /arcade/NovaLance.txt   line 1 title, line 2 description, line 3 save id (optional)
// =====================================================================
#ifndef ARCADE_SIM
#include <SD_MMC.h>
#include <Update.h>
#include <esp_ota_ops.h>
#endif
#include <ArcadeCore.h>
#include "assets.h"

using namespace gfx;

// ------------------------------------------------------------ music (original)
namespace lm {
using namespace audio;
static const uint8_t LEADS[][16] = {
  {76, 1, 1, 1, 1, 1, 1, 1, 79, 1, 1, 1, 1, 1, 1, 1},
  {77, 1, 1, 1, 1, 1, 1, 1, 76, 1, 1, 1, 72, 1, 1, 1},
  {79, 1, 1, 1, 1, 1, 1, 1, 84, 1, 1, 1, 1, 1, 1, 1},
  {83, 1, 1, 1, 1, 1, 1, 1, 81, 1, 1, 1, 79, 1, 0, 0},
};
static const char* const DRUMS[] = {"....h.......h...", "K.......K...h...", "................"};
static const Bar BARS[] = {
  {AM, -1, 2}, {F_, -1, 2}, {C_, -1, 2}, {G_, -1, 2},
  {AM, 0, 0}, {F_, 1, 0}, {C_, 2, 1}, {G_, 3, 1},
};
static const SongDef SONGS[] = {{nullptr, 0, 100, false, false, 0}, {BARS, 8, 92, true, false, 0x30}};
}  // namespace lm
static const audio::Music MUSIC = {audio::STD_CHORDS, lm::LEADS, lm::DRUMS, lm::SONGS, 2};

// ------------------------------------------------------------ game list
static const int TW = 160, TH = 120;   // thumbnail size
struct Game {
  char path[72];
  char title[28];
  char desc[56];
  char id[16];
  uint32_t size;
  uint32_t mtime;
  uint16_t* thumb;   // swapped RGB565, TW x TH, or nullptr
  uint32_t hi;
};
static Game games[24];
static int ngames = 0;
static int sel = 0;
static float pos = 0;
enum LState { L_MENU, L_NOSD, L_EMPTY, L_INSTALL, L_BOOT };
static LState lstate = L_MENU;
static int installPct = 0;
static const char* installTitle = "";
static input::Repeat repL, repR;
static int bootTimer = 0;

static void niceTitle(const char* stem, char* out, size_t n) {
  // "NovaLance" -> "NOVA LANCE", "brick_storm" -> "BRICK STORM"
  size_t j = 0;
  for (size_t i = 0; stem[i] && j + 2 < n; i++) {
    char c = stem[i];
    if (c == '_' || c == '-') c = ' ';
    if (i > 0 && isupper((unsigned char)c) && islower((unsigned char)stem[i - 1]) && j + 2 < n) out[j++] = ' ';
    out[j++] = toupper((unsigned char)c);
  }
  out[j] = 0;
}

static void sortGames() {
  for (int i = 1; i < ngames; i++)
    for (int k = i; k > 0 && strcmp(games[k].title, games[k - 1].title) < 0; k--) {
      Game t = games[k]; games[k] = games[k - 1]; games[k - 1] = t;
    }
}

static void readHiScores() {
  for (int i = 0; i < ngames; i++) {
    Preferences p;
    games[i].hi = 0;
    if (p.begin(games[i].id, true)) { games[i].hi = p.getUInt("hi", 0); p.end(); }
  }
}

#ifndef ARCADE_SIM
static bool sdBegin() {
  SD_MMC.end();
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN, SD_D1_PIN, SD_D2_PIN, SD_D3_PIN);
  if (SD_MMC.begin("/sdcard", false)) return true;
  SD_MMC.end();
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);   // fall back to 1-bit mode
  return SD_MMC.begin("/sdcard", true);
}

static void readLine(File& f, char* out, size_t n) {
  size_t j = 0;
  while (f.available()) {
    int c = f.read();
    if (c == '\n') break;
    if (c == '\r') continue;
    if (j + 1 < n) out[j++] = (char)c;
  }
  out[j] = 0;
}

static uint16_t* loadThumb(const char* png) {
  if (!SD_MMC.exists(png)) return nullptr;
  auto* spr = new LGFX_Sprite();
  spr->setColorDepth(16);
  spr->setPsram(true);
  if (!spr->createSprite(TW, TH)) { spr->setPsram(false); if (!spr->createSprite(TW, TH)) { delete spr; return nullptr; } }
  spr->fillSprite(0);
  spr->drawPngFile(SD_MMC, png, 0, 0);
  return (uint16_t*)spr->getBuffer();
}

static void scanGames() {
  ngames = 0;
  File dir = SD_MMC.open("/arcade");
  if (!dir || !dir.isDirectory()) return;
  File f;
  while ((f = dir.openNextFile()) && ngames < (int)(sizeof(games) / sizeof(games[0]))) {
    if (f.isDirectory()) continue;
    String name = f.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.startsWith(".") || !name.endsWith(".bin")) continue;
    String stem = name.substring(0, name.length() - 4);
    Game& g = games[ngames];
    memset(&g, 0, sizeof(g));
    snprintf(g.path, sizeof(g.path), "/arcade/%s", name.c_str());
    g.size = f.size();
    g.mtime = (uint32_t)f.getLastWrite();
    niceTitle(stem.c_str(), g.title, sizeof(g.title));
    String low = stem; low.toLowerCase();
    strncpy(g.id, low.c_str(), sizeof(g.id) - 1);
    String txt = "/arcade/" + stem + ".txt";
    if (SD_MMC.exists(txt)) {
      File t = SD_MMC.open(txt);
      char line[64];
      readLine(t, line, sizeof(line)); if (line[0]) strncpy(g.title, line, sizeof(g.title) - 1);
      readLine(t, line, sizeof(line)); strncpy(g.desc, line, sizeof(g.desc) - 1);
      readLine(t, line, sizeof(line)); if (line[0]) strncpy(g.id, line, sizeof(g.id) - 1);
      t.close();
    }
    g.thumb = loadThumb(("/arcade/" + stem + ".png").c_str());
    ngames++;
  }
  sortGames();
}
#else
// ---- simulator: fake SD card
static uint16_t* simThumb(const char* ppm) {
  FILE* f = fopen(ppm, "rb");
  if (!f) return nullptr;
  int w, h, m;
  if (fscanf(f, "P6 %d %d %d", &w, &h, &m) != 3) { fclose(f); return nullptr; }
  fgetc(f);
  auto* t = new uint16_t[TW * TH];
  unsigned char* rgb = new unsigned char[w * h * 3];
  fread(rgb, 1, w * h * 3, f); fclose(f);
  for (int y = 0; y < TH; y++) for (int x = 0; x < TW; x++) {
    unsigned char* p = rgb + ((y * h / TH) * w + (x * w / TW)) * 3;
    t[y * TW + x] = rgbS(p[0], p[1], p[2]);
  }
  delete[] rgb;
  return t;
}
static bool sdBegin() { return true; }
static void scanGames() {
  const char* names[][3] = {{"NOVA LANCE", "Side-scrolling synthwave shoot-'em-up", "out/nl_play.ppm"},
                            {"BLOCKFALL", "Stack falling blocks, clear lines", "out/bf_play.ppm"},
                            {"BRICK STORM", "Smash every brick with power-ups", "out/bs_stage4.ppm"},
                            {"ALIEN TIDE", "Hold back the descending waves", "out/at_play.ppm"},
                            {"NEON SERPENT", "Eat, grow longer, dodge the walls", "out/ns_level3.ppm"},
                            {"ASTRO DRIFT", "Split space rocks, dodge the hunters", "out/ad_play.ppm"},
                            {"HOP RUSH", "Cross the road, ride the river", "out/hr_play.ppm"},
                            {"VOLT RALLY", "Paddle tennis against six rivals", "out/vr_play.ppm"},
                            {"MAZE MUNCH", "Clear the maze, outrun the wisps", "out/mm_play.ppm"},
                            {"PIXEL PEAKS", "A little judoka climbs three mountain worlds", "out/pp_w1a.ppm"},
                            {"TURBO HORIZON", "Race the clock down a sunset highway", "out/th_s1a.ppm"},
                            {"GEM CASCADE", "Swap gems, chain cascades, set off novas", "out/gc_special.ppm"},
                            {"CITY SHIELD", "Intercept the warheads, save the cities", "out/cs_play.ppm"}};
  ngames = sizeof(names) / sizeof(names[0]);
  for (int i = 0; i < ngames; i++) {
    memset(&games[i], 0, sizeof(Game));
    strcpy(games[i].title, names[i][0]); strcpy(games[i].desc, names[i][1]);
    snprintf(games[i].id, 16, "g%d", i);
    games[i].thumb = simThumb(names[i][2]);
    games[i].hi = i == 0 ? 31200 : 0;
  }
}
#endif

// ------------------------------------------------------------ launching
static void restartInto(bool slot) {
#ifndef ARCADE_SIM
  audio::music(0);
  delay(150);
  if (slot) {
    const esp_partition_t* p = esp_ota_get_next_update_partition(nullptr);
    if (p) esp_ota_set_boot_partition(p);
  }
  for (int b = 200; b >= 0; b -= 25) { lcd.setBrightness(b); delay(12); }
  lcd.fillScreen(0);
  esp_restart();
#endif
}

static void launch(Game& g) {
  audio::play(SFX_SELECT);
  arcade::shared.putString("last", g.id);
  char key[96];
  snprintf(key, sizeof(key), "%s|%u|%u", g.path, (unsigned)g.size, (unsigned)g.mtime);
  char have[96] = {0};
  arcade::shared.getString("slot", have, sizeof(have));
  installTitle = g.title;
  if (strcmp(key, have) == 0) {      // already installed: boot straight away
    lstate = L_BOOT; bootTimer = 20;
    return;
  }
#ifndef ARCADE_SIM
  lstate = L_INSTALL; installPct = 0;
  File f = SD_MMC.open(g.path);
  if (!f || !Update.begin(f.size(), U_FLASH)) {
    arcade::toast("INSTALL FAILED: NOT ENOUGH SPACE?"); lstate = L_MENU; return;
  }
  static uint8_t buf[8192];
  size_t done = 0, total = f.size(), lastDraw = 0;
  while (f.available()) {
    size_t r = f.read(buf, sizeof(buf));
    if (Update.write(buf, r) != r) break;
    done += r;
    installPct = (int)(done * 100 / total);
    if (done - lastDraw > 48 * 1024) { lastDraw = done; gfx::frameNo++; gfx::render(arcade::drawAll); }
  }
  f.close();
  if (!Update.end(true)) {
    arcade::toast("INSTALL FAILED - BAD .BIN FILE?"); lstate = L_MENU;
    arcade::shared.putString("slot", "");
    return;
  }
  arcade::shared.putString("slot", key);
  installPct = 100;
  gfx::render(arcade::drawAll);
  restartInto(false);   // Update.end() already selected the game slot
#else
  lstate = L_INSTALL; installPct = 64;
#endif
}

// ------------------------------------------------------------ background
static uint16_t skyPat[SH][4];
struct Star { int16_t x, y; uint8_t b; };
static Star stars[70];
static const int HORIZON = 150;

static void buildBackground() {
  struct StopC { int y; uint8_t r, g, b; };
  static const StopC stops[] = {{0, 5, 4, 18}, {70, 18, 12, 50}, {120, 50, 20, 90}, {150, 120, 40, 130},
                                {151, 12, 6, 30}, {240, 30, 10, 60}};
  for (int y = 0; y < SH; y++) {
    int i = 0;
    while (i < 4 && y >= stops[i + 1].y) i++;
    const StopC &a = stops[i], &b = stops[i + 1];
    int level = (y - a.y) * 16 / max(1, b.y - a.y);
    uint16_t ca = rgbS(a.r, a.g, a.b), cb = rgbS(b.r, b.g, b.b);
    for (int x = 0; x < 4; x++) skyPat[y][x] = (BAYER[y & 3][x] < level) ? cb : ca;
  }
  for (auto& s : stars) { s.x = rnd() % SW; s.y = rnd() % (HORIZON - 10); s.b = rnd() % 3; }
}

static void drawBackground() {
  for (int r = 0; r < STRIP; r++) {
    const uint16_t* p = skyPat[Y0 + r];
    uint32_t p01 = p[0] | (p[1] << 16), p23 = p[2] | (p[3] << 16);
    uint32_t* d = (uint32_t*)(B + r * SW);
    for (int x = 0; x < SW / 2; x += 2) { d[x] = p01; d[x + 1] = p23; }
  }
  static const uint16_t sc[3] = {rgbS(80, 80, 140), rgbS(160, 160, 220), rgbS(255, 255, 255)};
  for (auto& s : stars) {
    if (s.b == 0 && ((frameNo + s.x) & 63) < 8) continue;
    pset(s.x, s.y, sc[s.b]);
  }
  // perspective grid floor
  if (Y0 + STRIP > HORIZON) {
    uint16_t gl = rgbS(255, 60, 200), gd = rgbS(120, 30, 120);
    float phase = (frameNo % 40) / 40.0f;
    for (int k = 0; k < 12; k++) {
      float p = (k + phase) / 12.0f;
      int y = HORIZON + (int)((SH - HORIZON) * p * p);
      if (y >= Y0 && y < Y0 + STRIP) rectf(0, y, SW, 1, p > 0.35f ? gl : gd);
    }
    for (int i = -9; i <= 9; i++) {
      int xb = SW / 2 + i * 44, xh = SW / 2 + i * 6;
      line(xh, HORIZON, xb, SH - 1, c565(160, 40, 160));
    }
    rectf(0, HORIZON, SW, 1, rgbS(255, 140, 230));
  }
}

// ------------------------------------------------------------ cards
static void blitScaled(const uint16_t* src, int dx, int dy, int dw, int dh, bool dim) {
  int y0 = max(Y0, dy), y1 = min(Y0 + STRIP, dy + dh);
  int x0 = max(0, dx), x1 = min(SW, dx + dw);
  for (int yy = y0; yy < y1; yy++) {
    const uint16_t* row = src + ((yy - dy) * TH / dh) * TW;
    uint16_t* d = B + (yy - Y0) * SW;
    for (int xx = x0; xx < x1; xx++) {
      uint16_t c = row[(xx - dx) * TW / dw];
      d[xx] = dim ? sw16((sw16(c) >> 1) & 0x7BEF) : c;
    }
  }
}

static void drawPlaceholder(const Game& g, int x, int y, int w, int h, bool dim) {
  // no thumbnail: a coloured "cartridge" with the initials
  uint32_t hsh = 0;
  for (const char* p = g.title; *p; p++) hsh = hsh * 31 + *p;
  uint8_t r = 60 + (hsh & 0x7F), gg = 40 + ((hsh >> 8) & 0x7F), b = 90 + ((hsh >> 16) & 0x7F);
  if (dim) { r /= 2; gg /= 2; b /= 2; }
  for (int yy = y; yy < y + h; yy++)
    if (yy >= Y0 && yy < Y0 + STRIP) rectf(x, yy, w, 1, lerpS(r, gg, b, r / 3, gg / 3, b / 3, (float)(yy - y) / h));
  char ini[3] = {g.title[0], 0, 0};
  const char* sp = strchr(g.title, ' ');
  if (sp && sp[1]) ini[1] = sp[1];
  text(ini, x + w / 2, y + h / 2 - (w > 100 ? 12 : 6), TFT_WHITE, w > 100 ? 3 : 2, top_center);
}

static void drawCards() {
  for (int i = 0; i < ngames; i++) {
    float d = i - pos;
    if (d < -2 || d > 2) continue;
    float ad = fabsf(d);
    float s = 1.0f - min(ad, 1.0f) * 0.45f;
    int w = (int)(TW * s), h = (int)(TH * s);
    int cx = SW / 2 + (int)(d * 128);
    int x = cx - w / 2, y = 42 + (TH - h) / 2;
    if (!rowsVisible(y - 4, h + 8)) continue;
    bool dim = ad > 0.5f;
    if (!dim) {
      // animated neon frame
      float t = frameNo * 0.06f;
      uint16_t c1 = lerpS(62, 198, 224, 255, 60, 200, 0.5f + 0.5f * sinf(t));
      rect(x - 3, y - 3, w + 6, h + 6, c1);
      rect(x - 2, y - 2, w + 4, h + 4, c1);
      rect(x - 1, y - 1, w + 2, h + 2, rgbS(10, 8, 30));
    } else {
      rect(x - 1, y - 1, w + 2, h + 2, rgbS(60, 50, 110));
    }
    if (games[i].thumb) blitScaled(games[i].thumb, x, y, w, h, dim);
    else drawPlaceholder(games[i], x, y, w, h, dim);
  }
}

// ------------------------------------------------------------ screens
static void panel(int x, int y, int w, int h) {
  if (!rowsVisible(y, h)) return;
  shade(x, y, w, h); shade(x, y, w, h);
  rect(x, y, w, h, rgbS(122, 61, 184));
}

static void draw() {
  drawBackground();
  blit(SPR_LOGO, (SW - SPR_LOGO.w) / 2, 6);
  // status icons
  blit(SPR_PADICON, SW - 22, 4, input::pad.connected ? pal : palDark);
  blit(SPR_SDICON, 8, 3, lstate == L_NOSD ? palDark : pal);

  if (lstate == L_NOSD || lstate == L_EMPTY) {
    panel(30, 60, 260, 112);
    if (lstate == L_NOSD) {
      text("INSERT SD CARD", SW / 2, 74, TFT_WHITE, 2, top_center);
      text("Copy the 'arcade' folder from the", SW / 2, 104, c565(184, 243, 255), 1, top_center);
      text("SD pack onto a FAT32 card.", SW / 2, 116, c565(184, 243, 255), 1, top_center);
    } else {
      text("NO GAMES FOUND", SW / 2, 74, TFT_WHITE, 2, top_center);
      text("Put game .bin files in /arcade", SW / 2, 104, c565(184, 243, 255), 1, top_center);
      text("on the SD card.", SW / 2, 116, c565(184, 243, 255), 1, top_center);
    }
    if ((frameNo >> 4) & 1) text("PRESS A TO RETRY", SW / 2, 146, c565(255, 216, 74), 1, top_center);
    return;
  }

  drawCards();
  if (ngames) {
    const Game& g = games[sel];
    panel(0, 168, SW, 52);
    text(g.title, SW / 2, 172, TFT_WHITE, 2, top_center);
    text(g.desc, SW / 2, 191, c565(184, 243, 255), 1, top_center, false);
    if (g.hi) textf(SW / 2, 202, c565(255, 216, 74), 1, top_center, "HI-SCORE %lu", (unsigned long)g.hi);
    // page dots
    int dx = SW / 2 - (ngames - 1) * 5;
    for (int i = 0; i < ngames; i++) rectf(dx + i * 10 - 2, 214, 4, 3, i == sel ? rgbS(255, 255, 255) : rgbS(90, 70, 140));
  }
  if (input::pad.connected)
    text("A:PLAY  \x11\x10:CHOOSE  SELECT+\x1e\x1f:VOLUME", SW / 2, 226, c565(150, 140, 200), 1, top_center);
  else if ((frameNo >> 4) & 1)
    text("PAIR CONTROLLER: HOLD Y + STADIA", SW / 2, 226, c565(255, 138, 61), 1, top_center);

  if (lstate == L_INSTALL || lstate == L_BOOT) {
    panel(40, 84, 240, 64);
    textf(SW / 2, 94, TFT_WHITE, 1, top_center, lstate == L_BOOT ? "STARTING %s" : "INSTALLING %s", installTitle);
    int w = lstate == L_BOOT ? 200 : installPct * 2;
    if (rowsVisible(112, 14)) {
      rect(58, 112, 204, 14, rgbS(122, 61, 184));
      rectf(60, 114, w, 10, rgbS(62, 198, 224));
      rectf(60, 114, w, 3, rgbS(184, 243, 255));
    }
    if (lstate == L_INSTALL) textf(SW / 2, 132, c565(184, 243, 255), 1, top_center, "%d%%", installPct);
  }
}

static void rescan() {
  if (!sdBegin()) { lstate = L_NOSD; ngames = 0; return; }
  scanGames();
  readHiScores();
  lstate = ngames ? L_MENU : L_EMPTY;
  char last[16] = {0};
  arcade::shared.getString("last", last, sizeof(last));
  for (int i = 0; i < ngames; i++) if (!strcmp(games[i].id, last)) sel = i;
  pos = sel;
}

static void step(const Pad& in) {
  pos += (sel - pos) * 0.22f;
  switch (lstate) {
    case L_NOSD:
    case L_EMPTY:
      if (in.hit(BTN_A | BTN_START)) { audio::play(SFX_BLIP); rescan(); }
      break;
    case L_MENU:
      if (repL.tick(in.down(BTN_LEFT) && !in.down(BTN_SELECT)) && ngames) { sel = (sel + ngames - 1) % ngames; audio::play(SFX_MOVE); }
      if (repR.tick(in.down(BTN_RIGHT) && !in.down(BTN_SELECT)) && ngames) { sel = (sel + 1) % ngames; audio::play(SFX_MOVE); }
      if (in.hit(BTN_A | BTN_START) && ngames) launch(games[sel]);
      break;
    case L_BOOT:
      if (--bootTimer <= 0) restartInto(true);
      break;
    default: break;
  }
}

void setup() {
  arcade::begin("launcher", &MUSIC, true);
  setPalette(PAL565, PAL_SIZE);
  buildBackground();
  lstate = L_EMPTY;
  gfx::render(draw);            // show something while the card is read
  rescan();
  audio::music(1);
}

void loop() { arcade::run(step, draw); }
