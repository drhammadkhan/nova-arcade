#pragma once
// =====================================================================
//  ArcadeCore audio: chiptune synth with 4 music voices (lead, arp, bass,
//  drums), 2 SFX voices, a step sequencer and an echo on the melody.
//  Runs as a FreeRTOS task on core 0 and streams 16-bit audio over I2S.
//  Each game supplies its own music (see struct Music below).
// =====================================================================
#include <Arduino.h>
#include "Config.h"
#ifndef ARCADE_SIM
#include <Wire.h>
#include <driver/i2s.h>
#endif
#include <initializer_list>

// Shared sound-effect set. `param` (0-255) varies pitch where noted.
enum Sfx : uint8_t {
  SFX_SHOOT, SFX_HIT, SFX_EXPLODE, SFX_BIG_EXPLODE, SFX_POWERUP,
  SFX_PLAYER_DIE, SFX_BOMB, SFX_START, SFX_WARNING, SFX_BLIP,
  SFX_MOVE,        // tiny tick (menu / piece move)
  SFX_ROTATE,      // short chirp
  SFX_LAND,        // soft thud
  SFX_LINE,        // line clear, param = lines (1-4)
  SFX_BOUNCE,      // paddle bounce, param = pitch step
  SFX_BRICK,       // brick hit, param = combo (rising pitch)
  SFX_MARCH,       // invader step, param = 0-3 note
  SFX_UFO,         // warble (retrigger while the saucer is on screen)
  SFX_SELECT,      // menu confirm
  SFX_COUNT
};

namespace audio {

static const int RATE = AUDIO_RATE;
static const int BLOCK = 256;

// ------------------------------------------------------------ music format
struct Chord { uint8_t root; uint8_t arp[4]; };
struct Bar { uint8_t chord; int8_t lead; uint8_t drum; };   // lead -1 = none
struct SongDef { const Bar* bars; uint8_t nbars; uint8_t bpm; bool loop; bool driveBass; uint8_t leadDuty; };
struct Music {
  const Chord* chords;
  const uint8_t (*leads)[16];   // 0 = rest, 1 = hold, else MIDI note
  const char* const* drums;     // 16-char patterns: K kick, S snare, h hat, . none
  const SongDef* songs;         // index 0 should be the "silence" song
  uint8_t nsongs;
};

// A handy default chord set (games may use their own)
enum { AM, F_, C_, G_, E_, BB, DM, EM, D_, A_ };
static const Chord STD_CHORDS[] = {
  {45, {57, 60, 64, 60}},  // Am
  {41, {53, 57, 60, 57}},  // F
  {48, {55, 60, 64, 60}},  // C
  {43, {55, 59, 62, 59}},  // G
  {40, {56, 59, 64, 59}},  // E
  {46, {58, 62, 65, 62}},  // Bb
  {50, {57, 62, 65, 62}},  // Dm
  {40, {55, 59, 64, 59}},  // Em
  {50, {54, 57, 62, 57}},  // D
  {45, {57, 61, 64, 61}},  // A
};

// ------------------------------------------------------------ synth state
static float noteHz[128];

struct Voice {
  uint32_t phase = 0;
  float hz = 0, slide = 1.0f;
  float vol = 0, decay = 0, sustain = 0;
  uint32_t duty = 0x80000000u;
  uint8_t wave = 0;                 // 0 pulse, 1 triangle, 2 noise
  uint16_t lfsr = 1;
  uint32_t age = 0;
  bool vibrato = false;
};

static Voice vLead, vArp, vBass, vDrum, vSfxA, vSfxB;
static uint8_t sfxArp[8], sfxArpLen = 0, sfxArpPos = 0;
static uint32_t sfxArpStep = 0, sfxArpCount = 0;
static uint8_t sfxPrioA = 0, sfxPrioB = 0;
static uint32_t warnCount = 0, warnToggle = 0, warbleCount = 0;

static const Music* mus = nullptr;
static volatile uint8_t requestedSong = 0;
static uint8_t currentSong = 0xFF;
static uint32_t stepSamples = 0, stepCounter = 0;
static uint16_t barIdx = 0;
static uint8_t stepIdx = 0;
static bool songDone = true;

static int16_t echoBuf[4096];
static uint16_t echoPos = 0;
static float lp = 0;

static QueueHandle_t sfxQueue = nullptr;
static bool running = false;
static volatile float master = 1.0f;

static inline float musicGain() { return MUSIC_VOLUME / 100.0f; }
static inline float sfxGain() { return SFX_VOLUME / 100.0f; }

static inline void noteOn(Voice& v, uint8_t note, float vol, float decaySec, float sustain) {
  v.hz = noteHz[note & 127];
  v.slide = 1.0f;
  v.vol = vol;
  v.sustain = sustain * vol;
  v.decay = decaySec > 0 ? vol / (decaySec * RATE) : 0;
  v.age = 0;
}

static inline float voiceSample(Voice& v) {
  if (v.vol <= 0.0005f) return 0;
  float hz = v.hz;
  if (v.vibrato && v.age > RATE / 6) {
    uint32_t p = (v.age * 12 / RATE) & 1;
    float t = (float)((v.age * 12) % RATE) / RATE;
    float lfo = p ? (1 - t) : t;
    hz *= 1.0f + (lfo - 0.5f) * 0.01f;
  }
  uint32_t inc = (uint32_t)(hz * (4294967296.0f / RATE));
  uint32_t prev = v.phase;
  v.phase += inc;
  float s;
  switch (v.wave) {
    case 0: s = (v.phase < v.duty) ? 1.0f : -1.0f; break;
    case 1: {
      uint32_t st = v.phase >> 27;
      int q = st < 16 ? st : 31 - st;
      s = (q - 7.5f) / 7.5f;
      break;
    }
    default:
      if (v.phase < prev) {
        uint16_t bit = ((v.lfsr >> 0) ^ (v.lfsr >> 1)) & 1;
        v.lfsr = (v.lfsr >> 1) | (bit << 14);
      }
      s = (v.lfsr & 1) ? 1.0f : -1.0f;
      break;
  }
  s *= v.vol;
  if (v.vol > v.sustain) v.vol -= v.decay;
  if (v.vol < 0) v.vol = 0;
  v.hz *= v.slide;
  v.age++;
  return s;
}

static inline void drumHit(char c) {
  if (c == 'K') {
    vDrum.wave = 1; vDrum.hz = 180; vDrum.slide = 0.99965f;
    vDrum.vol = 1.0f; vDrum.decay = 1.0f / (0.16f * RATE); vDrum.sustain = 0;
  } else if (c == 'S') {
    vDrum.wave = 2; vDrum.hz = 6000; vDrum.slide = 0.99995f;
    vDrum.vol = 0.75f; vDrum.decay = 0.75f / (0.14f * RATE); vDrum.sustain = 0;
  } else if (c == 'h') {
    vDrum.wave = 2; vDrum.hz = 16000; vDrum.slide = 1.0f;
    vDrum.vol = 0.22f; vDrum.decay = 0.22f / (0.03f * RATE); vDrum.sustain = 0;
  }
}

static inline void startSong(uint8_t id) {
  currentSong = id;
  songDone = true;
  vLead.vol = vArp.vol = vBass.vol = vDrum.vol = 0;
  if (!mus || id >= mus->nsongs) return;
  const SongDef& s = mus->songs[id];
  songDone = (s.nbars == 0);
  barIdx = 0; stepIdx = 0; stepCounter = 0;
  stepSamples = (uint32_t)(RATE * 60.0f / (s.bpm * 4));
}

static inline void sequencerStep() {
  if (songDone || !mus) return;
  const SongDef& s = mus->songs[currentSong];
  const Bar& b = s.bars[barIdx];
  const Chord& ch = mus->chords[b.chord];
  float g = musicGain();

  if (b.lead >= 0) {
    uint8_t n = mus->leads[b.lead][stepIdx];
    if (n >= 2) {
      vLead.wave = 0; vLead.duty = s.leadDuty ? ((uint32_t)s.leadDuty << 24) : 0x40000000u; vLead.vibrato = true;
      noteOn(vLead, n, 0.22f * g, 0.35f, 0.55f);
    } else if (n == 0) {
      vLead.sustain = 0; vLead.decay = vLead.vol / (0.04f * RATE);
    }
  }
  vArp.wave = 0; vArp.duty = 0x20000000u; vArp.vibrato = false;
  noteOn(vArp, ch.arp[stepIdx & 3] + 12, 0.09f * g, 0.09f, 0.0f);

  if (s.driveBass || (stepIdx % 2 == 0)) {
    uint8_t n = ch.root + ((stepIdx % 4 == 2) ? 12 : 0);
    vBass.wave = 1; vBass.vibrato = false;
    noteOn(vBass, n, 0.30f * g, s.driveBass ? 0.10f : 0.18f, 0.3f);
  }
  char d = mus->drums[b.drum][stepIdx];
  if (d != '.') {
    drumHit(d);
    vDrum.vol *= 0.34f * g;
    vDrum.decay *= 0.34f * g;
  }
  if (++stepIdx >= 16) {
    stepIdx = 0;
    if (++barIdx >= s.nbars) {
      if (s.loop) barIdx = 0; else songDone = true;
    }
  }
}

static inline void triggerSfx(uint8_t id, uint8_t param) {
  float g = sfxGain();
  auto pulse = [&](uint8_t prio, float hz, float slide, float vol, float secs, uint32_t duty) {
    if (prio < sfxPrioA && vSfxA.vol > 0.01f) return false;
    sfxPrioA = prio;
    vSfxA.wave = 0; vSfxA.duty = duty; vSfxA.vibrato = false;
    vSfxA.hz = hz; vSfxA.slide = slide; vSfxA.vol = vol * g;
    vSfxA.decay = vol * g / (secs * RATE); vSfxA.sustain = 0; vSfxA.age = 0;
    sfxArpLen = 0; warnCount = 0; warbleCount = 0;
    return true;
  };
  auto noise = [&](uint8_t prio, float hz, float slide, float vol, float secs) {
    if (prio < sfxPrioB && vSfxB.vol > 0.01f) return;
    sfxPrioB = prio;
    vSfxB.wave = 2; vSfxB.hz = hz; vSfxB.slide = slide;
    vSfxB.vol = vol * g; vSfxB.decay = vol * g / (secs * RATE); vSfxB.sustain = 0;
  };
  auto arp = [&](std::initializer_list<uint8_t> notes, float stepSec) {
    sfxArpLen = 0;
    for (uint8_t n : notes) if (sfxArpLen < 8) sfxArp[sfxArpLen++] = n;
    sfxArpPos = 0; sfxArpStep = (uint32_t)(stepSec * RATE); sfxArpCount = 0;
    vSfxA.hz = noteHz[sfxArp[0]];
  };
  const uint32_t D50 = 0x80000000u, D25 = 0x40000000u, D12 = 0x20000000u;
  switch (id) {
    case SFX_SHOOT:   pulse(1, 1500, 0.99975f, 0.16f, 0.07f, D50); break;
    case SFX_HIT:     noise(1, 12000, 0.9999f, 0.18f, 0.05f); break;
    case SFX_EXPLODE:
      noise(3, 3000, 0.99994f, 0.45f, 0.40f);
      pulse(2, 220, 0.99985f, 0.18f, 0.25f, D50);
      break;
    case SFX_BIG_EXPLODE:
      noise(5, 2200, 0.99997f, 0.6f, 1.0f);
      pulse(4, 160, 0.99992f, 0.3f, 0.9f, D50);
      break;
    case SFX_POWERUP: if (pulse(4, 0, 1, 0.22f, 0.45f, D25)) arp({72, 76, 79, 84, 88, 91}, 0.05f); break;
    case SFX_PLAYER_DIE:
      pulse(6, 900, 0.99988f, 0.30f, 0.9f, D50);
      noise(6, 4000, 0.99995f, 0.5f, 0.9f);
      break;
    case SFX_BOMB:
      noise(6, 1500, 0.99998f, 0.7f, 1.4f);
      pulse(6, 60, 1.00008f, 0.3f, 1.2f, D50);
      break;
    case SFX_START:   if (pulse(5, 0, 1, 0.22f, 0.5f, D25)) arp({69, 73, 76, 81, 85, 88}, 0.06f); break;
    case SFX_WARNING: if (pulse(5, 440, 1, 0.22f, 1.6f, D50)) { warnCount = (uint32_t)(1.6f * RATE); warnToggle = 0; } break;
    case SFX_BLIP:    if (pulse(5, 0, 1, 0.18f, 0.2f, D25)) arp({84, 79}, 0.07f); break;
    case SFX_MOVE:    pulse(0, 880, 1, 0.07f, 0.025f, D25); break;
    case SFX_ROTATE:  pulse(1, 660, 1.0003f, 0.10f, 0.05f, D12); break;
    case SFX_LAND:    noise(2, 900, 0.9998f, 0.25f, 0.08f); pulse(1, 110, 0.9995f, 0.15f, 0.06f, D50); break;
    case SFX_LINE:
      if (param >= 4) { if (pulse(6, 0, 1, 0.26f, 0.8f, D25)) arp({72, 76, 79, 84, 88, 91, 96}, 0.06f); noise(4, 5000, 0.99995f, 0.3f, 0.6f); }
      else if (pulse(5, 0, 1, 0.22f, 0.35f, D25)) arp({(uint8_t)(76 + param * 2), (uint8_t)(83 + param * 2), (uint8_t)(88 + param * 2)}, 0.05f);
      break;
    case SFX_BOUNCE:  pulse(2, 440.0f * powf(1.0595f, param % 24), 1, 0.16f, 0.06f, D50); break;
    case SFX_BRICK:   pulse(3, 660.0f * powf(1.0595f, min((int)param, 24)), 1, 0.16f, 0.08f, D25); noise(1, 9000, 1, 0.08f, 0.03f); break;
    case SFX_MARCH: {
      static const float notes[4] = {98.0f, 87.3f, 77.8f, 73.4f};
      pulse(1, notes[param & 3], 1, 0.28f, 0.09f, D50);
      break;
    }
    case SFX_UFO:
      if (pulse(2, 700, 1, 0.12f, 0.25f, D25)) warbleCount = (uint32_t)(0.25f * RATE);
      break;
    case SFX_SELECT:  if (pulse(5, 0, 1, 0.22f, 0.3f, D25)) arp({79, 84, 91}, 0.05f); break;
  }
}

static inline void renderBlock(int16_t* out, int frames) {
  uint16_t msg;
  while (xQueueReceive(sfxQueue, &msg, 0) == pdTRUE) triggerSfx(msg & 0xFF, msg >> 8);
  if (requestedSong != currentSong) startSong(requestedSong);

  for (int i = 0; i < frames; i++) {
    if (++stepCounter >= stepSamples && stepSamples) { stepCounter = 0; sequencerStep(); }
    if (sfxArpLen && ++sfxArpCount >= sfxArpStep) {
      sfxArpCount = 0;
      if (++sfxArpPos >= sfxArpLen) sfxArpPos = sfxArpLen - 1;
      vSfxA.hz = noteHz[sfxArp[sfxArpPos]];
    }
    if (warnCount) {
      warnCount--;
      if (++warnToggle >= (uint32_t)(0.15f * RATE)) { warnToggle = 0; vSfxA.hz = (vSfxA.hz > 400) ? 330 : 440; }
    }
    if (warbleCount) {
      warbleCount--;
      vSfxA.hz = 700 + 180 * sinf(warbleCount * 0.0035f);
    }
    float mel = voiceSample(vLead) + voiceSample(vArp);
    float echo = echoBuf[echoPos] / 32768.0f;
    float wet = mel + echo * 0.38f;
    echoBuf[echoPos] = (int16_t)constrain(wet * 32767.0f, -32767.0f, 32767.0f);
    echoPos = (echoPos + 1) & 4095;
    float mix = mel + echo * 0.30f + voiceSample(vBass) + voiceSample(vDrum) +
                voiceSample(vSfxA) + voiceSample(vSfxB);
    lp += (mix - lp) * 0.55f;
    float s = lp * 26000.0f * master;
    if (s > 32767) s = 32767;
    if (s < -32767) s = -32767;
    out[i * 2] = out[i * 2 + 1] = (int16_t)s;
  }
}

#ifndef ARCADE_SIM
static inline void task(void*) {
  static int16_t buf[BLOCK * 2];
  size_t written;
  for (;;) {
    renderBlock(buf, BLOCK);
    i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
  }
}

#if AUDIO_MODE == AUDIO_ES8311
static inline void codecWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(CODEC_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}
static inline bool codecInit() {
  Wire.begin(CODEC_SDA_PIN, CODEC_SCL_PIN, 400000);
  Wire.beginTransmission(CODEC_I2C_ADDR);
  if (Wire.endTransmission() != 0) return false;
  codecWrite(0x00, 0x1F); delay(20);
  codecWrite(0x00, 0x00);
  codecWrite(0x00, 0x80);
  codecWrite(0x01, 0x3F);
  codecWrite(0x02, 0x00);
  codecWrite(0x05, 0x00);
  codecWrite(0x03, 0x10);
  codecWrite(0x04, 0x10);
  codecWrite(0x07, 0x00);
  codecWrite(0x08, 0xFF);
  codecWrite(0x06, 0x03);
  codecWrite(0x09, 0x0C);
  codecWrite(0x0A, 0x0C);
  codecWrite(0x0B, 0x00);
  codecWrite(0x0C, 0x00);
  codecWrite(0x10, 0x1F);
  codecWrite(0x11, 0x7F);
  codecWrite(0x0D, 0x01);
  codecWrite(0x0E, 0x02);
  codecWrite(0x12, 0x00);
  codecWrite(0x13, 0x10);
  codecWrite(0x1C, 0x6A);
  codecWrite(0x37, 0x08);
  codecWrite(0x32, CODEC_VOLUME);
  codecWrite(0x31, 0x00);
  return true;
}
#endif
#endif  // ARCADE_SIM

inline void begin() {
  for (int n = 0; n < 128; n++) noteHz[n] = 440.0f * powf(2.0f, (n - 69) / 12.0f);
  sfxQueue = xQueueCreate(24, sizeof(uint16_t));
#if !defined(ARCADE_SIM) && AUDIO_MODE != AUDIO_NONE
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = BLOCK;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return;
  i2s_pin_config_t pins = {};
#if AUDIO_MODE == AUDIO_ES8311
  pins.mck_io_num = I2S_MCLK_PIN;
#else
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
#endif
  pins.bck_io_num = I2S_BCLK_PIN;
  pins.ws_io_num = I2S_LRCK_PIN;
  pins.data_out_num = I2S_DOUT_PIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_NUM_0, &pins);
#if AUDIO_MODE == AUDIO_ES8311
  delay(10);
  if (!codecInit()) Serial.println("ES8311 not found on I2C - check CODEC pins");
#endif
#ifdef AMP_EN_PIN
  pinMode(AMP_EN_PIN, OUTPUT);
  digitalWrite(AMP_EN_PIN, AMP_EN_ACTIVE_LOW ? LOW : HIGH);
#endif
  running = true;
  xTaskCreatePinnedToCore(task, "audio", 4096, nullptr, 3, nullptr, 0);
#endif
}

inline void setMusic(const Music* m) { mus = m; currentSong = 0xFF; }
inline void play(Sfx s, uint8_t param = 0) {
  if (!sfxQueue) return;
  uint16_t msg = (uint16_t)s | ((uint16_t)param << 8);
  xQueueSend(sfxQueue, &msg, 0);
}
inline void music(uint8_t song) { requestedSong = song; }

// Volume 0..10 (0 = mute). On the ES8311 board the codec's DAC volume is used.
inline void setVolume(int v) {
  v = constrain(v, 0, 10);
#if !defined(ARCADE_SIM) && AUDIO_MODE == AUDIO_ES8311
  master = 1.0f;
  if (running) {
    int reg = (v == 0) ? 0 : CODEC_VOLUME - (10 - v) * 12;
    codecWrite(0x32, (uint8_t)constrain(reg, 0, 255));
    codecWrite(0x31, v == 0 ? 0x60 : 0x00);
  }
#else
  master = (v == 0) ? 0.0f : powf(v / 10.0f, 1.6f);
#endif
}

}  // namespace audio
