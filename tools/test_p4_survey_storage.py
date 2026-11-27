"""Run the P4 survey writer against a simulated card and synthetic access points."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    target = Path(temp) / ("survey-test.exe" if sys.platform == "win32" else "survey-test")
    subprocess.run([sys.executable, "-m", "ziglang", "c++", "-std=c++17", "-DP4_SURVEY_STANDALONE",
                    "-I" + str(root / "firmware/include"), "-I" + str(root / "firmware/src"),
                    str(root / "tests/p4_survey_storage_test.cpp"), "-o", str(target)], check=True)
    subprocess.run([str(target)], check=True)
print("P4 survey CSV, privacy, SD ownership and failed-write recovery passed.")
