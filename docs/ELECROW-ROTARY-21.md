# Elecrow CrowPanel 2.1 Rotary

This experimental profile targets the **CrowPanel 2.1-inch HMI ESP32-S3 rotary display**, hardware V1.0, with a 480 × 480 IPS touchscreen, 16 MB flash and 8 MB PSRAM. It is intended for stationary USB-powered use. Startup, color order and ring navigation have been checked on a physical unit. Extended display stability and touchscreen validation are in progress.

The profile includes the full radar interface, 35 built-in themes, logo watch faces, the optional personal theme, badge, clock, stopwatch and locksport timers, notebook and shared radio tools. It has no SD controls. Captures and profiles use internal storage and can be downloaded over USB. No speaker, haptic motor, battery gauge or external RTC is configured.

## Controls

| Action | Result |
|---|---|
| Turn the outer ring | Move the highlight between visible controls; lists scroll to the focused control |
| Press and release the ring | Select the highlighted control |
| Press on a slider, roller or dropdown | Enter adjustment; turn to change the value and press again to finish |
| Hold the ring for about one second | Return to the radar |
| Turn or press while the screen is off | Wake the display; this first action does not select a control |
| Touch | Use the normal touchscreen interface, including gestures and the keyboard |

The rear **BOOT** and **RESET** buttons are for startup and recovery. The ring's press switch is separate from BOOT. Clock and timer pages pause background scanning, as on the other boards.

Navigation advances one control at a time, at most once every 180 ms. Fast turns are coalesced so a burst cannot skip through a whole menu or keep scrolling after you stop. A press selects the currently highlighted control.

## Installation

1. Connect the board directly to the computer with a USB data cable.
2. In the [web flasher](https://valleytech-dragon-detector.pages.dev/#install), choose **Elecrow CrowPanel 2.1 Rotary · Experimental**.
3. Use **Fresh install** for Elecrow's factory demo or any other firmware. This replaces saved files and settings. **Update** requires this exact Dragon Detector profile, version **0.13.20 or newer**.
4. Leave automatic clock setup enabled. Keep USB connected through restart and wait for the clock verification result.

For manual download mode, hold the rear **BOOT** button, briefly press and release **RESET**, then release BOOT. The screen may be blank in download mode; Windows must still enumerate a USB device before flashing is possible. A power indicator alone does not confirm a data connection.

If Windows reports a USB descriptor error or no new device, check a known-good data cable and a direct computer port. Firmware cannot repair a USB connection that never reaches the chip's bootloader. Preserve a full flash backup before replacing a working factory installation. Elecrow also supplies [factory firmware](https://github.com/Elecrow-RD/CrowPanel-2.1inch-HMI-ESP32-Rotary-Display-480-480-IPS-Round-Touch-Knob-Screen/tree/52c772195033bf0c08568fa21cfc8606bac75df0/factory_firmware).

## Time and date

USB setup copies the computer's current time and timezone and checks the device's response. **Settings → Time** and **Settings → Date** provide separate manual editors.

There is no external RTC configured. The clock keeps running while powered, but a complete power loss requires a fresh time source. For an unattended desk display, configure a saved Wi-Fi profile in the web flasher and choose **Use selected slot for clock**. It can then recover time when that network is available. See [clock setup](CLOCK-SETUP.md). The firmware does not present an old saved timestamp as the current time.

## Hardware configuration

| Function | Connection |
|---|---|
| ST7701 command bus | Software SPI: CS 16, SCK 2, MOSI 1 |
| RGB timing signals | DE 40, VSYNC 7, HSYNC 15, PCLK 41 |
| RGB data | R: 46/3/8/18/17; G: 14/13/12/11/10/9; B: 5/45/48/47/21 |
| RGB timing | 8 MHz; horizontal and vertical front porch 10, pulse 4, back porch 20; 20-line bounce buffer |
| Shared I²C | SDA 38, SCL 39 |
| PCF8574 at 0x21 | P0 touch reset, P2 touch interrupt line, P3 LCD power, P4 LCD reset, P5 active-low ring press |
| CST826 at 0x15 | Touch coordinates; chip identifier 0x11 |
| Backlight | GPIO6 PWM |
| Rotary encoder | A: GPIO42, B: GPIO4; four valid quadrature transitions per detent |
| Application partitions | Two 4 MiB slots, at 0x10000 and 0x410000 |
| Internal files | 0x810000, 7.94 MiB shared filesystem |
| Capture limits | 1 MiB RAM capture buffer; 256 KiB per recon session |

Panel commands use Arduino_GFX 1.5.6. RGB output uses the ESP-IDF driver directly, with two complete frame buffers and 20-line DMA bounce buffers. LVGL draws a complete frame before it is submitted. The old frame buffer is reused only after two frame-boundary callbacks, including a callback that could race with submission. The flush path converts LVGL byte order and the panel's BGR channels without modifying the LVGL drawing buffer.

This profile builds Arduino 3.0.7 as an ESP-IDF 5.1.4 component using `hardware/elecrow-rotary-21/sdkconfig.defaults`. Instructions and read-only data are copied to PSRAM at startup, keeping the display's source memory accessible during flash writes. The RGB interrupt remains in internal RAM, and the data cache uses 64-byte lines. Routine display restarts are disabled; recovery is requested only after a frame wait times out. This follows [Espressif's guidance for RGB display drift](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html#why-do-i-get-drift-overall-drift-of-the-display-when-esp32-s3-is-driving-an-rgb-lcd-screen). Other board profiles retain their existing Arduino builds and display drivers.

## Physical validation

Initial checks on 2026-10-19 verified the 16 MB flash / 8 MB PSRAM identity, a full backup, startup, color order, ring navigation and USB clock readback. The earlier build developed intermittent display drift. On 2026-10-20, the revised ESP-IDF build passed 23 firmware self-tests and a five-minute passive scan with repeated settings saves, without a reset or display-buffer timeout. Free memory remained steady. These diagnostics do not establish physical centering; visual confirmation of the revised build is still required.

Extended checks remain:

- Chip identity, 16 MB flash and PSRAM availability; save a private full flash backup.
- Boot animation, color order, centering and stability while scanning.
- Touch at the center and around the rim, including all four orientation settings.
- Ring turns in both directions, short press, hold, wake, scrolling, value editing and confirmation dialogs.
- Clock setup, timer operation and internal capture export.
- A longer powered run with no display drift, reset or memory allocation failure.

Serial commands `rotary status` and `touch status` report input counts and I²C errors for troubleshooting. `rotary status` also reports completed display frames and buffer-wait timeouts. A successful compile or encoder unit test does not validate the physical wiring.

## Sources

- [Elecrow product page](https://www.elecrow.com/crowpanel-2-1inch-hmi-esp32-rotary-display-480-480-ips-round-touch-knob-screen.html).
- [Elecrow wiki](https://www.elecrow.com/wiki/CrowPanel_2.1inch-HMI_ESP32_Rotary_Display_480_IPS_Round_Touch_Knob_Screen.html).
- [Official examples and hardware files](https://github.com/Elecrow-RD/CrowPanel-2.1inch-HMI-ESP32-Rotary-Display-480-480-IPS-Round-Touch-Knob-Screen/tree/52c772195033bf0c08568fa21cfc8606bac75df0).
- [Arduino_GFX display driver](https://github.com/moononournation/Arduino_GFX/tree/58ac1d46438b9406f87a53ae87ca9f377ca805fb).
