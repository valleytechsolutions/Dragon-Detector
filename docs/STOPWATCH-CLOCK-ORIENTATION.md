# Stopwatch, clock and orientation · 0.13.6

Open **Tools → Utilities → Stopwatch**. Start begins or resumes elapsed time; Pause stops it; Lap records a split while running. Reset is available only while paused and asks before clearing the timer and laps. Home, Tools and the lap list leave a running timer running. Detection continues in the background; detection popups do not cover the timing controls. Audio alerts retain their existing settings.

The timer uses a 64-bit monotonic microsecond clock and displays hundredths. Clock synchronization, timezone changes and the 49-day `millis()` rollover do not change elapsed time. The display refresh is not a guarantee of reaction-time accuracy. The latest 50 laps are retained, with split and cumulative times; older laps roll out of the list. Elapsed time and laps are RAM-only and reset after reboot/power loss.

| Board | Start / pause while on Stopwatch | Lap while running / reset while paused | Clock hardware |
|---|---|---|---|
| M5Stack StopWatch C152 | Yellow button | Blue button | RX8130CE |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | BOOT button | Short PWR press | PCF85063, when detected |
| Spotpear / Guition JC3636W518 | Touch Start / Pause | Touch Lap / Reset | Software clock |
| Seeed XIAO ESP32-S3 + Round Display | BOOT or touch | Touch Lap / Reset | BM8563 / PCF8563-compatible |

All boards have the same touch controls. Hardware stopwatch shortcuts apply only on the Stopwatch page. M5Stack navigation elsewhere retains yellow = radar and blue = Tools/Badge. Waveshare BOOT opens Tools outside Stopwatch; short PWR returns to radar. Long PWR retains the board's power-off behavior; it is not a lap shortcut.

## TOOOL Locksport Timer

Select **Settings → Theme & radar labels → TOOOL, TOOOL Dark or Covert Instruments / McNally** (or save the theme in Studio). Utilities now shows **Locksport timer**, and the matching watch is selected. TOOOL Dark uses near-black, red and soft white throughout radar, watch, timer and split log. Covert Instruments × McNally uses black, white and silver with both supplied logos and distinct pick hands. Changing to a non-locksport theme restores the ordinary stopwatch; it does not reset elapsed time or splits. The watch remains independently swipeable.

**START** begins an attempt; **PAUSE / RESUME** excludes pauses. **SPLIT** records a milestone without stopping. **OPEN** records the final split and stops at the same timestamp; the screen shows **OPENED** and the total elapsed time. **LOG** shows the latest 50 split and cumulative times for the current attempt. **RESET** asks before clearing this attempt for the next lock. Results are RAM-only, cleared by reset or reboot; this is not a persistent results archive. Home/Tools leaves a running attempt running.

On M5Stack, the blue secondary button performs OPEN while picking; on Waveshare, short PWR performs OPEN. Their primary Start/Pause buttons are unchanged. XIAO uses BOOT for Start/Pause and touch for OPEN/SPLIT. Other themes retain the ordinary hardware Lap/Reset action. USB `stopwatch finish` records and stops; it rejects an idle timer or a zero-length split.

DMAC uses the same controls under **Locksport counter**. Its original teal/pink logo, silver details and acrylic pick hands carry through the full GUI. Select **DMAC** to choose watch 24 automatically.

A small comet follows the outer rim in all four locksport themes. Each lap represents ten seconds of elapsed attempt time. PAUSE and OPEN freeze it; RESUME continues from the same position; RESET returns it to twelve. The motion stays outside the timer digits and buttons.

## Set the clock

The fastest setup is **Connect detector** in the local or hosted Studio: connection copies the computer's actual date, time and current timezone offset automatically. **Sync my local time** refreshes it in one click. Use this after travel, a daylight-saving change, or a restart on a board without a working RTC. History remains UTC; the watch shows local time.

On the device, open **Settings → Clock & date → Set manually**. Scroll the year/month/day, 1–12 hour/minute and AM/PM wheels, then tap **Save**. Seconds start at zero. Short months and leap years clamp the selected day. **Zone** opens the advanced field editor, including UTC offset and seconds. Cancel/Home discards the unsaved draft. The offset remains fixed until changed or resynced; it does not contain geographic daylight-saving rules.

## Twenty-four clock faces

Open **Tools → Utilities → Clock**, then swipe left or right. The chosen face is remembered across restarts. All analog faces use a 12-hour dial. Date labels never include AM/PM. LAB5 and the M5Stack digital window use 12-hour time. **JCMK military uses 24-hour digital time; the Covert hybrid also has a separate 24-hour window.** Original logos and drawn emblems replace decorative face-name captions.

| Index | Face | Design |
|---|---|---|
| 0 | BWM | Black and warm gold, original BWM mark, Roman numerals |
| 1 | ValleyTech | Clean black dial, supplied gold VTS mark, twelve Arabic numerals and broad beveled gold/silver hands |
| 2 | JCMK military | Supplied JCMK mark, muted green instrument dial, large 24-hour digital time |
| 8 | Biscuit Shop | Dark monochrome, supplied white logo, silver hands and continuously rotating gears |
| 9 | TOOOL | White/red, supplied keyway logo, Arabic dial, pin chambers and broad red/dark hands |
| 10 | Church of Malware | Full original emblem, monochrome weighted binary H/M/S lights; 12-hour time with AM/PM beside H |
| 11 | TOOOL Dark | Near-black/red analog, original keyway logo in a circular inset, soft white numerals, pin chambers and broad hands |
| 12 | Covert Instruments × McNally | Original CI/McNally artwork, silver Reaper short-hook hour and McTickler minute hands, pin chambers and an independent 24-hour digital window |
| 13 | AWOK | Original dripping logo, neon mushroom ornament, paint drips and sculpted cyan/pink hands |
| 14 | Baph0Mau5 | Supplied emblem, violet sigils, moon ornament, Roman numerals and ornate hands |
| 15 | WDGWARS | Original logo, stylized world map, cyan/magenta signal halos and beveled analog hands |
| 16 | GhostStrats | Purple glowing slit eyes, faceted grey details, silver/purple hands and “Safety is an Illusion.” |
| 17 | N3rdsec | Supplied skull, black/white/gold, Arabic dial and broad gold/silver hands |
| 18 | HaleHound | Both supplied logos, animated pink/violet flames, Roman dial and sculpted hands |
| 19 | Waveshare | Original green logo, silver hands and a precision waveform dial. |
| 20 | M5Stack | Original blue and white logo, modular dial and a separate 12-hour digital window. |
| 21 | LAB5 | Original scientist artwork, cyan and magenta circuits and large 12-hour digital time. |
| 22 | Talking Sasquach | The cartoon becomes a character watch: furry arms and pointing hands tell the time. The portrait head appears on the forest radar. |
| 23 | Nyan Devices | The original logo colors run through the dial and menus. A pixel Nyan Cat flies across the radar with a moving rainbow trail. |
| 24 | DMAC | Teal, pink, charcoal and silver. Two acrylic McTickler profiles with copper inlays and pearl rivets tell the time; Utilities becomes Locksport Counter. |
| 25 | Seeed Studio | Supplied circular logo, deep teal and lime, leaf-circuit traces, Arabic numerals and broad lime/silver hands. |
| 26 | DagNazty | White field, supplied skull silhouette, black tapered hands, fang details and restrained talon outlines. |
| 27 | 0ct0’s Pig | ASCII octopus and wandering pig; live 24-hour time, blinking eyes and tap reactions. |
| 28 | DeFlock | Supplied camera-raven, navy and steel blue, viewfinder framing, large 24-hour time and an optical seconds rim. |

The Church of Malware watch keeps the complete emblem clear. Add the white bits in each bank: H uses 8, 4, 2, 1; M/S each use 32, 16, 8, 4, 2, 1. Dark bits are off. Midnight/noon both show H = 8 + 4, with AM/PM beside the hour. An unset clock shows SET and inactive bits; tap its gear to synchronize. Selecting the Church of Malware radar theme selects this watch.

On classic dials, a dot strip indicates the current face. The gear opens time settings; the list icon or a downward swipe opens Tools. HaleHound flames and Biscuit mechanisms refresh at up to five frames per second, with only the changing region redrawn between clock ticks. Gearwork, moon phases, map outlines, radio halos and celestial details are decorative; they are not mechanical or astronomical measurements.

Clock and stopwatch suppress detection popups without stopping background listening. An unset clock displays a setup prompt rather than invented time. These features apply to all four full boards; Lite has no clock UI.

M5Stack and Waveshare read a valid hardware RTC at boot, and time settings write/read back the RTC. If it is uninitialized, stopped, reports lost clock integrity or cannot be read, the clock stays unset. An RTC requires its board's battery/power supply to maintain time when USB is removed. Spotpear can display a synchronized software clock, but it needs setting again after restart. Dates are limited to 2000–2099, matching the hardware year registers.

## Saved display orientation

Open **Settings → Orientation** and choose **0°, 90°, 180° or 270° clockwise**. The selected angle is marked and saved across restarts. The same choices are available in the USB web tools. They apply to radar, badge, clock and touch input on every board.

Version 0.9.1 uses manual orientation only. There is no automatic tilt rotation or motion-sensor polling. Fresh installations start upright at 0°; an update retains the last saved fixed angle and clears the old Auto preference.

Rotation preserves AMOLED even-coordinate update windows, RGB565 byte order and LVGL's matching touch-coordinate transform. Waveshare and Spotpear use fixed DMA tile buffers. M5Stack and Seeed XIAO Round use two persistent PSRAM frames with synchronous display transfers, reducing repeated radar drawing and freeing internal memory. No frame buffer is allocated on each turn. M5Stack touch transitions and timestamped hardware keys are buffered at 100 Hz, so a short press can be delivered after a redraw finishes.

## USB commands

`screen startup` replays the BWM2 logo-only boot animation on black, then returns to radar. It does not reset the device.

`screen stopwatch`, `screen laps`, `screen clock`, `screen clock-settings`, `screen clock-edit`, `screen orientation`

`clock face` reads the saved face. `clock face <index>` selects and saves a valid face from the table above (0.13.6). IDs 3–7 are retired and rejected over USB; saved retired selections migrate to BWM. Existing logo IDs remain unchanged.

`stopwatch start|pause|lap|finish|reset|status|laps` — reset fails while running; lap fails while paused. The USB reset command is explicit and does not display the touch confirmation.

`time <UTC epoch seconds>`, `clock offset <minutes>`, `clock status`

`orientation set 0|90|180|270`, `orientation status`

`loop diagnostics` — recent/maximum main-loop stage durations, useful for diagnosing drawing or storage stalls (0.9.1).

Firmware 0.9 exposes these commands; older versions keep their earlier protocol. See [validation](VALIDATION.md) for physical-board test limits and [hardware sources](HARDWARE.md) for the actual components.
