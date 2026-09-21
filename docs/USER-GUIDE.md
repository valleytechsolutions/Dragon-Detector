# Using Dragon Detector

## Radar and scanning

Dots represent recently received Wi-Fi/BLE observations. Radius follows RSSI; it is not a distance measurement. The angle is a stable visual arrangement, not compass bearing. A moving sweep is a UI animation, not directional radio hardware.

Tap a dot or contact row to inspect its advertised name, address, RSSI, channel, candidate family, confidence and evidence. Device labels with a question mark are candidates. Alerts describe observed patterns and do not establish who caused them.

Open Recon to browse Wi-Fi contacts, Bluetooth contacts, alerts and channel activity. Scroll vertically within lists. Refresh a contact list to rebuild its rows. Selecting a Wi-Fi channel locks monitoring there and pauses BLE; choose Auto to resume alternating scans. Settings provides radio pause/resume and logging controls.

## Navigation by board

| Control | StopWatch | Spotpear |
|---|---|---|
| Return to radar | Yellow KEY A, GPIO2 | On-screen navigation |
| Open tools | Blue KEY B, GPIO1; or bottom-edge radar swipe | Bottom-edge radar swipe |
| Switch menu tab | Blue button cycles tabs, or tap a tab | Tap a visible tab |
| Scroll | Vertical movement inside list | Vertical movement inside list |

## Logs and captures

Spotpear uses microSD. StopWatch uses internal flash and has no built-in SD slot. StopWatch event logging is opt-in to limit flash wear; logs rotate at 256 KiB and each capture stops at 512 KiB. Spotpear limits are documented in its feature guide. Timestamps are uptime, not UTC. Stored observations may include nearby device identifiers; inspect them before sharing a bug report.

The file browser opens folders and previews small text files. Binary PCAPs should be downloaded for analysis rather than read as text. StopWatch USB diagnostics at 115200 baud include `status`, `contacts`, `selftest`, `pause`, `resume`, `ls /dragon`, `capture on`, and `capture off`. Its `get PATH` command emits a `FILE_BYTES` header, exact binary data, then `FILE_END`; a regular terminal is not a binary download utility.

## StopWatch badge images

![Badge mode](../hardware/m5stack-stopwatch/docs/badge.png)

Install the helper dependencies from the repository root:

```powershell
py -m pip install pillow pyserial
py hardware/m5stack-stopwatch/tools/badge_import.py "C:/Pictures/my-logo.png" --port COM4 --name MyLogo
```

Replace the input image and COM port. PNG/JPEG/WebP/BMP are converted to 466×466 RGB565. `--fit contain` keeps the whole rectangular image; `--fit cover` fills the square. The physical circular screen can clip square corners, so keep essential content near the center. Each image occupies approximately 424 KiB. Use a unique name for each upload; existing names are protected.

For a graphical importer, run `py -m http.server 8000 --directory hardware/m5stack-stopwatch/tools`, open `http://localhost:8000/Badge-Importer.html` in Chrome/Edge, choose an image, connect the device and import. Close any serial monitor first. Stop the local server with Ctrl+C when finished. The Python importer was tested end-to-end; the browser importer still needs end-to-end validation.

Open Tools → Badge mode. Tap to advance; hold to toggle a ten-second slideshow. Yellow exits to radar. The scanner continues in badge mode, except while USB transfers temporarily pause it. Settings offers optional alert sound and vibration.

## Spotpear audio

The Spotpear variant retains its original local music playback and manual microphone recording controls. These features are intentionally absent from StopWatch. Use its Media tab and microSD file browser; stop/save a recording before playback or transferring files.
