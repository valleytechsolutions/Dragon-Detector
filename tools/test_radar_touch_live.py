"""Opt-in StopWatch UI checks through its LVGL pointer driver; no RF writes."""
import argparse
import asyncio
import json
import re
import time
from pathlib import Path
from test_ble_lab_live import Board


async def run(args):
    board = Board(args.port)
    transcript = []

    async def command(text, seconds=.2):
        response = await board.command(text, seconds)
        marker = ("TOUCH_INJECT_OK" if text.startswith("touch inject ") else
                  {"badge info":"BADGE_INFO", "radar dots":"RADAR_DOTS_END",
                   "radar status":"RADAR_SETTINGS", "ui status":"UI_STATUS"}.get(text))
        deadline=time.monotonic()+5
        while marker and marker not in response and time.monotonic()<deadline:
            response+=board.port.read(board.port.in_waiting or 1).decode(errors="replace")
            await asyncio.sleep(.01)
        transcript.append({"command": text, "response": response})
        return response

    async def dot():
        await command("screen radar")
        rows = await command("radar dots")
        points = re.findall(r"RADAR_DOT \d+ x=(\d+) y=(\d+) color=([0-9a-f]+)", rows)
        assert points, "No visible dots; test needs an observed signal"
        assert len({p[2] for p in points}) == len(points), "Duplicate displayed color"
        return tuple(map(int, points[0][:2]))

    async def touch(x, y, pressed):
        response = await command(f"touch inject {x} {y} {int(pressed)}", .12)
        assert "TOUCH_INJECT_OK" in response, response

    async def details():
        for _ in range(10):
            status=await command("ui status")
            if "page=4 " in status:return
            await asyncio.sleep(.1)
        raise AssertionError(status)

    try:
        info = await command("badge info")
        assert "BADGE_INFO 0.13.22 466 READY M5Stack StopWatch C152" in info, info
        x, y = await dot()
        await touch(x, y, True)
        status = await command("radar status")
        assert "selected=1" in status, status
        # Previously, a redraw using an older frame timestamp erased this selection.
        await asyncio.sleep(.4)
        assert "selected=1" in await command("radar status")
        await touch(0, 0, False)
        await touch(x, y, True)
        await touch(x, y, False)
        await details()
        x, y = await dot()
        await touch(x, y, True)
        await touch(x, y, False)
        await asyncio.sleep(.5)
        await touch(210, 335, True)
        await touch(210, 335, False)
        await details()
        print("PASS StopWatch: immediate selection, selection survives redraw, "
              "release movement, double-tap details, summary button, distinct colors")
    finally:
        await command("touch inject 0 0 0")
        await command("screen radar")
        board.port.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(transcript, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    asyncio.run(run(parser.parse_args()))
