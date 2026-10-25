# VectorPotential preview

Private device preview, version `0.13.36-vp-preview`, based on `1999990`.
This build has not been uploaded to the web flasher.

- Select **VectorPotential** in Radar Style, or send `radar theme vectorpotential`.
- The theme selects the matching clock face (watch ID 44, theme ID 41).
- Radar: large supplied VP logo, cyan rings and live detection markers.
- Clock: large supplied glowing lock, VP inside the shackle, a long Nessie
  minute hand and shorter hook hour hand. A red perimeter marker shows seconds.
- Locksport timer: START/PAUSE/RESUME, SPLIT, OPEN to record and stop, LOG and
  confirmed RESET. Uses the existing monotonic timer; time spent paused is
  excluded. The latest 50 splits are retained in RAM and reset on reboot.
- M5Stack StopWatch physical button A starts/pauses; B records OPEN while running.
  Touch controls offer the same actions plus SPLIT and LOG.

The two original WebP files are preserved in `media/theme-logos/`.
`tools/encode_radar_logo.py` converts them into native indexed display assets.
`tools/render_vectorpotential.py` renders the actual drawing code at several
display sizes, hand positions and invalid-clock state for local review.
See [credits](../CREDITS.md) for artwork and lockpick references.

No public gallery, flasher manifest or other board's packaged firmware was changed.

## Validation on October 25, 2026

- StopWatch build passed; app size 3,402,688 bytes, within the 4 MiB app slot.
- All 204 native image decodes matched their expected pixel CRCs.
- Existing native core/timer suite passed, including paused-time exclusion,
  finish/split behavior and 150,000 parser fuzz inputs.
- Actual LVGL artwork rendered at 240, 360, 466 and 480 pixels, with three
  hand positions and the unset-clock state checked.
- A complete 16 MiB backup of the connected StopWatch's previous 0.13.26
  installation was read and verified against the halted device before writing.
  The private backup and restore notes live outside this source tree.
- App-only flash at `0x10000` verified successfully. The device reports
  `0.13.36-vp-preview`, theme 41, matching clock 44 and a valid RTC.
- Injected touch exercised the real UI's START, SPLIT, PAUSE, RESUME, OPEN and
  LOG actions. Paused and finished elapsed time remained fixed; both splits
  appeared in the log. Test results were cleared and the clock left visible.
- CRC-checked screenshots of radar, clock and locksport timer were captured
  from the physical StopWatch and visually reviewed. Finger/button feel is
  awaiting the user's review; synthetic touches do not verify physical inputs.
