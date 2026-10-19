# Dragon Detector 0.13.20

**Find My Beep now connects directly to your selected tracker.** An AirTag sound request was acknowledged on the Waveshare 1.75C and its owner confirmed the beep.

- Supports the known AirTag, FMNA and DULT sound services, with response subscriptions where required, explicit failure details and radio restoration after a direct request.
- Adds configurable iBeacon UUID, major, minor and measured power, plus copying fields from a selected beacon.
- Fixes browser advertisement replay using a stale selection instead of the device shown in the dropdown.
- Preserves existing themes, settings, saved files and board partition layouts when using **Update matching Dragon Detector**.

[Install firmware](https://valleytech-dragon-detector.pages.dev/#install) · [Find My Beep instructions](BLE-TOOLS-01320.md) · [Validation and hardware limits](VALIDATION.md)

All six board profiles are packaged. Active BLE tools run on the five full profiles; Lite remains passive. The non-C Waveshare AMOLED 1.75 port is still experimental and awaits hardware testing. Tracker access rules and phone behavior vary; an acknowledged request alone is not proof of an audible sound.
