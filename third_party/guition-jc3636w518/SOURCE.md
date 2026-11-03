# Guition JC3636W518 display source record

These reference files support the separate `guition_185_beta` target. They are not a replacement for the legacy Spotpear initialization table.

## Pinned source

- Repository: [Guition-JC/Guition](https://github.com/Guition-JC/Guition).
- Revision: `dbadb44943658338602c5f5a584b2af026fb32da`.
- Directory: [components/board/guition_boards/jc3636w518](https://github.com/Guition-JC/Guition/tree/dbadb44943658338602c5f5a584b2af026fb32da/components/board/guition_boards/jc3636w518).
- Retrieved: 2026-11-03.
- The initialization table was introduced by commit `a9caa4164d9c46394c27f625d84919df67c42235` on 2026-10-13.
- Guition's [published component](https://components.espressif.com/components/guition/guition_boards/versions/0.2.1/readme?language=en) links this repository. Its board definition describes physical validation as pending. Source provenance is not evidence of successful physical operation on the affected owner's device.

The upstream table has 184 entries. Its sole positive per-command delay is 120 ms after sleep-out (`0x11`); its final command is display-on (`0x29`). The beta adapter changes the array/type names to the existing local display-library interface while preserving command order, parameter bytes, lengths and delays.

## Retained files and licenses

`lcd_init_commands.upstream.h` is an unchanged copy of upstream `lcd_init_commands.h`. It retains the 2023–2025 Espressif Systems (Shanghai) CO LTD copyright and Apache-2.0 identifier. Upstream `NOTICE` traces the bytes to the original JC3636W518 custom Arduino configuration. The Apache-2.0 license text and unchanged notice are retained beside it.

The board YAML and `setup_device.c` files are unchanged reference copies under Guition's MIT license, copyright 2026 Guition contributors. They are not compiled into Dragon Detector; they document the pin map, RGB565 big-endian configuration, inversion and custom-table factory. The executable beta uses the existing display library and its existing licenses.

| Local file | Upstream file |
|---|---|
| `lcd_init_commands.upstream.h` | `lcd_init_commands.h` |
| `board_devices.yaml` | `board_devices.yaml` |
| `board_peripherals.yaml` | `board_peripherals.yaml` |
| `setup_device.c` | `setup_device.c` |
| `NOTICE` | `NOTICE` |
| `LICENSE-APACHE-2.0.txt` | `LICENSES/Apache-2.0.txt` |
| `LICENSE-MIT.txt` | `LICENSE` |

[SHA256SUMS.txt](SHA256SUMS.txt) records the retained reference-file hashes.

## Comparison with the stable driver

The currently linked [Spotpear Arduino demo](https://cdn.static.spotpear.com/uploads/picture/learn/ESP32/esp32-S3-1.8-touch/Demo_Arduino_en.rar) was independently retrieved on 2026-11-03. Archive SHA-256:

`e9264326dc8f8985fdecced722788e1019d44275b68a64b6ebd52d142d557ff9`

Its `lib/ESP32_Display_Panel/src/lcd/base/esp_lcd_st77916.c` matches the existing local stable driver line-for-line. It supplies a 216-command default sequence starting `F0=08, F2=08, 9B=51, 86=53`. The newer Guition board-specific sequence starts `F0=28, F2=28, 73=F0, 7C=D1` and changes panel power, gamma, resolution and gate configuration. The beta therefore uses a named, explicit alternative table; it does not modify the existing library default.

The Spotpear demo requests a 50 MHz QSPI clock. Guition's newer YAML requests 80 MHz, but the isolated beta requests 20 MHz to provide more timing margin while testing the affected panel. Both use the same GPIOs, RGB order, 16-bit pixels, big-endian transfer and inverted colors. No additional byte swap is needed with the current `LV_COLOR_16_SWAP=1`.

## Diagnostic read limits

The [ST77916 datasheet](https://dl.espressif.com/AE/esp-iot-solution/ST77916_SPEC_V1.0.pdf), page 155, documents `0x04` as a dummy byte followed by three ID bytes. Page 39 specifies a QSPI read cycle of at least 150 ns. The beta can read at 3 MHz through the existing GPIO12 data1/MISO connection, independently of its 20 MHz write clock.

There is no Guition-specific revision-to-ID map in the pinned board source. Raw IDs, including failed or empty reads, are diagnostic information only. They must not select the Waveshare 1.85B initialization tables or be described as verified automatic revision detection.
