# Installation and hosting status

Updated November 26, 2026. Current firmware release: **0.14.10**.

## Official release flasher

**[Open Dragon Detector](https://valleytechsolutions.github.io/Dragon-Detector-Flasher/)**

The installer uses the latest published firmware packages from
[this repository's GitHub Releases](https://github.com/valleytechsolutions/Dragon-Detector/releases).
It offers the released hardware profiles, USB installation and verified local
date, time and timezone setup. Firmware is copied from the release packages
without rebuilding or changing it.

The [website flasher](https://valleytechsolutions.tech/pages/dragon-detector)
embeds this same installer, so its board list and release files match. The
[original GitHub Pages address](https://valleytechsolutions.github.io/Dragon-Detector/)
forwards installation links to the release flasher.

## Badge Studio, themes and separate hardware

[Badge Studio](https://valleytech-dragon-detector.pages.dev/#studio) and
[Theme Creator](https://valleytech-dragon-detector.pages.dev/#theme-studio) remain available
on the companion site. Install firmware first, finish clock setup, close the
installer connection, then connect the editor to upload your design.
The release flasher does not automatically upload badge or theme artwork.
Customization support depends on the board.

The P4 Knob, [Guition / Surenoo beta](GUITION-SURENOO-BETA.md) and Tuya T5-E1
keep their separate installers or downloads on the
[companion site](https://valleytech-dragon-detector.pages.dev/#install). They are not
substituted with another board's GitHub release package. Existing guides, hardware
purchase links, SD resources and screenshots remain there. Old editor bookmarks
and guide paths continue to forward to that site.

## Installation

1. Open the release flasher in desktop Chrome or Edge and connect a USB data cable.
2. Choose the exact printed board model. Use **Update Dragon Detector · preserve saved data** for a compatible installation, or **Fresh installation · erase saved data** when replacing unrelated firmware or correcting a wrong board image.
3. Confirm the model, connect and install. Keep USB connected through clock verification.

The installer checks physical flash capacity before writing. A capacity match
does not identify the display or wiring: boards with equal flash sizes still
need the correct selection.

[Installation and recovery](FLASHING.md) · [Clock setup](CLOCK-SETUP.md) · [Custom theme guide](CUSTOM-THEME.md) · [Release notes](RELEASE-0.14.10.md)

## Release updates

The [release flasher workflow](https://github.com/valleytechsolutions/Dragon-Detector-Flasher/actions)
checks GitHub Releases every 30 minutes and can also be run manually. GitHub may
delay scheduled jobs. Packaging verifies each release ZIP, board identity,
version, flash layout and binary checksum. The browser verifies the selected
files again before allowing installation. A failed site build leaves the
previous deployment available.

A source commit alone is not a new firmware release. The main firmware repository
continues to own the release ZIPs. The separate flasher repository hosts the
installer; the original repository's Pages workflow publishes its forwarding page.
Gitea is a separate mirror and is not the release flasher's firmware source.

## M5Burner

Clean 0.14.0 factory packages are available for StopWatch C152 (16 MB) and
M5Stack Dial (8 MB), both at offset 0x0. M5Burner submissions require M5Stack
catalog review independently of web-flasher availability; a pending submission
is not an approved catalog release. [M5Burner details](M5BURNER.md).
