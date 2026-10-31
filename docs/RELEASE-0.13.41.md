# Dragon Detector 0.13.41 — 44Delta

Adds **44Delta**, the 45th built-in theme, across all eleven supported hardware
profiles. Existing theme IDs and saved selections stay compatible.

- Cyan-on-black 44 DELTA skull and wordmark with matching menus and timer.
- Analog-only clock with Roman quarter markers, silver hook-pick hands, cyan
  handle scales and a moving rim seconds marker. No digital time overlay.
- Supplied spider-engraved black padlock and brass shackle at the radar center,
  with a cyan perimeter sweep.
- Matching Locksport Timer with the existing controls for each hardware profile.
- Web Device tools include 44Delta and require 0.13.41 before selecting it.

See [44Delta artwork and controls](44DELTA.md). Public showcase photographs
remain the previously selected Valleytech/BWM/Dragon Ball and TOOOL designs.

## Included Waveshare development changes

This release also packages the previously committed 0.13.39/0.13.40 work:
dedicated Wi-Fi/BLE wardrive sessions on the 1.75-G, GPS and SD logging fixes,
SD-configured upload support, clock recovery, and optional external UART GPS
setup on the standard 1.75. See [Waveshare 1.75](WAVESHARE-AMOLED-175.md) and
[wardrive uploads](WARDRIVE-UPLOADS.md).

The existing hardware beta status remains. Moving-route performance, live
authenticated uploads, and external GPS acquisition still require field or
hardware validation. A USB-only RTC cannot keep time without backup power;
GPS or configured Wi-Fi time recovery resynchronizes after power returns.

## Update and validation

Use **Update matching Dragon Detector** for a compatible existing installation;
the update preserves settings and filesystems. Fresh installation resets stored
data. Always select the exact hardware profile. The M5Burner package is the
clean StopWatch C152 merged factory image at offset 0x0, not a device backup.

Validation covers native LVGL renders at four resolutions, bounded logo decoder
CRC checks, web theme readback and firmware gating, and each board's build,
partition layout, flash reserve and package hashes. Physical validation of this
release is limited to the connected StopWatch; other profiles are build-validated.

The StopWatch passed all 23 self-tests, RTC readback, saved selection after a
software restart, and USB-injected timer control checks. See the
[validation record](VALIDATION-0.13.41.md) for methods and limits.
