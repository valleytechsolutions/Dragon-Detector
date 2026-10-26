# External GPS on the standard Waveshare AMOLED 1.75

Available in local development **0.13.40** for the non-C **1.75 / 1.75-B**
(`waveshare_175`). No GPS is assumed to be fitted: the option is **off by default**.
The **1.75-G** keeps its onboard LC76G driver and does not expose these UART settings.
The **1.75C** is a different board and is not included in this option.

## Wire the module with power disconnected

The receiver must output checksum-valid **NMEA RMC and GGA** sentences over a
**3.3 V UART signal**. Binary-only receivers need to be configured for NMEA first.
The firmware receives data only; it sends no module configuration commands.

| Connection | Default wiring |
|---|---|
| GPS TX / data output | Waveshare **GPIO18**, H2 expansion **pin 7** |
| GPS GND | Waveshare **GND**, H2 **pin 2** |
| GPS power input | Supply the voltage/current specified for that module |
| GPS RX / data input | Leave unconnected |

H2 pin 3 is the board's 3.3 V rail; H2 pin 1 is USB VBUS. Use either only when
compatible with the module's power rating and the board's supply limits. VBUS
availability depends on USB power. A module that accepts 5 V power must still
provide a 3.3 V UART output to the ESP32; GPIO is not 5 V tolerant. External
regulated supplies need common ground and must not backfeed the board rails.
Follow the H2 numbering/labels in the schematic for your revision.

The selectable receive pins are deliberately limited:

| GPIO | H2 pin |
|---:|---:|
| **18**, default | **7** |
| 17 | 6 |
| 16 | 8 |
| 44 | 4 |

Native USB GPIO19/20, SD, display, touch and audio pins are not accepted. GPIO43
is not offered because UART0 can emit console output there. No transmit GPIO is
attached by this feature.

## Configure on the device

Open **TOOLS → SYSTEM → GPS setup**. The RECON tab's top button also opens GPS
setup while external GPS is disabled. **Radio tools → GPS setup** is another route.

1. Tap the RX button to match the pin you wired; GPIO18 is recommended.
2. Tap BAUD to select the receiver's actual output rate. Choices are **9600,
   19200, 38400, 57600, 115200 and 4800**. The initial choice is 9600; this is not
   a claim about every module's factory default.
3. Turn **EXTERNAL GPS: ON**. Each setting is saved in device preferences.
4. Confirm **GPS DATA**, then **GPS FIX** outdoors with a suitable antenna.
   Bytes alone are not a fix: correct checksums, fresh UTC/position and GGA fix
   quality are required. A wrong baud usually produces no valid NMEA/fix.
5. With FAT32 SD inserted, use **START WARDRIVE**. The normal WiFi/BLE dashboard,
   WiGLE CSV logger, clean End/save behavior and upload screen are available.

Stop recording and uploads before changing receiver settings. Disabling GPS
releases the UART and clears the last fix. Reconfiguring the receiver clears old
data so an earlier position cannot be reused under the new settings. Driver
overflow discards stale input; expired/missing fixes pause located rows.
The UART listener pauses during Wi-Fi connections (including uploads and Wi-Fi
clock recovery) to release internal RAM. It resumes on disconnect or connection
failure, with the same saved settings and no stale fix carried across the pause.

The GPS-only listening option does not require a new firmware image or a pin-map
rebuild. Keep using the **standard `waveshare_175` firmware**, not the `-G` image.

## Clock recovery

Both non-C variants have a PCF85063 RTC, but it needs power while the main board
is off. With **USB only and no battery**, removing USB also removes RTC power;
firmware cannot measure elapsed off-time in that state. On October 26, both
connected RTCs passed resynchronization and advancing-second readback; the
standard board reported oscillator loss after USB-only power loss.

With an onboard or enabled external GPS, 0.13.40 restores an **unset clock** from
a fresh validated fix once acquired. It writes UTC to the RTC and preserves the
saved local timezone offset. It leaves an already-valid clock alone. **SYNC CLOCK
FROM GPS** explicitly synchronizes an existing clock when a fresh fix is available.
This is recovery after power returns, not timekeeping while completely unpowered.
The touch sync button waits up to five seconds for a fresh fix and a free shared
bus; its status confirms the RTC write or explains why the clock was not changed.
Without GPS, USB sync or configured Wi-Fi time recovery remains available.

A correctly connected, charged, compatible single-cell 3.7 V battery provides
the board's retention supply. The connector is MX1.25; verify polarity rather
than assuming every cable with that connector has the same wiring. Battery-backed
shutdown retention still requires a real test with a battery attached.

## Validation and commands

Native tests exercise the actual UART loop/settings, allowed pins, baud choices,
receive-only initialization, default-off state, saved preferences, bad settings,
bounded reads, overflow, stale-fix rejection and timer rollover. Existing scan,
SD, upload and board capability tests cover the newly enabled standard profile.
Physical receiver acquisition on the external GPIO path needs a wired module;
native NMEA fixtures are not evidence of antenna reception.

USB: `gps external status`, `gps external on 18 9600`, `gps external off`,
`gps reception`, `gps clock sync`, `screen gps-setup`. An enable command takes
the **ESP32 receive GPIO**, followed by the module's **baud rate**.

References: [Waveshare hardware reference, pinned revision](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/e4344e70c2fa78a13e8a06566507f1ba8af6672a/HARDWARE_REFERENCE.md),
[schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75/ESP32-S3-Touch-AMOLED-1.75.pdf),
[official product documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75).
See also [wardrive/upload setup](WARDRIVE-UPLOADS.md) and [clock setup](CLOCK-SETUP.md).
