# Credits, provenance and licenses

Dragon Detector is by **Your Pal Kal / Valleytech Custom Solutions**. The current original application (`firmware/`), web UI, tests and tools are [MIT](LICENSE), as requested by the project owner. This does not relicense third-party code, trademarks, artwork or historical GPL builds.

## Colonel Panic, OUI Spy and Flock Noir

Thanks to **[Colonel Panic](https://colonelpanic.tech/)** for [OUI Spy](https://github.com/colonelpanichacks/oui-spy), [Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue) and [Flock-You](https://github.com/colonelpanichacks/flock-you). Please support the original research and tooling. The Axon signature set and proximity-feedback workflow were also checked against **[AXEOFF](https://github.com/colonelpanichacks/axeoff)** as integrated in Unified Blue commit `6e5e2829540925ff1ab5fa6d0d365378818ed56e`. Our RTTTL parser, original melodies and tone scheduler are independently written; no Unified Blue/AXEOFF implementation is bundled. Their signature research informed our independent bounded parser; Unified Blue application/UI code is not copied or relicensed. The 0.9.3 reception audit uses the same pinned AXEOFF source and documents its broader partial-UUID/random-address matching separately from registered evidence in [Axon detection notes](docs/AXON.md). Reserved receive capacity, transport recovery and first-hit alert policy are independent implementations.

The direct reference for the Flock prefix union and protocol facts is the owner's **[Flock Noir](https://github.com/valleytechsolutions/Flock-Noir)** snapshot `0559417e4f5e96f615ca4c4b42c3e0f0d56e2337`, particularly `radio_protocol.h`, `alert_tones.h`, the RTTTL web configuration workflow and its attribution ledger. [Its MIT notice is retained](docs/licenses/FLOCK-NOIR-LICENSE.txt). Continued research credits include **NitekryDPaul**, **DeFlockJoplin**, and **[Will Greenberg / wgreenberg](https://github.com/wgreenberg/flock-you)**. No endorsement or field-validation claim is implied.

## JustCallMeKoko / ESP32 Marauder

**[JustCallMeKoko](https://github.com/justcallmekoko)** and [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) supplied references for the existing passive Wi-Fi/BLE detectors, Pwnagotchi peer signature, pairing-pattern observation and bounded parser work. Original reference commit: `8ae4622abcc9c9c5729d4e97491907581d7f0c34`. [The original MIT notice is preserved](docs/licenses/MARAUDER-LICENSE.txt). This is not a complete or official Marauder port; 0.10.0 adds the bounded Waveshare workflows described in the radio-toolkit guide.

## CodeHedge and protocol research

- **[CodeHedge](https://github.com/CodeHedge)**: [Biscuit GATT reference](https://codehedge.github.io/Biscuit-Wiki/3rd-party-integration/gatt-reference.html), community Marauder/CYD work and [Biscuit flashing tools](https://github.com/CodeHedge/biscuit_flasher). Name/service facts inform detection; no proprietary Biscuit code is included and Pro/Ultra compatibility is not claimed.
- **[Flipper Devices](https://github.com/flipperdevices/flipperzero-firmware)**: public serial-profile service/appearance values. No GPL Flipper application code is copied.
- **[Samsung SmartTag research](https://github.com/pogen300/galaxy_smarttag_research)**: public service/protocol findings. No research attack scripts or personal captures are bundled.
- **Bluetooth SIG**: company and member-service assignments used as vendor evidence, not guaranteed product identity. Service-width decoding follows the [Core Supplement advertising data types](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/CSS_v14/out/en/core-supplementary-features/data-types-specification.html) and exact Bluetooth Base UUID representation; implemented independently.
- **NoPeek**: earlier manufacturer-ID reference documented in the historical Spotpear feature notes.

## SquachWatch feature reference

Thanks to **[skizzophrenic / SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD)** for the detection categories and daily alert/hunt workflow reference, reviewed at `1485b0122aa527f8309c3f4e6ba8dc1614be5bb1`. SquachWatch is GPL-3.0. Its application code, graphics, games and mesh implementation are not copied into this MIT application. Dragon Detector implements the selected detector features independently using public protocol facts and the separately credited sources. [Comparison and scope](docs/DETECTION-PARITY.md). The 0.5 audit records every public identifier in a [test fact ledger](docs/data/squachwatch-coverage.json); no upstream matching functions are compiled. Thanks also to [jbohack / nyanBOX](https://github.com/jbohack/nyanBOX) for the Apple pairing/Find My observations cited by SquachWatch; our parser independently bounds these to valid AD structures and labels pairing-only evidence weak.

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

The root previously contained a GPL-3.0 license because the Spotpear multimedia build included **ESP8266Audio**. Those existing `hardware/` source trees and binaries keep their original licenses and notices. [The previous root GPL text is preserved](docs/licenses/LEGACY-GPL-3.0.txt). ESP8266Audio is **not compiled into any current 0.3.x/0.4.x/0.5.x/0.6.x/0.7.x/0.8.x/0.9.x/0.10.x/0.11.x/0.12.x/0.13.x build**. This MIT change applies to the owner's current original code; it is not a claim that retained GPL material became MIT.

Hardware and vendor configuration credits: **Spotpear / Guition**, **Waveshare**, **M5Stack**, **Seeed Studio**, **Espressif**. VTS artwork and the [BWM1 radar emblem](media/BWM1-official.png) were supplied by the project owner. The original PNG is retained unchanged; tools/encode_radar_logo.py creates native-size RGB565/alpha display assets. Brand/trademark rights remain with their respective owners. The 0.6 traffic monitors, RSSI time sampling and gap-aware trace are original implementations, informed by the credited protocol and workflow references. Manufacturer photographs remain with their owners and are linked for identification. Promotional artwork is not a hardware photograph. UI/browser screenshots describe their capture method and version. Dragon Ball is visual inspiration; no affiliation, endorsement, or trademark rights are granted by the software license.

The 0.7 classic grid, yellow dots and center marker are original LVGL drawing code informed by the [official Bandai / Tamashii PROPLICA Dragon Radar visual reference](https://tamashiiweb.com/item/13034/?sa=JAPAN). No Bandai product photographs, graphics or sounds are bundled as firmware assets. Dragon Ball and associated marks remain with their respective owners.

The Green and Black/gold/red themes, sweep animation, bounded label placement, six-contact radar pages and explicitly aged recent markers are original Dragon Detector implementations. Theme screenshots are captures of its own LVGL renderer; private local identifiers are not published.

The 0.8 Badge Studio transforms, per-board previews, guides, PNG export and manifest validation are original application code. StopWatch touch, display, audio and power handling use the pinned M5Unified/M5GFX drivers credited above; the board adapter supplies its native-coordinate mapping. No additional third-party image editor or image-processing dependency is bundled.

## 0.9 daily tools

Stopwatch/lap arithmetic, calendar validation, manual rotation buffering, device-family colors, buffered M5Stack input and USB clock controls are original MIT application code. RTC integration uses the already-credited M5Unified RX8130 driver. Waveshare PCF85063 register access follows the manufacturer register specification; AXP2101 short-button events use XPowersLib. No new third-party library is added. Hardware sources are listed in [HARDWARE.md](docs/HARDWARE.md).

## 0.9.2 Seeed XIAO Round Display

Thanks to **Seeed Studio** for the XIAO ESP32-S3 and Round Display hardware, schematics, pin definitions and CHSC6X protocol example in [Seeed_Arduino_RoundDisplay](https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay), reviewed at `e811877f5b150981a481c39abd9c7ee0a029cd1c`. [Its MIT notice is retained](docs/licenses/Seeed-RoundDisplay-MIT.txt). The adapter implements checked touch/RTC reads, buffered input and SPI-card integration using these references; the complete demo/UI is not bundled. **Bodmer / TFT_eSPI**, with its retained **Adafruit** notices, provides the pinned GC9A01 display driver. The RTC register layout follows Seeed’s linked PCF8563 datasheet. The small-screen layout, web board identity and 240-pixel badge support are original Dragon Detector code.

### Waveshare ESP32-S3-LCD-1.28 Lite

The independent board adapter follows Waveshare's [official board documentation](https://docs.waveshare.com/ESP32-S3-LCD-1.28) and [schematic](https://files.waveshare.com/wiki/ESP32-S3-LCD-1.28/Esp32-s3-lcd-.128-sch.pdf). It uses the same pinned MIT-licensed TFT_eSPI dependency as XIAO Round. The shared Dragon Detector code remains MIT; no third-party port source was incorporated.

## BLE Lab / 0.9.8

**[Matthew KuKanich / ChimeraBLE](https://github.com/MatthewKuKanich/ChimeraBLE)** inspired the advertisement-inspection, GATT exploration, snapshot and peripheral-emulation workflows. Its published README was used as a feature reference. Dragon Detector's BLE Lab, DDG1 format, comparison logic, bounded queues, storage layout and interfaces are independently written MIT code. No ChimeraBLE GPL firmware source or its Wireshark-derived OUI database is bundled or relicensed. Thanks to Matthew for the research and workflow ideas.

BLE operations use the already-credited **NimBLE-Arduino / h2zero** Apache-2.0 library and its bundled Apache/Mynewt components. GATT roles and attributes follow the public Bluetooth protocol; UUIDs alone do not prove a product's identity. LittleFS uses the framework's existing BSD-3-Clause implementation. The opt-in Windows test script uses Bleak (MIT) and PyWinRT (MIT) as host tools; they are not linked into the firmware. The demo GATT service, values and test traffic are original synthetic fixtures.

## Waveshare radio toolkit / 0.10.0

Selectable focused modes are independently implemented from the documented workflows of [Colonel Panic / Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue). No Unified Blue source was copied. The bounded packet builders and helper formats were adapted from Valleytech HackerPhone's original MIT implementation (local source revision `c80bf5e5e1ed8c6b957f87b98603942a9e2f90a8`), which credits [JustCallMeKoko / ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder). The [Marauder MIT copyright/permission notice](third_party/licenses/MARAUDER.txt) is retained alongside the existing notice. The raw-frame compatibility hook follows Marauder's published linkage technique and is limited to the pinned SDK. New mode scheduling, capture transport, round-display UI and storage integration are original Dragon Detector code.

## 0.11 recon and signature audit

The feature audit pins **Marauder** at `d3ceb28afeda1196ca27c13adeaa69ada52383f3`, **ChimeraBLE** at `2330423dae8f8fa163617172310784889d8972a7`, **SquachWatch** at `1485b0122aa527f8309c3f4e6ba8dc1614be5bb1` and **Unified Blue** at `6e5e2829540925ff1ab5fa6d0d365378818ed56e`. Public identifier/protocol facts and documented workflows inform original MIT parsers, interfaces and state machines; GPL application code is not linked or redistributed. New SquachWatch facts are individually recorded in the coverage ledger. Vendor facts use the previously obtained 2026-10-13 IEEE-derived registry snapshot.

Marauder's MIT public tracker-sound UUID/command references informed the explicit AirTag/Find My/DULT write requests. Its portal, probe-SSID and CSI workflows informed feature scope; our DNS/HTTP service, bounded logs, raw-key selection, CSI amplitude-shape estimator, calibration and queue are original code. CSI driver integration follows [Espressif's pinned ESP-IDF 5.1.4 Wi-Fi documentation](https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32s3/api-guides/wifi.html#wi-fi-channel-state-information). The score is not Marauder's fused phase/amplitude algorithm.

ChimeraBLE inspired expanded GATT read/write, subscriptions, security controls, advertisement interpretation, HID layout inspection and replay workflows. No ChimeraBLE source/database is copied. Bluetooth SIG assigned numbers, the public HID item format and the existing NimBLE APIs underpin the independent implementation. Attribution is not a claim of equivalent security bypasses, hardware support or endorsement.

## Continuous recon / 0.12.0

The independent Recon Notebook, cooperative USB receive processing, jitter-tolerant scan scheduler, memory reserve checks and advertisement simulators are original MIT application code. [IEEE Registration Authority](https://standards-oui.ieee.org/oui/oui.csv) supplies the factual MA-L prefix/organization registry, fetched 2026-10-15; no addresses or implementation code were imported from another firmware. [Snapshot provenance, exact hashes and limitations](docs/VENDOR-REGISTRY.md). Common Bluetooth protocol labels follow the previously credited assigned-number references. Manufacturer names and protocol marks remain their owners’ names/marks; registry context is not an endorsement or product identity. ChimeraBLE remains a feature reference; no GPL implementation was integrated.

## 0.13 independent implementations

Recon-session records use factual RCN1/PRB1/REL1 layouts from the already credited Marauder revision `d3ceb28afeda1196ca27c13adeaa69ada52383f3`; its MIT notice remains included. The bounded recipe engine, AP archive, UI scaling, GATT bridge and HID semantic decoder are original code. HID parsing follows the [USB HID 1.11 specification](https://www.usb.org/sites/default/files/hid1_11.pdf). Registry compression follows the public [LZ4 block format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md) using an independently written encoder and bounded decoder; no LZ4 library implementation is bundled. ChimeraBLE remains a workflow reference, not a linked dependency.

## Radar theme artwork · 0.13.1

The project owner supplied the unchanged source PNGs for [Valleytech Custom Solutions](media/theme-logos/valleytech.png), [Biscuit Shop](media/theme-logos/biscuit-shop.png), and [JustCallMeKoko / Marauder](media/theme-logos/jcmk.png). Artwork and brand rights remain with their respective owners; the software MIT license does not grant rights to these marks or imply endorsement. These are optional Dragon Detector appearance themes, not official firmware ports or affiliations.

`tools/encode_radar_logo.py` encodes native 240/360/466-pixel panel assets from the supplied originals, preserving aspect ratio and alpha. The transparent Valleytech mark uses silver radar ink and gold watch ink, with no background plate; Biscuit's white artwork retains its transparency; JCMK retains its supplied black background. The original three themes retain the BWM1 logo. Palettes, layout and drawing code are original Dragon Detector work. Lite keeps its existing fixed green radar and does not include the new logo arrays.

The enlarged digital clock numerals are generated from the bundled Montserrat Medium font by The Montserrat Project Authors (copyright 2011), under [SIL OFL 1.1](docs/licenses/Montserrat-OFL.txt). `tools/encode_watch_digits.py` creates a compact 4-bit digit subset at each native display size. Font data retain OFL licensing; the encoder and watch layout are original project code.

## TOOOL theme artwork · 0.13.6

The project owner supplied the unchanged [TOOOL logo](media/theme-logos/toool.png) for this optional appearance theme and watch. The mark remains its owner’s artwork; its inclusion does not imply an official TOOOL product or endorsement. Native display-size copies are generated by `tools/encode_radar_logo.py`; the white/red layout, pin-tumbler ornament and Locksport Timer interface are original project work.

## Church of Malware artwork · 0.13.6

The project owner supplied the unchanged [Church of Malware emblem](media/theme-logos/church-of-malware.png). It is used as a small radar mark and a large native-resolution centerpiece in the binary watch, preserving the complete artwork. Brand/artwork rights remain with their owner; this optional theme does not imply endorsement. The surrounding binary layout and circuit ornament are original project work.

## Covert Instruments and McNally artwork · 0.13.6

- The user supplied the CI logo in `media/theme-logos/covert-instruments.png` and McNally artwork photograph in `media/theme-logos/mcnally-source.webp`. Both source files are retained unchanged. The encoder selects the original square McNally artwork from its fabric background for display; it does not redraw the mark.
- The supplied Reaper McTickler and Reaper short-hook photos are retained in `media/watch-references/`. The small native vector hand silhouettes in `pick_hands.inc` are traced from those references and widened slightly for legibility, generated by `tools/encode_pick_hands.py`.
- Brand names and artwork belong to their respective owners. Their inclusion is user-requested customization and does not imply affiliation or endorsement.

## BWM startup artwork · 0.13.6

The user supplied [BWM2-official.png](media/BWM2-official.png), retained byte-for-byte. Native display copies preserve its aspect ratio and transparency. All full boards show this centered logo alone on black at startup, independent of theme. Lite retains its direct radar startup.

## AWOK, Baph0Mau5 and WDGWARS artwork · 0.13.6

The user supplied media/theme-logos/awok-source.jxl, awok-mushrooms.png, baph0mau5.jpg and wdgwars.jpg; the source files are retained unchanged. awok.png is a lossless format decode of the JXL using pillow-jxl-plugin 1.3.8. Its original alpha silhouette is recolored white by LVGL for contrast; no lettering is redrawn. The native vector mushrooms, paint drips, sigils, moon ornament, map silhouettes and signal accents are original layout work inspired by these references. The marks remain their respective owners’ artwork and do not imply endorsement or affiliation.

## GhostStrats and N3rdsec artwork · 0.13.6

The supplied GhostStrats profile and banner are retained as `media/theme-logos/ghoststrats-profile.jpg` and `ghoststrats-banner.jpg`. The glowing slit-eye mark is native vector geometry inspired by that banner; the requested tagline is “Safety is an Illusion.”. `media/theme-logos/n3rdsec.jpg` retains the supplied skull source unchanged; the asset encoder excludes its blank outer margins and LVGL applies a circular display mask. The marks remain their respective owners’ artwork.

## HaleHound artwork · 0.13.6

The supplied `hh-stl-splash.webp` and `HOUND_LOGO_1280x640_e9cd8634-c7b6-4077-91e4-7658e7153c3b.webp` are preserved unchanged as `media/theme-logos/halehound-splash.webp` and `halehound-wordmark.webp`. The white splash retains its original transparency. The moving flame geometry and dial layout are original code; the supplied logos remain their owner’s artwork.

## Maker, character and DMAC artwork · 0.13.6

Original supplied Waveshare, M5Stack, LAB5, Talking Sasquach portrait/cartoon, Nyan Devices and DMAC artwork is preserved in `media/theme-logos/`. The two DMAC acrylic pick photographs remain unchanged as `dmac-pick-profile.webp` and `dmac-pick-angle.webp`; their native vector watch profiles reproduce the curved black handles, copper inlays, rivets and steel tips. Sasquach body/arms, pixel cat/rainbow animation, waveform/module/circuit ornaments and dial layouts are original code. Logos remain their owners’ artwork. The [source-artwork validation ledger](docs/THEME-ARTWORK.json) records byte comparisons and hashes.

| Supplied brand / creator | Preserved source artwork |
|---|---|
| Waveshare | [Green logo](media/theme-logos/waveshare.png) |
| M5Stack | [Blue/white circular logo](media/theme-logos/m5stack.jpg) |
| LAB5 | [Scientist / Control the Chaos artwork](media/theme-logos/lab5.jpg) |
| Talking Sasquach | [Cartoon](media/theme-logos/sasquach-cartoon.jpg), [portrait](media/theme-logos/sasquach-head.jpg) |
| Nyan Devices | [Multicolor cat logo](media/theme-logos/nyan-devices.png) |
| DMAC | [Logo](media/theme-logos/dmac.jpg), [profile pick reference](media/theme-logos/dmac-pick-profile.webp), [angled pick reference](media/theme-logos/dmac-pick-angle.webp) |

The tiny animated cat is an original code-drawn Nyan Cat homage. Nyan Cat is associated with [Chris Torres / prguitarman](https://x.com/PRguitarman); this project includes no original Nyan Cat sprite, GIF, music or audio. The software license does not extend to third-party logos, supplied photographs or brand artwork. No endorsement is implied.

## Seeed Studio artwork · 0.13.6

The owner supplied the unchanged [Seeed Studio PNG](media/theme-logos/seeed-studio.png) and [AVIF reference](media/theme-logos/seeed-studio-source.avif). The circular lime/deep-teal PNG is used on the radar and watch, with native display-size indexed assets. The leaf-circuit ornament, analog hands and GUI palette are original project work. The Seeed Studio mark remains its owner’s artwork; this optional theme does not imply affiliation or endorsement.

## DagNazty artwork · 0.13.6

The owner supplied the unchanged [DagNazty skull artwork](media/theme-logos/dagnazty.jpg), originally `38539274.jpg`. Its light background is matched by the white-room palette; the original mark is circularly clipped on the radar/watch. The talon outlines, fang accents and analog hands are original native drawing code. The supplied artwork remains its owner’s work and is not covered by the project’s software license; inclusion does not imply endorsement.

## 0ct0’s Pig and ASCII lettering · 0.13.6

The owner supplied [0ct0S3c’s pink octopus](media/theme-logos/octosec.webp) and an [M5Porkchop ASCII pig reference](media/theme-logos/octosec-pig-reference.png). Both original files remain unchanged. The octopus is sampled into an ASCII grid at build time; the pig, its animation, speech bubble and interaction are new drawing code inspired by that reference. No M5Porkchop firmware code is included. The supplied artwork remains its owner’s work; inclusion does not imply endorsement. The requested display name is **0ct0’s Pig**.

Pixel lettering is generated from the bundled **Unscii-8** ASCII glyphs by viznut, released as public domain / CC0. The Unifont-derived full variants are not used. [Upstream font and licensing](https://github.com/viznut/unscii) · [local notice](docs/licenses/Unscii-NOTICE.txt).

## DeFlock artwork · 0.13.6

The owner supplied the unchanged [camera-raven illustration](media/theme-logos/deflock.png), originally `unnamed.png`. Native palette-indexed display copies omit only blank outer margins. The navy/steel-blue interface, viewfinder ornaments and optical clock layout are original project code. The supplied artwork remains its owner’s work and is not covered by the software license; this optional appearance theme does not imply affiliation or endorsement.

### 0.13.7 installer and Lite refinements

The installer continues to use [ESP Web Tools 10.4.0](https://github.com/esphome/esp-web-tools/tree/10.4.0), with its dialog port/manifest properties and normal close event for local clock setup. In 0.13.10, `web/clock-install-dialog.js` also adapts the pinned `_initialize`, `_startInstall` and `_closeDialog` hooks to hand off automatically and enforce the selected erase mode. These version-specific hooks have lifecycle tests and must be rechecked before changing the dependency. The dependency is pinned independently of firmware versions. The animated flasher radar is original code-drawn decoration. Lite reuses the already credited supplied theme artwork and native GhostStrats/ASCII marks; no new third-party art was added.

## Covert Instruments and LockPickingLawyer · 0.13.8

- The user supplied the LockPickingLawyer seal, retained unchanged as `media/theme-logos/lockpickinglawyer.jpg`, and the CI door mark (identical to the existing `media/theme-logos/covert-instruments.png`). These marks belong to their respective owners; their inclusion does not imply affiliation, endorsement or a grant of trademark rights.
- Native-size indexed conversion preserves the original artwork. The LPL seal uses a circular display mask; the navy/gold UI, rope-like watch bezel and rotating two-pan balance are code-drawn ornament. The silver hands reuse the Reaper short-hook and McTickler profiles credited above.
- Included on Waveshare AMOLED, Spotpear, Seeed XIAO Round and M5Stack StopWatch. Waveshare Lite includes the radar palette, CI header, LPL emblem and rotating scales; watches and timers remain full-board features.

## Ghostbusters and Lockpickers United · 0.13.9

The owner supplied [the Ghostbusters no-ghost logo](media/theme-logos/ghostbusters.jpg), originally `ghostbusters-logo-1.jpg`, and [the Lockpickers United logo](media/theme-logos/lockpickers-united.jpg), originally `LPU-logo.jpg`. Both source files are retained byte-for-byte; SHA-256 provenance is in [the artwork ledger](docs/THEME-ARTWORK.json). These artworks/trademarks remain their respective owners' property and are not relicensed under the software's MIT license. Inclusion is an optional appearance theme, without an affiliation or endorsement claim.

The proton stream, containment-trap dial and moving pin-rim animation are original drawing code. LPU uses the already credited Reaper short-hook and McTickler hand profiles. Native indexed assets are now losslessly compressed with this project's bounded LZ4-block encoder/decoder, then decoded once into PSRAM. The compression preserves their prior palette and pixels; it introduces no new artwork source.

Clock recovery uses the bundled Espressif ESP-IDF system-time and `esp_netif_sntp` APIs. No additional external firmware is incorporated. [Espressif system-time documentation](https://docs.espressif.com/projects/esp-idf/en/v5.3.3/esp32s3/api-reference/system/system_time.html).

## Universal VTS startup · 0.13.9

The owner supplied [VTS-official.png](media/VTS-official.png), retained byte-for-byte with its source hash in the artwork ledger. Every board, including Lite, fades in this black/gray logo on a matching white field, with no title or additional words. Native panel-size copies fit entirely inside each circular display. This replaces the historical BWM2 startup; the old original remains archived for attribution.

## Hackazillarex and m33k_t3ch · 0.13.12

The owner supplied [Hackazillarex](media/theme-logos/hackazillarex.jpg) and the [m33k_t3ch profile screenshot](media/theme-logos/m33k-t3ch-source.jpg). Both originals are retained unchanged with SHA-256 provenance in the artwork ledger. The [circular m33k_t3ch display artwork](media/theme-logos/m33k-t3ch.png) is an AI-prepared derivative of that screenshot, made with the built-in image-generation tool to isolate the circular logo and omit the phone interface; it is not a pixel-exact crop. The preparation prompt is retained in [the asset note](docs/M33K-ASSET.md).

The bone sweep, pixel T-rex, cacti, jump animation and neon circuit ornaments are original project drawing code. No Chrome Dino sprites or game code are included. These supplied marks and derived brand artwork remain their respective owners' property and are not relicensed as MIT software. Theme inclusion does not imply affiliation or endorsement.

## Dragon Detector identity · 0.13.13

The owner supplied the orange Dragon Ball illustration and BWM1 emblem, retained unchanged in `media/branding/`. The new [Dragon Ball / BWM mark](media/branding/dragon-detector.png) is an AI-generated derivative prepared with the built-in image tool, replacing the original star with the BWM emblem. It is not a pixel-exact composite. [Generation prompt and asset details](docs/DRAGON-BRAND.md); original and derivative hashes are in the artwork ledger. It replaces the VTS startup and the first three themes' radar header artwork. The earlier startup sources remain archived. Artwork and trademark rights stay with their respective owners; inclusion grants no endorsement or trademark rights.

## Capsule Corp · 0.13.15

Capsule Corp and Dragon Ball belong to their respective rights holders, including BIRD STUDIO/SHUEISHA and TOEI ANIMATION. The user supplied the Capsule Corp emblem; its original is preserved at `media/theme-logos/capsule-corp-source.webp`. A transparent derivative was prepared with built-in image generation. The firmware bezel, grid and watch are code-drawn. This community theme is not an official or endorsed product. [Artwork provenance and prompt](docs/CAPSULE-CORP.md).

## Sparrows · 0.13.15

SPARROWS and its keyhole/wordmark artwork belong to Sparrows Lock Picks. The supplied `sparrows logo.jpg` is preserved unchanged as `media/theme-logos/sparrows-source.jpg` and encoded directly for black display surfaces. The segmented dial and stylized picks are code-drawn interpretations informed by Sparrows’ official product designs; they are not exact tool reproductions. This community theme is not affiliated with or endorsed by Sparrows. [References and theme guide](docs/SPARROWS.md).

## DZ_AZ and personal themes · 0.13.17

The user supplied the unchanged [crow reference](media/theme-logos/dzaz-reference.jpg). The transparent [DZ_AZ crow emblem](media/theme-logos/dzaz.png) is a new illustrated derivative created with the built-in image-generation tool; [prompt and artwork details](docs/DZ-AZ.md#artwork). Feather geometry and animation are original drawing code. The source photograph remains its owner’s work.

The personal-theme editor, DDT1 format, previews and firmware renderers are original project code. Users provide their own images; those images are transferred locally over USB and are not added to this repository or published by the editor. The example design uses the already credited Dragon Detector/BWM mark.
