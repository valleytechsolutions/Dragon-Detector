# 0.13.40 local hardware validation

October 26, 2026. Branch `codex/waveshare-gps-hardware`. This is local development;
it does not announce a public flasher release. The preceding real located
Wi-Fi/BLE CSV evidence is recorded in [0.13.39 validation](VALIDATION-0.13.39.md).

## RTC finding

The owner confirmed both connected non-C Waveshare AMOLED 1.75 boards are powered
by USB only, with no battery. The standard board reported PCF85063 oscillator
loss after USB removal. Both RTCs accepted UTC synchronization, read back valid
time and advanced by two seconds across a two-second check. Hardware time was
within two seconds of the host; the saved local offset was UTC−04:00 for New York
on the test date. A fresh onboard GPS fix also successfully wrote the GPS board's
RTC through `gps clock sync`, with oscillator-loss clear on readback.

The schematic supplies VCC-RTC through AXP2101 RTCLDO. With neither USB nor battery,
there is no retention supply. Firmware cannot count elapsed time in that state.
0.13.40 restores an **unset** clock once a fresh validated GPS fix is available;
it does not overwrite an already-valid clock automatically. The manual touch
sync waits for a free I2C transaction slot and reports the result. Native tests
cover unset-clock recovery and preserving an already-valid clock. Battery-backed
shutdown retention remains untested because no batteries are attached.

After the owner reported reconnecting the GPS board, a read-only check found the
RTC valid, oscillator-loss clear, and time advancing two seconds over two seconds.
It was three seconds behind the host UTC clock; the saved offset remained −240
minutes. GPS reported a fresh fix using 20 satellites. No USB time-setting or
manual GPS-sync command was sent during this check. The observed uptime continued
from the latest flash, so the check establishes current clock/GPS health but does
not independently capture an invalid-RTC → automatic-recovery cold-boot transition.

## Optional external GPS on the standard board

The standard `waveshare_175` image now offers **SYSTEM → GPS setup**. It defaults
off, receives checksum-validated NMEA over UART1 without attaching a TX pin, and
accepts only the documented free header GPIOs. Settings are saved atomically in
NVS. The onboard `waveshare_175_gps` LC76G driver remains separate.

Physical checks on the standard board:

- Default off, RX18, 9600 baud; reserved GPIO19 rejected.
- Enabled RX18/115200 survived a software restart with the same saved values.
- Returned to RX18/9600 and disabled after testing; no module is wired yet.
- A 4.1-second no-fix wardrive observed 10 Wi-Fi and 14 BLE records, skipped all
  24 located rows and finalized a 287-byte header-only file with no late/dropped
  rows. No synthetic positions were supplied to the device.
- Both new 466×466 setup pages were captured with screenshot CRC verification
  and visually checked for legibility and placement.
- The onboard GPS reacquired a fresh fix reporting 20 satellites.
- With external GPS enabled, the physical standard board successfully began
  Wi-Fi joins, suspended UART, and restored it after both explicit disconnect
  and the 25-second join timeout. A second connection passed after correcting
  resident TCP/IP memory accounting. The temporary offline test network profile
  was removed, and external GPS was left off at RX18/9600.

The UART listener releases its ring buffer and task during network connections,
then resumes using its saved settings after disconnect/failure. Fix state is
cleared across the pause. The Wi-Fi memory check runs after the resident TCP/IP
initialization so subsequent joins use the same memory accounting as the first.
TLS buffers use PSRAM; allocation failures still stop the operation safely.

Actual reception from a user-wired external GPS is **not validated** by the native
UART fixtures. Module voltage, baud and NMEA output must match the
[external GPS guide](EXTERNAL-GPS-AMOLED175.md). Live authenticated WiGLE/WDGoWars
uploads and moving-route GPS performance also remain unverified.

## Automated validation

All native test programs below compile with assertions enabled (`-UNDEBUG`):

- `test_gps_uart.py`: default-off, all pin/baud combinations, atomic settings,
  invalid pins/encoding, busy/save failures, receive-only UART, live NMEA,
  bounded reads, overflow, stale fixes, suspend/resume and rollover.
- `test_gps_network.py`: GPS suspension across successful network ownership,
  repeated attempts, low memory and every network setup failure; restoration
  on disconnect, plus clock-only connection behavior.
- `test_gps_transport.py`: LC76G transaction, recovery and freshness behavior.
- `test_wardrive_session.py`: production lifecycle, validated GPS clock recovery,
  no overwrite of valid time, no-fix handling, dedupe, SD failure and safe exit.
- `test_wardrive_radio.py`: asynchronous Wi-Fi/BLE scanner ownership and errors.
- `test_wardrive_upload.py`: config, streaming HTTP/SD failures, response parsing,
  service receipts and explicit uncertain-upload retry handling.
- `test_amoled175.py`: C/non-C isolation, optional GPS capability, partition
  layout, RTC UTC round trips and I2C/oscillator failures.
- `test_sd_ui_gates.py`: capability/menu gates across all 12 profiles.
- `test_sd_resources.py`: validated resources, malformed packs and board gates.

The UART work exposed assertion elision in three recently added wardrive test
scripts. Their builds now explicitly use `-UNDEBUG`; the session, radio and
uploader fault tests all passed with assertions executing.

Builds passed for `waveshare_175`, `waveshare_175_gps`, `watcher` and `waveshare`
(1.75C), using the isolated Arduino 3.0.7 / IDF 5.1 package directory. Both non-C
applications fit the 4 MiB OTA slots. App-only writes at `0x10000` were verified
by esptool; factory backups, NVS, internal files and SD contents are preserved.
Private diagnostic logs and captures remain outside tracked/public content.
