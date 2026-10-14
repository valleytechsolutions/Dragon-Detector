# Stopwatch, clock and orientation · 0.9

Open **Tools → Daily → Stopwatch**. Start begins or resumes elapsed time; Pause stops it; Lap records a split while running. Reset is available only while paused and asks before clearing the timer and laps. Home, Tools and the lap list leave a running timer running. Detection continues in the background; detection popups do not cover the timing controls. Audio alerts retain their existing settings.

The timer uses a 64-bit monotonic microsecond clock and displays hundredths. Clock synchronization, timezone changes and the 49-day `millis()` rollover do not change elapsed time. The display refresh is not a guarantee of reaction-time accuracy. The latest 50 laps are retained, with split and cumulative times; older laps roll out of the list. Elapsed time and laps are RAM-only and reset after reboot/power loss.

| Board | Start / pause while on Stopwatch | Lap while running / reset while paused | Clock hardware | Motion hardware |
|---|---|---|---|---|
| M5Stack StopWatch C152 | Yellow button | Blue button | RX8130CE | BMI270 |
| Waveshare ESP32-S3-Touch-AMOLED-1.75C | BOOT button | Short PWR press | PCF85063, when detected | QMI8658 |
| Spotpear / Guition JC3636W518 | Touch Start / Pause | Touch Lap / Reset | Software clock | No supported onboard IMU |

All boards have the same touch controls. Hardware stopwatch shortcuts apply only on the Stopwatch page. M5Stack navigation elsewhere retains yellow = radar and blue = Tools/Badge. Waveshare BOOT opens Tools outside Stopwatch; short PWR returns to radar. Long PWR retains the board's power-off behavior; it is not a lap shortcut.

## Set the clock

Open **Settings → Clock & date → Set date & time**, or Tools → Daily → Clock. Use **Field** to select year, month, day, hour, minute, second or UTC offset, then +/− to adjust it. Holding +/− repeats. Save applies the complete setting; Cancel/Home discards the draft. Invalid dates are rejected, and changing month/year clamps the day to that month's length.

The RTC stores UTC. The display applies your saved offset; detection history continues to store UTC. Offsets are fixed, not geographic daylight-saving rules. On-device changes use 15-minute steps from UTC−12:00 through UTC+14:00. In the USB web tools, **Use computer time & offset** copies the computer's current clock and offset; use it again after a daylight-saving change. The ordinary **Sync clock** control updates UTC without changing your display offset.

M5Stack and Waveshare read a valid hardware RTC at boot, and time settings write/read back the RTC. If it is uninitialized, stopped, reports lost clock integrity or cannot be read, the clock stays unset. An RTC requires its board's battery/power supply to maintain time when USB is removed. Spotpear can display a synchronized software clock, but it needs setting again after restart. Dates are limited to 2000–2099, matching the hardware year registers.

## Stable automatic rotation

**Settings → Orientation** offers a persistent fixed angle (0°, 90°, 180°, 270° clockwise), **Lock current orientation**, or **Auto rotate** when an accelerometer is detected. Fresh installations default to locked 0°. These settings also apply to radar, badge, clock and touch input.

Automatic rotation samples acceleration at 20 Hz and filters gravity. It requires a clear axis-aligned tilt, a near-gravity magnitude and **1.2 seconds** continuously in the candidate orientation. Flat and diagonal positions, sudden movement, missing samples and active touch reset the pending turn. Touch keeps it fixed until 0.5 seconds after release; a 1.5-second cooldown separates accepted turns. A dangling or shaking badge should therefore hold its current view rather than chase every reading.

If the sensor's natural upright edge differs from how you wear the board, choose the desired fixed angle, hold it upright and still for 1.2 seconds, and tap **Set upright reference**. Then enable Auto. Calibration is rejected when flat, moving or missing data. Auto uses accelerometer gravity; it does not infer viewing direction when lying flat. Manual angles work on Spotpear without an accelerometer.

Rotation uses a fixed DMA buffer sized to each board's existing display tile. It preserves AMOLED even-coordinate update windows, RGB565 byte order and LVGL's matching touch-coordinate transform. It does not allocate a full-screen framebuffer on each turn.

## USB commands

`screen stopwatch`, `screen laps`, `screen clock`, `screen orientation`

`stopwatch start|pause|lap|reset|status|laps` — reset fails while running; lap fails while paused. The USB reset command is explicit and does not display the touch confirmation.

`time <UTC epoch seconds>`, `clock offset <minutes>`, `clock status`

`orientation set auto|0|90|180|270`, `orientation calibrate`, `orientation status`

Firmware 0.9 exposes these commands; older versions keep their earlier protocol. See [validation](VALIDATION.md) for physical-board test limits and [hardware sources](HARDWARE.md) for the actual components.
