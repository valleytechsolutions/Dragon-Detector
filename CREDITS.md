# Credits and licensing

## JustCallMeKoko / ESP32 Marauder

[ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) by **JustCallMeKoko (Just Call Me Koko)** and contributors supplies upstream references for selected passive detectors. The Spotpear development reference was commit `8ae4622abcc9c9c5729d4e97491907581d7f0c34`. Preserved MIT notices: [Spotpear](hardware/spotpear/docs/MARAUDER-LICENSE.txt) and [StopWatch](hardware/m5stack-stopwatch/MARAUDER-LICENSE.txt). See the Spotpear feature notes for specific detector references. This is not an official or full-feature Marauder release.

## CodeHedge

Special acknowledgment to **[CodeHedge](https://github.com/CodeHedge)** for related Marauder/CYD and Biscuit community work, including the [ESP32Marauder fork](https://github.com/CodeHedge/ESP32Marauder), [Biscuit flasher](https://github.com/CodeHedge/biscuit_flasher), and [DIY/Node web flasher](https://github.com/CodeHedge/diy_node_web_flasher). No direct CodeHedge code contribution to this firmware has been established by the source audit; this is a community acknowledgment, not a claim of authorship, endorsement or compatibility with Biscuit firmware.

## Hardware, libraries and other references

- **M5Stack:** [M5Unified](https://github.com/m5stack/M5Unified), [M5GFX](https://github.com/m5stack/M5GFX), and StopWatch hardware documentation. Driver versions are pinned in the StopWatch PlatformIO file; upstream license notices accompany downloaded dependencies.
- **Spotpear / Guition:** original JC3636W518 board, vendor demo and display configuration. See the [Spotpear dependency table](hardware/spotpear/THIRD_PARTY.md).
- **Espressif:** ESP32-S3, Arduino framework and ESP-IDF radio drivers.
- **LVGL:** MIT-licensed graphics library, with licenses in the vendored copies.
- **NimBLE-Arduino:** Apache-2.0 BLE library, with notices in each vendored copy.
- **ESP8266Audio:** GPL-3.0 audio library, used only in the Spotpear build.
- **NoPeek:** additional manufacturer-identifier reference, described in the [signature notes](hardware/spotpear/docs/FEATURES.md).

## Project and media rights

Project source uses the root GPL-3.0 license. Third-party code retains its original notices. VTS artwork was supplied by the project owner; this software license does not grant unrelated trademark rights. The promotional cover is generated artwork, not a product photograph. UI screenshots are renderer captures. Manufacturer photographs are linked from their original hosts for hardware identification and are not relicensed. Dragon Ball is referenced as visual inspiration only; no affiliation is claimed.
