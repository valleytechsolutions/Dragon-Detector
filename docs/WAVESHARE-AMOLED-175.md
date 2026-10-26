# Waveshare AMOLED 1.75 and 1.75-G — SD and GPS

Local development **0.13.40** adds **SYSTEM → GPS setup** to the standard 1.75:
optional NMEA UART input, saved RX/baud controls and GPS clock recovery.
It starts disabled. See [external GPS wiring and setup](EXTERNAL-GPS-AMOLED175.md).
The -G continues to use its onboard LC76G and can also restore an unset clock
from a fresh fix. Neither RTC can count through USB-only total power loss without
a battery supply.

Version **0.13.39** has been bench tested on both 16 MB **non-C** models.
Factory flash backups were read and verified before installation. Wi-Fi PCAP and
BLE CSV captures from both cards passed byte-count, CRC and format validation.
The GPS model's display and touch were confirmed by the owner; its LC76G now
delivers valid NMEA with no transport errors in the bench run. With the antenna
outside the window it acquired a fix (12 satellites at one observation), and
6,701 located records passed SD readback checks. A moving route remains a separate
field test. See the
[validation record](VALIDATION-0.13.39.md) for the precise scope.

Choose `waveshare_175` for the standard board or enclosure-only **-B**;
choose `waveshare_175_gps` only for **-G** hardware with an onboard LC76G.
The **1.75C** remains a different 32 MB profile with different reset pins.
The original display, touch, RTC, SD pins and 16 MB partition layout are preserved.

## SD card setup

Use a FAT32 card, insert it with power off, then boot. **No downloaded SD pack is
required.** Firmware creates missing directories and never formats the card.
Core radar, clock, badge and lab functions still boot without one.

With a card present at boot, recon storage uses `/dragon` on SD:

| Card directory | Contents |
|---|---|
| `/dragon/captures` | Radiotap Wi-Fi PCAP; raw BLE advertisement CSV; saved network/portal/CSI exports |
| `/dragon/profiles` | CRC-checked DDG1 BLE profiles |
| `/dragon/events` | BLE Lab event CSV; current/previous, up to 16 MiB each |
| `/dragon/surveys` | Explicit Wi-Fi inventory snapshots, including raw SSID bytes |
| `/dragon/missions` | Named RCN1/PRB1/REL1 recon sessions and manifests |
| `/dragon/wardrive` | GPS profile's WiGLE CSV segments |
| `/dragon/wardrive-uploads` | Separate upload receipts for WiGLE and WDGoWars |
| `/dragon/lists`, other tool directories | SSID lists, templates and saved tool data |
| `/dragon/session-*` | Optional detection event log and session information |

Badge images, settings, clock state and detection history keep their existing
internal storage. Existing internal profiles are preserved, not automatically
moved or overwritten. Export/import a profile, or place its checked `.ddg` file
in `/dragon/profiles`. Boot without SD to access the previous internal workspace.
Storage is selected at boot; reboot after inserting or replacing a card.

**Capture studio:** choose Wi-Fi or BLE, then Stop. On SD, records stream through
a 64 KiB PSRAM queue into 32 MiB segments and Stop finalizes the last file. There
is no 15-minute RAM-capture limit for SD streaming. Filenames include a random
session identifier and segment number; existing names are never overwritten.
Without SD, the existing 2 MiB RAM recorder remains: Stop, then Save.

Radio callbacks never write to the card. The application drains complete records
in bounded batches, flushes roughly every second, counts queue drops, and reserves
32 MiB of free space. Starting a segment also requires room for its 32 MiB maximum.
A slow/full/removed card can still lose records: counters report drops and write
failures stop recording. Incomplete files retain `.part` for recovery; they are not
listed as finalized captures. Sudden power loss can lose queued/unflushed data and
can damage FAT metadata. Stop and wait for **Saved** before card removal or shutdown.

Recon sessions on SD can run up to **8 hours / 64 MiB**, with 512 saved session
folders; internal storage retains the earlier limits. Wi-Fi surveys hold up to
**128 APs / 256 clients** on these two profiles. Use **Radio tools → Survey / select
network → Save Wi-Fi survey**, or a named recon session for continuous observations.
These ordinary surveys are not GPS exports.

## GPS wardriving

On the **-G GPS build**, open **TOOLS → RECON → START WARDRIVE**. One tap starts
the session and opens a dedicated dashboard. **Radio tools → GPS wardrive** is
also available for reviewing status before starting.
A FAT32 SD card is required for this recording mode. Take the unit outdoors and
wait for **GPS: FIX**. Start is permitted while waiting, but rows are written only
with a usable fix. No-fix and delayed observations are counted and skipped;
there are no fabricated coordinates or stale fixes carried through an outage.

Wardriving alternates asynchronous active **2.4 GHz Wi-Fi channels 1–11** scans
(100 ms maximum dwell per channel) and **1,500 ms passive BLE** listening after
each Wi-Fi sweep. This scheduling follows the Piglet reference workflow while
adding BLE between sweeps. AP scan results are drained in batches of four per
application loop and released; each AP counts once per sweep. BLE repeats use a
1,500 ms per-address cooldown and a bounded 512-entry cache. There is no claim of
simultaneous Wi-Fi/BLE reception or one unique device per counter increment.
Three consecutive Wi-Fi scan failures reset its STA control state; each scan
also has an eight-second deadline. Other lab/transmit/network/hunt sessions must stop first.
Detection analysis, history writes, alerts, audio, badge traffic and other tool
loops are suspended while wardriving. The dashboard updates only changed labels,
roughly three times per second, rather than running the animated radar. Firmware commands
and touch navigation cannot start another radio tool until recording stops and
SD saving completes. Stop restores the previous radio configuration. Nothing
starts wardriving automatically at boot.

The **WiFi** and **BLE** counters show received observations, not unique devices.
**Located rows** separately counts observations accepted into
the SD queue after fresh-fix, age and per-address cooldown checks. The screen
also shows elapsed time, GPS/satellites, bytes written, queued bytes, drops and
late observations. Waiting for a fix still shows radio activity, but does not
write invented locations. Queue acceptance does not itself guarantee durable
storage; check the SD counters and finish the session.

Tap **END & RETURN TO MENU**. The button switches to **SAVING** until queued
records are drained, the file is flushed/closed and its `.part` name is finalized.
Only then does the menu return. BOOT/Home requests use the same path. A write
or finalization error stays visible on the dashboard, with the incomplete `.part`
file retained; it is never presented as a successful save. Pause an active timer
and stop other radio sessions before starting.

LC76G uses existing I²C pins **SDA15/SCL14**, command address **0x50**, read
address **0x54**, and wake-write address **0x58**. Transactions temporarily use 100 kHz and restore the shared bus
clock. Reads are bounded, with cooperative waits and error backoff; no second I²C
driver, expansion UART assumption or expander-wide reset is introduced. The GPS
FIFO is drained completely for valid lengths up to 16 KiB; backlog parsing uses
the latest 2 KiB and pauses located rows until that backlog read finishes. Normal
reads can use the previous complete, still-fresh fix; partial sentences never
publish a new fix. Each response
is consumed in full with its own command, at most 128 bytes. Other shared-bus
devices wait through the short command/reply interval. Startup recovery handles
a transmitter left full/asleep during flashing; repeated failures use bounded
backoff and recovery. These paths have native fault tests and real-board NMEA
verification. Initial acquisition is verified; moving routes, loss/reacquisition
and long operation still need field testing.

The independent parser validates NMEA checksums, RMC date/position and GGA fix
quality, rejects dead reckoning/manual/simulated fixes, requires matching UTC
seconds and expires fixes after three seconds. Repeated frozen RMC seconds do not
renew freshness. WiGLE **1.6 UTF-8 CSV** includes MAC, name, advertised security,
GPS UTC, channel/frequency, RSSI, coordinates, altitude and `WIFI`/`BLE` type.
BLE channel/frequency and metric accuracy are **0 (unknown)**; HDOP is not falsely
reported as metres. Times use the latest fresh GNSS second. Uploads require the
user's own service credentials. No automatic uploads, Bluetooth Classic or
5/6 GHz scanning are included.

Stop and wait for saving, then read `/dragon/wardrive` with a card reader, or
choose a finalized `wigle-*.csv` from the web flasher's capture downloads. USB
exports check byte count, chunk offsets and CRC. The device library shows up to
32 finalized files; use the SD browser/card reader for larger archives. No live
authenticated service upload or moving hardware route test has been performed.

After saving, open **GPS wardrive → UPLOADS** to choose **WiGLE** or **WDGoWars**.
The firmware creates a blank `/wardriver.cfg` on SD only when absent. Fill its
home Wi-Fi and API fields locally, using the same names as Piglet. Separate
service receipts preserve both upload destinations, pending jobs and uncertain
results across reboots. CSVs are retained. See [upload setup and behavior](WARDRIVE-UPLOADS.md).

USB commands: `wardrive start`, `wardrive status`, `wardrive counters`,
`wardrive radio`, `gps transport`, `gps reception`, `gps status`, `wardrive stop`, `wardrive end`, `screen wardrive`,
`recon files`, `recon get <filename>`. `wardrive end` uses the dashboard's
save-and-return behavior. Bulk USB transfers are blocked during a session.

## Tap a device

One tap on a radar dot displays a color-coded device-family/RSSI summary and
selects its tag. Two taps on the **same device within 450 ms** open Device Details.
A later tap remains a summary; tapping empty radar clears selection. The summary
can also be tapped to open details. Contact keys include radio/address type, so
cache eviction cannot silently substitute a different target.

Details provides Hunt and target controls. BLE devices also get **BLE Toolkit**
and **GATT Clone Tools**. The latter guides discovery, profile save and emulation;
the workspace must match the selected device. Connecting and emulating remain
explicit actions. GATT profiles reproduce supported services/values, not protected
keys or proprietary device behavior. Axon retains its primary tag even while a
tracker is selected. Existing family colors and tracker priority are preserved.

## Installation and scope

Use the exact matching board package. A firmware-only update preserves internal
files/settings; a fresh install replaces internal files/settings, never SD files.
The GPS target has its own manifest identity and minimum version 0.13.21.
Both profiles retain the full theme collection, custom designs, badge, RTC clock, timers, BLE Lab,
notebook, detection, recon and existing radio tools. This extends the shared tool
suite; it is not every feature of Marauder or ChimeraBLE. Microphone recording and
IMU gesture/step functions remain unimplemented.

For download mode, hold BOOT while connecting USB. The official factory image
remains available in Waveshare's repository for recovery. Synchronize time through
USB as before. GNSS logging uses GNSS UTC. From 0.13.40, a fresh fix restores an
unset RTC clock, and **GPS setup → SYNC CLOCK FROM GPS** requests an explicit sync.

## Clock retention

The external RTC can keep time while its supply remains powered through the board's battery/power circuit. USB-only operation cannot keep time after every power source is disconnected. A dedicated battery-retention test is still needed. A depleted/disconnected battery can require another synchronization.

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

## New-feature field checklist

- Confirm the exact model and 16 MB flash; keep the factory recovery image available.
- Check the boot logo, all touch edges and four orientations. Verify BOOT start/pause and short-PWR lap behavior.
- Run USB `clock hardware` twice, confirm advancing seconds, then test a reset and a battery-backed USB disconnect. Separately test total power loss and recovery.
- Check radar scan counters, then verify clock/timer standby stops scanning and returning to radar resumes it.
- Test an empty FAT32 card, no-card boot, Wi-Fi/BLE captures, 32 MiB rollover, USB CRC download and a full-card stop.
- On -G hardware, test outdoor acquisition, a walking/driving route, loss/reacquisition of fix, and WiGLE import. Confirm touch and RTC continue working during GPS reads.
- Confirm Start blocks conflicting lab/transmit modes, Home stops and finalizes, and the previous radio mode resumes.
- Tap/double-tap neighboring dots, including a crowded tracker scene; verify the chosen GATT/Hunt identity.
- Test a quiet speaker preview, USB/battery telemetry and a longer clock/timer run for stability.

## Sources

- [Official product documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75).
- [Waveshare hardware reference](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/e4344e70c2fa78a13e8a06566507f1ba8af6672a/HARDWARE_REFERENCE.md).
- [Official maintained BSP](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/e4344e70c2fa78a13e8a06566507f1ba8af6672a/firmware/brookesia/components/waveshare__esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c).
- [Schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75.pdf).
- [Hamspiced/Piglet](https://github.com/Hamspiced/piglet/tree/fb71eded9c7bdec6b646f14dff3711ff4bd1bf5a): scan scheduling and SD-configured upload behavior reference; no source copied.

Reference revision: `e4344e70c2fa78a13e8a06566507f1ba8af6672a`. The existing shared board adapter and original application code are used; vendor example applications are not bundled into the firmware.
