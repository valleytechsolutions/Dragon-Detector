# Dragon Detector 0.14.6 · Waveshare 1.75C clock retention

The smaller-button shutdown added in 0.14.5 cut power to the ESP32 clock, leaving
the **32 MB Waveshare AMOLED 1.75C** with unset time at the next start. Version
0.14.6 changes that action to deep sleep so the clock continues advancing while
the display and radios are off.

Hold the smaller **BOOT** button for at least two seconds, then release it to
sleep. Short-press the larger **PWR** button to wake. A charged battery or
continuous USB power must remain connected. The touch controller and audio
output are also disabled during sleep.

Short presses keep the existing controls: larger PWR cycles
**Radar → Tools → Badge → Clock → Radar** and starts/pauses the stopwatch;
smaller BOOT records Lap / OPEN on the timer and opens Tools elsewhere.
The long hold does not also record a lap or change screens.

Sleep is blocked while a timer, recording or radio session is running, while
saving, or while a dialog is open. Pause the timer and finish those operations
first. Wake restarts the firmware, so paused timer results and laps are cleared.

Holding the larger PWR button still performs hardware power-off and loses the
1.75C's system time. Complete loss of battery/USB power has the same effect.
Use USB synchronization or configured saved-Wi-Fi recovery afterward. The
internal sleep oscillator can drift; USB synchronization refreshes time and
the local offset. Firmware does not present a timestamp saved before power
loss as current time.

Use **Update** in the
[web flasher](https://valleytech-dragon-detector.pages.dev/#install) to preserve
saved files and settings, and leave automatic clock setup enabled. Other board
downloads are unchanged.

## Validation

On November 8, 2026, the application was installed on the 32 MB 1.75C with
write-hash verification. Two 15-second timer-wake tests retained the date,
time and UTC offset without synchronization after wake. Including startup and
USB reconnection, the device clock advanced 21 seconds against host intervals
of 20.97 and 21.04 seconds. Diagnostics reported deep-sleep resets, working
display/touch and PMU initialization, and all 23 self-tests passing.

Host tests pass for navigation, timing, sleep guards, wake/display failure
recovery, debounce and counter rollover, including unchanged non-C controls.
RTC-driver and 1.85B controls regressions pass, along with 19 web clock-setup
and verification tests.

The battery-powered smaller-button sleep and larger-button wake check is
pending. Multi-hour sleep drift and battery consumption have not been measured.

[Clock setup](CLOCK-SETUP.md) ·
[Clock and timer controls](STOPWATCH-CLOCK-ORIENTATION.md) ·
[0.14.5 shutdown change](RELEASE-0.14.5.md)
