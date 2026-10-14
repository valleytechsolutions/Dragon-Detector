# Daily detection and hunting · 0.7.0

The radar starts at **100% brightness with no automatic dimming or screen-off timeout**. It listens passively on 2.4 GHz Wi-Fi and Bluetooth LE. The optional training fox transmitter is off at every boot.

## Radar and evidence

Every enabled signature appears from its first valid sighting. **Hollow dots are weak clues**; filled dots indicate stronger evidence. Select **Tools → Detectors** to change individual families or the **audible alert threshold** (medium+ by default). Raising that threshold does not hide weaker radar clues. All families, including iBeacon, start enabled. Existing customized family masks are retained; the previous untouched default gains iBeacon.

Use **TARGETS** for a ten-row paged list ordered by evidence and signal; refresh to update it. Tap the radar speaker icon for quick quiet mode. Tap a marker or contact for its signal, evidence, confidence and last observation. Remote ID contacts show decoded aircraft/operator coordinates when broadcast; these fields have their own age and are self-reported. Hidden/silent devices and scan gaps can produce an empty radar. A signature does not establish intent or recording.

**Settings → Radar** switches between candidates and all contacts. Disabled families and ignored addresses remain suppressed; explicitly watched addresses stay visible unless ignored. Angle is a stable visual arrangement, not a bearing; radius represents signal strength, not distance.

The BWM1 emblem replaces the radar heading. RX mode, total radar contacts and battery percentage (where a fuel gauge exists) remain below it. **Settings → Radar theme & labels** offers Green radar (default), Dragon Ball, and Black / gold / red. The sweep animates and live dots pulse; rings have no numbered distance scale.

Every plotted dot has a device-family label and leader by default. Tap either to open its evidence. Six targets fit each labeled radar page; previous/next appears when needed. Weak identities say WEAK CLUE. After ten seconds without reception, labels show last-seen age while preserving weak-clue wording. Recent signals remain for up to two minutes with segmented muted markers, unless this option is disabled. A stale dot is not a live signal; alerts and hunt feedback retain their freshness rules. Switch labels off to show all dots together.

Inward means stronger filtered RSSI. The triangle and sweep do not indicate heading. Detailed dBm readings remain on contact and Hunt screens. [Check a Flipper or Pineapple Pager](DEVICE-CHECKS.md).

**Tools → Activity** shows Wi-Fi disconnect and BLE spam candidates. A recent burst also adds a tappable banner to the radar. Events and contact evidence retain the reason for each alert. [Thresholds and limits](RADAR-AND-ACTIVITY.md).

## Hunt a signal

1. Select a radar dot, nearby contact or saved history record, then **Hunt signal**.
2. BLE hunts dedicate reception to BLE. Wi-Fi hunts lock to the last observed channel, or sweep Wi-Fi if the channel is unknown. Other-radio coverage is paused while hunting.
3. Walk and compare dBm, signal trend and the trace. A less negative value is stronger. **Mark** saves the current filtered value for comparison. The peak and stability label help distinguish a sustained change from a spike.
4. After ten seconds without reception, the gauge reports signal lost. The filter resets after a long gap; the 24-second trace shows missing reception as gaps, and the selected slot cannot be silently replaced by another device.
A Wi-Fi target absent for twelve seconds triggers a channel sweep; reception locks to its new channel when it returns. This follows its saved address, not a guessed identity after address rotation.

5. **Stop** restores balanced Wi-Fi/BLE scanning. Returning home keeps the hunt active; tap its radar status or Tools → Hunt to return.

Settings offers Fast (0.6 s), Balanced (1.8 s) and Steady (4 s) smoothing after the median filter. RSSI depends on power, antenna orientation, walls and body shielding. The arrow means rising/falling signal, not a compass direction. A target that rotates its BLE address may be lost; unrelated addresses are not merged.

For a training fox, explicitly enable **Settings → Fox beacon** on another Dragon Detector. It transmits an original non-connectable training advertisement, identified as **Dragon fox**. Select it on the hunting board. It is public and unauthenticated, and is not SquachMesh-compatible. Restarting or pausing the transmitter turns the beacon off.

## Alerts and rules

- **Target → Controls → Watch:** priority card when that address is observed; persists until dismissed. Watch bypasses confidence and auto-quiet limits, but does not override an explicit ignore/snooze or disabled family.
- **Ignore:** persistent suppression of cards and radar markers; history can continue.
- **Snooze:** temporary alert suppression until restart. Tap again to undo.
- **Settings → Watch / ignore list:** manage up to 64 radio/address rules. Rules are address-specific, including BLE address type.
- **Auto quiet:** off, after 5 or after 10 alerts. Beyond the allowance, a repeat interruption needs a 7 dB increase above its previous alert peak. The allowance resets after 30 minutes. Ordinary repeats have a 30-second cooldown.
- **Quiet mode:** normal cards are suppressed while logging continues. Watch cards remain available. All automatic audio respects quiet mode; StopWatch haptics have a separate switch.

Normal cards disappear after ten seconds; watch cards remain until dismissed. Cards have Hunt and Snooze shortcuts when they refer to one address. Dense pairing/tag traffic receives a grouped burst alert; a busy place can trigger it without an attack.

## History and USB tools

Tools → **History** shows up to 512 unique address/radio/family records, newest first. Records contain evidence, peak/last signal and encounter count. More than one family can be retained for the same address. History is enabled by default and uses internal flash on all three boards, with record CRC and bounded journals. Updates are queued to limit write traffic; abrupt power loss can lose recent pending updates. Old journal records eventually rotate out. **Clear** asks before removing stored records; active scanning can create new ones afterward.

Open [Detector tools](https://valleytechsolutions.github.io/DragonBall-Z-Radar/#detector) in desktop Chrome/Edge. Connect a running 0.4.x or newer board to synchronize UTC and download a CSV. Close other serial monitors first. Without a clock sync since boot, records explicitly use boot-relative time. Clock sync affects new observations; it does not invent dates for older records.

Display privacy masks names/addresses and hides evidence/coordinates on screen. It does not alter stored or exported data. CSV includes the full records. Export briefly pauses reception. No account or cloud upload is involved.

Spotpear additionally supports optional SD event CSV and bounded 16 MB PCAP captures. SD logging/capture are off by default. Waveshare 1.75C and StopWatch do not claim SD support.

## Display and navigation

**Settings → Display & timeouts** provides brightness, dim-after, dim level, screen-off-after and wake-on-alert. Both timers default to **Never**. **Restore always-on defaults** sets 100% brightness and disables both timers. The web detector tools can save the same settings. Reception continues when the screen dims or turns off. Touch wakes a dark display; the waking touch is consumed so it does not accidentally press a hidden button.

Tap Tools or swipe upward from the radar's bottom edge. Lists scroll normally. Waveshare BOOT toggles radar/tools. StopWatch yellow returns to radar; blue opens tools and, from tools, opens badge.

## Existing image badge

[Badge Studio](https://valleytechsolutions.github.io/DragonBall-Z-Radar/#studio) still accepts PNG/JPEG/WebP, with drag, zoom, center, fit and background controls. It detects 360/466 sizing and transfers CRC-checked RGB565 over USB. A failed upload preserves the prior image. Hold the displayed image for tools; Settings → Boot to radar restores radar startup. Images are local to browser/device. Further badge features are deferred for this release.

## USB diagnostics

115200 baud, newline-terminated commands:

```text
status
contacts
selftest
detector status
history export
time <Unix-seconds>
display status
display set <brightness-percent> <dim-seconds-or-0> <off-seconds-or-0> <dim-percent>
hunt BLE <address-type> AA:BB:CC:DD:EE:FF 0
hunt WiFi 0 AA:BB:CC:DD:EE:FF <channel>
hunt stop
screen radar
screen hunt
screen history
screen detectors
screen display
pause
resume
```

`history clear` erases the history without a serial confirmation; use it deliberately. `screenshot` returns the display render, not a photograph. In 0.5.1 its `SCREENSHOT width height bytes` header precedes RGB565 pixels and a `SCREENSHOT_END crc32` footer; reject a short transfer or checksum mismatch. It briefly pauses scanning. The badge commands from 0.3.x are retained.

## Audio alerts and foxhunting

Open **Settings → Audio & hunt tones** for the speaker switch, volume, hunt feedback and automatic family focus. Fresh installs enable audio at 100% volume; updates preserve the previous sound switch. Each of the 26 detector families has a distinct original RTTTL melody. Alert filtering, ignore/snooze rules and auto-quiet still apply. Per-family duplicate requests coalesce, with a four-second cooldown and ten-second queue expiry, so packets do not restart a melody. Quiet mode and volume zero mute automatic audio, including watched devices.

**Hunt feedback** follows the manually selected device. With no manually selected target, **Audio focus** can automatically follow the strongest fresh matching candidate; a six-dB margin reduces switching between similar signals. The on-board focus choices are Axon, Flock/ALPR/Raven and camera glasses; the web editor exposes all families. Focus changes the heartbeat target; other enabled families still retain their alert melodies. Audio focus uses the normal balanced Wi-Fi/BLE scan. Select a specific contact and tap Hunt to dedicate reception to that radio/channel.

Higher, faster pulses mean a stronger filtered RSSI: approximately two seconds at -95 dBm down to 250 ms at -35 dBm. A rising/falling second chirp follows the signal trend. Axon uses a distinctive double heartbeat plus its own acquisition melody. After twelve seconds without observations, heartbeat feedback stops and a falling two-note cue plays once. Signal changes can also result from shielding, orientation and reflections; this is not measured distance or bearing.

The **[USB audio editor](https://valleytechsolutions.github.io/DragonBall-Z-Radar/#sound)** sits beside Badge Studio:

1. Connect in Detector tools. Choose a detector family and load its saved melody.
2. Edit a single ASCII RTTTL line such as `Axon:d=16,o=5,b=180:c,e,g,c6,g,e,8c6`.
3. Preview explicitly on the board speaker, or save without playing. An empty melody silences that family’s detection melody; hunt feedback has a separate switch. Restore default removes the override.
4. Save the audio settings separately for master enable, volume, hunt feedback and focus.

RTTTL bounds: 512 characters, 96 notes, 15 seconds total; duration divisors 1/2/4/8/16/32, octaves 4–7, tempo 25–900, sharps and dotted notes, `p` rests. Invalid input leaves the saved tone unchanged. Preferences and custom melodies survive restart. Preview intentionally works with automatic alerts disabled or quiet mode enabled, at the **saved** volume. Stop ends the current sound; disable automatic alerts to remain silent. Badge images retain their separate crop/upload controls.

Speaker adapters: Waveshare 1.75C uses its ES8311, I2S GPIO 16/9/45/8 and amplifier GPIO 46; Spotpear uses I2S GPIO 18/16/17 and amplifier GPIO 48; StopWatch uses M5Unified's speaker. Connect a suitable speaker where the board only supplies a speaker connector. No microphone recording or MP3 decoder is enabled.

USB commands: `tone settings`, `tone config <enabled 0|1> <volume 0..100> <hunt 0|1> <focus -1..25>`, `tone get <family>`, `tone set <family> <RTTTL or ->`, `tone preview <family> <RTTTL or ->`, `tone play <family>`, `tone reset <family>`, `tone stop`. Family IDs follow `Family` in `detection.h`; Axon is 3 and glasses is 4. A dash selects silence. Commands longer than 600 bytes are rejected rather than executed after truncation.

## Reception and diagnostics

**Settings → BLE scan** switches Passive/Active. Passive listens only. Active sends scan requests to obtain response-only names; it does not connect or pair. Some devices ignore requests. Active mode bounds and renews its result cache each second. Wi-Fi remains passive in both modes.

**Settings → Radio health** shows received Wi-Fi/BLE counts, queue drops/occupancy, worst processing time, queue lag, contact evictions and available memory. Counters reset at boot. Increasing drops/lag indicates reception pressure; a paused radio or a dedicated hunt also reduces coverage. There are 384 live contact slots shared across the radios; history remains 512 address/radio/family records.

Additional USB commands: `radio active`, `radio passive`, `screen targets`, `screen health`. `status` includes `RADIO_STATS`. They use the same settings and screens as the device controls.
