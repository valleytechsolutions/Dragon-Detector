# Credits, provenance and licenses

Dragon Detector is by **Your Pal Kal / Valleytech Custom Solutions**. The current original application (`firmware/`), web UI, tests and tools are [MIT](LICENSE), as requested by the project owner. This does not relicense third-party code, trademarks, artwork or historical GPL builds.

## Colonel Panic, OUI Spy and Flock Noir

Thanks to **[Colonel Panic](https://colonelpanic.tech/)** for [OUI Spy](https://github.com/colonelpanichacks/oui-spy), [Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue) and [Flock-You](https://github.com/colonelpanichacks/flock-you). Please support the original research and tooling. The Axon signature set and proximity-feedback workflow were also checked against **[AXEOFF](https://github.com/colonelpanichacks/axeoff)** as integrated in Unified Blue commit `6e5e2829540925ff1ab5fa6d0d365378818ed56e`. Our RTTTL parser, original melodies and tone scheduler are independently written; no Unified Blue/AXEOFF implementation is bundled. Their signature research informed our independent bounded parser; Unified Blue application/UI code is not copied or relicensed.

The direct reference for the Flock prefix union and protocol facts is the owner's **[Flock Noir](https://github.com/valleytechsolutions/Flock-Noir)** snapshot `0559417e4f5e96f615ca4c4b42c3e0f0d56e2337`, particularly `radio_protocol.h`, `alert_tones.h`, the RTTTL web configuration workflow and its attribution ledger. [Its MIT notice is retained](docs/licenses/FLOCK-NOIR-LICENSE.txt). Continued research credits include **NitekryDPaul**, **DeFlockJoplin**, and **[Will Greenberg / wgreenberg](https://github.com/wgreenberg/flock-you)**. No endorsement or field-validation claim is implied.

## JustCallMeKoko / ESP32 Marauder

**[JustCallMeKoko](https://github.com/justcallmekoko)** and [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) supplied references for the existing passive Wi-Fi/BLE detectors, Pwnagotchi peer signature, pairing-pattern observation and bounded parser work. Original reference commit: `8ae4622abcc9c9c5729d4e97491907581d7f0c34`. [The original MIT notice is preserved](docs/licenses/MARAUDER-LICENSE.txt). This is not a complete or official Marauder port; active attack workflows are not included.

## CodeHedge and protocol research

- **[CodeHedge](https://github.com/CodeHedge)**: [Biscuit GATT reference](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html), community Marauder/CYD work and [Biscuit flashing tools](https://github.com/CodeHedge/biscuit_flasher). Name/service facts inform detection; no proprietary Biscuit code is included and Pro/Ultra compatibility is not claimed.
- **[Flipper Devices](https://github.com/flipperdevices/flipperzero-firmware)**: public serial-profile service/appearance values. No GPL Flipper application code is copied.
- **[Samsung SmartTag research](https://github.com/pogen300/galaxy_smarttag_research)**: public service/protocol findings. No research attack scripts or personal captures are bundled.
- **Bluetooth SIG**: company and member-service assignments used as vendor evidence, not guaranteed product identity. Service-width decoding follows the [Core Supplement advertising data types](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/CSS_v14/out/en/core-supplementary-features/data-types-specification.html) and exact Bluetooth Base UUID representation; implemented independently.
- **NoPeek**: earlier manufacturer-ID reference documented in the historical Spotpear feature notes.

## SquachWatch feature reference

Thanks to **[skizzophrenic / SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD)** for the detection categories and daily alert/hunt workflow reference, reviewed at `a6167dbb0693963b5ad7022e7923cfb458d7676c`. SquachWatch is GPL-3.0. Its application code, graphics, games and mesh implementation are not copied into this MIT application. Dragon Detector implements the selected detector features independently using public protocol facts and the separately credited sources. [Comparison and scope](docs/DETECTION-PARITY.md). The 0.5 audit records every public identifier in a [test fact ledger](docs/data/squachwatch-coverage.json); no upstream matching functions are compiled. Thanks also to [jbohack / nyanBOX](https://github.com/jbohack/nyanBOX) for the Apple pairing/Find My observations cited by SquachWatch; our parser independently bounds these to valid AD structures and labels pairing-only evidence weak.

Vendor ownership facts were independently checked against the **[Wireshark IEEE-derived manufacturer registry](https://www.wireshark.org/download/automated/data/manuf)** on 2026-10-13. The selected assignment facts are recorded in [vendor-prefixes.csv](docs/data/vendor-prefixes.csv); broad component vendors remain weak clues. Google Find Hub framing follows [Google's public accessory specification](https://developers.google.com/nearby/fast-pair/specifications/extensions/fmdn). Protocol identifiers are not evidence of a particular operator or intent.

## Current build dependencies

| Component | Source / version | License / notice |
|---|---|---|
| ESP32 Arduino / ESP-IDF / tools | Pinned pioarduino platform in `platformio.ini` | Framework LGPL-2.1 and IDF/component licenses, including Apache-2.0; upstream distributions retain their notices. MIT applies to original app code, not a claim that the complete linked binary has one license. |
| ES8311 codec driver | Espressif / Waveshare Arduino adaptation, pinned source | [Apache-2.0](lib/es8311/LICENSE), unmodified source and [provenance](lib/es8311/UPSTREAM.md) |
| OpenDroneID | Core commit `849c03adbe0cdd68997a462fafab397edcfe53f3`, unmodified message decoder | [Apache-2.0](lib/opendroneid/LICENSE), retained copyright/SPDX and [provenance](lib/opendroneid/UPSTREAM.md) |
| LVGL | Retained 8.3.11 source in `hardware/spotpear/lib/lvgl` | [MIT notice](hardware/spotpear/lib/lvgl/LICENCE.txt); embedded fonts retain source notices |
| NimBLE-Arduino | Retained 2.3.6 source | Apache-2.0 / embedded NimBLE notices in the library |
| ESP32 Display Panel / IO Expander | Spotpear vendor 0.1.4 / 0.0.2 | [Panel Apache-2.0](hardware/spotpear/lib/ESP32_Display_Panel/license.txt), [expander Apache-2.0](hardware/spotpear/lib/ESP32_IO_Expander/license.txt) |
| CO5300 ESP-IDF panel driver | KODE DIY 1.0.2, commit `610bdac5ea02ed3ac38ce3d638c2482d95db74a5` | [Apache-2.0](lib/esp_lcd_co5300/license.txt), unmodified source/header and [provenance](lib/esp_lcd_co5300/UPSTREAM.md) |
| SensorLib / XPowersLib | Lewis He 0.3.1 / 0.2.6 | [SensorLib MIT](docs/licenses/SensorLib-MIT.txt), [XPowersLib MIT](docs/licenses/XPowersLib-MIT.txt) |
| M5Unified / M5GFX | Pinned Git commits in `platformio.ini` | [M5Unified MIT](docs/licenses/M5Unified-LICENSE.txt), upstream M5GFX MIT and embedded LovyanGFX/component notices |
| TFT_eSPI | Bodmer, 2.5.44 / commit `16e37595040eac69cd628e4bffb56fc30cad6299`, XIAO only | [Upstream MIT/Adafruit notices](docs/licenses/TFT_eSPI-LICENSE.txt); source and embedded notices retained by the pinned dependency |
| ESP Web Tools | ESPHome contributors, 10.4.0 | [Apache-2.0](https://github.com/esphome/esp-web-tools/blob/10.4.0/LICENSE); externally loaded distribution retains upstream notices |
| Barlow Condensed / DM Sans | Google Fonts | [Barlow OFL](https://github.com/google/fonts/blob/main/ofl/barlowcondensed/OFL.txt), [DM Sans OFL](https://github.com/google/fonts/blob/main/ofl/dmsans/OFL.txt); remotely hosted, not bundled |

Source code and pinned build recipes are provided for the application and linked components; toolchain/framework packages can be retrieved through PlatformIO. The current Waveshare adapter uses the explicitly licensed KODE DIY driver rather than the older Arduino_GFX dependency.

## Historical releases and media

The root previously contained a GPL-3.0 license because the Spotpear multimedia build included **ESP8266Audio**. Those existing `hardware/` source trees and binaries keep their original licenses and notices. [The previous root GPL text is preserved](docs/licenses/LEGACY-GPL-3.0.txt). ESP8266Audio is **not compiled into any current 0.3.x/0.4.x/0.5.x/0.6.x/0.7.x/0.8.x/0.9.x build**. This MIT change applies to the owner's current original code; it is not a claim that retained GPL material became MIT.

Hardware and vendor configuration credits: **Spotpear / Guition**, **Waveshare**, **M5Stack**, **Seeed Studio**, **Espressif**. VTS artwork and the [BWM1 radar emblem](media/BWM1-official.png) were supplied by the project owner. The original PNG is retained unchanged; tools/encode_radar_logo.py creates native-size RGB565/alpha display assets. Brand/trademark rights remain with their respective owners. The 0.6 traffic monitors, RSSI time sampling and gap-aware trace are original implementations, informed by the credited protocol and workflow references. Manufacturer photographs remain with their owners and are linked for identification. Promotional artwork is not a hardware photograph. UI/browser screenshots describe their capture method and version. Dragon Ball is visual inspiration; no affiliation, endorsement, or trademark rights are granted by the software license.

The 0.7 classic grid, yellow dots and center marker are original LVGL drawing code informed by the [official Bandai / Tamashii PROPLICA Dragon Radar visual reference](https://tamashiiweb.com/item/13034/?sa=JAPAN). No Bandai product photographs, graphics or sounds are bundled as firmware assets. Dragon Ball and associated marks remain with their respective owners.

The Green and Black/gold/red themes, sweep animation, bounded label placement, six-contact radar pages and explicitly aged recent markers are original Dragon Detector implementations. Theme screenshots are captures of its own LVGL renderer; private local identifiers are not published.

The 0.8 Badge Studio transforms, per-board previews, guides, PNG export and manifest validation are original application code. StopWatch touch, display, audio and power handling use the pinned M5Unified/M5GFX drivers credited above; the board adapter supplies its native-coordinate mapping. No additional third-party image editor or image-processing dependency is bundled.

## 0.9 daily tools

Stopwatch/lap arithmetic, calendar validation, manual rotation buffering, device-family colors, buffered M5Stack input and USB clock controls are original MIT application code. RTC integration uses the already-credited M5Unified RX8130 driver. Waveshare PCF85063 register access follows the manufacturer register specification; AXP2101 short-button events use XPowersLib. No new third-party library is added. Hardware sources are listed in [HARDWARE.md](docs/HARDWARE.md).

## 0.9.2 Seeed XIAO Round Display

Thanks to **Seeed Studio** for the XIAO ESP32-S3 and Round Display hardware, schematics, pin definitions and CHSC6X protocol example in [Seeed_Arduino_RoundDisplay](https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay), reviewed at `e811877f5b150981a481c39abd9c7ee0a029cd1c`. [Its MIT notice is retained](docs/licenses/Seeed-RoundDisplay-MIT.txt). The adapter implements checked touch/RTC reads, buffered input and SPI-card integration using these references; the complete demo/UI is not bundled. **Bodmer / TFT_eSPI**, with its retained **Adafruit** notices, provides the pinned GC9A01 display driver. The RTC register layout follows Seeed’s linked PCF8563 datasheet. The small-screen layout, web board identity and 240-pixel badge support are original Dragon Detector code.
