# 44Delta locksport theme

Available in firmware **0.13.41** on all eleven profiles. Choose **44Delta** in
Settings → Theme & radar labels, the Dial theme menu, or the web flasher's
Device tools. USB: `radar theme 44delta`. Persistent theme ID 45 and clock ID 48
are appended; existing saved selections retain their meanings.

The original cyan 44 DELTA skull and wordmark sit on black. The analog clock has
slender Roman quarter markers, silver hook-pick hands with cyan handle scales,
and a small moving seconds marker at the rim. The longer, brighter pick shows
minutes; the shorter, darker pick shows hours. It has no digital time or date
overlay. An unset clock displays SET TIME until the clock is synchronized.

Version **0.13.42** enlarges the clock's original skull/wordmark by about 60%
in each dimension and removes the LOCKSPORT caption at the bottom of the dial.
The clock uses a separate native-size logo asset, so radar and timer headers
retain their existing layout.

The radar centers the supplied black spider-engraved padlock with its brass
shackle. Its cyan sweep travels around the outside of the padlock. Signal dots
and labels retain their detection-family colors. The sweep is decorative; it
does not indicate measured signal direction.

The matching Locksport Timer retains Start/Pause/Resume, Split, Open, Log and
confirmed Reset on full profiles. Open ends the current attempt. Dial and Lite
use their existing compact timer controls; they do not gain the full log page.
Timer splits are kept in RAM and clear at reboot.

## Artwork

- `media/theme-logos/delta44-logo-source.webp`: byte-for-byte supplied banner;
  the original logo geometry is retained. Firmware only resizes and encodes it.
- `media/theme-logos/delta44-padlock-source.webp`: byte-for-byte supplied photo.
- `media/theme-logos/delta44-padlock.png`: imagegen background-extraction
  derivative of that photo, used at native panel sizes.
- Clock hands, rings and sweep are original vector drawing code. They are
  decorative lockpick profiles, not an exact manufacturer's product drawing.

44Delta owns its branding. Inclusion follows the project owner's request and
does not imply endorsement. Source hashes are in [THEME-ARTWORK.json](THEME-ARTWORK.json).

The image-generation prompt used for the padlock derivative was:

> Use case: background-extraction. Asset type: small centered radar emblem for an embedded watch screen. Edit target: the supplied photograph. Extract ONLY the entire black rectangular padlock with its gold/brass shackle. Preserve the actual photographed padlock design, the long white/silver geometric spider engraving and delta arrow marking, the small holes and the black brushed-metal body. Remove the separate key, fabric, and all surrounding background. Keep the complete closed shackle and padlock body, with crisp clean edges and a genuinely transparent background, including the space within the shackle. Center the upright padlock and crop tightly with a small transparent margin. No added text, no extra symbols, no scenery, no square backdrop, no glow. The padlock should remain recognizable as this exact provided product; do not redesign the engraving.

`tools/render_delta44.py` renders the actual LVGL drawing code at 240, 360, 466
and 480 pixels with several hand/sweep positions and unset time. Its radar
labels/buttons are illustrative; USB screenshots verify the complete on-device UI.
