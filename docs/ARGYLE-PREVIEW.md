# Argyle locksport theme

Argyle ships with the four-theme locksport collection in [0.13.38](RELEASE-0.13.38.md). The existing public showcase images are unchanged.

- **Radar:** the supplied black-and-white emblem, light dial, dark rings and
  one rotating dimple-style pick. The pick has a banded cylindrical handle,
  slim shaft and short sideways flag tip. It replaces the normal fan sweep
  and center arrow; live signal markers and labels remain functional.
- **Clock:** the supplied emblem with two hook-shaped picks. The longer pick
  reads minutes and the shorter pick reads hours. A rim marker shows seconds,
  with digital time and date below.
- **Locksport timer:** START/PAUSE/RESUME, SPLIT, OPEN to record and stop, LOG
  and confirmed RESET. Uses the existing monotonic timer; the latest 50 splits
  are held in RAM and clear at reboot.

Select **Argyle** in Radar Style or send `radar theme argyle`. Theme ID 42 selects
watch ID 45. VectorPotential keeps theme ID 41 and watch ID 44.

The sweep is decorative animation, not a measured signal direction. It pauses
when scanning is paused, using the shared radar animation clock.

`tools/render_argyle.py` renders the actual drawing code at 240, 360, 466 and
480 pixels, three clock positions, an unset clock and four radar sweep angles.
See [credits](../CREDITS.md) for the original artwork and tool reference.
