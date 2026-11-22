# Dragon Detector 0.14.9 · Node modes with BadUSB, DagNazty and LockpickingVlad

Version 0.14.9 joins two lines that both called themselves 0.14.8: the
wardriving node release on the web flasher, and Dag's BadUSB keyboard mode and
DagNazty watches. It also adds the LockpickingVlad theme and the streamlined web
flasher. Saved themes and watch selections from the published 0.14.8 keep their
IDs.

Use **Update** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install)
to keep your settings.

## Wardriving node modes

**BISCUIT NODE** and **JCMK Node** from 0.14.8 are unchanged and stay on every
board profile: the first two entries under **Tools → Modes** on the full boards,
**Menu → Tools → Modes** on the M5Stack Dial, the **MODES** page on Waveshare
Lite, and USB `node biscuit` / `node jcmk`. BISCUIT NODE still waits for the
Biscuit wardrive, stops scanning the moment the Biscuit ends it, and keeps the
deliberate exit. See [the 0.14.8 notes](RELEASE-0.14.8.md) and
[BISCUIT NODE and JCMK Node](NODE-MODES.md).

## BadUSB keyboard scripts

By Dag. Native-USB S3 profiles get a one-shot USB **keyboard mode**. Normal
operation keeps the usual USB serial port, so flashing and the web flasher work
exactly as before. Open **Utilities → BadUSB** (Dial: **Menu → Tools → BadUSB**),
pick a script, press **KEYBOARD** to restart as a USB keyboard, then press
**RUN**. **EXIT**, the reset button or a power cycle returns to USB serial.
Scripts never start on their own: selecting, uploading, switching modes,
restarting or plugging in USB does not run one. The runner supports a basic
DuckyScript subset with a US keyboard layout. See
[BadUSB keyboard scripts](BADUSB.md) for commands, uploads and board support.

The Waveshare 1.28 Lite and SenseCAP Watcher keep their USB-to-serial bridges
and do not get the keyboard.

**Flashing.** Updates install normally. Only a detector that is currently in
keyboard mode has no serial port; press EXIT or power-cycle it before flashing.

## Node modes and BadUSB together

The two never share the device:

- While a node runs, BadUSB does not select a script, enter keyboard mode or
  run. USB `badusb` commands answer `BADUSB_ERROR Stop the node first`, except
  `badusb status`, `badusb stop` and `badusb cancel`.
- In keyboard mode, and while a script runs, the node modes refuse to start
  (*Leave USB keyboard mode first*, *Stop the BadUSB script first*).
- The restart into keyboard mode does not resume a remembered node. The choice
  is kept, so the node resumes after EXIT, the reset button or a power cycle.
- KEYBOARD and `badusb keyboard` refuse while the stopwatch runs or a radio
  tool or BLE Lab holds the radio, as `system restart` does.
- A node start frees BadUSB's idle 8 KB script buffer, which the M5Stack Dial
  (no PSRAM) needs for the node; BadUSB selects the same script again when it
  is next opened.
- On the Dial, Tools lists **Modes** and then **BadUSB**. A refused node start
  under Modes scrolls its full reason again.

## Themes and watches

| Theme / USB name | Theme / watch ID | Design |
|---|---|---|
| DagNazty Dark / `dagnazty-dark` | 46 / 49 | By Dag. Black field and blood-red interface and radar; the supplied skull is recolored by `tools/render_dagnazty_dark_art.py` (the original file is unchanged). |
| DagNazty Digital / `dagnazty-digital` | 47 / 50 | By Dag. DagNazty colors with the digital fang clock. |
| DagNazty Dark Digital / `dagnazty-dark-digital` | 48 / 51 | By Dag. DagNazty Dark colors with the digital fang clock. |
| LockpickingVlad / `lockpickingvlad` | 49 / 52 | Black eagle and padlock portrait, gold McTickler radar sweep, lockpick watch hands and the locksport timer. |

Dag also redesigned the DagNazty analog dial (watch 26, and 49 for the dark
version): a larger skull, fang hour markers paired at the quarters, a red
seconds trail, drips off the rim and a fang-weighted seconds hand. The digital
faces are also reached by swiping the clock or with `clock face 50` and
`clock face 51`. Details are in [Themes](THEMES.md).

The DagNazty IDs are the ones already published with Dag's work. LockpickingVlad
was only ever on a local test build, where it used 46 and 49; it now takes the
next free IDs. A board that ran that test build shows DagNazty Dark after this
update until LockpickingVlad is chosen again. The firmware cannot tell the two
apart in saved settings, so there is no automatic migration.

## Waveshare 1.85B and SenseCAP Watcher input

Two input repairs that were prepared for 0.14.3 but never released are now
included:

- **Waveshare 1.85B touch.** The CST816-family touch controller gets a reset and
  retry, and a tap made while the radar redraws is kept and acted on for the
  page it was made on, instead of being lost or landing on the next page.
- **SenseCAP Watcher touch and wheel.** Touch and the wheel button are polled
  separately. A touch fault restarts only the touch bus, with a backoff. Wheel
  turns move focus one detent at a time with no backlog after a slow frame, and
  a touch cancels a queued wheel action. `watcher touch recover` restarts the
  touch bus on request, and `watcher status` adds input counters.

On both boards the node screen keeps its own controls: the 1.85B BOOT hold and
the Watcher knob hold ask to leave, and nothing else reaches the page.

## Web flasher

- The streamlined install page and the Badge Studio artwork repair that are
  already live. Old detector-tool bookmarks open the installer.
- The streamlined page does not pick themes. Its DagNazty compatibility
  helper now starts at 0.14.9: the published 0.14.8 does not have these themes,
  and Dag's GitHub build reported the same 0.14.8. Every script cache token is
  `?v=0.14.9`.

## Validation

### Automated checks on the combined source

Run on the combined source on November 22, 2026, on Windows with PlatformIO
6.2.0 and Zig 0.14.1 for the native tests:

- **JavaScript:** 184 tests passed.
- **Native host suites:** 33 passed, including the node session (`node_mode_test`,
  with the new BadUSB refusal, skipped-resume and memory-release cases), the
  simulated Biscuit, JCMK and Piglet links in `core_test`, the BadUSB runtime
  (`test_badusb`, with the node and restart-guard cases), the Dial menus with
  Modes and BadUSB, theme IDs and retirement, and `test_vlad_art`.
  `test_core`, `test_sd_labels` and `test_badusb` also passed under
  `--sanitize`; on this toolchain that means UndefinedBehaviorSanitizer, not
  AddressSanitizer.
- **Renders:** the node screen at every panel size (84 images, no layout
  problems) and the production Dial and Lite screens passed.
- **Builds:** all 14 board profiles (the 13 stable boards and the Guition
  beta) compiled and packaged from one commit, and every application fits its
  slot with the 5% reserve. The tightest are the Waveshare 1.75 (3,915,824 of
  4,194,304 bytes, 6.64% free), the 1.75-G (6.94%), the XIAO Round (7.53%) and
  the StopWatch (9.02%). The release validator, the browser manifest check and
  the Dial package test passed.

The size check caught one integration problem: with BadUSB and the DagNazty
art added, the Waveshare 1.75 and 1.75-G images overran the 5% reserve of their
4 MiB application slots. The 466 px LockpickingVlad portrait is now stored
compressed and decoded into PSRAM when the theme first draws; its pixels are
unchanged.

### Hardware

No board has run 0.14.9 yet, and no 0.14.9 build has been flashed for this
release. The hardware results in the [0.14.8 notes](RELEASE-0.14.8.md) (node
modes with a real Biscuit on the 1.75C and C152) and Dag's 0.14.8 checks on a
Waveshare 1.85B (USB serial on normal boots, `badusb` commands, a saved script
upload, keyboard-only enumeration, DagNazty screenshots) were made on the
separate builds before they were combined.

Still to confirm on hardware:

- BISCUIT NODE and JCMK Node on 0.14.9, including the refusals in keyboard mode
  and the resume after EXIT.
- Typing from keyboard mode on a host computer, on each native-USB board.
- **JCMK Node** with a real JustCallMeKoko core or a Piglet.
- The node modes and hold-to-leave rules on boards other than the 1.75C and
  C152, including the CrowPanel 2.1 and Watcher knobs and the Dial and Lite
  buttons.
- The DagNazty and LockpickingVlad faces on panels other than the 1.85B.

[Validation status](VALIDATION.md) · [Installation and recovery](FLASHING.md)
