# Dragon Detector 0.13.10 — verified clock setup

Flashing now proceeds automatically into date/time setup on the same USB port. The flasher verifies the computer's current epoch and timezone, then directly reads any detected hardware RTC twice to verify saved time and ticking. Missing expected RTCs and stale/stopped readback fail explicitly. Users can retry setup after USB re-enumeration without flashing again.

The Waveshare 1.75C clock claim is corrected. Its schematic omits the external PCF85063 found on the non-C 1.75, and the tested unit does not acknowledge address 0x51 at either 100 or 400 kHz while other bus peripherals respond. It receives the correct current time over USB; full shutdown needs USB or opt-in saved-Wi-Fi recovery. This release does not invent a hardware RTC or claim to reconstruct elapsed off-time from a saved timestamp.

Update mode explicitly disables the upstream installer's default full erase for non-Improv devices, preserving time-zone settings, network profiles and user files. Fresh installation retains its explicit erase behavior. Manifests request erase confirmation as a fallback for external ESP Web Tools consumers and skip unused Improv setup.

All five firmware profiles include the new direct clock diagnostic. Existing VTS startup, themes and watch faces are retained. The 21% application-slot and managed-storage reserve is unchanged. See [clock setup](CLOCK-SETUP.md) for the per-board retention limits and [validation](VALIDATION.md) for published build and hardware evidence.
