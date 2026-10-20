# MITM theme

Available in firmware 0.13.26. Select **MITM** in Settings → Theme & radar labels, then open Clock. USB: `radar theme mitm`; full-board clock face ID: 42. Theme ID 39 is appended without renumbering existing selections.

The supplied photo fills the circular dial behind a translucent time/date panel. The segmented rim, panel border and date cycle through magenta, violet, cyan, lime and amber over 15 seconds. A white dot marks seconds. Menus and radar use matching dark violet/cyan colors. The image is embedded; no SD card is required.

Actual LVGL renders cover 240, 360, 466 and 480 pixels, including unset time and five color phases. The combined release includes browser version-gating and theme-selection tests and native decoder checks. Physical display validation of this artwork remains pending.
