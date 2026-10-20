#ifndef _PINCFG_H_
#define _PINCFG_H_

#define BTN_PIN 0

#if defined(BOARD_CROWPANEL_21)
#define BOARD_NAME "Elecrow CrowPanel 2.1 Rotary"
#define SCREEN_RES 480
#define BOARD_HAS_AUDIO 0
#define BOARD_HAS_SD 0
#define CROW_SDA 38
#define CROW_SCL 39
#define CROW_BACKLIGHT 6
#define CROW_ENCODER_A 42
#define CROW_ENCODER_B 4
#define CROW_EXPANDER_ADDR 0x21
#define CROW_TOUCH_ADDR 0x15
#elif defined(BOARD_RADAR_LITE)
#define BOARD_NAME "Waveshare ESP32-S3-LCD-1.28 Lite"
#define SCREEN_RES 240
#define BOARD_HAS_AUDIO 0
#define BOARD_HAS_SD 0
#define LITE_BACKLIGHT 40
#elif defined(BOARD_XIAO_ROUND)
#define BOARD_NAME "Seeed XIAO ESP32-S3 + Round Display"
#define SCREEN_RES 240
#define BOARD_HAS_AUDIO 0
#define BOARD_HAS_SD 1
#define BOARD_SD_SPI 1
#define XIAO_SD_CS 3
#define XIAO_LCD_CS 2
#define XIAO_BACKLIGHT 43
#define XIAO_TOUCH_INT 44
#define XIAO_SDA 5
#define XIAO_SCL 6
#elif defined(BOARD_STOPWATCH)
#define BOARD_NAME "M5Stack StopWatch C152"
#define SCREEN_RES 466
#define BOARD_HAS_AUDIO 0
#define BOARD_HAS_SD 0
#elif defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175)
// The two AMOLED variants share their buses but not reset, SD or MCLK wiring.
// Official non-C schematic/BSP reference: docs/WAVESHARE-AMOLED-175.md.

#ifdef BOARD_AMOLED_175
#define BOARD_NAME "Waveshare ESP32-S3-Touch-AMOLED-1.75"
#define LCD_RESET 39
#define TP_RESET 40
#define AUDIO_I2S_MCK_IO 42
#define BOARD_HAS_SD 1
#define SD_MMC_CLK_PIN 2
#define SD_MMC_CMD_PIN 1
#define SD_MMC_D0_PIN 3
#else
#define BOARD_NAME "Waveshare ESP32-S3-Touch-AMOLED-1.75C"
#define LCD_RESET 1
#define TP_RESET 2
#define AUDIO_I2S_MCK_IO 16
#define BOARD_HAS_SD 0
#endif
#define SCREEN_RES 466
#define BOARD_HAS_AUDIO 0

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_COL_OFFSET 6

// Touch, AXP2101 and QMI8658 share this bus. Probe optional RTC revisions.
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_I2C_ADDR 0x5A

// Waveshare official 07_ES8311 example / pin_config.h.
#define AUDIO_I2S_BCK_IO 9
#define AUDIO_I2S_WS_IO 45
#define AUDIO_I2S_DO_IO 8
#define AUDIO_MUTE_PIN 46

#else

#define BOARD_NAME "Spotpear JC3636W518"
#define SCREEN_RES 360
#define BOARD_HAS_AUDIO 0
#define BOARD_HAS_SD 1

#define TFT_BLK 15

#define TFT_RST 47
#define TFT_CS 10
#define TFT_SCK 9
#define TFT_SDA0 11
#define TFT_SDA1 12
#define TFT_SDA2 13
#define TFT_SDA3 14

#define TOUCH_PIN_NUM_I2C_SCL 8
#define TOUCH_PIN_NUM_I2C_SDA 7
#define TOUCH_PIN_NUM_INT 41
#define TOUCH_PIN_NUM_RST 40

#define SD_MMC_D0_PIN 2
#define SD_MMC_D1_PIN 1
#define SD_MMC_D2_PIN 6
#define SD_MMC_D3_PIN 5
#define SD_MMC_CLK_PIN 3
#define SD_MMC_CMD_PIN 4

#define AUDIO_I2S_MCK_IO -1 // MCK
#define AUDIO_I2S_BCK_IO 18 // BCK
#define AUDIO_I2S_WS_IO 16  // LCK
#define AUDIO_I2S_DO_IO 17  // DIN
#define AUDIO_MUTE_PIN 48   // 低电平静音

#define MIC_I2S_WS 45
#define MIC_I2S_SD 46
#define MIC_I2S_SCK 42

#endif

#endif
