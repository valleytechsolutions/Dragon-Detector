"""Exercise the 1.85B's actual touch publication and LVGL pointer callbacks."""
from pathlib import Path
import argparse
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / ".work/waveshare185b-pointer"
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument("--lvgl-library", type=Path,
                    help="Existing native LVGL archive with matching lv_conf.h beside it")
args = parser.parse_args()
lv = root / "hardware/spotpear/lib/lvgl"
zig = [sys.executable, "-m", "ziglang"]
includes = ["-DLV_CONF_INCLUDE_SIMPLE", "-I" + str(work), "-I" + str(lv),
            "-I" + str(root / "firmware/include")]
if args.lvgl_library:
    library = args.lvgl_library.resolve()
    shutil.copyfile(library.parent / "lv_conf.h", work / "lv_conf.h")
else:
    library = work / "lvgl.lib"
    (work / "lv_conf.h").write_text("#ifndef LV_CONF_H\n#define LV_CONF_H\n"
                                    "#define LV_COLOR_DEPTH 16\n#define LV_TICK_CUSTOM 0\n"
                                    "#define LV_USE_LOG 0\n#endif\n")
    sources = sorted((lv / "src").rglob("*.c"))
    flags = ["-lc", "-O", "ReleaseFast", "--name", "lvgl", *includes,
             "-femit-bin=" + str(library), "-cflags", "-std=c99", "--", *map(str, sources)]
    response = work / "lvgl.rsp"
    response.write_text(" ".join('"' + arg.replace("\\", "/") + '"' for arg in flags))
    subprocess.run(zig + ["build-lib", "@" + str(response)], cwd=root, check=True)

def function(text, signature):
    start = text.index(signature)
    cursor = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        cursor += 1
    return text[start:cursor]

board = (root / "firmware/src/board_waveshare_185b.cpp").read_text()
ui = (root / "firmware/src/ui.cpp").read_text()
production = [function(ui, "bool uiTouch(bool pressed,")]
production += [function(board, signature) for signature in
               ["void publishTouchSample(bool pressed,", "void touchRead(lv_indev_drv_t*",
                "void waveshare185bResetPointer()", "bool waveshare185bServicePointer()"]]
(work / "production.inc").write_text("\n".join(production))
exe = work / ("pointer-test.exe" if sys.platform == "win32" else "pointer-test")
subprocess.run(zig + ["c++", "-std=c++17", "-O1", "-UNDEBUG", *includes,
                      str(root / "tests/waveshare185b_pointer_native.cpp"),
                      str(library), "-o", str(exe)], cwd=root, check=True)
subprocess.run([str(exe)], cwd=root, check=True)
