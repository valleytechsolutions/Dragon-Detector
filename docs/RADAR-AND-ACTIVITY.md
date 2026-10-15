# Radar, hunt and traffic evidence · 0.7

The same implementation runs on Spotpear / Guition JC3636W518 (360×360), Waveshare ESP32-S3-Touch-AMOLED-1.75C and M5Stack StopWatch C152 (466×466). Each board retains its own display, touch, audio and button adapter. The exact Spotpear model matters; similarly shaped boards are not interchangeable.

## Reading the radar

The owner-supplied BWM1 emblem replaces the heading. RX mode, target count and available battery percentage remain visible. Spotpear has no supported fuel gauge, so it does not display an invented battery value.

Choose **Settings → Radar theme & labels** for **Green radar** (default), **Dragon Ball**, or **Black / gold / red**. Each has an animated sweep and pulsing live targets. Green and gold include unnumbered signal rings; Dragon Ball keeps the emerald square grid and yellow dots. The BWM1 logo and RX/count/battery row stay in every theme. The sweep and red center marker are decorative, not direction sensors.

Every visible contact stays on one radar as a family-colored dot; there are no radar pages. Axon receives the primary tag and leader line. Flipper, Flock/ALPR, camera glasses, Ring, other cameras and drones receive automatic tags next. Up to six tags fit on full boards (four on Lite); additional contacts remain visible as dots. AirTag/Find My, SmartTag, Google tags, Tile and iBeacon have smaller dots and no automatic tags or sounds. Tap any dot to reveal its tag and leader for ten seconds; tap the selected dot/tag again for details and Hunt. Tapping a tracker does not displace Axon's primary tag. Details retain the actual received evidence and its specificity; the radar has no CLUE or hint labels.

Radius follows filtered RSSI: stronger signals move inward. Values stronger than -35 or weaker than -100 dBm clamp at the plotting limits internally; this is not a distance calibration. Angle is a decorative address arrangement, **not bearing**. Markers are kept clear of the logo and controls while preserving their RSSI radius. Live dots use family colors; tags display measured dBm.

After ten seconds without reception, a selected label shows its last-seen age. **Recent signals** retains previously recognized contacts for up to two minutes as segmented, muted markers with age. This display-only retention never refreshes evidence, triggers an alert, or makes Hunt audio treat a lost signal as live. Turn it off to return to the 30-second radar expiry. Disabled families and ignored addresses remain suppressed. Slot pressure can evict older contacts before that limit.

USB commands: `radar status`, `radar theme green|dragon|gold`, `radar labels on|off`, `radar recent on|off`, `radar scope candidates|all`. Preferences also load/save from the 0.7 web detector tools.

Signal filtering takes at most one sample per 100 ms, applies a five-sample median and then an elapsed-time exponential filter. Repeating the same radio packet faster does not make a target move inward. Fast/Balanced/Steady smoothing settings remain available. No transmit-power assumptions are used to invent metres. RSSI remains the available passive proximity cue. ESP32-S3 FTM ranging requires a compatible cooperating peer and is not a universal passive measurement: [Espressif FTM reference](https://docs.espressif.com/projects/esp-idf/en/v5.0.5/esp32s3/api-guides/wifi.html#fine-timing-measurement-ftm).

Hunt dedicates reception to the chosen radio/address. Raw RSSI helps expose filter lag; Mark, peak and trend compare changes. The trace uses 48 half-second buckets; missing reception creates gaps instead of a continuous line. A lost Wi-Fi address triggers a channel sweep after twelve seconds. Address rotation can lose a target; unrelated devices are not merged.

## Wi-Fi deauthentication and disassociation

Six received disconnect frames within a rolling three seconds trigger a medium-confidence traffic candidate, with a fifteen-second burst cooldown. A dedicated 32-entry cache suppresses interleaved retry frames using source, destination, subtype and sequence/fragment for one second. Ordinary data traffic does not evict that cache. It is bounded, so very large bursts can exceed its history.

Activity reports total observed disconnect frames, recent count (capped at 64), burst count, last channel and readable reason code. Protected management frames can be counted by subtype but their encrypted reason is unavailable. The contact/history stores the evidence; its displayed address is a participant and may be spoofed. A Hunt follows that observed address, not an authenticated attacker. Legitimate network disconnects can resemble a burst.

## BLE spam candidates

Four groups are monitored independently: Apple pairing, Google Fast Pair, Microsoft Swift Pair, and tracker/beacon advertisements.

- Twelve distinct addresses within five seconds trigger an aggregate candidate.
- Pairing groups also trigger at twenty-four distinct advertising payload fingerprints within five seconds, including a fixed address changing payload. Tracker telemetry is excluded from this second rule.
- Identical repeated adverts do not create new variants. BLE address type is part of identity. Active scan-response bytes are excluded from the payload fingerprint to avoid counting an advert twice.
- The shared table holds 96 recent address/payload facts. Counts are bounded observations, not exhaustive population estimates. Each group has a thirty-second cooldown.

Crowded environments and address rotation can trigger these heuristics. Identical high-rate advertising alone is not classified as an attack; ordinary devices can advertise quickly. A burst does not establish intent or a physical device count.

## Controls and limits

Open **Tools → Activity**, or tap a recent-activity banner on the radar. Events stores the alert detail; contact evidence explains the matched participant. Family toggles, confidence controls and quiet mode continue to apply to alerts. The web tool's **Show radio activity** shortcut requires 0.6.x or 0.7.x. USB `traffic status` returns aggregate counters; `screen activity` opens the panel.

Balanced reception alternates Wi-Fi and BLE; hunting prioritizes one radio, so other activity can be missed. Only the currently received Wi-Fi channel is observed. These are decoded-frame detectors, not RF jamming measurements. All 134 pinned SquachWatch signature checks remain enforced. See [coverage](COVERAGE-AUDIT.md), [validation](VALIDATION.md) and [credits](../CREDITS.md).
