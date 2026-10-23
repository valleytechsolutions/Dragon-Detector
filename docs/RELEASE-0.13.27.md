# Dragon Detector 0.13.27 — M5Stack Dial v1.1

This release makes the M5Stack Dial v1.1 profile available outside beta following physical display, touch, ring, button, radar selection and RTC restart checks. Other board downloads keep their existing versions.

- Press the front button on Radar, Clock or Status to open Menu. Turn to highlight a choice and press to select. Every page also has a touchscreen MENU button.
- Tools opens the timer, signal details and status. Settings includes separate Time and Date editors, brightness and alert sound. Themes change only through an explicit selection.
- Tap a radar dot for Details, Hunt/Stop and Close. The ring also selects these actions. Tap the same dot again or empty space to dismiss its panel. Stale signals cannot start a new hunt.
- Radar rings and grid render without a PSRAM backdrop. Tracker dots are larger, leader lines are thinner, and notifications have a themed banner with tap-to-dismiss.
- The display uses the correct SPI configuration for this Arduino core. The touch/RTC bus initializes after display reset and can recover from communication failures. RTC reads and writes retry detection after a transient startup failure.
- Clock and timer pause Wi-Fi/BLE scans. The hardware clock is verified after USB synchronization and was checked across a firmware restart without resynchronizing.

The Dial is a compact, passive profile with 32 contact slots and 40 built-in themes. It has no PSRAM, SD slot, active BLE/Wi-Fi labs, badge uploads or saved captures. The original K130 has not been physically tested; this release was verified on v1.1. RTC retention after removing every power source is not guaranteed; keep its RTC supply powered or synchronize over USB after reconnecting.

Use **Fresh installation** when replacing the factory demo. Existing Dragon Detector Dial 0.13.26 beta installations use the same partitions and can choose **Update**. The M5Burner package is an 8 MB merged factory image flashed at **0x0**, exclusively for M5Stack Dial hardware. It is not a StopWatch image.

[Install firmware](https://valleytech-dragon-detector.pages.dev/#install) · [Dial controls and clock setup](M5STACK-DIAL.md)
