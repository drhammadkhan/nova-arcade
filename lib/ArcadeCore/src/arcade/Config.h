#pragma once
// =====================================================================
//  NOVA ARCADE - hardware configuration (shared by the launcher and every game)
//
//  Defaults are for the LCDWIKI "2.8inch ESP32-S3 Display" (ES3N28P /
//  ES3C28P): ILI9341V 240x320 IPS panel, ES8311 audio codec + speaker amp,
//  16MB flash, 8MB OPI PSRAM. If your board differs, change the pins here.
// =====================================================================

// ---------------- Display (SPI) ----------------
#define TFT_SCLK      12
#define TFT_MOSI      11
#define TFT_MISO      13
#define TFT_CS        10
#define TFT_DC        46
#define TFT_RST       -1        // tied to the ESP32 reset on this board
#define TFT_BL        45        // backlight (PWM)

// 80 and 40 MHz gave pixel glitches on this board; 27 MHz is the safe setting.
#define TFT_SPI_HZ    27000000

#define TFT_INVERT    true      // IPS panels usually need inversion
#define TFT_SWAP_RB   false     // set true if red and blue look swapped
#define TFT_ROTATION  1         // 1 or 3 = landscape (use 3 if upside down)

// ---------------- Audio ----------------
#define AUDIO_NONE    0
#define AUDIO_ES8311  1         // I2S + ES8311 codec (this board)
#define AUDIO_I2S_AMP 2         // plain I2S amp such as MAX98357 / NS4168
#define AUDIO_MODE    AUDIO_ES8311

#define I2S_MCLK_PIN  4
#define I2S_BCLK_PIN  5
#define I2S_LRCK_PIN  7
#define I2S_DOUT_PIN  8         // ESP32 -> codec (speaker data)

#define CODEC_SDA_PIN 16
#define CODEC_SCL_PIN 15
#define CODEC_I2C_ADDR 0x18

#define AMP_EN_PIN    1         // FM8002E shutdown pin
#define AMP_EN_ACTIVE_LOW 1     // low = amplifier on

#define AUDIO_RATE    22050
#define CODEC_VOLUME  0xB0      // ES8311 DAC volume (0xBF = 0 dB); lower if it distorts
#define MUSIC_VOLUME  60        // 0-100 (mix balance)
#define SFX_VOLUME    80        // 0-100 (mix balance)

// ---------------- SD card (4-bit SDMMC) ----------------
#define SD_CLK_PIN    38
#define SD_CMD_PIN    40
#define SD_D0_PIN     39
#define SD_D1_PIN     41
#define SD_D2_PIN     48
#define SD_D3_PIN     47

// ---------------- Misc ----------------
#define RUMBLE        1         // controller rumble
#ifndef DEFAULT_VOLUME
#define DEFAULT_VOLUME 3        // 0-10
#endif
