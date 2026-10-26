# Dragon Detector 0.13.38 — locksport collection

Four additional built-in themes bring the collection to 44 themes. Existing
saved theme IDs, files and settings remain compatible with matching-board updates.

| Theme | Radar | Clock |
| --- | --- | --- |
| VectorPotential | Supplied VP logo | Large lock artwork, VP under the shackle, Nessie minute hand and hook hour hand |
| Argyle | Supplied emblem and one rotating dimple-style pick | Two hook-pick hands on the monochrome emblem |
| Law Lock Tools | Blue-on-white logo and Ronin-inspired pick | Ronin-inspired hands and sixty pin-tumbler second markers |
| Moki | Green-on-black logo and rotating AKAB-inspired pick | Handled hook picks with visible handle fasteners |

All four select a matching clock and locksport timer. On full profiles the timer
supports Start/Pause/Resume, Split, Open, Log and confirmed Reset. Open stops and
records the completed attempt. The latest 50 splits are held in RAM and clear
at reboot. Compact Dial/Lite profiles retain their existing compact timer controls;
they do not gain the full-profile log page. Law Lock Tools uses pin markers for
elapsed seconds on both full and compact timers.

Select the theme in Radar Style, the Dial theme menu or the web flasher's Device
tools. Lite cycles themes with a short button press on radar/clock. USB commands
are `radar theme vectorpotential`, `argyle`, `lawlocktools` and `moki`.
The web controls require released firmware 0.13.38 or newer.

## Compatibility and validation

Release packages target all eleven supported profiles: StopWatch, Watcher,
M5Dial, LILYGO 1.75, Elecrow 2.1, Waveshare 1.75, Waveshare 1.75-G, Spotpear,
Waveshare 1.75C, XIAO Round and Waveshare Lite. Existing hardware beta labels
and board limitations still apply.

Native renderers exercise actual LVGL drawing code at 240, 360, 466 and 480
pixels, multiple hand/sweep angles and unset time. Asset checks verify 222
native logo decodes against their pixel CRCs. Web checks cover palette contrast,
theme ID bounds, firmware compatibility and installation safeguards. Core checks
include timer state and detector fuzz coverage. Each board package checks flash
budgets, partition offsets and SHA-256 hashes.

The connected StopWatch passed theme selection, saved selection after restart,
and USB-injected Start/Split/Pause/Resume/Open/Log checks for all four themes.
The owner also confirmed that the physical screen looks good and touch works.
Other boards are build-validated, not newly physically tested. Injected UI checks
do not validate physical buttons.
Original source artwork and references are documented in [credits](../CREDITS.md).
Radar sweeps are decorative and do not measure signal direction.

Use **Update matching Dragon Detector** for an existing compatible installation.
Fresh install resets stored data. Choose the exact board profile.
