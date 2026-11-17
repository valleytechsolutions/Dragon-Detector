# Dragon Detector 0.14.8 · Wardriving node modes and a sleeker radar

Version 0.14.8 turns any Dragon Detector into a scan-only wardriving node for a
Biscuit or a JustCallMeKoko core, gives the full-board radar a new look with
less drawing per frame, and makes alerts and radar dots calmer and more honest.
Every board profile gets the update.

Use **Update** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install)
to keep your settings. **Fresh installation** is only needed for a board that
has never run Dragon Detector or one that will not start.

## Wardriving node modes

**BISCUIT NODE** and **JCMK Node** are the first two entries under
**Tools → Modes** on the full boards, under **Menu → Tools → Modes** on the
M5Stack Dial and on the **MODES** page of Waveshare Lite. USB `node biscuit` and
`node jcmk` start them too.

- The board finds the core by itself on channel 6 and sends each Wi-Fi network
  it hears (and, for a JCMK core, each Bluetooth LE device) over ESP-NOW. The
  core adds the time and its GPS position and keeps the log.
- While a node runs the board does nothing else: the radar, detection, alerts,
  audio and the other radio tools pause. The screen shows the core's logo, the
  Wi-Fi network count, what has been sent and the link state.
- **BISCUIT NODE** joins a Biscuit in Extended mode and stays a node for the
  whole Biscuit wardrive. Once linked it reads **WAITING FOR WARDRIVE** and only
  checks in. It scans when a wardrive started in the Biscuit app gives it the
  Wi-Fi role, and goes back to waiting the moment the wardrive is stopped,
  when the Biscuit says so (or about two minutes later at most, if the node
  misses that message while it scans). A new wardrive gets every network in
  view again. A Biscuit in Secure mode is named on screen
  (**SET BISCUIT TO EXTENDED MODE**).
- **Leaving is deliberate**, so a bump or a thumb on a side button cannot end a
  drive. Tap EXIT, then CONFIRM, or, on boards whose button reports a hold
  (the 1.75C, 1.85B and 1.43C, the CrowPanel 2.1 and Watcher knob, the Dial
  and Lite), hold it and then hold it again (or tap CONFIRM) within 5 seconds. Quick presses, double presses and
  knob clicks never leave; they show how to leave for 3 seconds.
- A node that was running resumes after a power loss.
- USB `node status` reports the link, the role, the counts and the timing, and
  `NODE_EVENT` and `NODE_EXIT_PRESS` lines show what the core sent and which
  presses were made. `node trace on` logs every node message, for field tests.

[Cores, screen, USB commands and troubleshooting](NODE-MODES.md)

## Radar

- **A sleeker radar on the full boards.** Valleytech gets a gold bezel and
  labels with an electric-cyan phosphor sweep, a ring that lights where the
  sweep passes and a breathing centre. The palette themes get the same
  phosphor sweep in their own sweep colour. A live dot sends out one ring when
  it is heard. Lite and Dial keep their simpler radar.
- **Less drawing per frame.** The radar face is drawn once per theme and
  reused, and each frame repaints only what changed. On a desktop build of the
  firmware's own renderer at 466 px a frame repaints 36 to 38% of the panel at
  about half the cost of the old full redraw.
- **Fewer lost observations** on boards with PSRAM: a larger queue that is
  emptied more often while the radar draws.
- **More honest dots.** Dots are live (heard in the last 10 seconds), fading
  (10 to 30 seconds) or recent, and the status row counts live and recent
  separately. The signal filter reacts faster after a gap, Wi-Fi contacts use
  the channel the access point advertises, and weak dots no longer stack in
  two rows at the outer ring.
- A quiet **BLE radios near** line on every board counts strong nearby
  Bluetooth radios and how many have stayed about five minutes. It counts
  radios, not people.

## Alerts and radio tools

- **One notification per device per visit.** A device notifies again only
  after 15 minutes with nothing heard from it (Settings → *Alert again*). Time
  the radar is paused does not count as a device being away.
- **Room motion** replaces the CSI signal monitor on the full boards. It senses
  movement near the detector while it sits still, and picks its network on its
  own screen.
- *Survey / select network* puts the radio back the way it was when you leave.

## Validation

Host tests cover the node link against simulated JCMK, Biscuit and Piglet cores
(handshakes and retries, the Biscuit's role and channel plan, the wardrive
start and end, lost confirmations, link loss and random frames), the node
session and its USB commands, and the exit rules for every kind of input: the
touch EXIT, the side buttons, the CrowPanel 2.1 and Watcher knob, and the Dial
and Lite buttons. The node screen is drawn at every panel size and checked for
text outside the round glass. The radar renderer, the alert rules and the
signal filter have their own host tests. All browser tests passed, and all 14
board profiles build and fit their application slots.

On a Waveshare 1.75C:

- **BISCUIT NODE with a real Biscuit**, November 17, 2026. In Extended mode the
  Biscuit joined the node within half a second, gave it the Wi-Fi role and its
  channel list during a wardrive, and received Wi-Fi records with no
  failures or link losses over 20 minutes. In two more listens of 10 and 15
  minutes during a running wardrive the link never dropped. A Biscuit in
  Secure mode answered and, as intended, was not joined.
- **Waiting and the wardrive stop**, the same day. A node that joined the
  Biscuit with no wardrive running waited without scanning. Stopping a
  wardrive in the app made the Biscuit send its nodes a stop message, and the
  node went back to waiting and stopped scanning. This version acts on that
  message at once; the build used in the field noticed the stop 84 seconds
  later by the two-round rule that is now the fallback.
- **The new radar**, November 17, 2026: over two minutes every frame was a
  partial repaint, taking 73 ms on average and 102 ms at most, at about 10
  frames a second. The UI pass took 71 ms, against 168 ms in 0.14.7, and none
  of 8,730 observations was dropped.
- **Radar accuracy changes**, November 16, 2026, in a build without the new
  radar look: no observation was dropped out of 13,638 in 5½ minutes, where
  the previous version dropped about 30%.

Not yet confirmed on hardware:

- A wardrive started while the node waits, the immediate stop on the
  Biscuit's stop message, and the hold-to-leave rules, with a real Biscuit.
- **JCMK Node** with a real JustCallMeKoko core or a Piglet.
- The node modes on any board other than the 1.75C, including the knob on the
  CrowPanel 2.1 and Watcher and the buttons on the Dial and Lite.
- Room motion and the BLE radios near line in real rooms.

[Validation status](VALIDATION.md) · [Installation and recovery](FLASHING.md)
