"""Derive the DagNazty Dark artwork from the supplied black-on-white skull.

The supplied media/theme-logos/dagnazty.jpg stays unchanged. This recolors it
for a black dial: the outside field becomes transparent, the skull stays bone
white, the hood turns deep blood red, the halo ring bright red and the eye and
nose sockets glow. Antialiased edges keep their original coverage.
Requires Pillow and NumPy. Outputs media/theme-logos/dagnazty-dark.png and
media/theme-logos/dagnazty-cutout.png (original colors, transparent field).
"""
from collections import deque
from pathlib import Path
import numpy as np
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = Image.open(root / 'media/theme-logos/dagnazty.jpg').convert('L')
light = np.asarray(source, dtype=np.float32) / 255.0
h, w = light.shape
dark = light < 0.5


def components(mask):
    labels = np.zeros(mask.shape, dtype=np.int32)
    sizes = [0]
    for y, x in zip(*np.nonzero(mask)):
        if labels[y, x]:
            continue
        label = len(sizes)
        labels[y, x] = label
        queue = deque([(y, x)])
        size = 0
        while queue:
            cy, cx = queue.popleft()
            size += 1
            for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not labels[ny, nx]:
                    labels[ny, nx] = label
                    queue.append((ny, nx))
        sizes.append(size)
    return labels, sizes


# Light regions touching the border are the open field around the emblem.
light_labels, _ = components(~dark)
border = set(np.unique(np.concatenate([light_labels[0], light_labels[-1], light_labels[:, 0], light_labels[:, -1]]))) - {0}
field = np.isin(light_labels, list(border))

# Dark regions: the two biggest are the hood and the halo ring; the ring is the
# one with the larger average distance from the center (hood claws reach
# further out, but most of the hood sits near the middle). Small enclosed shapes are sockets.
dark_labels, dark_sizes = components(dark)
order = sorted(range(1, len(dark_sizes)), key=lambda i: dark_sizes[i], reverse=True)
yy, xx = np.mgrid[0:h, 0:w]
radius = np.hypot(xx - w / 2, yy - h / 2)
big = order[:2]
ring = max(big, key=lambda i: radius[dark_labels == i].mean())
hood = min(big, key=lambda i: radius[dark_labels == i].mean())

BONE = np.array([242, 234, 216], np.float32)
HOOD = np.array([92, 9, 18], np.float32)
RING = np.array([214, 22, 38], np.float32)
GLOW = np.array([255, 52, 44], np.float32)


def grow(labels, steps=3):
    # Spread each dark label a few pixels so antialiased edges pick its color.
    grown = labels.copy()
    for _ in range(steps):
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            shifted = np.roll(grown, (dy, dx), axis=(0, 1))
            grown = np.where(grown == 0, shifted, grown)
    return grown


near = grow(dark_labels)
ink = np.zeros((h, w, 3), np.float32)
ink[:] = HOOD
ink[near == ring] = RING
socket = (near != 0) & (near != ring) & (near != hood)
ink[socket] = GLOW

coverage = 1.0 - light  # 1 where the supplied art is black.
rgb = np.where(field[..., None], ink, ink * coverage[..., None] + BONE * light[..., None])
alpha = np.where(field, coverage, 1.0)
# Soften the field edge into the ring/hood rather than leaving a light halo.
alpha = np.clip((alpha - 0.08) / 0.92, 0, 1)
out = np.dstack([rgb, alpha * 255]).round().clip(0, 255).astype(np.uint8)
Image.fromarray(out, 'RGBA').save(root / 'media/theme-logos/dagnazty-dark.png', optimize=True)

# The light dial uses the original black-on-white colors with the same
# transparent field, so a larger skull needs no white backing disc.
gray = (light * 255)[..., None].repeat(3, axis=2)
cutout = np.dstack([gray, alpha * 255]).round().clip(0, 255).astype(np.uint8)
Image.fromarray(cutout, 'RGBA').save(root / 'media/theme-logos/dagnazty-cutout.png', optimize=True)
print(f'hood={dark_sizes[hood]}px ring={dark_sizes[ring]}px sockets={int(socket.sum())}px')
