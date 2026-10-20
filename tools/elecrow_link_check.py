"""Keep Elecrow board hooks unique despite the existing SDK radio override."""
from pathlib import Path
import os
import subprocess

Import("env")

# The pinned radio adapter intentionally overrides one SDK symbol.
env.Append(LINKFLAGS=["-Wl,-zmuldefs"])


def check_board_hooks(source, target, env):
    package = Path(env.PioPlatform().get_package_dir("toolchain-xtensa-esp32s3"))
    nm = package / "bin" / ("xtensa-esp32s3-elf-nm.exe" if os.name == "nt" else "xtensa-esp32s3-elf-nm")
    hooks = {name: [] for name in (
        "boardBegin()", "boardLoop()", "scr_lvgl_init()",
        "set_brightness(unsigned char)", "boardSensorsBegin()",
    )}
    for obj in (Path(env.subst("$BUILD_DIR")) / "firmware" / "src").glob("*.o"):
        symbols = subprocess.check_output([str(nm), "-g", "-C", "--defined-only", str(obj)], text=True)
        for line in symbols.splitlines():
            fields = line.split(maxsplit=2)
            if len(fields) == 3 and fields[1] == "T" and fields[2] in hooks:
                hooks[fields[2]].append(obj.name)
    for name, owners in hooks.items():
        if owners != ["board_crowpanel_21.cpp.o"]:
            raise RuntimeError(f"Elecrow hook {name} must have exactly one board driver: {owners}")
    print("Verified unique Elecrow display, input and sensor hooks")


env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", check_board_hooks)
