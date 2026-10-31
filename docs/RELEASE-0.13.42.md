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

See the [44Delta guide](44DELTA.md). Use **Update matching Dragon Detector** in
the [web flasher](https://valleytech-dragon-detector.pages.dev/#install) for a
compatible existing installation. The separate M5Burner candidate is the clean
0.13.42 StopWatch C152 factory BIN at 0x0; catalog upload/approval is separate.
