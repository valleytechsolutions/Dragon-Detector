# Build the current firmware

Use the root [platformio.ini](../platformio.ini); the projects under `hardware/` are historical releases.

```
python -m pip install platformio==6.2.0 esptool==5.4.0
python -m platformio run -e spotpear -e waveshare -e stopwatch -e xiao_round -e waveshare_lite
python tools/build_release.py --skip-build
```

The pinned pioarduino platform provides Arduino ESP32 and ESP-IDF. Full profiles use octal PSRAM; Waveshare Lite has 2 MB QSPI PSRAM. The M5Stack Dial beta has no PSRAM and uses a separate compact memory configuration. Waveshare AMOLED 1.75C uses 32 MB flash, XIAO uses 8 MB, and the other profiles use 16 MB. Firmware uses one shared application in `firmware/`; display/power and speaker-output adapters vary.

`tools/build_release.py` builds a blank badge filesystem, checks image/partition sizes, generates split factory/update manifests, merges a factory image, verifies every merged segment, rejects embedded workstation paths, and writes SHA-256 checksums plus source-commit metadata. Do not publish manually renamed binaries from a different board.

## Tests

```
node --test tests/*.test.js
python tools/test_core.py --cxx g++
python tools/test_core.py --cxx g++ --sanitize
```

On Windows without a native compiler, install `ziglang==0.13.0` and run `python tools/test_core.py` without `--cxx`. The packet tests include positive and negative fixtures, bounds checks, rollover, RSSI outliers/reacquisition, CRC consistency 150,000 random radio inputs and 50,000 random RTTTL inputs. The runner explicitly keeps test assertions enabled in optimized builds. CI additionally runs AddressSanitizer and UndefinedBehaviorSanitizer.

## Local web preview

```
python -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open `http://127.0.0.1:8765` in Chrome/Edge. There is no frontend build step. ESP Web Tools is pinned to 10.4.0 and loaded from its distribution; fonts are loaded from Google Fonts. These require network access. The image editor and CSV export process data locally. USB access is available only in supported Chromium browsers. The native test helper compiles the pinned OpenDroneID C decoder separately and links it into the C++ transport tests.

## License boundaries

For a preview with standalone guides, verified firmware downloads and publication checks, follow [Publishing](GITHUB-UPLOAD.md#independent-hosting).

The current original application/web/tooling is MIT. Vendored drivers/libraries retain their own license notices. `ESP8266Audio` is explicitly excluded from every current build. Historical projects under `hardware/` keep their previous licenses and must not be relabeled. See [CREDITS.md](../CREDITS.md).

## Non-C AMOLED 1.75 experimental build

Run `pio run -e waveshare_175` and `python tools/build_release.py --board waveshare_175`. This uses the shared AMOLED adapter with a separate 16 MB partition table. It does not use the `waveshare` C-variant environment. See [port status](WAVESHARE-AMOLED-175.md).

## M5Stack Dial beta

Build and package only this board with `python tools/build_release.py --board m5dial`. The profile uses 8 MB flash, two 3 MiB app slots and a 1.94 MiB internal filesystem. Do not rename another board image. Run `python tools/test_m5dial.py` and `python tools/test_rtc.py` for native checks; `python tools/render_m5dial.py` checks actual LVGL output against the normal indexed-image renderer. These do not substitute for the [first-unit hardware checklist](M5STACK-DIAL.md).
