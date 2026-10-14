# Radar, hunt and traffic evidence · 0.6

The same implementation runs on Spotpear / Guition JC3636W518 (360×360), Waveshare ESP32-S3-Touch-AMOLED-1.75C and M5Stack StopWatch C152 (466×466). Each board retains its own display, touch, audio and button adapter. The exact Spotpear model matters; similarly shaped boards are not interchangeable.

## Reading the radar

The owner-supplied BWM1 emblem replaces the heading. RX mode, target count and available battery percentage remain visible. Spotpear has no supported fuel gauge, so it does not display an invented battery value.

Markers move inward as filtered RSSI increases. Rings label -40, -60, -80 and -100 dBm; values stronger than -35 or weaker than -100 clamp at the inner/outer plotting limits. Angle is a stable address arrangement, **not bearing**. The sweep is animation, not a directional antenna. A hollow marker is weak signature evidence; filled markers are stronger. Labels include dBm and LIVE/AGING state, and markers fade until their 30-second expiry.

Signal filtering takes at most one sample per 100 ms, applies a five-sample median and then an elapsed-time exponential filter. Repeating the same radio packet faster does not make a target move inward. Fast/Balanced/Steady smoothing settings remain available. No transmit-power assumptions are used to invent metres.

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

Open **Tools → Activity**, or tap a recent-activity banner on the radar. Events stores the alert detail; contact evidence explains the matched participant. Family toggles, confidence controls and quiet mode continue to apply to alerts. The web tool's **Show radio activity** shortcut requires 0.6.x. USB `traffic status` returns aggregate counters; `screen activity` opens the panel.

Balanced reception alternates Wi-Fi and BLE; hunting prioritizes one radio, so other activity can be missed. Only the currently received Wi-Fi channel is observed. These are decoded-frame detectors, not RF jamming measurements. All 134 pinned SquachWatch signature checks remain enforced. See [coverage](COVERAGE-AUDIT.md), [validation](VALIDATION.md) and [credits](../CREDITS.md).
