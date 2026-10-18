# Dragon Detector 0.13.16 — Clock and timer power saving

Clock and stopwatch/Locksport Timer screens now pause background Wi-Fi and Bluetooth scanning on every supported board. Lap history and time/date settings also stay paused. Scanning resumes when leaving these screens; timing, animations, touch, buttons and USB time synchronization remain available.

- Applies to Waveshare AMOLED 1.75C, M5Stack StopWatch, Spotpear, Seeed XIAO Round and Waveshare Lite.
- Stops active radio sessions and captures on entry while retaining collected data. Fox advertising is stopped and requires explicit re-enabling.
- Previously configured Wi-Fi time recovery can briefly synchronize the clock without restarting background detection.
- All themes and saved settings remain compatible. Use the matching **Update** image to preserve files and settings.

See [clock power saving](https://git.churchofmalware.org/Valleytech/Dragon-Detector/src/branch/main/docs/CLOCK-POWER.md) and [the web flasher](https://valleytech-dragon-detector.pages.dev/).
