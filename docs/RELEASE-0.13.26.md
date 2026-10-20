# Dragon Detector 0.13.26

OGG now has an analog green-and-black clock with the original eye emblem, a fine minute track, and two beveled knife hands. The hour knife is shorter and wider; the minute knife is longer and lighter. A small 24-hour readout and date remain below the hands. An unset clock shows a setup prompt instead of a fabricated hand position.

XomBi retains the supplied zombie-hacker portrait and adds two original undead figures around a moss-edged digital time window, worn bezel markings and a seconds track. These details are drawn directly by the firmware and scale to every supported display, including Lite.

The coordinated [MITM theme](MITM-THEME.md) adds the owner-supplied photograph with slowly changing neon accents. It appends a new theme ID; existing selections keep their IDs. OGG and XomBi keep their existing names and selections. The public showcase photographs are unchanged.

This release retains the [0.13.25 RTC fixes](RELEASE-0.13.25.md) and all nine profiles, including the [LILYGO round 1.75 beta](LILYGO-AMOLED-175.md). Select **Update** for your exact matching board to preserve settings and files. No SD artwork pack is required.

Choose **Ogg**, **XomBi**, or **MITM** in Settings → Theme & radar labels, then open Clock. On full boards, USB commands are `radar theme ogg`, `radar theme xombi`, and `radar theme mitm`. Clock faces remain independently selectable.

Validation includes actual LVGL renders at 240, 360, 466 and 480 pixels, unset-time rendering, 109 browser-module tests and native compressed-artwork decoder checks. These are software-rendered previews; physical panel checks of the new artwork remain pending. RTC off-time testing remains confirmed on StopWatch, with XIAO, Waveshare and LILYGO awaiting their own hardware checks.
