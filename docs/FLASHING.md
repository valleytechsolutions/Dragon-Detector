> **0.13.2:** Waveshare AMOLED and M5Stack use internal flash with no SD controls. Matching updates preserve badge, settings and files. [GUI tour](GUI.md) · [Hardware feature comparison](HARDWARE.md).

# Flash Dragon Detector

**[Open the web flasher](https://valleytech-dragon-detector.pages.dev/)** in desktop Chrome or Edge. HTTPS or localhost is required for Web Serial. Use a USB data cable and close serial monitors first.

1. Check the exact printed model against the table below. Waveshare **1.75C**, **1.75**, **1.75-G** and non-touch **LCD-1.28** have separate profiles. The touch LCD-1.28 is not supported by the non-touch Lite build.
2. Select that board. The ESP32-S3 chip check does not distinguish these boards.
3. Choose **Update matching Dragon Detector** if the exact board already has a compatible Dragon Detector layout (table below). The page applies the selected erase policy. Choose **Fresh install** for a new board or migration from unrelated firmware; this replaces saved files. Back up existing firmware, files and settings first.
4. Confirm the board/type checkbox, Connect & install, choose its serial port, and wait for verification. Reset if it stays in the bootloader.
5. Keep USB connected until automatic clock setup and verification finish, then connect from Badge Studio.

| Exact board | Existing Dragon Detector layout accepted for Update |
|---|---|
| Spotpear JC3636W518 | This profile's 0.3.x–0.13.x layout |
| M5Stack StopWatch C152 | This profile's 0.3.x–0.13.x layout |
| Waveshare AMOLED **1.75C** | This profile's 0.3.x–0.13.x layout; older 16 MB builds need the one-time recon initialization below |
| XIAO ESP32-S3 + Round Display | This profile's **0.9.2 or newer**, 8 MB layout |
| Waveshare non-touch LCD 1.28 Lite | This profile's **0.9.4 or newer** layout; unrelated/older ports are not assumed compatible |
| Waveshare AMOLED **1.75**, non-C | This profile's **0.13.19 or newer**, 16 MB layout |
| Waveshare AMOLED **1.75-G**, LC76G GPS | This profile's **0.13.21 or newer**, 16 MB layout |
| Elecrow CrowPanel 2.1 Rotary | This profile's **0.13.20 or newer**, 16 MB layout |
| LILYGO T-Display-S3 AMOLED 1.75, H0175Y003AM | The supplied **0.13.20** baseline or this profile's **0.13.25 or newer**, 16 MB layout |

The update manifest writes bootloader at `0x0`, partition table at `0x8000`, OTA selection data at `0xE000`, and application at `0x10000`; it skips NVS (`0x9000`) and user filesystems. Leave erase unchecked. Firmware cannot preserve settings if you explicitly erase them.

After installation, open Tools → Utilities on a full board. **Waveshare AMOLED needs no SD card:** Saved files opens internal-flash captures, and Settings → Capture studio starts a recording. Download captures from the web page's Radio tools → Capture library. Badge Studio uploads directly to internal flash. [Screens and controls](GUI.md).

**Waveshare AMOLED 1.75C / 0.9.8:** the build now uses its full 32 MB flash. The original lower 16 MB partition addresses and sizes stay unchanged. Update also skips the new recon filesystem at `0x1000000`; existing badges, settings and saved BLE profiles are preserved. When upgrading older 16 MB firmware, use BLE Lab → Initialize new BLE storage after making a complete backup. This initializes only the upper 16 MB, once. Fresh install writes blank badge and recon filesystems. [BLE Lab and migration details](BLE-LAB.md).

**XIAO Round starts at 0.9.2:** use Fresh install when migrating from any other firmware, and Update only for this exact XIAO profile’s 0.9.2-or-newer layout. Its 8 MB flash cannot hold the other board images. [XIAO setup](XIAO-ROUND.md).

The factory filesystem is at `0xC10000` on Spotpear/Waveshare, `0x810000` on StopWatch, and `0x610000` on Seeed XIAO Round. A merged `factory-0x0.bin` writes intervening blank regions too, so it resets NVS and files. **Never use a merged factory BIN as a preservation update.** Earlier 0.1/0.2 binaries are retained under `hardware/` and use their older instructions.

## Manual installation

For the M5Stack StopWatch C152 factory package in M5Burner, follow the [M5Burner guide](M5BURNER.md), including the required post-burn browser clock setup. The factory image resets saved settings and files; use the web flasher's Update option for compatible installations you want to preserve.

Build/package with `python tools/build_release.py --board waveshare` (or `spotpear` / `stopwatch` / `xiao_round` / `waveshare_lite`). Verify the files against `web/firmware/<board>/SHA256SUMS.txt`.

```
python -m esptool --chip esp32s3 --port YOUR_PORT write-flash 0x0 web/firmware/waveshare/factory-0x0.bin
```

Replace the port and board folder with the exact device. This is a factory write. A complete backup can be made with esptool `read-flash 0 ALL backup.bin`; keep backups private because they may include credentials and previous user data.

## Recovery

### ESP32 drivers and an empty port list

Use a USB data cable, a direct computer USB port, and desktop Chrome or Edge. If serial access is blocked inside an embedded page, [open the direct flasher](https://valleytech-dragon-detector.pages.dev/#install). Close Arduino, PlatformIO, other flashers and serial monitors before connecting.

Install the driver matching the USB bridge shown in Device Manager or the board documentation, then reconnect:

- [CP210x Universal Windows driver ZIP](https://espterminator.com/downloads/CP210x_Universal_Windows_Driver.zip)
- [CH340 / CH341 Windows driver](http://www.wch-ic.com/downloads/CH341SER_EXE.html)
- [CH343 / CH9102 Windows driver ZIP](https://www.wch-ic.com/downloads/CH343SER_ZIP.html) — the **Waveshare non-touch LCD-1.28 Lite uses CH343**, not CP210x or CH340.

Native ESP32-S3 USB normally uses the operating system's built-in driver. See the [Waveshare Lite USB instructions](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.28) and [Silicon Labs driver page](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) for other operating systems.

### Interrupted or cancelled installation

Cancelling the installer skips clock and personal-theme setup. A completed firmware write followed by failed clock setup is reported separately: reconnect the running board and select **Finish clock setup**. It does not require erasing or reinstalling.

If a firmware file cannot be reached, the board chooser keeps installation disabled and offers **Retry release check**. The installer downloads all required binaries before erasing. If writing itself was interrupted, re-enter download mode and retry the same board and installation type. Use **Copy troubleshooting details** and include the printed board model and the error inside the installer when reporting a failure.

### Entering download mode

**M5Stack StopWatch:** connect USB, hold the power button for about two seconds until its green LED lights, then release to enter download mode. Leave all buttons alone during flashing and verification. After flashing, a short power-button press restarts it. If the screen stays blank, close the serial connection, unplug USB, double-press power to turn it off, wait five seconds, then press power once. Reconnect USB after the logo appears and choose **Finish clock setup** or **Connect & sync local time**. This full power cycle recovered a C152 that remained in download mode after a verified 0.13.20 factory installation. [Official power-button controls](https://docs.m5stack.com/en/core/StopWatch).

**Spotpear/Waveshare/XIAO:** Hold BOOT, tap RESET, release BOOT, then select the bootloader port. Try a known data cable and another direct USB port. Stop any other program holding the serial port. If the display is blank but USB responds, confirm the board variant before trying another build. For badge storage errors, back up existing flash before reinstalling the correct factory image. The firmware never auto-formats a failed mount.

## Hosting

The current public flasher is hosted on Cloudflare Pages. The repository also includes GitHub Actions packaging; if using GitHub Pages, set its source to **GitHub Actions** and publish source and all nine build artifacts from the same successful run. Failed builds must not be substituted with older binaries. The public website contains release binaries; device dumps and local scan data are excluded.

The web page validates the exact board name, ESP32-S3 family, image filenames, partition offsets and factory/update erase policy, then checks file availability and declared sizes before enabling installation. Some hosts omit sizes on HEAD responses; release packaging still verifies every full binary and its offsets. The availability check is not a checksum verification; published SHA-256 files are available for manual verification. This does not automatically identify connected display wiring; select the printed model carefully.

ESP Web Tools 10.4.0 is hosted locally under `web/vendor/`, including its bundled imports and license, so installation does not depend on a separate script CDN. `python tools/vendor_esp_web_tools.py` reproduces these files from the npm package after checking its registry integrity. `UPSTREAM.json` records the source and file hashes. The adapter's success/cancel behavior is covered by the JavaScript tests.

## Automatic local time (0.13.7)

Leave **Automatically set and verify my local date, time & timezone** enabled. In 0.13.10+, the installer closes automatically after flashing, then copies the computer's current time and UTC offset to that exact detected board. Keep USB connected until verification finishes. Detected hardware RTCs are read directly and checked for ticking; missing expected RTCs or invalid readback fail explicitly. If USB changes ports, choose **Finish clock setup** and select the running board. A failed clock sync does not mean flashing failed. Lite supports USB clock setup in 0.13.9+. Without a working external RTC, full shutdown requires USB sync or configured saved-Wi-Fi recovery (full boards). The Waveshare **1.75C** is in that category on tested hardware; do not confuse it with the non-C model. [Clock setup and retention guide](CLOCK-SETUP.md).
