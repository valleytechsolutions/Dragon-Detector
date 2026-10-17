# Date and time setup

Use desktop Chrome or Edge on the [web flasher](https://valleytechsolutions.github.io/Dragon-Detector/). Check that your computer's date, time and timezone are correct before installing.

1. Select the exact board. Choose **Update** to preserve existing settings and user files.
2. Leave automatic local-time setup checked, connect the matching USB device, and install.
3. Keep USB connected. The installer closes automatically after successful flashing and starts clock setup on that same granted port. Wait for the date/time verification result before unplugging.
4. If USB re-enumerates, click **Finish clock setup**, select that running device, and wait for verification. Failed setup is explicitly reported and can be retried without reflashing.

Connecting **Detector tools** also sets and verifies the computer's current time and timezone. **Sync my local time** repeats this at any time. UTC offsets are fixed; resync after travel or a daylight-saving change. Firmware uses UTC internally and the saved offset for the displayed date/time.

## Hardware clock versus powered system clock

| Device | Time source | After shutdown |
|---|---|---|
| M5Stack StopWatch C152 | RX8130CE hardware RTC | Keeps time while its backup supply is powered |
| Seeed XIAO + Round Display | BM8563 hardware RTC | Keeps time while its RTC battery/supply is powered |
| Waveshare AMOLED **1.75C** | ESP32 system clock on tested hardware | USB sync or configured saved-Wi-Fi recovery required after full power-off |
| Spotpear | ESP32 system clock | USB sync or configured saved-Wi-Fi recovery required |
| Waveshare Lite | ESP32 system clock | USB sync required after full power-off |

The 1.75C schematic does not show the PCF85063 present on the **non-C** 1.75. The manufacturer introduction mentions an RTC, but its [C schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf) and direct I2C checks of the tested unit do not support promising a separate calendar RTC. The PMU's RTCLDO is only a supply output. Firmware still probes for an optional RTC; detected hardware is verified before any persistence claim.

In 0.13.10+, the flasher reads the hardware clock directly after setting it and checks again after 1.2 seconds to confirm that it advances. A missing RTC on StopWatch/XIAO, invalid/stale readback, or stopped oscillator fails setup. A correctly set software clock is reported separately with its power-off limitation. The firmware never restores an old saved timestamp and presents it as current time.

Powered software resets retain system time in 0.13.9+. Hardware reset/power cycling are different: without an independent powered RTC, elapsed off-time cannot be reconstructed offline. Keeping a software-clock board powered preserves time; complete battery discharge also loses it.

## Optional recovery over saved Wi-Fi

All four full boards support saved-Wi-Fi clock recovery; Lite does not. Follow [the network setup steps](SPOTPEAR-CLOCK.md#recover-automatically-using-a-saved-wi-fi-network) for any full board. The profile must be explicitly saved and selected for clock recovery. A network with Internet access is required. The firmware retries after a failed attempt; while offline after power loss it cannot know the current date/time. Wi-Fi secrets are entered on the flasher, not in chat.

## Diagnostic commands

`clock status` reports the system time and local offset. `clock hardware` in 0.13.10+ reports a direct RTC read as `CLOCK_HARDWARE detected=1 valid=1 epoch=...` when available; a software-only board returns zeros. `net time status` reports the selected Wi-Fi recovery state. A valid system time alone does not prove a hardware RTC is present.
