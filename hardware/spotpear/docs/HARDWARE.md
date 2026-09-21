# Board wiring

Source: `vendor/Demo_Arduino_en/ST77916_LVGL_DEMO/pincfg.h`, obtained from the user's exact Spotpear product guide. Do not substitute Waveshare 1.85-inch board pin assignments.

| Peripheral | Pins |
|---|---|
| ST77916 QSPI | CS 10, CLK 9, D0 11, D1 12, D2 13, D3 14, reset 47 |
| Backlight | 15 |
| CST816 touch | SDA 7, SCL 8, interrupt 41, reset 40 |
| microSD, 1-bit SDMMC | CLK 3, CMD 4, D0 2 |
| PCM5100 audio DAC | BCLK 18, WS 16, DATA 17, amplifier mute 48 (low mutes) |
| I2S microphone | WS 45, DATA 46, CLK 42 |
| BOOT | 0 |

USB reports ESP32-S3 revision 0.2, embedded 8 MB PSRAM, 16 MB flash. Display uses 24 scan lines of DMA-capable internal RAM. The 32-entry packet queue is allocated in PSRAM. Radio callbacks enqueue fixed-size observations; UI, classification and SD logging execute outside radio callbacks. Audio uses a separate FreeRTOS task and an SD mutex.

The microphone receives both I2S slots and chooses the active slot by energy. Its physical gain and quality still require testing on each microphone revision. There is no compass, directional antenna array, distance-ranging radio or GPS in this board profile.
