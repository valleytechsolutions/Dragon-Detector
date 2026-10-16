> **0.13.2:** Waveshare AMOLED and M5Stack use internal flash with no SD controls. Utilities → Saved files opens captures; Settings → Capture studio opens recording controls. Spotpear/XIAO retain optional SD tools. [GUI tour](GUI.md) · [Hardware comparison](HARDWARE.md).

> **0.13:** Utilities replaces Daily. Badge editing stays in the web Badge Studio. The first Recon row now includes **Attacks**, with separate Wi-Fi and BLE lists on all four full boards. Existing partitions, badge and settings are preserved by a matching Update. [New features](RECON-013.md).

> **0.12 update:** Hunt, badge and ordinary screens continue BLE/Wi-Fi detection. Channel selection is now a priority after a full sweep, and manual Pause is removed. Explicit Waveshare radio modes remain exclusive when selected. Open **Tools → System → Recon notebook** for passive payload/probe evidence, or use **Settings → Recon notebook**. [Scan timing and export](RECON-NOTEBOOK.md).

# Detection, hunting and utilities · 0.13.2

This guide covers the four full-feature boards. The non-touch Waveshare ESP32-S3-LCD-1.28 has a dedicated [Lite radar guide](WAVESHARE-LITE.md); it has no menus, settings or badge mode.

The radar starts at **100% brightness with no automatic dimming or screen-off timeout**. It listens passively on 2.4 GHz Wi-Fi and Bluetooth LE. The optional training fox transmitter is off at every boot.

## BLE Lab on all four full boards

On any full board running 0.13 or newer, open **Tools → Recon → BLE Lab** to inspect advertisements, discover GATT services, read values, monitor notifications, save profiles and emulate a local test peripheral. The [BLE Lab guide](BLE-LAB.md) explains the workflow and storage upgrade. Radar reception pauses during a connection or emulation, and resumes on Stop/Home. Merely opening the lab leaves radar listening active.

## Radar and evidence

Every enabled signature appears from its first valid sighting. **All live devices use filled family-colored dots**; trackers use smaller dots. Select **Tools → Detectors** to change individual families or the **audible alert threshold** (medium+ by default). The threshold applies to non-priority families; evidence specificity remains available in details. All families, including iBeacon, start enabled. Existing customized family masks are retained; the previous untouched default gains iBeacon.

**Axon priority** is on by default in Tools → Detectors. It alerts immediately on any recognized Axon signature, even with the general threshold set to strong-only. It honors quiet, ignore/snooze, family filters and cooldowns. Turn it off to use the general threshold and repeat rule for Axon too. [Evidence, AXEOFF comparison and diagnostics](AXON.md).

Use **TARGETS** for a ten-row paged list ordered by family priority, freshness and signal; refresh to update it. Tap the radar speaker icon for quick quiet mode. Tap a marker or contact for its signal, evidence, confidence and last observation. Remote ID contacts show decoded aircraft/operator coordinates when broadcast; these fields have their own age and are self-reported. Hidden/silent devices and scan gaps can produce an empty radar. A signature does not establish intent or recording.

**Settings → Radar** switches between candidates and all contacts. Disabled families and ignored addresses remain suppressed; explicitly watched addresses stay visible unless ignored. Angle is a stable visual arrangement, not a bearing; radius represents signal strength, not distance.

The BWM1 emblem replaces the radar heading. RX mode, total radar contacts and battery percentage (where a fuel gauge exists) remain below it. **Settings → Radar theme & labels** offers Green radar (default), Dragon Ball, Black / gold / red, Valleytech, Biscuit Shop and Marauder. The three branded themes replace the BWM1 emblem with their own logo. The sweep animates and live dots pulse; rings have no numbered distance scale.

Every visible contact stays on one radar as a family-colored dot; there are no radar pages. Axon receives the primary tag and leader line. Flipper, Flock/ALPR, camera glasses, Ring, other cameras and drones receive automatic tags next. Up to six tags fit on full boards (four on Lite); additional contacts remain visible as dots. AirTag/Find My, SmartTag, Google tags, Tile and iBeacon have smaller dots and no automatic tags or sounds. Tap any dot to reveal its tag and leader for ten seconds; tap the selected dot/tag again for details and Hunt. Tapping a tracker does not displace Axon's primary tag. Details retain the actual received evidence and its specificity; the radar has no CLUE or hint labels.

Inward means stronger filtered RSSI. The triangle and sweep do not indicate heading. Detailed dBm readings remain on contact and Hunt screens. [Check a Flipper or Pineapple Pager](DEVICE-CHECKS.md).

**Tools → Activity** shows Wi-Fi disconnect and BLE spam candidates. A recent burst also adds a tappable banner to the radar. Events and contact evidence retain the reason for each alert. [Thresholds and limits](RADAR-AND-ACTIVITY.md).

## Hunt a signal

1. Select a radar dot, nearby contact or saved history record, then **Hunt signal**.
2. Hunts prioritize the selected signal while both BLE and Wi-Fi continue scanning. Wi-Fi channel priority follows a complete channel sweep; stale targets trigger a broader search. Explicit radio modes or active labs can intentionally specialize reception.
3. Walk and compare dBm, signal trend and the trace. A less negative value is stronger. **Mark** saves the current filtered value for comparison. The peak and stability label help distinguish a sustained change from a spike.
4. After ten seconds without reception, the gauge reports signal lost. The filter resets after a long gap; the 24-second trace shows missing reception as gaps, and the selected slot cannot be silently replaced by another device.
A Wi-Fi target absent for twelve seconds triggers a channel sweep; reception locks to its new channel when it returns. This follows its saved address, not a guessed identity after address rotation.

5. **Stop** clears the target priority while Wi-Fi/BLE scanning continues. Returning home keeps the hunt active; tap its radar status or Tools → Hunt to return.

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

Open [Detector tools](https://valleytechsolutions.github.io/Dragon-Detector/#detector) in desktop Chrome/Edge. Connect a running 0.4.x or newer board to synchronize UTC and download a CSV. Close other serial monitors first. Without a clock sync since boot, records explicitly use boot-relative time. Clock sync affects new observations; it does not invent dates for older records.

Display privacy masks names/addresses and hides evidence/coordinates on screen. It does not alter stored or exported data. CSV includes the full records. Export briefly pauses reception. No account or cloud upload is involved.

Spotpear and XIAO additionally support optional SD event CSV and bounded 16 MB PCAP captures. SD logging/capture are off by default. Waveshare 1.75C and StopWatch do not claim SD support.

## Display and navigation

**Settings → Display & timeouts** provides brightness, dim-after, dim level, screen-off-after and wake-on-alert. Both timers default to **Never**. **Restore always-on defaults** sets 100% brightness and disables both timers. The web detector tools can save the same settings. Reception continues when the screen dims or turns off. Touch wakes a dark display; the waking touch is consumed so it does not accidentally press a hidden button.

Tap **Tools** to open the menu and **Home** to return to radar. There is no swipe-to-open gesture; lists still scroll normally. Waveshare BOOT toggles radar/tools. StopWatch yellow returns to radar; blue opens tools and, from tools, opens badge.

## Image and text badge

In the 0.9.7 studio, **Use BWM stock badge** loads the supplied logo centered and sized to fill the round display with a small edge margin. Full-board firmware uses this stock image when no custom badge is saved. Existing custom badges remain in place after an application update.

Expand **Add text to your image** for up to three lines, text size, vertical position and color. Text has a black outline for contrast and stays within the round screen. It is included in the USB upload and PNG download. Clear the text box for an image-only badge. Use matching 0.9.7 firmware for the Bluetooth reset and USB acknowledgment fixes; an automatic clock-sync failure no longer blocks the studio connection.

[Badge Studio](https://valleytechsolutions.github.io/Dragon-Detector/#studio) accepts PNG/JPEG/WebP (up to 20 MB and 40 megapixels). Choose the exact board to preview its 240×240, 360×360 or 466×466 circle before connecting. Images initially fit completely inside a small safe margin. Choose **Fill circle** to crop edge-to-edge, then drag or zoom. Rotate 90° in either direction, mirror, center, or reset the crop. Fine positioning offers horizontal/vertical sliders and one-pixel arrow buttons; hold Shift for ten pixels. The focused preview accepts arrow keys too. Board changes preserve relative positioning. Alignment guides are preview-only and never become part of the image.

**Download badge preview** saves a circular PNG. **Connect your badge** identifies the exact firmware model and locks the matching size, then **Upload** sends CRC-checked RGB565 directly over USB. A failed or interrupted transfer preserves the previous image. A successful upload enables badge startup. Hold the displayed image for Tools; Settings → Boot to radar restores radar startup. Images remain local to the browser/device.

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

Open **Settings → Audio & hunt tones** for the speaker switch, volume, hunt feedback and automatic family focus. Fresh installs enable audio at 100% volume; updates preserve the previous sound switch. Each of the 26 detector families has a distinct original RTTTL melody. Priority families alert on the first recognized match. Other alert filtering, ignore/snooze rules and auto-quiet still apply. Ordinary tracker detections stay silent unless explicitly watched. Axon preempts lower-priority audio and notification cards. Per-family duplicate requests coalesce, with a four-second cooldown and ten-second queue expiry, so packets do not restart a melody. Quiet mode and volume zero mute automatic audio, including watched devices.

**Hunt feedback** follows the manually selected device. With no manually selected target, **Audio focus** can automatically follow the strongest fresh matching candidate; a six-dB margin reduces switching between similar signals. The on-board focus choices are Axon, Flock/ALPR/Raven and camera glasses; the web editor exposes all families. Focus changes the heartbeat target; other enabled families still retain their alert melodies. Audio focus uses the normal balanced Wi-Fi/BLE scan. Select a specific contact and tap Hunt to prioritize it while retaining background coverage.

Higher, faster pulses mean a stronger filtered RSSI: approximately two seconds at -95 dBm down to 250 ms at -35 dBm. A rising/falling second chirp follows the signal trend. Axon uses a distinctive double heartbeat plus its own acquisition melody. After twelve seconds without observations, heartbeat feedback stops and a falling two-note cue plays once. Signal changes can also result from shielding, orientation and reflections; this is not measured distance or bearing.

The **[USB audio editor](https://valleytechsolutions.github.io/Dragon-Detector/#sound)** sits beside Badge Studio:

1. Connect in Detector tools. Choose a detector family and load its saved melody.
2. Edit a single ASCII RTTTL line such as `Axon:d=16,o=5,b=180:c,e,g,c6,g,e,8c6`.
3. Preview explicitly on the board speaker, or save without playing. An empty melody silences that family’s detection melody; hunt feedback has a separate switch. Restore default removes the override.
4. Save the audio settings separately for master enable, volume, hunt feedback and focus.

RTTTL bounds: 512 characters, 96 notes, 15 seconds total; duration divisors 1/2/4/8/16/32, octaves 4–7, tempo 25–900, sharps and dotted notes, `p` rests. Invalid input leaves the saved tone unchanged. Preferences and custom melodies survive restart. Preview intentionally works with automatic alerts disabled or quiet mode enabled, at the **saved** volume. Stop ends the current sound; disable automatic alerts to remain silent. Badge images retain their separate crop/upload controls.

Speaker adapters: Waveshare 1.75C uses its ES8311, I2S GPIO 16/9/45/8 and amplifier GPIO 46; Spotpear uses I2S GPIO 18/16/17 and amplifier GPIO 48; StopWatch uses M5Unified's speaker. Connect a suitable speaker where the board only supplies a speaker connector. No microphone recording or MP3 decoder is enabled.

USB commands: `tone settings`, `tone config <enabled 0|1> <volume 0..100> <hunt 0|1> <focus -1..25>`, `tone get <family>`, `tone set <family> <RTTTL or ->`, `tone preview <family> <RTTTL or ->`, `tone play <family>`, `tone reset <family>`, `tone stop`. Family IDs follow `Family` in `detection.h`; Axon is 3 and glasses is 4. A dash selects silence. Commands longer than 600 bytes are rejected rather than executed after truncation.

## Reception and diagnostics

**Settings → BLE scan** switches Passive/Active. Passive listens only. Active sends scan requests to obtain response-only names; it does not connect or pair. Some devices ignore requests. Active mode bounds and renews its result cache each second. Wi-Fi remains passive in both modes.

**Settings → Radio health** shows received Wi-Fi/BLE counts, queue drops/occupancy, worst processing time, queue lag, contact evictions and available memory. Counters reset at boot. Increasing drops/lag indicates reception pressure; an explicit radio mode or active lab session can intentionally reduce background coverage. There are 384 live contact slots shared across the radios; history remains 512 address/radio/family records.

Additional USB commands: `radio active`, `radio passive`, `screen targets`, `screen health`. `status` includes `RADIO_STATS`. They use the same settings and screens as the device controls.

## Daily timing and orientation

Tools → Utilities → Stopwatch provides touch Start/Pause, Lap/Reset, and a lap list; hardware keys work while that page is open. Tools → Utilities → Clock and Settings → Clock & date configure the local clock. Settings → Orientation saves a fixed 0°/90°/180°/270° angle. [Full stopwatch, RTC and orientation guide](STOPWATCH-CLOCK-ORIENTATION.md).

## Seeed XIAO Round Display

The 0.9.2 XIAO profile fits the same tools to 240×240. Use touch Tools/Home; BOOT starts/pauses Stopwatch or opens Tools elsewhere. Lap/reset use touch. Visual alerts and on-screen foxhunting are supported; there is no speaker. The Round Display SD slot supports opt-in CSV/PCAP and file browsing, while badges/history remain in internal flash. [Hardware and switch guide](XIAO-ROUND.md).

## Waveshare selectable modes and timed tools

Open **Tools → Recon → Modes** for Dragon radar, Axon watch, ALPR watch, Sky watch, Wi-Fi survey, BLE survey and capture modes. **Radio tools** opens inventory, timed transmission tests and Capture Studio. Focused audio respects your saved quiet/volume preferences. Home stops active testing. See the [complete radio-toolkit guide](RADIO-TOOLKIT.md) for capture formats, targets, commands and actual limitations.
