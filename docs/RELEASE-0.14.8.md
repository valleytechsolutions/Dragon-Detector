# Dragon Detector 0.14.8 · BadUSB scripts and DagNazty watches

Version 0.14.8 adds a USB keyboard script runner on native-USB boards, a new
DagNazty Dark theme, a redesigned DagNazty analog watch and two DagNazty
digital watches. Saved themes and watch selections keep their IDs.

## BadUSB keyboard scripts

Native-USB S3 profiles get a one-shot USB **keyboard mode**. Normal operation
keeps the usual USB serial port, so flashing and the web flasher work exactly as
before. Open **Utilities → BadUSB** (Dial: **Menu → Tools → BadUSB**), pick a
script, press **KEYBOARD** to restart as a USB keyboard, then press **RUN**.
**EXIT**, the reset button or a power cycle returns to USB serial. Scripts never
start on their own: selecting, uploading, switching modes, restarting or
plugging in USB does not run one. The runner supports
a basic DuckyScript subset with a US keyboard layout. See
[BadUSB keyboard scripts](BADUSB.md) for commands, uploads and board support.

The Waveshare 1.28 Lite and SenseCAP Watcher keep their USB-to-serial bridges
and do not get the keyboard.

**Flashing.** Updates install normally. Only a detector that is currently in
keyboard mode has no serial port; press EXIT or power-cycle it before flashing.

**Validation.** On a Waveshare ESP32-S3-LCD-1.85B, normal boots appear as the
hardware USB serial port and accept automatic-reset uploads; the `badusb` serial
commands answered, a script upload was saved and listed, and keyboard mode
re-enumerated as a USB keyboard only. Typing on a host computer still needs a
physical check on each supported board.

## DagNazty Dark (theme 46, watch 49)

Select **DagNazty Dark** on the device or with `radar theme dagnazty-dark`. It
has a black field and a blood-red interface and radar. The supplied skull is
recolored for it (bone skull, glowing red eyes, blood-red hood and halo) by
`tools/render_dagnazty_dark_art.py`; the original artwork file is unchanged.
The web flasher offers it for firmware 0.14.8 and newer.

## DagNazty watches

- **Analog (watch 26 and 49).** A larger skull, fang hour markers paired at the
  quarters, a red seconds trail around the bezel, drips off the rim and a
  fang-weighted seconds hand. DagNazty uses the light version and DagNazty Dark
  the dark version.
- **Digital (watch 50 light, 51 dark).** Pick the **DagNazty Digital** or
  **DagNazty Dark Digital** theme (47 / 48), swipe the clock, or use
  `clock face 50` / `clock face 51`. Sixty rim fangs fill red as the minute passes, large
  12-hour digits carry a blood-red shadow, and a dripping fang divider sits
  above the seconds, AM/PM, weekday and date.

All four DagNazty watches and the dark radar were checked by screenshot on the
1.85B. Every board profile builds; other panels were not checked on hardware.
