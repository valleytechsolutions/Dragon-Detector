# Dragon Detector 0.13.9

This release adds **Ghostbusters** and **Lockpickers United**, bringing full boards to **29 GUI themes and 27 selectable logo watches**. Existing saved theme/watch IDs are preserved.

- **Universal VTS startup:** the supplied black/gray VTS logo fades in on white on all five profiles, without added title text. The entire mark fits within each circular panel.
- **Ghostbusters:** original supplied no-ghost logo, black/white/red GUI, animated proton-stream radar, containment-trap watch details, analog hands and a protected 24-hour date/time window. Decorative effects do not represent additional detected devices.
- **Lockpickers United:** prominent original logo, monochrome GUI, silver short-hook/McTickler clock hands, and a Locksport Timer with START/PAUSE, SPLIT, OPEN and LOG. Sixty rim pins mark seconds; the active pin lifts and settles, completed pins brighten, and the timer freezes the animation on PAUSE/OPEN. Radar pins follow the decorative sweep.
- **Waveshare Lite:** BOOT-operated radar, compact themed analog/digital clock, stopwatch/locksport timer, signal inspector and diagnostics. Tap changes themes or starts/pauses timing; double-tap records a lap/OPEN or resets a paused timer; hold changes pages. USB clock setup now supports Lite. Reception continues on each page. These are compact Lite layouts, not the full-board elaborate watch scenes.
- **Clock retention:** the ESP32 system clock retains time through powered software restarts. Complete loss of power still requires an RTC, USB time sync, or optional saved-Wi-Fi recovery.
- **Wi-Fi recovery:** opt-in on all four full boards. A saved profile can synchronize Internet time after boot and every six hours, with bounded connection attempts and 15-minute retry after failure. The saved UTC offset remains fixed. [Spotpear setup guide](SPOTPEAR-CLOCK.md).
- **Lossless asset compression:** all original indexed logo pixels/palettes are preserved. Assets decode once into PSRAM, reducing flash use without per-frame allocation. Native decoder tests check all 135 packed panel-size assets against their original CRC32.

The 21% free-space guard remains enforced for every application slot and managed filesystem writes. OTA slots, storage layouts, user files and NVS are preserved by matching updates. [Measured flash budgets](FLASH-BUDGET.md).

## Validation and limits

All five profiles pass local builds and package checks, with at least 21% of each application slot free. StopWatch and Waveshare AMOLED pass 23/23 device self-tests, web-module clock synchronization, powered restart time retention, VTS fade-stage captures, theme/watch/timer renders, and running/pause/OPEN timing checks. Both continue receiving Wi-Fi/BLE; runtime counters and exact clean-CI image checks are recorded in the release verification JSON. StopWatch reports an available RTC; this connected AMOLED reports no available external RTC and uses system time. Spotpear hardware checks cover 23 self-tests, clock readback, powered restart retention, theme/watch/timer renders, continuing Wi-Fi/BLE reception and memory use. Wi-Fi profile setup was deferred by the owner; real Internet time recovery remains unverified on that device. Lite hardware checking remains stopped at the owner's request; new Lite pages and gestures are build/native-test checked, not panel-verified. No physical update to Lite is implied.

[Web flasher](https://valleytech-dragon-detector.pages.dev/) · [Theme artwork and attribution](../CREDITS.md) · [Detailed validation](VALIDATION.md)
