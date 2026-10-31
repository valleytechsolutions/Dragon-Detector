# Dragon Detector 0.13.42 — larger 44Delta clock logo

Refines the 44Delta analog clock following feedback from the physical StopWatch:

- Enlarges the original 44 DELTA skull and wordmark by approximately 60% in
  each dimension, with native assets for the supported display sizes.
- Removes the LOCKSPORT caption from the bottom of the clock face.

The clock keeps its silver lockpick hands, cyan accents and analog-only time
display. A separate clock asset preserves the existing radar and Locksport Timer
header layouts. Persistent theme 45 and clock 48 keep the same IDs.

The owner confirmed that 0.13.41 looked good on the physical StopWatch and its
touchscreen worked normally before requesting these two visual adjustments.
The [0.13.41 validation and hardware limitations](VALIDATION-0.13.41.md) remain
applicable. This revision is checked with native LVGL renders, artwork decoder
checks, the existing web checks and the eleven-board build matrix. Build checks
do not constitute physical testing of every board.

The [eleven-board CI run](https://github.com/valleytechsolutions/Dragon-Detector/actions/runs/41220658748)
passed from clean source `bf309d741a0ef6496492787b7b9dac74cf6639c7`.
All 231 artwork variants decoded to their expected pixel CRCs, 142 web tests
passed, and native clock renders were inspected at 240, 360, 466 and 480 pixels.
Every release package passed its checksums, flash budget and manifest checks.

On October 31, a full private 16 MB backup of the connected C152 was saved
before updating its boot/application segments. NVS and LittleFS were preserved.
The write passed hash verification; the device booted 0.13.42 with theme 45 and
clock 48. Its 466px clock capture passed the USB CRC check and showed the larger
logo with the footer removed. USB transfers were then stopped. This cosmetic
revision does not claim another physical touch or factory-reset installation test.

The StopWatch package uses that locally built, device-tested application
(3,468,208 bytes; 17.31% application-slot space free). The other ten packages use
the verified CI artifacts. Their metadata records the clean source above.

See the [44Delta guide](44DELTA.md). Use **Update matching Dragon Detector** in
the [web flasher](https://valleytech-dragon-detector.pages.dev/#install) for a
compatible existing installation. The clean 0.13.42 StopWatch C152 factory BIN
at 0x0 was also uploaded to M5Burner on October 31 and is **Pending / Public**.
M5Stack's catalog review is separate from the live web-flasher release.
