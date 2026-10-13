"""Remove workstation-specific absolute paths from public firmware diagnostics."""
from pathlib import Path

Import("env")

project = Path(env.subst("$PROJECT_DIR")).resolve().as_posix()
profile = Path.home().resolve().as_posix()
env.Append(
    CCFLAGS=[
        f"-ffile-prefix-map={project}=DragonDetector",
        f"-fmacro-prefix-map={project}=DragonDetector",
        f"-ffile-prefix-map={profile}=/build-user",
        f"-fmacro-prefix-map={profile}=/build-user",
    ]
)
