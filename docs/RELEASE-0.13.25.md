# Dragon Detector 0.13.25

Fixes hardware clock retention and validation. StopWatch now enables and checks the M5PM1 L1 power hold so its RX8130 clock remains supplied during normal shutdown while the battery has charge. It distinguishes oscillator loss from a backup-voltage warning and restores system time only from a validated calendar.

XIAO Round and RTC-equipped Waveshare profiles also receive startup/save recovery checks. Explicit time setting exits chip test/stop modes, validates calendar data, and verifies hardware readback. The Waveshare oscillator load setting is preserved. All hardware-clock profiles retry transient startup reads and re-probe a previously missing RTC when synchronizing.

Choose **Update** for your exact board, then synchronize once if needed. Settings and user files are preserved. Themes, radar controls and detection behavior are unchanged. Hardware clocks still require power from their battery or backup supply; software-clock boards cannot retain elapsed time through complete power loss.

See [clock setup and retention testing](CLOCK-SETUP.md) for supported boards and the new `clock diagnostics` command. XIAO and Waveshare changes require physical off-time tests on those boards; their existing hardware support status remains unchanged.

Validation: StopWatch hardware save/read/tick checks, user-confirmed unplugged shutdown for at least three minutes, and direct post-shutdown/restart reads without USB or Wi-Fi synchronization. Native tests exercise the production RX8130, BM8563 and PCF85063 drivers, including lost-clock flags, calendar boundaries, power-hold readback and I2C failures. All 105 web tests and native core tests passed.

The subsequent [LILYGO 1.75 beta profile](LILYGO-AMOLED-175.md) preserves the owner's working 0.13.20 display/touch configuration and flash layout, with checked PCF8563 RTC support and SPI microSD. It has a separate build/source commit; the eight original RTC packages remain unchanged. The addition passes its firmware build, hardware pin/layout checks, native PCF8563 fault tests and 108 web tests. Its new RTC shutdown retention and SD behavior still need physical confirmation.
