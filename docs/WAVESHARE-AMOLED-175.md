# Waveshare AMOLED 1.75 — experimental port

This build targets **ESP32-S3-Touch-AMOLED-1.75**, the 16 MB non-C board. It is prepared from Waveshare's schematic and examples and has passed compilation and automated checks. Display, touch, sound, card access, power controls and RTC retention still need testing on a physical unit.

Select **Waveshare AMOLED 1.75 · Experimental** in the [web flasher](https://valleytech-dragon-detector.pages.dev/#install). Do not select the separate **1.75C** profile: its reset pins and flash layout differ. The standard board and the enclosure-only **-B** use the same hardware profile. The **-G** GNSS functions are not integrated.

## Included

- The full 466 × 466 interface, Dragon Detector boot animation, all 35 themes including Can Commander, logo clocks and optional personal theme.
- Radar, signal tracking, badge, notebook, stopwatch and locksport timers. Clock and timer pages pause background scanning.
- Shared radio tools, BLE Lab, 2 MiB capture buffer and 2 MiB recon sessions.
- PCF85063 clock initialization, UTC storage, checked writes and startup restoration; separate Time, Date and Sync settings.
- Optional FAT32 microSD logging, SD PCAP and file browser. Internal captures and profiles use the shared flash filesystem, independently of the card.
- ES8311 speaker alerts and battery telemetry through AXP2101.

There is no microphone recording, IMU gesture/step feature or GNSS integration in this port. A successful build does not establish hardware compatibility.

## First installation

1. Connect the board using a USB data cable and open the flasher in desktop Chrome or Edge.
2. Select the **1.75 experimental** board and **Fresh install** for the factory demo or another firmware. This replaces files and settings. **Update** is only for this exact Dragon Detector profile, version **0.13.19 or newer**.
3. Leave automatic time setup enabled. Check the computer's date, time and timezone first.
4. Install, keep USB connected through reboot and wait for **Hardware RTC readback and ticking verified**. If USB reconnects under a new port, use **Finish clock setup**.
5. Select Can Commander or another design under **Settings → Theme & radar labels**.

For download mode, hold **BOOT**, connect USB and release BOOT. If necessary follow Waveshare's [flashing instructions](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75/Firmware-Flashing). The official [factory firmware](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75#firmware-quick-start) is available for recovery.

## Clock retention

The external RTC can keep time while its supply remains powered through the board's battery/power circuit. USB-only operation cannot keep time after every power source is disconnected. Battery-backed retention remains to be measured on the arriving unit. A depleted/disconnected battery can require another synchronization.

Firmware does not reset the RTC to a build date or an old saved timestamp. An invalid oscillator/date is rejected. A missing RTC fails clock setup rather than claiming persistent time. USB setup writes UTC, reads it back, and confirms that seconds advance; the stored local offset controls the display. Resync after a timezone or daylight-saving change. Optional saved-Wi-Fi recovery is also available. See [clock setup](CLOCK-SETUP.md).

## Wiring and storage

| Function | Non-C profile |
|---|---|
| CO5300 QSPI | DATA0–3: 4/5/6/7, CLK: 38, CS: 12, reset: **39**, column offset: 6 |
| CST9217 | SDA: 15, SCL: 14, interrupt: 11, reset: **40**, address: 0x5A |
| PCF85063 / AXP2101 | Shared I2C, addresses 0x51 / 0x34 |
| Speaker | **MCLK: 42**, BCLK: 9, WS: 45, DOUT: 8, amplifier control: 46 |
| SDMMC, 1-bit | CLK: **2**, CMD: **1**, D0: **3** |
| Controls | BOOT: GPIO0; short PWR: AXP2101 key event |
| Flash / PSRAM | 16 MB / 8 MB |
| OTA app slots | Two 4 MiB slots at 0x10000 and 0x410000 |
| Shared internal files | 0x810000, 7.94 MiB; no second filesystem above 16 MB |

The older Arduino `pin_config.h` contains conflicting MCLK definitions (16 and 42). This port follows the maintained BSP and schematic: **GPIO42**. GPIO16 is an expansion pin on the non-C board. The C profile retains its existing wiring. PMU supply voltages and charge-current settings are left at the board defaults.

## Arrival checklist

- Confirm the exact model and 16 MB flash; keep the factory recovery image available.
- Check the boot logo, all touch edges and four orientations. Verify BOOT start/pause and short-PWR lap behavior.
- Run USB `clock hardware` twice, confirm advancing seconds, then test a reset and a battery-backed USB disconnect. Separately test total power loss and recovery.
- Check radar scan counters, then verify clock/timer standby stops scanning and returning to radar resumes it.
- Test a FAT32 card, file browser, a short SD log/capture and internal capture export.
- Test a quiet speaker preview, USB/battery telemetry and a longer clock/timer run for stability.

## Sources

- [Official product documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75).
- [Waveshare hardware reference](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/e4344e70c2fa78a13e8a06566507f1ba8af6672a/HARDWARE_REFERENCE.md).
- [Official maintained BSP](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/e4344e70c2fa78a13e8a06566507f1ba8af6672a/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c).
- [Schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75.pdf).

Reference revision: `e4344e70c2fa78a13e8a06566507f1ba8af6672a`. The existing shared board adapter and original application code are used; vendor example applications are not bundled into the firmware.
