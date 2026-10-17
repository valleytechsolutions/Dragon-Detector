"""Export web-sized copies of the selected transparent project artwork."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = Image.open(root / 'media/branding/dragon-detector.png').convert('RGBA')
assert source.width == source.height and source.getextrema()[3][0] == 0
for name, size in [('dragon-detector.png', 512), ('dragon-detector-icon.png', 64)]:
    source.resize((size, size), Image.Resampling.LANCZOS).save(root / 'web/assets' / name, optimize=True)
for size in (240, 360, 466):
    source.resize((size, size), Image.Resampling.LANCZOS).save(root / 'web/assets' / f'dragon-badge-{size}.png', optimize=True)
print('Exported transparent 512px web logo and 64px favicon.')
