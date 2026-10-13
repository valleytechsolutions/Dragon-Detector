# Credits, provenance and licenses

Dragon Detector is by **Your Pal Kal / Valleytech Custom Solutions**. The current original application (`firmware/`), web UI, tests and tools are [MIT](LICENSE), as requested by the project owner. This does not relicense third-party code, trademarks, artwork or historical GPL builds.

## Colonel Panic, OUI Spy and Flock Noir

Thanks to **[Colonel Panic](https://colonelpanic.tech/)** for [OUI Spy](https://github.com/colonelpanichacks/oui-spy), [Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue) and [Flock-You](https://github.com/colonelpanichacks/flock-you). Please support the original research and tooling. Their signature research informed our independent bounded parser; Unified Blue application/UI code is not copied or relicensed.

The direct reference for the Flock prefix union and protocol facts is the owner's **[Flock Noir](https://github.com/valleytechsolutions/Flock-Noir)** snapshot `0559417e4f5e96f615ca4c4b42c3e0f0d56e2337`, particularly `radio_protocol.h` and its attribution ledger. [Its MIT notice is retained](docs/licenses/FLOCK-NOIR-LICENSE.txt). Continued research credits include **NitekryDPaul**, **DeFlockJoplin**, and **[Will Greenberg / wgreenberg](https://github.com/wgreenberg/flock-you)**. No endorsement or field-validation claim is implied.

## JustCallMeKoko / ESP32 Marauder

**[JustCallMeKoko](https://github.com/justcallmekoko)** and [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) supplied references for the existing passive Wi-Fi/BLE detectors, Pwnagotchi peer signature, pairing-pattern observation and bounded parser work. Original reference commit: `8ae4622abcc9c9c5729d4e97491907581d7f0c34`. [The original MIT notice is preserved](docs/licenses/MARAUDER-LICENSE.txt). This is not a complete or official Marauder port; active attack workflows are not included.

## CodeHedge and protocol research

- **[CodeHedge](https://github.com/CodeHedge)**: [Biscuit GATT reference](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html), community Marauder/CYD work and [Biscuit flashing tools](https://github.com/CodeHedge/biscuit_flasher). Name/service facts inform detection; no proprietary Biscuit code is included and Pro/Ultra compatibility is not claimed.
- **[Flipper Devices](https://github.com/flipperdevices/flipperzero-firmware)**: public serial-profile service/appearance values. No GPL Flipper application code is copied.
- **[Samsung SmartTag research](https://github.com/pogen300/galaxy_smarttag_research)**: public service/protocol findings. No research attack scripts or personal captures are bundled.
- **Bluetooth SIG**: company and member-service assignments used as vendor evidence, not guaranteed product identity.
- **NoPeek**: earlier manufacturer-ID reference documented in the historical Spotpear feature notes.

## Current build dependencies

| Component | Source / version | License / notice |
|---|---|---|
| ESP32 Arduino / ESP-IDF / tools | Pinned pioarduino platform in `platformio.ini` | Framework LGPL-2.1 and IDF/component licenses, including Apache-2.0; upstream distributions retain their notices. MIT applies to original app code, not a claim that the complete linked binary has one license. |
| LVGL | Retained 8.3.11 source in `hardware/spotpear/lib/lvgl` | [MIT notice](hardware/spotpear/lib/lvgl/LICENCE.txt); embedded fonts retain source notices |
| NimBLE-Arduino | Retained 2.3.6 source | Apache-2.0 / embedded NimBLE notices in the library |
| ESP32 Display Panel / IO Expander | Spotpear vendor 0.1.4 / 0.0.2 | [Panel Apache-2.0](hardware/spotpear/lib/ESP32_Display_Panel/license.txt), [expander Apache-2.0](hardware/spotpear/lib/ESP32_IO_Expander/license.txt) |
| CO5300 ESP-IDF panel driver | KODE DIY 1.0.2, commit `610bdac5ea02ed3ac38ce3d638c2482d95db74a5` | [Apache-2.0](lib/esp_lcd_co5300/license.txt), unmodified source/header and [provenance](lib/esp_lcd_co5300/UPSTREAM.md) |
| SensorLib / XPowersLib | Lewis He 0.3.1 / 0.2.6 | [SensorLib MIT](docs/licenses/SensorLib-MIT.txt), [XPowersLib MIT](docs/licenses/XPowersLib-MIT.txt) |
| M5Unified / M5GFX | Pinned Git commits in `platformio.ini` | [M5Unified MIT](docs/licenses/M5Unified-LICENSE.txt), upstream M5GFX MIT and embedded LovyanGFX/component notices |
| ESP Web Tools | ESPHome contributors, 10.4.0 | [Apache-2.0](https://github.com/esphome/esp-web-tools/blob/10.4.0/LICENSE); externally loaded distribution retains upstream notices |
| Barlow Condensed / DM Sans | Google Fonts | [Barlow OFL](https://github.com/google/fonts/blob/main/ofl/barlowcondensed/OFL.txt), [DM Sans OFL](https://github.com/google/fonts/blob/main/ofl/dmsans/OFL.txt); remotely hosted, not bundled |

Source code and pinned build recipes are provided for the application and linked components; toolchain/framework packages can be retrieved through PlatformIO. The current Waveshare adapter uses the explicitly licensed KODE DIY driver rather than the older Arduino_GFX dependency.

## Historical releases and media

The root previously contained a GPL-3.0 license because the Spotpear multimedia build included **ESP8266Audio**. Those existing `hardware/` source trees and binaries keep their original licenses and notices. [The previous root GPL text is preserved](docs/licenses/LEGACY-GPL-3.0.txt). ESP8266Audio is **not compiled into any 0.3.x badge build**. This MIT change applies to the owner's current original code; it is not a claim that retained GPL material became MIT.

Hardware and vendor configuration credits: **Spotpear / Guition**, **Waveshare**, **M5Stack**, **Espressif**. VTS artwork was supplied by the project owner. Manufacturer photographs remain with their owners and are linked for identification. Promotional artwork is not a hardware photograph. UI/browser screenshots describe their capture method and version. Dragon Ball is visual inspiration; no affiliation, endorsement, or trademark rights are granted by the software license.
