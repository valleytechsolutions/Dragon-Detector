# Validation status · 0.3.1

This is an experimental development release, not a claim of field-proven detection accuracy or all-day battery life.

All three final board profiles compile locally: Spotpear, Waveshare 1.75C and StopWatch. Release images are built from source and checked by the packager; physical-device verification is separate.

## Automated checks

- Native C++ fixtures cover Axon company/services, SmartTag, Meta composite false positives, generic Xuntong, Biscuit's shared example UUID, Flipper appearance, Find My minimum length, invalid AD fields, public versus random BLE OUI lookup, probe-role interpretation, Flock strict IE matching, disconnect windows, CRC-32, signal spikes, reacquisition and timer rollover.
- 150,000 random inputs exercise both bounded parsers. CI additionally runs AddressSanitizer and UndefinedBehaviorSanitizer.
- Eleven Node tests cover both image resolutions, RGB565 byte order, CRC agreement, invalid payload sizes, crop geometry, serial framing, small receive queues, ACK pacing and transfer errors.
- The release packager checks image sizes, partition offsets, every merged image segment, SHA-256 checksums and workstation-path exclusion.
- Browser QA exercised local image selection, fit controls and narrow-screen layout. Hardware uploads below used the same USB protocol from a host script; end-to-end browser USB selection/upload has not yet been exercised.

## Waveshare 1.75C hardware checks · 2026-10-13

The owner confirmed the ESP32-S3-Touch-AMOLED-1.75C model. A private 16 MB recovery backup was saved before writing firmware. The corrected 0.3.1 build was flashed and verified by esptool.

- Wi-Fi/BLE observation counters advance; USB reports a 466×466 display, AXP2101 battery status and mounted badge storage. The on-device self-test reports 17 passed, zero failed.
- A 434,320-byte badge upload was accepted and its LVGL screenshot matched the uploaded RGB565 pixels. Incorrect CRC and interrupted transfers were rejected, preserving the accepted image.
- A real restart through the ROM loader restored the saved badge, verified by pixel comparison. Radar startup was restored after testing.
- The owner reported the expected red/green/blue/white regions and yellow center on the physical panel, and reported that touch is working. This is functional confirmation, not a complete touch calibration or colorimetry test.
- Testing exposed the original USB receive queue overflow; 0.3.1 reserves 4096 bytes and the browser uses acknowledged 128-byte chunks. The temporary color test image was removed after validation.

## Remaining hardware limits

Spotpear and StopWatch 0.3.x builds still require physical testing. Button/haptic behavior, full touch alignment, battery runtime and long-duration stability remain unverified. No controlled field test against real examples of all named device families has been performed. Do not describe the signature list as guaranteed device detection or RSSI as measured distance/direction.

Historical 0.1/0.2 validation reports remain with their archived board projects and do not validate new 0.3.x changes.
