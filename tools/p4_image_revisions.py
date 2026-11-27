"""Apply the silicon revision limits used to compile each P4 image."""

import json
from pathlib import Path

Import("env")

build = Path(env.subst("$BUILD_DIR"))
for image, config_dir in (("${PROGNAME}.bin", build), ("bootloader.bin", build / "bootloader")):
    target = env.File(env.subst(f"$BUILD_DIR/{image}"))
    if target.has_builder():
        config_file = config_dir / "config" / "sdkconfig.json"
        config = json.loads(config_file.read_text(encoding="utf-8"))
        target.get_build_env().Append(ELF2BINFLAGS=[
            "--min-rev-full", str(config["ESP_REV_MIN_FULL"]),
            "--max-rev-full", str(config["ESP_REV_MAX_FULL"]),
        ])
        env.Depends(target, str(config_file))
