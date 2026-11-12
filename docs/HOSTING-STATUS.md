# Installation and hosting status

Updated November 12, 2026.

## Working web flasher

**[Open Dragon Detector](https://valleytech-dragon-detector.pages.dev/)**

This independently hosted copy includes firmware for all thirteen board profiles: 0.14.7 for the new Waveshare ESP32-S3-Touch-AMOLED-1.43C (physical validation in progress), 0.14.6 for the Waveshare ESP32-S3-Touch-AMOLED-1.75C and 0.14.2 for the other eleven, including the Waveshare ESP32-S3-Touch-LCD-1.85B. It also offers USB date/time setup, device tools and the optional personal-theme creator. Guides, SD resource packs and screenshots are served here too. The firmware on an already-installed device is unaffected by a website outage.

A separate [Guition / Surenoo JC3636W518 Beta 1](GUITION-SURENOO-BETA.md) installer uses a newer manufacturer panel sequence. It is physically unverified and does not replace or modify the thirteen released firmware packages, including the original Spotpear profile.

Public GitHub repository access is restored. The [repository](https://github.com/valleytechsolutions/Dragon-Detector)
links directly to this working flasher. The original
[GitHub Pages address](https://valleytechsolutions.github.io/Dragon-Detector/)
forwards visitors here, preserving tab links, query parameters and guide paths.
Cloudflare remains the primary installer, so there is one public firmware source.

## Installation

1. Open the flasher in desktop Chrome or Edge and connect a USB data cable.
2. Choose your exact board. Select **Update matching Dragon Detector** for a compatible installation; **Fresh install** replaces saved files and settings.
3. Keep USB connected until firmware installation and date/time verification finish.

The Custom theme tab is optional. Its personal-theme slot stays hidden on the device unless a design is explicitly installed.

[Full installation guide](FLASHING.md) · [Clock setup](CLOCK-SETUP.md) · [Custom theme guide](CUSTOM-THEME.md) · [Release notes](RELEASE-0.14.7.md)

## GitHub and Gitea

GitHub now contains the source changes made while access was unavailable. The
Gitea repository remains a separate copy; its commits and releases can differ.
The GitHub Pages workflow publishes only the forwarding page. Firmware CI builds
remain validation artifacts until explicitly released to the primary flasher.
The 0.14.2 release adds the three sound levels, the physically checked 1.85B
speaker adjustment and stable-ID theme retirement. Publication requires all
thirteen profile builds and checked packages; other boards retain their documented
physical-validation limits. A Git push alone does not deploy
firmware to the web flasher.

## M5Burner

Clean 0.14.0 factory packages are available for StopWatch C152 (16 MB) and
M5Stack Dial (8 MB), both at offset 0x0. M5Burner submissions require M5Stack
catalog review independently of web-flasher availability; a pending submission
is not an approved catalog release. [M5Burner details](M5BURNER.md).
