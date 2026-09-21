# Dragon Detector feature status

This is custom firmware for the Spotpear/Guition JC3636W518 360×360 round ESP32-S3 board. It adapts selected Marauder passive detection logic into a new hardware interface. It is **not a complete build of the upstream Marauder application**. Do not describe it as feature-complete Marauder.

| Requested function | Implementation and limits |
|---|---|
| Dragon Ball inspired radar | Live dots, sweep, circular green grid, signal strength radius, tap for evidence. Angular positions are decorative, not measured bearings. |
| Wi-Fi + Bluetooth | Passive 2.4 GHz Wi-Fi channels 1–11 and BLE advertisement reception. Alternating 7.7 s / 2.3 s windows; channel lock pauses BLE. No 5/6 GHz or Bluetooth Classic. |
| Deauthentication monitoring | Reports 20 deauth/disassociation frames per observed source within 5 seconds. Source addresses are spoofable; packet loss and channel hopping reduce sensitivity. |
| BLE spam monitoring | Flags 12 distinct addresses with Apple popup, Fast Pair, or Swift Pair advertisement patterns within 5 seconds. Duplicate advertisements alone do not trigger. Crowded environments can still trigger. |
| Flock | Marauder-derived Xuntong company ID plus Penguin/external battery name; medium-confidence candidate. Does not detect every Flock model. |
| Axon | Advertised Axon name or AB2-/AB3-/AB4- name prefix; weak indicator. Cannot identify a silent camera. |
| Meta glasses | Luxottica manufacturer candidate; generic Meta company IDs and Marauder service UUID heuristics are weak indicators and may be non-glasses products. Cannot determine recording state. |
| Pineapple/Pager | Pineapple/Hak5 advertised names and narrow Marauder OUI heuristic; weak. Does not verify Pager model. |
| Pwnagotchi | Marauder special peer address plus peer fields; strong protocol signature, still spoofable. Name-only matches remain weak. |
| Nyanbox/HaleHound | Advertised name indicators only. No validated unique radio fingerprint is claimed. |
| Find My / trackers | Apple Find My advertisement pattern, FMNA and DULT service UUIDs adapted from Marauder; candidate, not owner or tracking-intent attribution. |
| Flipper | Marauder's advertised service UUID indicators; candidate, not proof of malicious activity. |
| Skimmer detection | Generic HC module names are weak indicators. Classic-only HC-05/06 devices cannot be seen by the S3 BLE radio. |
| Wi-Fi analysis | Contact inventory, beacon/probe names, channel activity normalized to observed dwell time, EAPOL frame count, optional raw 802.11 PCAP. |
| Logs | CSV under `/dragon/session-<random>/`; evidence, confidence, uptime, address, RSSI, channel. CSV rotates at 4 MB. PCAP stops at 16 MB. No UTC/GPS claim. |
| Boot logo | Supplied VTS logo, encoded directly with a scale animation. |
| Navigation | Bottom-edge swipe up opens tools only from the radar. Lists reserve vertical gestures for scrolling; visible tab and Home buttons handle navigation. |
| Audio | MP3/WAV playback, volume, microphone WAV recording, stop/save, recording playback. Recording runs only on explicit user command. |
| SD browser | Folders, MP3/WAV launch, first 2 KB preview of other files. Up to 96 entries per folder. |

## Remaining upstream parity

Upstream Marauder's complete CLI, active attack workflows, advanced mission scripting, all specialized sniffers, tracker sound commands, GPS wardriving, and external-radio features are not ported. Bluetooth Classic, 5 GHz, GPS, and external transceiver features require different or additional hardware. A working radar does not establish that every requested device family can be identified reliably.

## Sources

- [Spotpear board and official vendor demo](https://spotpear.com/wiki/EPS32-S3R8-1.8inch-360x360-Round-LCD-Display-WIFI-Secondary-TouchScreen-AIDA64-Arduino-LVGL-WirelessAida64-Wireless-Power-Video-Electronic-Photo-Frame-MP3.html)
- [Marauder source pinned at 8ae4622](https://github.com/justcallmekoko/ESP32Marauder/tree/8ae4622abcc9c9c5729d4e97491907581d7f0c34), specifically `WiFiScan::isFlockCamera`, `beaconSnifferCallback`, `processPwnagotchiBeacon`, and the Pineapple OUI table.
- [Marauder Flock Sniff limitations](https://github.com/justcallmekoko/ESP32Marauder/wiki/Flock-Sniff)
- [NoPeek author's detector implementation](https://github.com/getnopeek/nopeek-android) for Luxottica/Meta manufacturer candidate identifiers; no zero-false-positive claim is adopted.
- [Espressif Wi-Fi driver documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi.html)
