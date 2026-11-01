# Adult Mode (18+)

Adult Mode is **off by default**. Enable it explicitly at the bottom of
**Settings**. Dial has the same opt-in at the bottom of its Settings menu;
the compact Waveshare Lite has a Settings page after Device Status (hold to
advance pages, tap to toggle). Existing saved detector masks cannot enable it
when upgrading. Disabling it removes previously classified adult candidates.

The feature listens for Bluetooth LE advertisements. It does not pair,
connect, query GATT, issue commands or control a device. It does not receive
the sub-GHz remote-control protocols used by some products. The supported
boards' built-in radios cannot substitute for a PortaPack SDR.

| Evidence actually advertised | Result |
|---|---|
| Lovense-style `LVS-` / `LOVE-` name with a nonempty alphanumeric suffix | Weak, spoofable candidate |
| That name plus FFF0 or Nordic UART service | Medium candidate |
| Lovense generation-three custom service | Medium candidate |
| We-Vibe `f000bb03-0451-4000-b000-000000000000` service | Medium candidate |
| Satisfyer `51361500-c5e7-47c7-8a6e-47ebc99d80e8` control service | Medium candidate |
| Generic UART alone; common names such as Nova, Sync or Bloom | No adult classification |

Services that a product only exposes after a connection will not be detected
by this passive feature. Advertisements can be absent, changed or spoofed;
these clues establish neither a specific model, use, owner nor intent.
Malformed fields and UUID bytes embedded inside unrelated data are rejected.
Stronger existing surveillance signatures retain precedence.

Adult candidates are quiet, live radar/target entries. They do not create
automatic popups, tones, flash detection history or SD detection-event rows.
The separate user-started raw capture, BLE notebook and wardrive facilities
still record ordinary broadcast radio data under their existing rules.

## Sources and implementation

Independent matching of public protocol identifiers documented by
[STPIHKAL: Lovense](https://buttplug.io/stpihkal/protocols/lovense/),
[We-Vibe](https://buttplug.io/stpihkal/protocols/we-vibe/), and
[Satisfyer](https://buttplug.io/stpihkal/protocols/satisfyer/), reviewed
2026-11-01. No upstream application source was copied.

Mayhem's
[adult-toys-controller application](https://github.com/portapack-mayhem/mayhem-firmware/tree/next/firmware/application/external/adult_toys_controller)
is a transmitter/controller, not a passive detector; its control functionality
is not part of Dragon Detector.

Native regression fixtures cover opt-in migration, supported service encodings,
name-plus-service correlation, malformed boundaries and negative examples.
Reception against physical products has not been field-validated.
