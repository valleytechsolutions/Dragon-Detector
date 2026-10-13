# Daily use

## Radar and tracking

The green radar starts in **Candidates** mode. Repeated fresh signature matches get markers; weak vendor-only hints remain in Wi-Fi/Bluetooth contact lists. Tap a marker for its address, evidence and confidence. Tools → Settings → Radar toggles all contacts. No candidates means no recent qualifying evidence, not a guarantee that no surveillance devices are nearby.

Tools → Wi-Fi or Bluetooth opens all contacts. Select one and choose **Track signal** for dedicated listening. Watch filtered dBm and whether the signal is rising/falling. RSSI depends on obstructions, orientation and transmit power; do not treat it as distance or a direction arrow. After ten seconds without an observation, the radar reports a stale target. Settings → Tracking returns to balanced Wi-Fi/BLE scanning.

Swipe upward from the radar's bottom edge or tap Tools. Lists scroll normally. StopWatch yellow returns to radar; blue opens tools and, from tools, opens badge. Waveshare BOOT toggles radar/tools. Touch wakes the dimmed display.

## Badge Studio

1. Open the [Badge Studio](https://valleytechsolutions.github.io/DragonBall-Z-Radar/#studio) in desktop Chrome/Edge and connect a board running 0.3.x. Close the flasher's serial connection first.
2. The board reports its 360×360 or 466×466 display size. Pick PNG, JPEG or WebP (under 20 MB / 40 MP).
3. Drag to position. Use Zoom, Center, Fill circle, Fit whole image and Background. The circular preview hides pixels outside the physical display. Transparent pixels are composited onto the chosen background.
4. Upload & show badge. The board validates dimensions, exact byte count and CRC-32 before replacing the saved image. A failed/incomplete transfer keeps the prior image. Scanning pauses during transfer and resumes afterward.
5. Your badge is shown and becomes the startup view. Hold the image to open tools. Settings → Boot to radar changes startup back to radar.

Images are processed locally in the browser and sent by USB, not uploaded to GitHub or a cloud service. This version uses a USB web UI; it does not serve a Wi-Fi upload portal. Factory reflashing removes saved badge files; matching 0.3.x split updates preserve them.

## Daily settings

Brightness defaults to 50%, cycles through 25/50/75/100%, and is saved. After 45 seconds without touch/key activity it drops to 8%; activity restores it. The image shifts a few pixels once a minute. These reduce sustained exposure and display power but do not guarantee burn-in prevention or a particular battery runtime. Scanning continues in badge mode until paused.

Event logging is off by default. Spotpear can enable logs and bounded PCAP capture with a FAT32 microSD card. The AMOLED builds keep observations in RAM and use internal LittleFS for the badge. No microphone/music workflow is compiled in 0.3.x; older Spotpear multimedia firmware remains available separately.

## USB diagnostics

115200 baud, newline-terminated commands: `status`, `contacts`, `selftest`, `pause`, `resume`, `screen radar`, `screen menu`, `screen badge`, `badge info`, `badge boot on`, `badge boot off`, `screenshot`. Screenshots encode the LVGL renderer as RGB565; they do not verify physical panel color transfer. `status` reports scan counts, dropped packets, heap and board display resolution.
