"""Check the LockpickingVlad artwork in firmware/src/vlad_art.cpp.

The 466 px portrait is stored zlib-deflated and inflated on the device by
vladPortraitImage(). It must decode to a complete LVGL 8-bit indexed image: a
256-entry opaque palette followed by 466 x 466 pixel indices. The other sizes stay
raw indexed images whose data matches their declared size.
"""
from pathlib import Path
import re
import zlib

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "firmware/src/vlad_art.cpp").read_text(encoding="utf-8")


def numbers(body):
    return bytes(int(x) for x in body.replace("\n", "").split(",") if x.strip())


def check_indexed(data, size, label):
    assert len(data) == 1024 + size * size, f"{label}: {len(data)} bytes, expected {1024 + size * size}"
    assert all(data[i + 3] == 255 for i in range(0, 1024, 4)), f"{label}: palette is not opaque"


sections = re.split(r"^#(?:if|elif) SCREEN_RES >= (\d+)$", source, flags=re.M)
sizes = [int(s) for s in sections[1::2]]
assert sizes == [466, 412, 360, 240], sizes
for size, body in zip(sizes, sections[2::2]):
    body = body.split("#else")[0]
    packed = re.search(r"static constexpr size_t vladPortraitBytes = (\d+);\nstatic const uint8_t vladPortraitPacked\[\] = \{\n(.*?)\n\};", body, re.S)
    if size == 466:
        assert packed, "466 px portrait must be stored packed"
        assert "#define VLAD_PORTRAIT_PACKED 1" in body
        data = zlib.decompress(numbers(packed.group(2)))
        assert len(data) == int(packed.group(1)), "declared size differs from the inflated size"
        check_indexed(data, 466, "466 portrait")
        assert "extern const lv_img_dsc_t vladPortrait " not in body
    else:
        assert not packed, f"{size} px portrait is expected raw"
        raw = re.search(r"static const uint8_t vladPortraitPixels\[\] = \{\n(.*?)\n\};\nextern const lv_img_dsc_t vladPortrait = \{\{LV_IMG_CF_INDEXED_8BIT,0,0,(\d+),(\d+)\}", body, re.S)
        assert raw and int(raw.group(2)) == int(raw.group(3)) == size, f"{size} px portrait descriptor"
        check_indexed(numbers(raw.group(1)), size, f"{size} portrait")
    seal = re.search(r"static const uint8_t vladSealPixels\[\] = \{\n(.*?)\n\};\nextern const lv_img_dsc_t vladSeal = \{\{LV_IMG_CF_INDEXED_8BIT,0,0,(\d+),(\d+)\}", body, re.S)
    assert seal and seal.group(2) == seal.group(3) == str(round(size * .14)), f"{size} px seal descriptor"
    check_indexed(numbers(seal.group(1)), int(seal.group(2)), f"{size} seal")
assert "const lv_img_dsc_t* vladPortraitImage()" in source
print("PASS LockpickingVlad art: 466 px portrait inflates to a complete indexed image; 412/360/240 raw images and every seal match their descriptors")
