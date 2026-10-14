# Seeed XIAO ESP32-S3 + Round Display

Current implementation: [`firmware/src/board_xiao_round.cpp`](../../firmware/src/board_xiao_round.cpp).

Build: `python -m platformio run -e xiao_round`.

Package: `python tools/build_release.py --board xiao_round`.

[Hardware, controls, SD and installation guide](../../docs/XIAO-ROUND.md). Uses the Round Display microSD slot, not the Sense camera expansion's slot. Do not flash another board profile onto this 8 MB device.
