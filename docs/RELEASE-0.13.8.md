# Dragon Detector 0.13.8

- New **Covert Instruments × LockPickingLawyer** theme: navy/gold GUI, CI header, central original LPL emblem, rotating two-pan scales, a Roman-dial watch with two silver lockpicks and 24-hour time, and the full Locksport Timer with moving rim indicator. Full boards now offer 27 themes and 25 logo watches. Lite includes the LPL radar with both logos and scales. [Theme preview](THEMES.md#covert-instruments--lockpickinglawyer--0138).

- The web flasher has a moving radar background with a pause/resume control. It honors reduced-motion preferences, remembers a manual pause and suspends drawing in hidden tabs. The background is decorative; it does not scan the computer’s surroundings.
- Automatic local-time setup is enabled by default for the four full boards. After installing, **close the installer window** and wait for clock verification. The flasher releases USB, checks the running board’s exact identity, copies the computer’s current UTC time and local offset (including current daylight saving), reads back the result, then closes USB. If the port changes after reboot, **Finish clock setup** lets you select the running board. The setting can be unchecked. Lite has no clock and skips setup.
- Boards with a working RTC retain time through appropriate battery-backed power cycles. A board reporting no usable RTC needs resync after power loss. The offset is the current local offset; reconnect and sync after travel or a daylight-saving change. Computer time must be correct.
- Waveshare ESP32-S3-LCD-1.28 Lite now cycles all 27 radar palettes and logos on a debounced BOOT press, remembers the selection, and keeps its fixed 12 fps instrument layout. Holding BOOT does not repeatedly change themes. BOOT + RESET recovery still works. It remains radar-only, without watches or timers; its LPL radar includes both logos and the rotating scales.
- Covert Instruments radar keeps its CI mark, black/silver palette and rotating McTickler pick; the pin-tumbler background under the logo is removed.
- Full-board palettes and Lite palettes come from one catalogue, retaining all existing saved theme indices. All five board-specific firmware packages and manifests are rebuilt together.

## Validation

The release verification asset records exact build hashes, automated tests, public-flasher checks and on-device checks. Native BOOT tests cover startup-held buttons, bounce, long holds, rearming and millisecond rollover. Clock tests cover all four identities, offset boundaries/fractional zones, installer ownership, opt-out, wrong devices, reconnect retries and failed readback. The verification asset identifies each physically tested board. XIAO Round and Waveshare Lite were build-tested; a successful build is not a claim of fresh on-device testing.

[Web flasher](https://valleytechsolutions.github.io/Dragon-Detector/) · [Lite controls and recovery](WAVESHARE-LITE.md) · [Artwork and software credits](../CREDITS.md)
