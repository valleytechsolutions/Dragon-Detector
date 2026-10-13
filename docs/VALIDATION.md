# Validation status · 0.3.0

This is an experimental development release, not a claim of field-proven detection accuracy or all-day battery life.

All three final board profiles compile locally: Spotpear, Waveshare 1.75C and StopWatch. Release images are built from source and checked by the packager; physical-device verification is separate.

## Automated checks

- Native C++ fixtures cover Axon company/services, SmartTag, Meta composite false positives, generic Xuntong, Biscuit's shared example UUID, Flipper appearance, Find My minimum length, invalid AD fields, public versus random BLE OUI lookup, probe-role interpretation, Flock strict IE matching, disconnect windows, CRC-32, signal spikes, reacquisition and timer rollover.
- 150,000 random inputs exercise both bounded parsers. CI additionally runs AddressSanitizer and UndefinedBehaviorSanitizer.
- Node tests cover both image resolutions, RGB565 byte order, CRC agreement, invalid payload sizes and crop geometry.
- The release packager checks image sizes, partition offsets, every merged image segment, SHA-256 checksums and workstation-path exclusion.
- Browser QA exercised local image selection, fit controls and narrow-screen layout. Actual USB upload/flash through the browser needs a connected and confirmed target board.

## Hardware limits

The user's existing Waveshare firmware responds on USB with a 466×466 display, real Wi-Fi/BLE observations and AXP2101 battery status. This verifies the **previous** running port, not the new 0.3.0 firmware. The exact C/non-C model must be confirmed before writing the connected hardware.

The new shared source, revised driver, physical touchscreen orientation, colors, button/haptic behavior, persisted badge, failed-transfer recovery, battery runtime and long-duration stability require testing on each physical board. Compile success alone does not establish those properties. No controlled field test against real examples of all named device families has been performed in this update. Do not describe the signature list as guaranteed device detection.

Historical 0.1/0.2 validation reports remain with their archived board projects and do not validate new 0.3.x changes.
