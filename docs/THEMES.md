# Radar themes

Firmware **0.13.1** adds three optional themes on Waveshare AMOLED, M5Stack StopWatch, Spotpear and Seeed XIAO + Round Display.

Open **Tools → System → Settings → Radar theme & labels** and scroll to the theme. It saves immediately; tap Home to return to radar. Or connect through the [web tools](https://valleytechsolutions.github.io/Dragon-Detector/#detector), choose Radar appearance → Theme, and save. Older firmware keeps its original three choices until updated. Waveshare Lite remains a fixed green radar without settings.

| Valleytech | Biscuit Shop | Marauder |
|---|---|---|
| ![Valleytech radar](../media/radar-valleytech-0131.png) | ![Biscuit Shop radar](../media/radar-biscuit-0131.png) | ![Marauder radar](../media/radar-marauder-0131.png) |
| Silver rings, cyan sweep, VTS logo | Amber rings, cream highlights, Biscuit Shop logo | Black field, red sweep, JCMK logo |

These are CRC-verified 466×466 logical display captures from the Waveshare, not panel photographs. The small logo is sized natively for each panel. The Valleytech artwork has a light backing for readability; Biscuit's white logo uses transparency; the JCMK source retains its black background. [Unmodified sources and attribution](../CREDITS.md#radar-theme-artwork--0131).

Green radar (default), Dragon Ball and Black / gold / red keep their BWM1 logo. Existing saved selections stay unchanged. All six themes share the same [26 device-family colors](RADAR-COLORS.md), labels and leader lines. Radio status, target count, battery indicator, RSSI placement, scan scheduling and badge images are unaffected by a theme change.

USB names: `radar theme dragon`, `green`, `gold`, `valleytech`, `biscuit`, or `marauder` (prefix each with `radar theme`). `radar status` reports saved indices 0–5 in that order. Unknown themes are rejected without changing the current selection.
