# 0.13.39 local hardware validation

Date: October 26, 2026. Development branch `codex/waveshare-gps-hardware`.
This record does not announce a public flasher release. Public firmware remains
0.13.38 until a separate deployment is performed.

## Hardware and recovery

The connected Waveshare **ESP32-S3-Touch-AMOLED-1.75** and **1.75-G** each reported
16 MiB flash and 8 MiB PSRAM. Complete factory flash images were read and verified
before installation. Subsequent development flashes write only the application
at `0x10000`, preserving NVS, other slots, internal files and SD contents. Private
backups, logs and radio/location captures are excluded from the repository.

The owner confirmed a clear display and working touch on the GPS board. Both
boards' RTCs were detected, held valid time through reboots and advanced in
readback. Complete power-loss/battery retention is not yet verified. The new
non-C speaker gain is lower, but physical audio validation remains pending.

## Real SD and GPS results

| Check | Result |
|---|---|
| Base Wi-Fi PCAP | 51,868 bytes / 129 records; offsets, CRC and PCAP record boundaries valid |
| Base BLE CSV | 26,062 bytes / 282 records; offsets, CRC and column counts valid |
| GPS Wi-Fi PCAP | 43,777 bytes / 120 records; offsets, CRC and PCAP record boundaries valid |
| GPS BLE CSV | 46,058 bytes / 501 records; offsets, CRC and column counts valid |
| GPS located WiGLE run | 705,387 bytes / 6,701 rows: 2,732 Wi-Fi + 3,969 BLE; CRC `e8c4d2bb`; two headers, 14 columns, valid timestamps and coordinate ranges |
| Located run stop | Queue drained, file finalized, returned to menu; zero late rows, dropped records and GPS transport errors |
| GPS fix | Antenna outside the window; 12 satellites at first checked fix, 8–10 in subsequent checks; valid RMC/GGA |
| Post-upload-build wardrive | At 77 seconds: 308 Wi-Fi + 517 BLE located rows, zero no-fix skips, late rows, drops or GPS errors; 28 completed sweeps |
| Post-update finalized CSV | 199,317 bytes / 1,906 rows: 701 Wi-Fi + 1,205 BLE; CRC `54169bef`; format/time/location checks passed; 177-second run had zero no-fix skips, late rows or drops |
| Radio schedule | Maximum observed sweep approximately 1,342 ms, 100 ms configured dwell, 1,500 ms BLE interval; zero scan errors in checked runs |
| Optional SD resources | Both boards uploaded/read back 1,270,072 bytes; SHA256 `66c709f56bc8d3c28f491003949a49c6a73958b50c9b66bcc51cd9dadf7c4bbe`; 54,282 entries loaded |
| USB upload recovery | Intentionally ignored an ACK; repeated cumulative ACK recovered transfer; full bytes/hash matched |
| Upload screen | Physical display capture checked; labels/buttons visible; blank SD configuration correctly blocks upload |

Counters describe observations, not globally unique devices. First acquisition
was stationary with the antenna outside a window, not a moving route test.
No invented locations were written during acquisition. The original run's
no-fix counter includes time waiting for acquisition; later runs began fixed.

LC76G recovery uses complete bounded command/reply transactions and shared-bus
reservation, preserves the bus clock and does not reset other peripherals.
Classic AMOLED radar backgrounds now cache in PSRAM; the earlier measured
redraw duration fell from roughly 195 ms to 126 ms for the checked classic theme.

## Automated checks

- `tools/test_core.py`: parser, tracking, clock, CSV and detector fixtures;
  150,000 fuzz inputs plus 50,000 tone fuzz inputs.
- `tools/test_gps_transport.py`: startup wake, interrupted transaction recovery,
  complete 128-byte responses, 9 KiB backlog, stale fixes, bus ownership,
  missing/short/oversized replies and timer rollover.
- `tools/test_wardrive_session.py`: conflicts, counts, deduplication, fresh fix
  publication during reads, asynchronous save-before-menu, SD failure and timing.
- `tools/test_wardrive_radio.py`: production asynchronous scan adapter, four-AP
  work budget, BLE interval, empty scans, timeouts/recovery, cancellation and restore.
- `tools/test_wardrive_upload.py`: production upload worker with fake HTTP/SD and
  real cJSON 1.7.18; bounded config/JSON, multipart partial writes, queued jobs,
  per-service receipts, crash recovery, cancellation, auth/rate errors, SD write
  failures and duplicate prevention.
- `tools/test_sd_records.py`: queue pressure, short writes, card loss/full card,
  segment finalization and 32 MiB rollover (fault simulation, not physical rollover).
- `tools/test_amoled175.py`: pins, partitions and RTC read/write/fault handling.

Builds passed for `waveshare_175_gps`, `waveshare_175`, `waveshare`, `watcher`
and `waveshare_lite`. Both connected non-C boards received verified application
updates. Other profiles were not rebuilt for this local development iteration.

The GPS build fits its 4 MiB app slot: 3,540,528-byte flashed image and 653,776
bytes remaining, above the configured 5% reserve. Its post-update wardrive had
about 26 KiB internal free heap and 4 MiB free PSRAM with the optional lookup pack.
Authenticated TLS heap/throughput has not yet been measured on hardware.

## Remaining validation

- Live authenticated WiGLE and WDGoWars uploads/imports with the owner's SD
  configuration. Native accepted/queued responses are fixtures, not service proof.
- Moving routes, reception loss/reacquisition, long sessions and power consumption.
- Physical 32 MiB rollover, full-card/removal faults, battery-backed RTC retention.
- Physical audio at the new gain, microphone/IMU features (still unimplemented).

See [hardware/SD guide](WAVESHARE-AMOLED-175.md) and
[upload configuration](WARDRIVE-UPLOADS.md) for the current controls and limitations.
