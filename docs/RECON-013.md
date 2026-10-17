# Four-board recon lab · 0.13.0

Waveshare AMOLED 1.75C, M5Stack StopWatch, Spotpear and Seeed XIAO Round now expose the same eight listening modes, 29 timed radio tools, GATT workspace, portal, connected-network tools and experimental CSI monitor. Lite remains a dedicated radar. The interface scales to each display; this release's live hardware checks use the Waveshare, not all four physical boards.

**Tools → Recon → Attacks** opens Wi-Fi and BLE attack lists directly. **Radio tools** contains surveys, captures and the other recon workflows. **Utilities** replaces Daily and holds Stopwatch, Clock, My badge, SD files and Activity. Image editing stays in the browser's Badge Studio; the redundant on-device Badge editor link is removed.

Normal screens, badges, utilities, passive recording and USB downloads keep both radios scheduled. Explicit single-radio modes or started connection/transmission sessions can specialize or own the radio. Home and Stop clean up active operations. A saved Wi-Fi-only or BLE-only mode remains selected until Dragon radar is chosen. The single ESP32-S3 radio cannot receive all channels simultaneously.

## Storage and reserve

Existing partition layouts stay unchanged. Waveshare uses its separate upper 16 MiB recon filesystem. The other full boards use their existing mounted filesystem alongside badges and history. Updates do not format either filesystem or NVS. New writes check available space before saving.

Capture RAM is 2 MiB on Waveshare and M5Stack, 1 MiB on Spotpear and 512 KiB on XIAO. Full boards retain all 40,333 IEEE prefixes using an independent bounded block decoder: 462,273 compressed flash bytes expand to 709,215 PSRAM bytes after a CRC check. No SD card is required. See [measured budgets](FLASH-BUDGET.md).

## Recon sessions and AP archives

Open Radio tools → Recon sessions, or use the web controls. A session records Wi-Fi or BLE observations without changing the existing radio schedule. Choose a unique name of 1–32 letters, digits, dashes or underscores. Stop and download a checked ZIP in the browser.

Each `/missions/<name>/` directory contains `session.json`, `obs.rlog`, `probes.rlog` and `relations.rlog`. The independent encoders follow Marauder's RCN1 (21-byte observations), PRB1 (45-byte probe records, at most 24 SSID bytes) and REL1 (12-byte station/AP pairs) formats. GPS fields are zero and the manifest declares `gps_fix: false`. This is file-format compatibility, not verified integration with every desktop consumer. Notebook CSV preserves complete 32-byte probe SSIDs when needed.

First observations and updates have separate record types. Updates require 30 seconds or a 12 dB change. Identity and relation tables are bounded at 512 and 128; drops are counted. Sessions stop after one hour or their size limit: 2 MiB on Waveshare and M5Stack, 256 KiB on Spotpear and XIAO. Up to 32 named directories are retained. Unexpected power loss can leave a manifest marked active; already-flushed records remain available, while buffered records can be lost.

AP archives save current observations as CSV, including raw SSID hex, RSSI, channel, security and age. Loading an archive displays historical records; it never makes an old AP eligible as a fresh attack target. Up to 32 archives are retained. Downloads have length and CRC checks.

USB commands: `mission start wifi|ble <name>`, `mission stop`, `mission status`, `mission list`, `mission get <name> <file>`, `mission remove <name>`; `survey save|load|get|remove <name>` and `survey list`.

## Reviewed recipes

The browser editor stores and runs bounded scripts: at most 512 ASCII bytes, 24 steps and one hour of declared wait/tool time. There are no loops, remote downloads or automatic startup scripts. A complete recipe is validated before it starts. Available lines:

```text
# A passive example; the previous listening mode is restored afterward.
MODE 0
MISSION WIFI
WAIT 15
MISSION STOP
STOP
```

Commands: `WAIT 1..600`, `MODE 0..7`, `CAPTURE WIFI|BLE|STOP|SAVE`, `MISSION WIFI|BLE|STOP`, `TOOL <0..28> <1..60 seconds> <1..100 rate>` and `STOP`. BLE tools retain their own ten-updates/second cap. Targets still need fresh observed evidence. Capture-save completion is awaited before the next step. A `STOP` line ends current radio/mission operations and then allows later steps; the separate Stop recipe control cancels the sequence. Home, Stop, a failed step or the runtime deadline ends the recipe, restores its prior mode/channel/rate/duration and leaves unsaved capture data available.

USB commands: `recipe put <name> <hex-encoded ASCII>`, `recipe list`, `recipe read|run|remove <name>`, `recipe status`, `recipe stop`. Up to 32 recipe files are retained. `tool source <unicast MAC>` changes eligible generated source fields in RAM; `tool source default` restores the detector address. AP-directed protocol fields continue to use their selected AP/client semantics.

## HID report inspection

BLE Lab decodes standard short HID Report Map items into report IDs, field lengths, usages, signed axes, relative values, keyboard keys/modifiers, mouse buttons and common consumer controls. GATT Report Reference descriptors identify report ID/direction; the GATT value does not itself include a report-ID prefix. Live events show decoded text where a complete supported map is available.

Limits: 512-byte maps, 128 fields, 16 reports, 512 stored usages and 8,192 bits per report. Malformed maps, unsupported items and wrong-length values remain raw evidence. No HID injection or USB keyboard emulation is added. Commands: `lab hid fields <map index>`, `lab hid decode <map index> <report ID> <direction 1|2|3> <hex>`, `lab hid events`.

## GATT bridge

After explicitly connecting to and discovering a complete owned peripheral, start the bridge from BLE Lab or the browser. It exposes a local application GATT server, forwards supported writes and notifications, and serves cached reads. Initial server registration briefly disconnects and reconnects the original peer. Local write acknowledgements do not prove that the remote device accepted a write; counters report forwarded writes and errors.

This is not a transparent ATT proxy: handles, pairing identities, protected permissions and proprietary behavior are not cloned. Pairing remains explicit. Stop/Home, client disconnect, peer loss or a ten-minute limit ends the session and restores radio scheduling. Commands: `lab bridge`, `lab bridge status`. See [validation](VALIDATION.md) for the tested synthetic fixture and remaining limits.

[Feature comparison](FEATURE-MATRIX.md) · [Credits](../CREDITS.md) · [BLE Lab](BLE-LAB.md) · [Radio toolkit](RADIO-TOOLKIT.md)
