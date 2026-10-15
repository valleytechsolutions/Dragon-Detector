# Flash Dragon Detector

**[Open the web flasher](https://valleytechsolutions.github.io/Dragon-Detector/)** in desktop Chrome or Edge. HTTPS or localhost is required for Web Serial. Use a USB data cable and close serial monitors first.

1. Check the exact printed model: Spotpear JC3636W518, Waveshare **ESP32-S3-Touch-AMOLED-1.75C**, M5Stack StopWatch C152, or **Seeed XIAO ESP32-S3 + Round Display**.
2. Select that board. The ESP32-S3 chip check does not distinguish these boards.
3. Choose **Fresh install** for the first 0.3.x/0.4.x/0.5.x/0.6.x/0.7.x/0.8.x/0.9.x installation. It writes a blank badge filesystem. Back up existing firmware, files and settings first. Selecting the installer's erase option erases flash.
4. Confirm the board/type checkbox, Connect & install, choose its serial port, and wait for verification. Reset if it stays in the bootloader.
5. Close the installer before connecting from Badge Studio.

Use **Update** only on an existing 0.3.x/0.4.x/0.5.x/0.6.x/0.7.x/0.8.x/0.9.x installation for the same exact board. The update manifest writes bootloader at `0x0`, partition table at `0x8000`, OTA selection data at `0xE000`, and application at `0x10000`; it skips NVS (`0x9000`) and badge storage. Leave erase unchecked. Firmware cannot preserve settings if you explicitly erase them.

**XIAO Round starts at 0.9.2:** use Fresh install when migrating from any other firmware, and Update only for this exact XIAO profile’s 0.9.2-or-newer layout. Its 8 MB flash cannot hold the other board images. [XIAO setup](XIAO-ROUND.md).

The factory filesystem is at `0xC10000` on Spotpear/Waveshare, `0x810000` on StopWatch, and `0x610000` on Seeed XIAO Round. A merged `factory-0x0.bin` writes intervening blank regions too, so it resets NVS and files. **Never use a merged factory BIN as a preservation update.** Earlier 0.1/0.2 binaries are retained under `hardware/` and use their older instructions.

## Manual installation

Build/package with `python tools/build_release.py --board waveshare` (or `spotpear` / `stopwatch` / `xiao_round` / `waveshare_lite`). Verify the files against `web/firmware/<board>/SHA256SUMS.txt`.

```
python -m esptool --chip esp32s3 --port YOUR_PORT write-flash 0x0 web/firmware/waveshare/factory-0x0.bin
```

Replace the port and board folder with the exact device. This is a factory write. A complete backup can be made with esptool `read-flash 0 ALL backup.bin`; keep backups private because they may include credentials and previous user data.

## Recovery

**M5Stack StopWatch:** connect USB, hold the power button for about two seconds until its green LED lights, then release to enter download mode. After flashing, a short power-button press restarts it. This differs from boards with separate BOOT/RESET buttons. [Official instructions](https://docs.m5stack.com/en/core/StopWatch).

**Spotpear/Waveshare/XIAO:** Hold BOOT, tap RESET, release BOOT, then select the bootloader port. Try a known data cable and another direct USB port. Stop any other program holding the serial port. If the display is blank but USB responds, confirm the board variant before trying another build. For badge storage errors, back up existing flash before reinstalling the correct factory image. The firmware never auto-formats a failed mount.

## Hosting

GitHub Settings → Pages → Source must be **GitHub Actions**. The firmware workflow tests and builds all five profiles. The Pages workflow publishes only a successful `main` push, taking source and all five build artifacts from that same completed run. Failed builds are never substituted with older binaries. The public website contains release binaries; device dumps and local scan data are excluded.

The web page validates the exact board name, ESP32-S3 family, image filenames, partition offsets and factory/update erase policy before enabling installation. This does not automatically identify connected display wiring; select the printed model carefully.
