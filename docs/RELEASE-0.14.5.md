# Dragon Detector 0.14.5 · Waveshare 1.75C power button

Adds shutdown to the smaller side button on the **32 MB Waveshare AMOLED 1.75C**,
while retaining the navigation and stopwatch mapping from 0.14.4.

Hold the smaller **BOOT** button for at least two seconds, then release it to
power off. A long hold does not also record a lap or open Tools. Short presses
still record Lap / OPEN in the timer, or open Tools elsewhere. The larger **PWR**
button still cycles **Radar → Tools → Badge → Clock → Radar** and starts/pauses
the stopwatch.

The smaller button is connected to ESP32 GPIO0. Shutdown is requested only after
release so that a subsequent start does not see a held download-mode strap.
After a full shutdown, use the larger **PWR** button to turn the device on;
the smaller button cannot operate while the ESP32 is off. See the
[Waveshare schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf).

Stop recording and active radio sessions before powering off. The firmware
leaves power on if storage is busy, a radio session is active, or a confirmation
dialog is open. If the power-management chip does not complete shutdown, the
display reports the failure and restores the prior scanning state.

Full power off clears RAM-only timer results and requires USB or saved-Wi-Fi
clock synchronization on the tested 1.75C. Use **Update** in the
[web flasher](https://valleytech-dragon-detector.pages.dev/#install) to preserve
saved files and settings. Other board downloads are unchanged.

## Validation

Host tests cover short presses, holds, release before shutdown, startup-held
BOOT, debounce, the two-second boundary, timer rollover, busy storage, active
recording, confirmation dialogs and a failed PMU shutdown. The existing 1.75C
navigation/stopwatch tests, non-C hardware-profile tests and 1.85B controls
regression tests also pass.

On November 8, 2026, the 0.14.5 application was installed on a 1.75C with write-hash
verification and all 23 startup self-tests passing. The owner confirmed that
holding and releasing the smaller button turns the display off, and the larger
PWR button powers it back on. USB diagnostics after restart confirmed 0.14.5,
working display/touch and PMU initialization. The software clock was unset after
full power loss and was synchronized again over USB, as documented above.

[Clock and timer controls](STOPWATCH-CLOCK-ORIENTATION.md) ·
[0.14.4 navigation update](RELEASE-0.14.4.md)
