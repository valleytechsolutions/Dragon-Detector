# Building from source

Use Python 3.11+ and PlatformIO Core. Each hardware folder is a complete independent PlatformIO project; run build commands inside the selected folder, not at repository root.

```powershell
py -m pip install "platformio==6.2.0" "esptool==5.4.0" pillow pyserial
```

## Spotpear

```powershell
cd hardware/spotpear
pio run
```

The app-only output is `.pio/build/dragon/firmware.bin`. Build configuration, libraries and source match the supplied v0.1.1 project. Use the supplied merged release for a fresh install; do not rename an app-only binary to `0x0`. `pio run --target upload --upload-port COM4` performs PlatformIO's configured component upload.

## StopWatch

```powershell
cd hardware/m5stack-stopwatch
pio run
pio run -t buildfs
py tools/build_release.py
```

The release builder assembles the bootloader, partition table, app and (for factory) clean filesystem, and verifies their positions. Never substitute a dump of your personal device. The StopWatch hardware libraries are pinned to exact Git commits. The application uses 16 MB flash and 8 MB octal PSRAM.

For an existing matching installation, `pio run --target upload --upload-port COM4` uploads compiled components. The initial filesystem still requires the factory image or explicit filesystem upload; use FLASHING.md for the complete-image method. Filesystem upload overwrites stored files.

## Checks

The supplied validation notes describe the tests actually run. On a flashed StopWatch, `py tools/ui_regression.py --port COM4` injects touch/button events and changes screens; run it when the device is idle. `py tools/serial_tool.py selftest --port COM4 --seconds 3` requests firmware self-tests. A successful compile or renderer screenshot does not prove physical color, touch, sound, RF coverage, or battery runtime.

When changing dependencies or hardware code, rebuild and repeat the relevant hardware checks before distributing a new BIN. No build was required for the repository documentation/organization update; the shipped BINs remain byte-identical to the validated release files.
