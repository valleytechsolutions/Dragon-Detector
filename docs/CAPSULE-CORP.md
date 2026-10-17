# Capsule Corp

Available in firmware **0.13.15** on Waveshare AMOLED, M5Stack StopWatch C152, Spotpear, Seeed XIAO + Round Display and Waveshare Lite.

Select **Capsule Corp** under Settings → Theme & radar labels, or in the web flasher's Radar appearance selector. Lite cycles themes with a short BOOT press on the radar or clock page. The USB command is `radar theme capsule-corp` (persistent ID 31); the full-board watch is ID 34.

The radar has a silver bezel, emerald square grid, red button detail and yellow contact markers. The original detection and signal-strength placement remain active; family labels retain their standard colors. Its refresh band is decorative. It does not report a measured direction or distance, and does not add fictional Dragon Ball targets.

The watch combines the Capsule Corp emblem with a white-and-blue analog dial, rounded hands, green 24-hour display and separate date. An amber marker travels around the rim each minute. The same design is rendered at each panel's native size, including Lite. Full-board menus, settings and dialogs use the matching light palette.

The shared Dragon Ball/BWM boot animation and existing saved theme indices remain unchanged. USB connection sets and verifies local date, time and UTC offset; see [clock setup](CLOCK-SETUP.md).

## Artwork

- Supplied original: [`capsule-corp-source.webp`](../media/theme-logos/capsule-corp-source.webp), copied unchanged from `Capsule_corp_logo.webp`.
- Transparent derivative: [`capsule-corp.png`](../media/theme-logos/capsule-corp.png), prepared with the built-in image generation tool. This is a generated derivative, not a pixel-exact crop. The blue exterior is removed while the complete emblem and banner remain.
- Firmware assets are encoded from that PNG at native panel sizes. Checksums and provenance are recorded in [THEME-ARTWORK.json](THEME-ARTWORK.json).
- The bezel, grid, hands and indicators are drawn in firmware. The [official Dragon Ball radar reference](https://en.dragon-ball-official.com/news/01_3263.html) informed the instrument styling.

Background extraction prompt:

> Use case: background-extraction. Edit the supplied Capsule Corp logo for a firmware UI asset. Remove ONLY the solid blue outer background outside the thick black outline of the complete circular C emblem and the curved CAPSULE CORP. banner. Preserve the exact emblem geometry, original black outlines, white areas, blue inside the C, and the exact text CAPSULE CORP. Keep the complete banner and outline intact; no redesign, no extra shading, no mockup, no new elements. Center the complete logo with a small transparent margin. Output an actual transparent PNG; all pixels outside the emblem and its banner must have alpha zero.

Capsule Corp and Dragon Ball belong to their respective rights holders. This is a community theme, not an official or endorsed product. See [attributions](../CREDITS.md).
