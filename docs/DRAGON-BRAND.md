# Dragon Detector artwork · 0.13.13

![Orange Dragon Ball with the red BWM emblem](../web/assets/dragon-detector.png)

The project owner requested the supplied orange Dragon Ball with its star replaced by the BWM emblem, keeping the simple illustrated Dragon Ball aesthetic. The selected master is [media/branding/dragon-detector.png](../media/branding/dragon-detector.png). It is an AI-generated derivative, not a pixel-exact composite of the supplied artwork.

Original references are retained unchanged as [dragon-ball-source.png](../media/branding/dragon-ball-source.png) and [bwm1-source.png](../media/branding/bwm1-source.png). Their hashes and the selected derivative's hash are recorded in [THEME-ARTWORK.json](THEME-ARTWORK.json). Artwork and trademark rights remain with their respective owners; these images are not relicensed under the software license.

Generation used the built-in image tool in edit mode with both original references and transparent_background=true. The selected result followed the owner's request to simplify an earlier draft. Final prompt:

> Use case: compositing. Create the final SIMPLE Dragon Detector icon by editing the two supplied references. Image 1 supplies the simple orange Dragon Ball aesthetic and circular shape. Image 2 supplies the BWM emblem to insert. Replace the star in Image 1 with the red-and-black BWM emblem from Image 2, centered and about 42 percent of the ball diameter in height. Preserve its exact distinctive shape and proportions. User's key requirement: simple Dragon Ball aesthetic. Use clean flat vector-style solid orange/gold fills, two or three broad curved darker-orange shading shapes and a single small cream-white highlight at the upper-left. NO gradients, NO glow, NO texture, NO grain, NO light rays, NO realism, NO extra borders, NO text. Keep the BWM mark clean red with crisp black outline, no glow around it. Remove the original star entirely. Perfect round silhouette centered within a square image, 6 percent transparent margin on all four sides. Only the circular ball is opaque; outside is genuine alpha transparency with no stray pixels, no checkerboard and no cast shadow. The clean app icon must remain recognizable on a 240-pixel display.

`tools/encode_project_brand.py` exports web-sized PNG copies. `tools/encode_radar_logo.py` converts the master into native panel assets. Startup/watch copies use a 16-color palette followed by lossless block compression; the same decoded image is reused for both screens. No per-frame image allocation or runtime scaling is needed.

The universal startup fades the mark in over 900 ms and leaves after 2.2 seconds. The Dragon Ball watch keeps the logo above its 24-hour clock and date, with seconds around the rim. It replaces the previous BWM face at saved ID 0; other saved IDs are unchanged.
