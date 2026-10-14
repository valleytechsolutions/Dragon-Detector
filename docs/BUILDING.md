# Build the current firmware

Use the root [platformio.ini](../platformio.ini); the projects under `hardware/` are historical releases.

```
python -m pip install platformio==6.2.0 esptool==5.4.0
python -m platformio run -e spotpear -e waveshare -e stopwatch
python tools/build_release.py --skip-build
```

The pinned pioarduino platform provides Arduino ESP32 and ESP-IDF. Each board requires 8 MB octal PSRAM. The Waveshare board's flash is deliberately addressed as 16 MB for the existing partition layout. Firmware uses one shared application in `firmware/`; only display/power adapters vary.

`tools/build_release.py` builds a blank badge filesystem, checks image/partition sizes, generates split factory/update manifests, merges a factory image, verifies every merged segment, rejects embedded workstation paths, and writes SHA-256 checksums plus source-commit metadata. Do not publish manually renamed binaries from a different board.

## Tests

```
node --test tests/web.test.js
python tools/test_core.py --cxx g++
python tools/test_core.py --cxx g++ --sanitize
```

On Windows without a native compiler, install `ziglang==0.13.0` and run `python tools/test_core.py` without `--cxx`. The packet tests include positive and negative fixtures, bounds checks, rollover, RSSI outliers/reacquisition, CRC consistency and 150,000 random inputs. CI additionally runs AddressSanitizer and UndefinedBehaviorSanitizer.

## Local web preview

```
python -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open `http://127.0.0.1:8765` in Chrome/Edge. There is no frontend build step. ESP Web Tools is pinned to 10.4.0 and loaded from its distribution; fonts are loaded from Google Fonts. These require network access. The image editor and CSV export process data locally. USB access is available only in supported Chromium browsers. The native test helper compiles the pinned OpenDroneID C decoder separately and links it into the C++ transport tests.

## License boundaries

The current original application/web/tooling is MIT. Vendored drivers/libraries retain their own license notices. `ESP8266Audio` is explicitly excluded from every current build. Historical projects under `hardware/` keep their previous licenses and must not be relabeled. See [CREDITS.md](../CREDITS.md).
