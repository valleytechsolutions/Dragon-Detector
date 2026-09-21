# Dragon Detector

Custom ESP32-S3 firmware for the **Spotpear / Guition JC3636W518**, with a 360×360 ST77916 round display, CST816 touch, 16 MB flash and 8 MB octal PSRAM. Uses the supplied VTS logo at boot.

**Development build 0.1.0.** The radar, passive radio monitoring, signature evidence, swipe tools, SD event logs, file browser and audio functions are implemented. This is a new firmware incorporating selected JustCallMeKoko Marauder detection logic; **full upstream Marauder parity is not implemented**. See [exact feature status](docs/FEATURES.md) and [validation](docs/VALIDATION.md).

## Controls

- Swipe up from the bottom edge of the radar for tools. Lists reserve vertical swipes for scrolling; use visible tabs and Home buttons to navigate.
- Tap a dot to see the address, received name, signal, signature, and confidence.
- Tools has Recon, Media and System tabs; tap the tabs.
- Recon offers Wi-Fi/BLE contacts, alerts, channel observation and packet capture.
- Media offers music, recorder, recording playback and SD files. Select an MP3 or WAV to play it. Recording starts only when Record is pressed; Stop / Save closes the WAV.
- System contains radio pause, logging, brightness and signature explanations.

The radar's radius represents smoothed RSSI. Its angle is an arbitrary stable arrangement. It does **not** measure direction or distance in meters. Detection does not prove recording or malicious intent. A quiet screen cannot establish that the area is free of cameras or other devices.

## SD card

Use a FAT32 microSD card inserted before boot. The device found an approximately 29.8 GB card during bring-up. Existing card contents are retained.

| Path | Content |
|---|---|
| `/music` | Your MP3 and PCM WAV files |
| `/recordings` | 16 kHz mono 16-bit WAV microphone recordings |
| `/dragon/session-xxxxxxxx` | CSV observations and optional PCAP captures |

Times are uptime, not UTC. Log files rotate at 4 MB. Individual packet captures stop at 16 MB; recordings stop at 32 MB. Headers/checkpoints flush periodically, but use Stop / Save before removing power. The browser shows up to 96 entries per folder and a 2 KB preview of text/other files. There is no file-delete function.

## Build on this computer

Open PowerShell in `C:\Users\khali\Documents\DragonDetector`:

```powershell
.\.venv\Scripts\pio.exe run
.\.venv\Scripts\pio.exe run --target upload --upload-port COM3
.\.venv\Scripts\python.exe tools\serial_tool.py status
```

The project pins Arduino-ESP32 3.0.7 through the pioarduino platform, NimBLE 2.3.6, ESP8266Audio 1.9.9 and the board vendor's modified display libraries. The archive is necessary: a generic ST77916 driver may use a different initialization sequence.

The ready-to-flash release is `release/Dragon-Detector-0.1.0-merged.bin`, at offset `0x0`, for this board only. It preserves the build's DIO bootloader header and contains the application configured for quad flash and octal PSRAM. `release/firmware.bin` is the application only at offset `0x10000`; do not flash the application-only file at offset zero. The source archive includes the modified vendor libraries and the radio/audio library sources.

For a fresh checkout, run `tools/setup.ps1` with Python 3.12 installed, then the build command above. Asset generation uses the supplied original logo copied under `assets/`.

## Recovery

The original firmware was read and verified before replacement:

`backups/original-flash-20260920.bin` (16,777,216 bytes)

SHA-256: `72cee75bb7b0a92371b5dfc2df5f835be4a82f34332e1f75b54c889dea8fe98e`

To restore **this same device**, close any serial monitor and run:

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM3 --baud 921600 write-flash 0 backups\original-flash-20260920.bin
```

If USB is unavailable, hold BOOT while connecting/resetting, release BOOT, and retry with the enumerated COM port. Flash backups are excluded from Git.

## Tests and diagnostics

```powershell
.\.venv\Scripts\python.exe -m ziglang c++ -std=c++17 -Iinclude -O1 -fsanitize=undefined -fsanitize-trap=all tests\detection_test.cpp -o tests\detection_test.exe
.\tests\detection_test.exe
.\.venv\Scripts\python.exe tools\serial_tool.py selftest
.\.venv\Scripts\python.exe tools\serial_tool.py screenshot
```

The USB CLI supports `status`, `contacts`, `selftest`, `pause`, `resume`, `record`, `stop`, `play /path.mp3`, `audio status`, `volume 50`, `ls /`, `capture on`, `capture off`, `screen menu`, and `screenshot`. This diagnostic CLI is not Marauder's upstream CLI.

USB file transfer (stop audio first):

```powershell
.\.venv\Scripts\python.exe tools\sd_transfer.py put 'C:\path\song.mp3' /music/song.mp3
.\.venv\Scripts\python.exe tools\sd_transfer.py get events.csv /dragon/session-xxxxxxxx/events-0.csv
```

Uploads refuse existing filenames. Transfers briefly pause scanning. A small generated test tone is already on the card. Microphone recording displays `MIC REC` on every screen, including radar.

## Attribution

Marauder detection adaptations: Just Call Me Koko, MIT, pinned source `8ae4622abcc9c9c5729d4e97491907581d7f0c34`; license retained in `docs/MARAUDER-LICENSE.txt`. See [third-party notices](docs/THIRD_PARTY.md). Firmware distribution is GPL-3.0-or-later because it links ESP8266Audio. The VTS artwork remains the user's artwork.
