# Radar, hunt and traffic evidence · 0.14

The shared detection and radar code runs across all thirteen supported ESP32-S3 profiles. Each board retains its own display, input, storage and audio capabilities. Choose the exact hardware profile; similarly shaped boards are not interchangeable.

## Reading the radar

The owner-supplied BWM1 emblem replaces the heading. RX mode, contact counts and available battery percentage remain visible. On full boards the count is split: **live** contacts were heard within the last ten seconds, **recent** is every other plotted contact (fading dots and segmented markers) and is omitted at zero. If the row would not fit on one line, `N recent` is shortened to `+N`. Lite and Dial keep one total. Spotpear has no supported fuel gauge, so it does not display an invented battery value.

Choose **Settings → Theme & radar labels** for **Green radar** (default), **Dragon Ball**, or **Black / gold / red**. Each has an animated sweep; a live dot flashes a halo when it is received. Green and gold include unnumbered signal rings; Dragon Ball keeps the emerald square grid and yellow dots. The BWM1 logo and RX/count/battery row stay in every theme. The sweep and red center marker are decorative, not direction sensors.

Every visible contact stays on one radar as a family-colored dot. ALPR/Flock receive the primary tag before Axon and cameras. Other device families use remaining description callouts, including SmartTag, Google Find Hub and Tile. Up to six tags fit on full boards (four on compact profiles); additional contacts remain visible as dots. AirTag/Find My and iBeacon have no automatic tags. All tracker families remain silent. Tap any dot to reveal its tag and leader for ten seconds; tap the selected dot/tag again for details and Hunt. A selected tracker does not displace the primary priority tag. Details retain the actual received evidence and its specificity.

**Targets** uses separate nonempty category pages, beginning with ALPR/Flock, Axon and cameras and ending with trackers. Use the arrows to change page. Detection popups appear only on Radar, including watch-list popups; leaving Radar clears queued cards so badge, clock, menus and tools stay uninterrupted. The instruction to tap a dot has been removed from the radar itself; device descriptions remain.

Radius follows filtered RSSI: stronger signals move inward. Values stronger than -35 or weaker than -100 dBm clamp at the plotting limits internally; this is not a distance calibration. Angle is a decorative address arrangement, **not bearing**. Markers are kept clear of the logo and controls while preserving their RSSI radius. A ring wider than the clear area has a legal arc on each side; the address angle is spread evenly over its side's arc. Earlier builds clamped the height instead, which put 256 of the 360 possible angles on two rows at the outer ring. Neighbouring angles can still share a pixel there. Dots use family colors; tags display measured dBm.

A dot has three states. **Live**: heard within ten seconds, full brightness with a white highlight. Ten seconds is two balanced radio cycles, because a present device is routinely missed for one. **Fading**: ten to thirty seconds without reception, a flat dot at about half brightness with no highlight or halo. **Recent**: the segmented marker described below. The halo is driven by reception, not by a timer: it flashes when a reading arrives and is gone 0.6 s later, so a dot that is heard continuously glows during its radio's slice and a dot with no halo is not being received right now. Tracker dots stay small and have no halo. A paused radio shows none. On the ASCII theme a fading contact is `o` instead of `*`.

After ten seconds without reception, a selected label shows its last-seen age. **Recent signals** retains previously recognized contacts for up to two minutes as segmented, muted markers with age. This display-only retention never refreshes evidence, triggers an alert, or makes Hunt audio treat a lost signal as live. Turn it off to return to the 30-second radar expiry. Disabled families and ignored addresses remain suppressed. Slot pressure can evict older contacts before that limit: long-silent contacts go first, then unwatched ones, then unclassified addresses the radar is not showing, then lower-priority families.

USB commands: `radar status`, `radar theme green|dragon|gold|valleytech|biscuit|marauder`, `radar labels on|off`, `radar recent on|off`, `radar scope candidates|all`. Preferences also load/save from the 0.7 web detector tools.

Signal filtering takes at most one sample per 100 ms, applies a median of up to five samples and then an elapsed-time exponential filter. Repeating the same radio packet faster does not make a target move inward. Fast/Balanced/Steady smoothing settings remain available.

Reception is time-shared (BLE 2.2 s, then Wi-Fi 2.2 s at 200 ms per channel), so most devices are heard in short runs separated by silence: an access point about twice per 4.4 s, a slow BLE advertiser once or twice. After more than 1.5 seconds without an accepted sample the median restarts and uses only what was heard since; before this change it still held pre-gap values and a sparse dot needed two or three cycles to move. The first sample after such a gap takes at most 55% of the weight and moves the value by at most 8 dB; the rest of the elapsed-time weight is applied when a second sample follows. If those two differ by more than 6 dB, one of them is a spike and the value follows the one nearer to where it already was. A first sample within 6 dB of the last one before the gap counts as its second. The cost is more movement on sparse dots: a single faded or spiked reading after a gap can shift the value by up to 8 dB, where the old filter ignored it, and two consecutive outliers that agree are followed. The rising/falling trend is still taken from the slower 0.14.7 filter, so a stationary device is not reported as changing more often than before. In a host simulation of this schedule with ideal timing and the default 1.8 s setting, a 20 dB change reaches 63% in about 2.3 s on average for an access point (6.6 s before), 2.1 s for a 100 ms BLE advertiser (3.1 s) and 5.9 s for a 2 s advertiser (9.9 s). These are simulated figures, not hardware measurements.

USB `radar signal` is read-only. For the Hunt target, or otherwise the eight strongest radar-visible contacts, it prints the contact index, the last two address bytes, radio, family number and label, raw and filtered RSSI, window spread, accepted filter samples with the mean and longest wait between them, age, and the receive and advertised Wi-Fi channels. Keep its output private: it describes nearby devices. No transmit-power assumptions are used to invent metres. RSSI remains the available passive proximity cue. ESP32-S3 FTM ranging requires a compatible cooperating peer and is not a universal passive measurement: [Espressif FTM reference](https://docs.espressif.com/projects/esp-idf/en/v5.0.5/esp32s3/api-guides/wifi.html#fine-timing-measurement-ftm).

Hunt gives the chosen radio a larger share of reception; it does not dedicate it. A BLE target is listened for during about two thirds of each 6.6 s cycle (4.4 s BLE, 2.2 s Wi-Fi). A Wi-Fi target's channel gets about a third (2.4 s of 6.6 s, 36%), because each cycle still completes one 1–11 sweep and a BLE slice. That share cannot grow without shortening the BLE slice or the 200 ms sweep dwell. What changed is how it is spent: during the 4.4 s Wi-Fi slice a 200 ms visit to the target channel follows each sweep hop, so the target is revisited every 400 ms instead of in one block at the end. With exact timing the longest wait for the target channel falls from 4.2 s to 2.4 s (the BLE slice plus one hop). The sweep order stays fixed while a Wi-Fi Hunt is active, so every channel returns within 6.4 s; with the rotating order one channel waited 8.4 s. A slow main loop drops visits before it delays the sweep: visits are budgeted at the pace recent hops kept. With hops a steady 340–500 ms apart the cycle is as long as without visits; with uneven loop times it can still run up to about one hop longer. These are schedule figures from a host simulation, not hardware measurements. The Wi-Fi Hunt channel is the one the access point states in its beacon or probe response (DS Parameter Set, channels 1–11) when it has stated one; otherwise it is the channel the frame was received on. A beacon decoded on a neighbouring channel therefore no longer moves the lock. An access point that states channel 12–14 is treated as having no stated channel and its earlier one is forgotten. Such readings still feed RSSI; `scan status` counts them as `adv_mismatch` out of `adv_frames`. Raw RSSI helps expose filter lag; Mark, peak and trend compare changes. The trace uses 48 half-second buckets; missing reception creates gaps instead of a continuous line. A lost Wi-Fi address triggers a channel sweep after twelve seconds. Address rotation can lose a target; unrelated devices are not merged.

## BLE radios near

A quiet line under the status row, on every board: `5 BLE RADIOS NEAR / 2 STEADY`, `1 BLE RADIO NEAR`, or `24+ BLE RADIOS NEAR`. Where the long form does not fit it reads `5 BLE RADIOS / 2 STEADY`. It is a count of what is being heard strongly now. Nothing is alerted, sounded, vibrated, logged or stored for it, no arrival or departure is announced, and no address is kept beyond the table in memory.

What it does and does not say:

- It counts **BLE radio addresses, not people**. One person may carry several radios or none.
- A hidden line or a low number **does not mean nobody is near**: Bluetooth may be off, a body or bag can hide a radio, and Wi-Fi devices are not counted. The count is a floor.
- "Near" is a **signal level**, -70 dBm by default and not calibrated per board. It is not a distance.
- A radio must be heard in two scan windows, about 5 to 15 seconds, before it counts, so people walking past are usually missed. The three hits must come in a row: one advert below the level starts the confirmation over, so a radio that hovers below the level with occasional peaks is not counted. It stays counted for about 40 seconds after it goes, and while it is heard up to 12 dB below the level.
- Phones change their BLE address about every 15 minutes. The old and new address are joined only on strong evidence, in a quiet place: at most six radios near or being confirmed (the new address included), nothing refused, the old address already steady, exactly one old address that fell silent and exactly one new one that fits it. The new address then waits uncounted while the old one is still counted, and takes its place and its time near only when the old one has stayed silent for the full leave time. If the old address is heard again, nothing is joined and both count. The number does not move for a joined change.
- Everywhere else — a busy room, a street, an old address not yet steady, two candidates — **nothing is joined**: the count is one too high for about 40 seconds and the radio's time near starts again, so it loses **STEADY** for about five minutes. In a busy place the steady number therefore runs low, also for your own phone, and the count runs slightly high. That is chosen over guessing: an earlier version joined on timing and level alone and in a crowd mostly joined two different radios.
- A join can still be wrong when a steady radio really leaves and an unrelated one arrives within 6.6 s of listening at a similar level with no other candidate in view. The newcomer then shows as steady early. A changed address missed for longer than 6.6 s of listening is not joined.
- A rotating radio that entered the count and now rests between the level and the level minus 12 dB (a phone left on a desk) keeps its place across an address change by the same rule; without it the new address would never be strong enough to enter. Up to two such weaker candidates wait at once, each address is considered once, and none is ever counted on its own.
- **Steady** means the radio has counted for 150 s of BLE listening. That is about five minutes in normal scanning and during a Wi-Fi Hunt up to seven and a half, but about 3.75 minutes during a BLE Hunt and 2.5 minutes in a BLE-only mode, where BLE is listened to more of the time. The leave time scales the same way (40 s normally, 20 s in a BLE-only mode). It is usually your own devices.
- `24+` means at least 24: the table was full or new addresses arrived faster than they could be confirmed.
- Addresses with an **Ignore** rule (full boards) are not counted.
- The line is hidden at zero, when the count is switched off, for the first seconds after the radio starts or resumes, and when BLE has not been listened to for 15 seconds (Clock, Timer, standby, wardriving, a BLE Lab session). After such a pause the number stays hidden for 10 s of listening (about 20 s) and then counts only radios heard again since the pause, so a radio that went during the pause is not shown; time near is kept for those still there. A deauth or BLE-spam warning takes the same line and wins; on the Dial the notice box can cover it.

All times are **BLE listening time**, so a Wi-Fi slice, a Hunt or a paused radio cannot make a present radio leave. In the balanced schedule BLE is listened to for 2.2 s of every 4.4 s, so wall time is about twice these figures; during a Wi-Fi Hunt it is three times.

| Step | Rule |
|---|---|
| First heard | One advert at or above the level (-70 dBm) starts a candidate; at most 8 candidates at once |
| Counted | 3 hits in a row at or above the level, at least 2.5 s of listening after the first |
| Candidate dropped | 6.6 s of listening without being heard, or one advert below the level |
| Kept alive | Any advert at or above the level minus 12 dB (-82 dBm) |
| Gone | 20 s of listening with nothing that strong |
| Return | Back within a further 150 s of listening: the same radio, its time near is kept |
| Address change | At most 6 radios near or candidate and none refused; one steady rotating address silent, one new rotating address first heard within 6.6 s of listening after it and within 10 dB, no other match either way. The new one waits uncounted; joined when the old one reaches the 20 s leave time, cancelled if it is heard again |
| Steady | Counted for 150 s of listening |
| Start or resume | No number for the first 10 s of listening. After a 15 to 60 s pause only radios heard again count; after 60 s the table restarts |
| Capacity | 24 radios, about 660 bytes of static memory on every board |

The level uses the same filtered RSSI as the radar dots. On the M5Dial the 32-entry contact table turns over quickly and each new entry restarts that filter, so entry into the count is noisier there.

Full boards: **Settings → Theme & radar** has **BLE radios near: ON/OFF** and **Near level** (-60 closer, -70, -80 wider). The line opens **Radio activity**, which repeats the numbers and these limits. Lite and Dial have no on-device control for it. USB on every board: `radar near` prints one aggregate line without addresses (`NEAR on= state= level_dbm= exit_dbm= near= steady= pending= capped= new_addresses= gone= handovers= returns= dropped= listen_ms=`); `radar near on|off` and `radar near level <-85..-55>` change and save the setting and restart the count. It is on by default.

The timings, the -70 dBm level, the 12 dB margin and the address-change pairing come from reasoning and a host simulation of the radio schedule. None has been measured on hardware.

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

Balanced reception alternates Wi-Fi and BLE; hunting prioritizes one radio, so other activity can be missed. Only the currently received Wi-Fi channel is observed. These are decoded-frame detectors, not RF jamming measurements. All 158 pinned signature-table checks remain enforced. See [coverage](COVERAGE-AUDIT.md), [validation](VALIDATION.md) and [credits](../CREDITS.md).

**Room motion** on full boards (Radio tools) is a separate, experimental CSI mode for a detector set down still: it senses movement near the detector, not people, position or count, and pauses the radar while it runs. See [Room motion](RADIO-TOOLKIT.md#room-motion-experimental-csi-monitor).

The three branded themes require firmware 0.13.1 or newer. [Previews and logos](THEMES.md).
