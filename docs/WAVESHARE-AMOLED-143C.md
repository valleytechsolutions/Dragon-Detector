# Waveshare ESP32-S3-Touch-AMOLED-1.43C

Choose **Waveshare AMOLED 1.43C** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install). The build profile is `waveshare_143c`, first available in **0.14.7**. Match the complete **ESP32-S3-Touch-AMOLED-1.43C** name, including the **C**. The ESP32-S3-Touch-AMOLED-1.43 and 1.43-B are different boards with 16 MB flash, another touch controller and other wiring. This firmware does not support them, and the 1.75C, 1.75 and LILYGO round displays each need their own profile.

[Buy the exact 1.43C model](https://www.waveshare.com/esp32-s3-touch-amoled-1.43c.htm?&aff_id=122099) · [Waveshare hardware documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43C) · [Release notes and validation](RELEASE-0.14.7.md)

**Physical validation is in progress.** The firmware has been installed and started on one board, and its automatic checks passed. That does not establish that the picture, touch, buttons or speaker work; [the release notes](RELEASE-0.14.7.md#validation) list what was checked and what was not.

## Hardware and installation

This profile provides the full 466 × 466 touch interface: radar, themes, badge, analog clocks, stopwatch and locksport timers, radio tools and BLE Lab. The board's ESP32-S3-PICO-1 carries 8 MB flash and 8 MB PSRAM inside the chip package. It also has an ES8311 speaker codec with a speaker connector, a lithium battery connector with a charger, and one programmable button. It has no microSD slot, no real-time clock chip, no GPS and no motion sensor. Waveshare sells it with and without a battery; both use this profile.

Back up the factory firmware before the first installation. Choose **Fresh installation** for the Waveshare demo or another firmware. **Update** is for this exact Dragon Detector 1.43C profile, 0.14.7 or newer; it preserves settings and internal files. It does not migrate files from another board profile.

The flasher normally restarts the board into its installer mode by itself. If the browser cannot connect, start download mode by hand: with the board switched off, hold **BOOT** and press **PWR**, or hold BOOT while plugging in USB, then release BOOT. The screen stays dark in download mode. If the board does not restart by itself after the installation, press PWR or reconnect USB.

The web flasher sets local time directly after installation. Keep USB connected until that check finishes. The board has no clock chip, so time lives in the ESP32 and is lost whenever the board loses power or is switched off with PWR. Reconnect to the flasher for clock synchronization, or configure [saved-Wi-Fi recovery](CLOCK-SETUP.md). Until then the clock shows a setup prompt instead of invented time.

## Buttons

**BOOT** is the only button the firmware can read, so it does all hardware navigation, the same way the single programmable button does on the Waveshare LCD 1.85B:

- A short press cycles **Radar → Main menu → Utilities → Badge → Clock → Radar**. From any other screen, a short press opens the main menu.
- Hold BOOT for about 0.7 seconds to return to the main menu.
- On the stopwatch or locksport timer, a single click starts or pauses timing. A double click performs the timer's Lap/Finish action while running. While stopped it opens the reset question, which you answer on the touchscreen. Timer singles wait about 0.25 seconds for a possible double click, but use the original press timestamp, so that delay does not change the recorded time.

The board has no clock chip, so after each power-on the **SET YOUR CLOCK** prompt opens on the radar, about ten seconds in, unless the time has already been set. Once the prompt has been on screen for about a second and a half, a BOOT press answers **Later**, and the next press navigates. A press before that does nothing, because it may have been meant for the screen underneath. BOOT waits while any other dialog is open, such as the stopwatch reset question; answer it on the touchscreen first. It also waits while an alert notice is on screen. An ordinary alert clears itself after about six seconds. An alert for a watched device stays until you tap **HUNT** or **DISMISS**. Alert notices do not open on the clock and timer screens, so they never block a timer click. Touchscreen controls remain available everywhere.

## Touch

A quick tap counts even while the radar redraws, and a quick swipe scrolls a list instead of opening the row it started on. A tap made while a new page, an alert or a dialog is still appearing does nothing, because the finger landed on what was there before; tap again once it is on screen. On a dark screen the first touch only wakes it, and a second tap before the picture is back does nothing. A finger that stays down while the screen changes under it, or through a pause of more than a second, has to lift before it can press again.

## Power button

**PWR** belongs to the board's power circuit and is not a second programmable button. The board powers on by itself when USB is connected. A short press powers it on, and a long press powers it off. The firmware cannot see PWR, and this profile has no sleep or timed power-off of its own. Stop recording and wait for the save result before switching off or unplugging USB.

Do not hold BOOT while switching the board on. That starts download mode instead of Dragon Detector; press PWR again or reconnect USB without touching BOOT.

## Battery and charging

The battery connector takes a single 3.7 V lithium cell on an MX1.25 two-pin plug. Waveshare's documentation does not state the polarity in words. Its board photograph shows **+** and **−** printed beside the connector marked **VBAT**, and a speaker connector of the same shape next to it. Before connecting a cell that did not come with the board, find those marks on your own board and match the cell's wires to them.

The board has no fuel gauge. The battery percentage on the radar status line is an **estimate from the cell voltage**: 3.30 V reads as empty and 4.15 V as full. It reads high while charging and sags under load. A charge symbol appears while the charger reports charging, and the firmware lights the board's LED for the same state. A board with **no battery connected reads as full on USB**, because the charger holds the battery rail at its float voltage.

## Sound

The ES8311 codec and the onboard amplifier drive the board's MX1.25 **speaker connector**. A board with no speaker plugged in cannot produce sound; alerts are then visual only. With a speaker connected, RTTTL alerts, hunt tones and previews are played the same way as on the other speaker boards. Open **Settings → Sound level** or **Audio & hunt tones** and choose **High, Medium or Silent**; fresh installations start at High. [Audio controls and USB commands](USER-GUIDE.md#audio-alerts-and-foxhunting).

The two onboard microphones are not used. This firmware does not record audio.

## Storage

There is no card slot. The 8 MB flash holds one 4 MiB application slot and **3.94 MiB of internal storage**, shared by the badge, detection history, BLE Lab profiles, captures and recon sessions. The capture buffer is 1 MiB and a recon session is limited to 256 KiB, the same class as the Spotpear profile. **Utilities → Saved files** opens the internal capture library, and the browser's Radio tools, BLE Lab and Detector tools download saved files over USB.

Storage sizes are partition allocations, not free-space promises. New writes stop before storage is full, and updates never reformat it.

## Diagnostics and remaining checks

The USB command `143c status` (also `touch status`) prints four lines:

- `143C_STATUS`: flash and PSRAM size, free internal memory and its lowest value since start, panel clock, column offset and the number of completed display transfers.
- `TOUCH_STATUS`: controller found, chip ID, samples, I²C errors, the number of touches, how many of them were quick taps that ended between two screen updates (`replayed`), discarded late input (`stale`), input dropped because the screen changed under it or was dark (`dropped`) and the last position.
- `KEY_STATUS`: BOOT level, presses, screen steps, holds, timer clicks and dropped events.
- `BATTERY_STATUS`: percentage, millivolts, charging state and the raw charger line.

A new unit is checked only when a person has seen each of these on the device:

1. **Picture:** the round image is centred, upright, not mirrored, and the colours are right.
2. **Touch:** taps land where touched, including near all four edges, and a quick tap on the radar registers.
3. **BOOT:** the five-screen cycle, two quick presses as two steps, the hold for the main menu, **Later** on the clock prompt, and single and double clicks on a timer.
4. **PWR:** a long press switches off and a short press switches on.
5. **Speaker:** a sound preview at High is audible, and Silent stops it.
6. **Battery:** the percentage follows a connected cell, and the charge symbol and LED appear on USB.
7. **Clock:** set over USB, a setup prompt after power-off, and recovery over saved Wi-Fi.
8. **Storage:** record, save, restart and download a capture.
9. **Download mode by hand**, as described above.

A successful build or installation does not validate the physical board.

## Port reference

| Peripheral | Connection |
|---|---|
| CO5300 AMOLED, 466 × 466 | QSPI CS15, CLK14, D0 9, D1 10, D2 11, D3 12; reset13; TE8 unused; 40 MHz; column offset 8, panel mounted rotated 180° |
| CST820 touch | I²C SDA47/SCL48, address0x15; reset16, interrupt17; both axes mirrored to match the panel |
| ES8311 speaker codec | Same I²C bus, address0x18; MCLK38, BCLK39, LRCK40, DOUT41; amplifier enable46; codec supply enable18 |
| ES7210 microphones | Present on the same bus, data on GPIO42; not used |
| Battery | Voltage on GPIO4 through a 1:2 divider; charger status GPIO7, low while charging; LED GPIO5, low = lit, driven by the firmware while charging |
| Buttons | BOOT0; PWR is hardware-only |
| Flash layout | Application `0x10000`, 4 MiB; filesystem `0x410000`, size `0x3F0000`; no second application slot |

The hardware reference is [Waveshare's 1.43C example repository](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.43C/tree/8cea11ddae6167dd591d1f5465fd7b615080360f) at that revision and the board schematic linked from the [Waveshare documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43C), read on November 10, 2026. [Pin definitions](../firmware/include/board_pins.h) are authoritative for this build. Wiring and a successful build do not prove battery life, speaker loudness or field performance; the [release validation record](RELEASE-0.14.7.md) separates what was checked on a board from what was not.
