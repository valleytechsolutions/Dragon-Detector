# Guition / Surenoo JC3636W518 display beta

This separate `guition_185_beta` profile is a test candidate for the **Guition JC3636W518** sold by [Surenoo](https://www.surenoo.com/products/23280116), following an owner's report of a white screen with lines after installing Dragon Detector. It uses Guition's current board-specific display initialization and a conservative QSPI clock.

**Physical validation is pending.** The affected board is not available on the development bench. A successful build or completed flash does not establish that its screen, touch, SD card or other hardware works. The original **Spotpear JC3636W518** target and its working display initialization remain separate; this beta does not replace that image.

## Match the hardware first

Check the PCB or manufacturer's exact model marking for **JC3636W518** and the expected **ESP32-S3, 16 MB flash, 8 MB octal PSRAM, 360 × 360 ST77916 display**. Storefront descriptions use both 1.8-inch and 1.85-inch names, so screen size alone is not an identification.

This profile is not for JC3636K518, JC3636K718, JC3636F118, Waveshare LCD 1.85B or other round boards. They can use different GPIOs or processors even when they share a display controller. Do not change pin assignments to guess at a panel revision.

| Function | JC3636W518 pins |
|---|---|
| Display QSPI | CLK9, CS10, data0–data3: 11, 12, 13, 14 |
| Display reset and backlight | RST47, BL15 |
| CST816 touch | SDA7, SCL8, RST40, IRQ41 |
| Four-bit SDMMC | CLK3, CMD4, data0–data3: 2, 1, 6, 5 |
| Native USB | D−19, D+20 |

## What this beta changes

The beta explicitly selects Guition's newer 184-command panel sequence. The existing Spotpear profile retains its 216-command sequence. Both use RGB565 big-endian data, inverted colors and the same display GPIOs. The beta requests a 20 MHz QSPI write clock; the stable profile requests 50 MHz.

A bounded 3 MHz panel-ID probe records diagnostic information. No verified Guition revision-to-ID mapping is available, so this is **not automatic LCD-revision detection**. The beta does not borrow the Waveshare 1.85B panel tables. Spotpear's documented V2 microphone change is not proof that this user's screen has a known LCD V2 revision.

The [pinned source record](../third_party/guition-jc3636w518/SOURCE.md) includes manufacturer links, the exact source commit, unchanged reference files, licenses and checksums.

## Install and test

1. Confirm the complete board model. Keep a copy of its original factory firmware or an existing backup, and preserve any saved settings or logs you need before testing.
2. In the [web flasher](https://valleytech-dragon-detector.pages.dev/#install), select **Guition / Surenoo JC3636W518 Beta 1**. When using a downloaded package, use this beta's own generated manifest or matching flash instructions; do not use a Waveshare or knob-board image.
3. Keep the board on a reliable USB data cable and power source. Let flashing finish, then disconnect and reconnect USB once so the panel receives a full power cycle.
4. Check that the radar fills the screen with correct colors, without white bands, lines or clipping. Open the menu, use touch near the center and edges, and try clock/badge screens. Check again after a second cold boot.
5. If a FAT32 card is present, check that it mounts and that a small recording saves and can be read back. This test does not require formatting the card.
6. Stop recording before disconnecting power. Report the result and whether it changes between cold boots.

For a failed test, provide:

- A clear photo of the PCB/model/revision marking and the displayed failure.
- The selected firmware profile and version.
- The boot serial log, including the beta display diagnostics and raw panel ID.
- Whether the factory firmware displayed correctly before this installation, and whether touch responds.
- Whether a full USB power cycle changes the result.

If the first boot messages were missed, the USB serial command `display hardware` prints the cached beta display result and raw ID without probing or resetting the panel again. `memory status` reports the detected flash size and available memory. Capture those lines with the version and board identity when reporting a failure.

Do not include home-network passwords, API tokens, location logs or unrelated captured radio data in a public diagnostic report.

## Return to the original image

The original **Spotpear JC3636W518** image remains available as a separate choice. Select that original profile and follow its normal installation path to leave the beta. If the screen is blank, use the manufacturer's BOOT/RESET procedure to enter USB download mode; a working touchscreen is not required to flash.

Returning to the original image restores its previous behavior; it does not guarantee that an incompatible panel will begin working. If Dragon Detector never worked on this particular device, a preserved factory image is the appropriate known-working recovery reference. Keep the beta and original manifests distinct.

## Developer build

Use the dedicated environment:

```text
pio run -e guition_185_beta
```

Package it through the existing release tool with the `guition_185_beta` board profile. Its source pin and licenses must accompany the beta package. Physical acceptance remains an independent step after the build and host checks pass.
