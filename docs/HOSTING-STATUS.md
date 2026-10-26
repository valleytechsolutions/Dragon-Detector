# Installation and hosting status

Updated October 25, 2026.

## Working web flasher

**[Open Dragon Detector](https://valleytech-dragon-detector.pages.dev/)**

This independently hosted copy includes the 0.13.38 firmware for all eleven board profiles, USB date/time setup, device tools and the optional personal-theme creator. Guides and screenshots are served here too. The firmware on an already-installed device is unaffected by a website outage.

The original GitHub Pages address, public repository pages and GitHub-hosted images are currently unavailable. The original source, artwork, release files and Git history remain intact. This is a hosting/account-access issue; reinstalling firmware or erasing a device will not repair the website.

## Installation

1. Open the flasher in desktop Chrome or Edge and connect a USB data cable.
2. Choose your exact board. Select **Update matching Dragon Detector** for a compatible installation; **Fresh install** replaces saved files and settings.
3. Keep USB connected until firmware installation and date/time verification finish.

The Custom theme tab is optional. Its personal-theme slot stays hidden on the device unless a design is explicitly installed.

[Full installation guide](FLASHING.md) · [Clock setup](CLOCK-SETUP.md) · [Custom theme guide](CUSTOM-THEME.md) · [Release notes](RELEASE-0.13.38.md)

## GitHub and Gitea

The Gitea migration copied repositories. It did not move GitHub Pages hosting or configure replacement workflow runners. The GitHub and Gitea repositories are independent copies, so their latest commits and releases can differ. Restoring GitHub access is being handled separately; this flasher does not depend on GitHub Pages to deliver its firmware.

## M5Burner

The 0.13.17 factory package is prepared, but the M5Burner catalog rejected the new-version upload. Use this web flasher for the current supported installation flow. [M5Burner details](M5BURNER.md).
