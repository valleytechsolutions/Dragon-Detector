# Dragon Detector 0.14.10 · Scanned-network selection and radio updates

Version 0.14.10 builds on 0.14.9 for every stable hardware profile. The
existing radar, detection and alert settings, security tools, wardrive and node
modes, BadUSB where supported, themes, clocks and saved-file workflows remain in
the firmware. Use **Update** for the exact board profile already running Dragon
Detector to preserve its settings and files.

## Choose a scanned Wi-Fi network

On full-interface boards, open **Attacks → Wi-Fi attacks → Select scanned
network**. The list uses the access points Dragon Detector is already scanning.
Choose an AP to return to
the Wi-Fi test menu; its name stays visible above the actions. Selecting an AP
does not transmit or start a test. Actions still check that the AP and any
required client are current when the action starts.

The compact M5Stack Dial and Waveshare Lite retain their existing smaller
menus; their 0.14.10 packages include the changes their hardware supports.

## Radio and serial changes

Reviewed [ESP32 Marauder `develop` at `95efbb2`](https://github.com/justcallmekoko/ESP32Marauder/commit/95efbb27119ec1471b5bf5800f198c2e8d614bdb)
(v1.21.0). Dragon Detector now includes the applicable open-beacon extended
rate element, keeps each random beacon's SSID and BSSID paired for a ten-second
window, and makes bounded retries when the Wi-Fi transmit queue is full. The
existing Dragon Detector target selection, EAPOL capture and portal cleanup
remain in place. Marauder's C5 display startup change applies to a different
hardware family.

On boards with SD storage, `get framed /path` downloads a file as base64 chunks
with offsets, total length and CRC32. The existing raw `get /path` command is
still available.

## Waveshare 1.85B touch

The 1.85B UI now processes a tap that arrives during a page update before the
next paint. This reduces the wait felt when opening a menu while the radar is
redrawing. Touch coordinates are unchanged.

## Board coverage and validation

The official release packages cover 14 stable profiles, including the 410 × 502
Waveshare AMOLED 2.06. Its complete application and rectangular interface are
included; display, touch, audio, power and radio behavior on that board still
await the physical device. Rectangular Theme Studio editing is deferred.
Guition / Surenoo remains a separate, physically unverified beta. The Waveshare
P4 Knob has its own firmware line and is not part of these packages.

On November 26, the owner checked a Waveshare LCD 1.85B running the release
candidate: its display worked, touch responded promptly after the timing fix,
and selecting a scanned AP showed the selection above the Wi-Fi actions. The
device reported a 360 × 360 display, mounted SD, working RTC, healthy touch
acquisition and no display flush errors. The application image was written at
`0x10000` with verification; its NVS and user-file partitions were preserved.
This was a candidate still reporting 0.14.9 before the release version bump.
The final 0.14.10 image was then flashed to the same application slot, its
written hash verified, and the board reported `BADGE_INFO 0.14.10 360 READY`
with the exact 1.85B model name. The final version was checked for boot and USB
identity; the owner's touch check was on the behavior-equivalent candidate.

Local checks passed: 192 JavaScript tests; the native host suites, including
sanitized core, SD labels and BadUSB runs; the 2.06 rectangular render check;
the node and Dial screen renders; and all 15 PlatformIO build/package profiles.
The package validator checked every factory and update manifest, image path,
flash offset and SHA-256, including the separate Guition beta. The browser
manifest checker accepted the board packages.

The changed radio behavior, framed serial transfer and the new 2.06 board have
not had physical acceptance checks. Successful compilation of another profile
confirms that the code is packaged for it, not that its peripherals were
tested. See the [hardware guide](WAVESHARE-AMOLED-206.md) for the 2.06 arrival
checklist and [installation guide](FLASHING.md) for board matching.
