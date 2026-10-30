# Installation and hosting status

Updated October 30, 2026.

## Working web flasher

**[Open Dragon Detector](https://valleytech-dragon-detector.pages.dev/)**

This independently hosted copy includes the 0.13.38 firmware for all eleven board profiles, USB date/time setup, device tools and the optional personal-theme creator. Guides and screenshots are served here too. The firmware on an already-installed device is unaffected by a website outage.

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

[Full installation guide](FLASHING.md) · [Clock setup](CLOCK-SETUP.md) · [Custom theme guide](CUSTOM-THEME.md) · [Release notes](RELEASE-0.13.38.md)

## GitHub and Gitea

GitHub now contains the source changes made while access was unavailable. The
Gitea repository remains a separate copy; its commits and releases can differ.
The GitHub Pages workflow publishes only the forwarding page. Firmware CI builds
remain validation artifacts until explicitly released to the primary flasher.
The public firmware is still 0.13.38 for all eleven profiles; the documented
0.13.40 Waveshare development source is not automatically promoted by a Git push.

## M5Burner

The 0.13.17 factory package is prepared, but the M5Burner catalog rejected the new-version upload. Use this web flasher for the current supported installation flow. [M5Burner details](M5BURNER.md).
