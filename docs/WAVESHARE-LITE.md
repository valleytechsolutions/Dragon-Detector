# Waveshare ESP32-S3-LCD-1.28 · Lite

First supported in **0.9.4**, build profile `waveshare_lite`. Use the [exact non-touch Waveshare board](https://www.waveshare.com/esp32-s3-lcd-1.28.htm). This is **not** ESP32-S3-Touch-LCD-1.28, Waveshare 1.75C AMOLED, or Seeed XIAO Round; their wiring and memory profiles differ.

![Lite radar framebuffer: BWM logo, live sweep and a labeled Find My signal](../media/waveshare-lite-radar.png)

Actual 240×240 framebuffer capture from the connected board, not a panel photograph. The displayed signal is a protocol candidate; it does not identify a particular accessory model.

## A dedicated radar

- Boots straight to the saved radar theme (green with BWM logo on first use), with live sweep and Wi-Fi/BLE reception status. Fine range rings, segmented arcs and edge ticks replace the dense grid; every device keeps its unique family color.
- Always 100% backlight; no dimming or timeout. The displayed **100% is brightness, not battery charge**.
- All contacts appear together as family-colored dots. Up to four priority devices receive readable tags and leaders; Axon is first. Trackers remain small dots. There are no automatic pages. Lite has no touch hardware, so tap-to-reveal is available only on the full touch boards.
- Signal radius uses the same median-filtered, smoothed RSSI as the full firmware. Stronger signals move inward. Neither the angle nor radius is a measured direction or distance.
- All 26 detector families remain enabled, including the 0.9.3 Axon priority/receive fixes, ALPR, Find My accessories, Ring, Flipper and Wi-Fi/BLE activity detection. [Signature coverage and limitations](DETECTION.md) apply equally to Lite.
- Passive BLE scanning and passive Wi-Fi reception alternate on the one shared radio, with Wi-Fi channels 1–11. Reception continues while drawing, but alternating modes/channel hopping still create listening gaps. No active probing, wireless attacks or fox transmitter.
- No touch interface, menus, badge mode, stopwatch, RTC interface, motion rotation, speaker code, SD capture, or persistent history. Volatile contacts/alerts reset at reboot. Old full-UI preferences cannot disable families, dim the screen or start a badge.
- **Press BOOT to cycle all 26 radar palettes and logos** (0.13.7+). Each press advances once; holding it does not repeat. The theme name appears briefly, and the selection is saved one second after the last change. RESET restarts the board. Hold BOOT while tapping RESET for ROM download/recovery as before; release BOOT before normal theme controls arm.
- Lite uses a compact shared instrument layout, including native GhostStrats slit eyes and an ASCII octopus. It does not load the full boards’ watch faces, locksport timers or elaborate animated scenes.

## Hardware configuration

| Item | Lite profile |
|---|---|
| MCU / memory | ESP32-S3R2 · 16 MB flash · 2 MB **QSPI** PSRAM |
| Panel | GC9A01A · 240×240 RGB565 · SPI3/HSPI at 40 MHz |
| LCD MOSI / SCLK | GPIO 11 / 10 |
| LCD CS / DC / RESET | GPIO 9 / 8 / 12 |
| Backlight | GPIO 40, continuously on |
| USB | CH343P UART bridge, 115200 diagnostics; native USB CDC disabled |
| Input / sound | BOOT / GPIO 0, active low with pull-up; no touch or onboard speaker/buzzer |

An 11,520-byte display tile uses internal RAM; the 384-contact store and receive queues use PSRAM. Animation time and RSSI positions stay fixed while the tiles of one frame are drawn. Radar artwork is rendered at startup and on a theme change into the same reusable 115,200-byte PSRAM cache. Animation is capped at twelve updates per second; the earlier green-radar hardware check measured about 66 ms per draw, down from 101 ms before this refinement. This leaves time between frames for reception; no menu objects, audio tasks or flash history journal run in Lite. The QMI8658 is intentionally unused. Battery percentage is not inferred from an uncalibrated ADC.

## Install and recover

Open the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/), choose **Waveshare 1.28 Lite**, confirm the exact board and select its CH343 USB serial port. Use desktop Chrome/Edge and a USB data cable. Close serial monitors first. If connection fails, hold BOOT, tap RESET, then release BOOT and retry. A missing CH343 port may require the driver linked in [Waveshare's documentation](https://docs.waveshare.com/ESP32-S3-LCD-1.28).

Fresh installation erases the previous firmware/data. Back up a third-party port first. Update is for this exact Lite 0.9.4+ layout: app0 `0x10000` (6 MB), app1 `0x610000` (6 MB), reserved LittleFS `0xc10000` (0x3f0000 bytes). An older port may be updated without erasing only after verifying the actual partition table; its version/name alone is insufficient. Lite does not mount or write the reserved filesystem.

Build: `python -m platformio run -e waveshare_lite`. Package: `python tools/build_release.py --board waveshare_lite`. Factory/update manifests and SHA-256 checksums are published with the other boards. The badge editor recognizes Lite but disables incompatible controls.

Read-only USB diagnostics: `status`, `contacts`, `selftest`, `axon status`, `traffic status`, `detector status`, `radar status`, `display status`, `ui status`, `loop diagnostics`, `badge info`. The developer `screenshot` command briefly pauses reception for its serial transfer and resumes it afterward. Commands that pause scanning, enter Hunt, transmit, change display settings or upload a badge are rejected.

See [validation status](VALIDATION.md) for checks actually completed. A matching signature is evidence of a radio pattern, not proof of malicious intent or recording state.
