# M5Stack Dial v1.1

Dragon Detector 0.13.27 is verified on **M5Stack Dial v1.1**, with 8 MB flash and no PSRAM. Display, touch, ring/front-button navigation, menus and radar actions passed physical checks. The hardware clock was set over USB and retained the correct time across a firmware restart without another sync. The original K130 shares this profile but has not been physically tested. Time retention while completely unplugged depends on the RTC power supply.

## Install

1. Open the [web flasher](https://valleytech-dragon-detector.pages.dev/#install) in desktop Chrome or Edge and choose **M5Stack Dial v1.1**.
2. Choose **Fresh installation** when replacing M5Stack's factory demo or another firmware. This replaces saved files/settings. Use Update only for this Dial profile’s 0.13.26 or newer partition layout.
3. Connect with a USB data cable. If no programming port appears, hold the rear **G0/BOOT button on the Stamp-S3** while connecting/resetting, then release it. The large front button is not BOOT.
4. Leave automatic clock setup enabled. After flashing, restart/reconnect if prompted; keep USB connected until the hardware clock is verified.
5. The combined `factory-0x0.bin` beside the flasher is an alternative for esptool at offset **0x0**. It is a factory image, not a settings-preserving update.

No SD pack or external assets are required. The Dial has no onboard SD slot. Built-in artwork lives in firmware; a blank internal filesystem image is included in fresh installation. Do not flash the StopWatch, XIAO or Waveshare image onto this board.

## Controls

Turn the ring to cycle **Radar → Clock → Stopwatch → Signals → Status**; reverse to go back. Tap the visible **MENU** button on any page, or press the front button on Radar, Clock or Status, to open the menu. Turn the ring to highlight an option, then press to select it. Menu rows also respond to touch.

The main menu contains **Radar, Clock, Tools, Themes, Settings and Close**. Tools opens the stopwatch/locksport timer, signal details or device status. Settings has separate **Time** and **Date** editors, brightness and alert sound. Themes change only when selected in the Themes menu. Hold the front button about 0.8 seconds to go back in a menu; outside menus, holding stops hunting and returns to Radar.

| Screen | Front button | Touch |
| --- | --- | --- |
| Radar | Press to open Menu | Tap a dot for Details, Hunt/Stop and Close; tap the same dot again or empty radar space to clear it |
| Clock | Press to open Menu | Tap MENU for navigation |
| Stopwatch | Press to start/pause; double press to lap (finish in locksport mode), or reset when paused | Tap the timer digits to start/pause; tap MENU for navigation |
| Signals | Press for next device; double press to start/stop hunting | Tap the screen to start/stop hunting the selected device |
| Status | Press to open Menu | Tap MENU for navigation |

A selected radar tag stays selected until dismissed or removed from the contact list. Contact identities, not changing list positions, anchor selection. **Details** opens family, name, radio type, MAC address, RSSI and age. **Hunt/Stop** controls signal-strength hunting; **Close**, another tap on the same dot, or a tap on empty radar space clears the selection. Turn the ring to choose an action and press to activate it, or tap the action directly. Closing the selection panel does not stop an active hunt. Notifications appear below the radio status and can be dismissed with a tap.

If a contact is evicted while its details are open, the page says **SIGNAL EXPIRED**. Press to select another. Hunting is signal strength only: dot position is not a compass bearing or measured distance. A target that has not been heard for 30 seconds cannot start a new hunt. Hunt chirps stop after three seconds without reception. Hold the front button to stop hunting and return to Radar, or double press on the Signals page (USB: `dial hunt stop`).

Clock and Stopwatch pause radio scans to keep time screens quiet. They restore the previous radio/hunt state when leaving. Stopwatch timing is software based and is not retained across power loss. Status mute affects detection melodies; hunt chirps are controlled by starting/stopping the hunt.

## RTC and power

The BM8563/RTC8563 is read at startup. Firmware rejects stopped/test-mode clocks, invalid dates and the oscillator-loss flag; it never overwrites a valid RTC merely because the processor restarted. USB time setup writes UTC, reads it back and reports failure if the chip is missing or the write failed. Timezone offset is saved separately. This profile does not include Wi-Fi time setup; reconnect USB to update the timezone/DST offset.

To adjust time on the device, open **Menu → Settings → Time**. Select Hour, Minute or Second, turn the ring to adjust it, then press to finish editing that field. Select **Save time** to write the RTC. Use **Settings → Date** for Year, Month and Day, then **Save date**. Saving one editor preserves the other part of the current clock. Cancel or hold to leave without saving. A failed RTC write stays in the editor and displays an error.

GPIO46 is asserted before the USB startup delay to hold the main power supply on. An RTC can only retain time while its RTC supply remains powered. Disconnecting USB with no remaining supply is not a retention guarantee. This profile does not add a software power-off command or claim a backup-cell retention duration. Verify the actual board's power arrangement against M5Stack's documentation.

For the first physical test: set time once, check `clock hardware`, restart and check it again **without syncing**, then test the intended battery/external-power shutdown arrangement for at least three minutes. Compare the elapsed RTC seconds before allowing automatic sync to mask a loss. `clock diagnostics` exposes I2C/control/oscillator-loss status. A valid read-back during installation does not prove off-power retention.

## Scope and memory

The Dial uses the same passive detection signatures and family priorities as the main firmware, with a bounded **32-contact pool** and **16 packet buffers**, four reserved for Axon traffic. High-priority camera/drone/device families remain ahead of tracking tags. Reception is time-shared between 2.4 GHz Wi-Fi and BLE; it cannot receive both continuously. Congested environments may drop observations, which are counted in Status.

Included: touch radar, unique device colors, priority labels, signal details, RSSI hunting, nonblocking buzzer melodies, 40 built-in radar/clock themes, stopwatch/locksport timer, RTC and USB diagnostics. An 11.5 KiB display tile and one indexed artwork image at a time avoid depending on PSRAM.

Not included in this compact profile: active BLE/GATT toolkit, FindMy beeping, active Wi-Fi tools, badge/personal-theme uploads, audio recording, file browser, persistent captures/history, wardriving, GPS, SD support or RFID reader operations. These controls stay unavailable in USB Studio. Firmware detection cannot prove a device is recording; the shared evidence/confidence rules still apply.

## Hardware mapping

| Peripheral | Pins / address |
| --- | --- |
| GC9A01 240×240 LCD | MOSI5, SCLK6, CS7, DC4, RESET8, backlight9 |
| I2C | SDA11, SCL12 |
| FT3267 touch | 0x38; polling, native 240×240 coordinates |
| RTC8563/BM8563 | 0x51 |
| Encoder | A41, B40 |
| Front push | GPIO42; active low |
| Power hold | GPIO46; active high |
| Piezo buzzer | GPIO3 |

RFID at 0x28 is intentionally unused. No calibrated battery percentage is displayed.

## Checks for a new unit

- Check the boot splash, display colors/orientation, all screen edges and touch dot selection after release.
- Rotate both directions slowly and quickly; confirm one page per deliberate step and front-button tap/double/hold behavior.
- Verify automatic UTC/offset setup, read-back, restart and power-retention tests above.
- Run crowded Wi-Fi/BLE scanning for at least 30 minutes; collect `status`, `memory status`, `scan status`, `dial status`, `touch status`, `rotary status` and `loop diagnostics` if it stalls.
- Cycle every built-in theme, including animated clocks, then return to Radar and check scan counters resume.
- Confirm detection melodies, mute, fresh-target hunt chirps, missing-target silence and stopping a hunt.

## Sources

Pin/power/memory mapping: [M5Stack Dial v1.1 documentation](https://docs.m5stack.com/en/core/Dial_v1.1), [original Dial documentation](https://docs.m5stack.com/en/core/Dial), [official schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/499/Sch_M5Dial.pdf), and [M5Dial source](https://github.com/m5stack/M5Dial/tree/533c9d0198e72621d55a4d35f726d3d53a7e1431). Native TFT_eSPI/LVGL rendering and checked RTC register handling are integrated into Dragon Detector's compact profile.
