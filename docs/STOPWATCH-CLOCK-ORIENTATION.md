# Stopwatch, clock and orientation · 0.9

Open **Tools → Daily → Stopwatch**. Start begins or resumes elapsed time; Pause stops it; Lap records a split while running. Reset is available only while paused and asks before clearing the timer and laps. Home, Tools and the lap list leave a running timer running. Detection continues in the background; detection popups do not cover the timing controls. Audio alerts retain their existing settings.

The timer uses a 64-bit monotonic microsecond clock and displays hundredths. Clock synchronization, timezone changes and the 49-day `millis()` rollover do not change elapsed time. The display refresh is not a guarantee of reaction-time accuracy. The latest 50 laps are retained, with split and cumulative times; older laps roll out of the list. Elapsed time and laps are RAM-only and reset after reboot/power loss.

| Board | Start / pause while on Stopwatch | Lap while running / reset while paused | Clock hardware |
|---|---|---|---|
| M5Stack StopWatch C152 | Yellow button | Blue button | RX8130CE |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | BOOT button | Short PWR press | PCF85063, when detected |
| Spotpear / Guition JC3636W518 | Touch Start / Pause | Touch Lap / Reset | Software clock |

All boards have the same touch controls. Hardware stopwatch shortcuts apply only on the Stopwatch page. M5Stack navigation elsewhere retains yellow = radar and blue = Tools/Badge. Waveshare BOOT opens Tools outside Stopwatch; short PWR returns to radar. Long PWR retains the board's power-off behavior; it is not a lap shortcut.

## Set the clock

Open **Settings → Clock & date → Set date & time**, or Tools → Daily → Clock. Use **Field** to select year, month, day, hour, minute, second or UTC offset, then +/− to adjust it. Holding +/− repeats. Save applies the complete setting; Cancel/Home discards the draft. Invalid dates are rejected, and changing month/year clamps the day to that month's length.

The RTC stores UTC. The display applies your saved offset; detection history continues to store UTC. Offsets are fixed, not geographic daylight-saving rules. On-device changes use 15-minute steps from UTC−12:00 through UTC+14:00. In the USB web tools, **Use computer time & offset** copies the computer's current clock and offset; use it again after a daylight-saving change. The ordinary **Sync clock** control updates UTC without changing your display offset.

M5Stack and Waveshare read a valid hardware RTC at boot, and time settings write/read back the RTC. If it is uninitialized, stopped, reports lost clock integrity or cannot be read, the clock stays unset. An RTC requires its board's battery/power supply to maintain time when USB is removed. Spotpear can display a synchronized software clock, but it needs setting again after restart. Dates are limited to 2000–2099, matching the hardware year registers.

## Saved display orientation

Open **Settings → Orientation** and choose **0°, 90°, 180° or 270° clockwise**. The selected angle is marked and saved across restarts. The same choices are available in the USB web tools. They apply to radar, badge, clock and touch input on every board.

Version 0.9.1 uses manual orientation only. There is no automatic tilt rotation or motion-sensor polling. Fresh installations start upright at 0°; an update retains the last saved fixed angle and clears the old Auto preference.

Rotation preserves AMOLED even-coordinate update windows, RGB565 byte order and LVGL's matching touch-coordinate transform. Waveshare and Spotpear use fixed DMA tile buffers. M5Stack uses two persistent PSRAM frames with its synchronous M5GFX transfer, reducing repeated radar drawing and freeing internal memory. No frame buffer is allocated on each turn. M5Stack touch transitions and timestamped hardware keys are buffered at 100 Hz, so a short press can be delivered after a redraw finishes.

## USB commands

`screen stopwatch`, `screen laps`, `screen clock`, `screen orientation`

`stopwatch start|pause|lap|reset|status|laps` — reset fails while running; lap fails while paused. The USB reset command is explicit and does not display the touch confirmation.

`time <UTC epoch seconds>`, `clock offset <minutes>`, `clock status`

`orientation set 0|90|180|270`, `orientation status`

`loop diagnostics` — recent/maximum main-loop stage durations, useful for diagnosing drawing or storage stalls (0.9.1).

Firmware 0.9 exposes these commands; older versions keep their earlier protocol. See [validation](VALIDATION.md) for physical-board test limits and [hardware sources](HARDWARE.md) for the actual components.
