"""Check native rectangular rotation, touch boundaries and board pin contracts."""
from pathlib import Path
import argparse
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
output = root / '.work' / ('waveshare206.exe' if sys.platform == 'win32' else 'waveshare206')
output.parent.mkdir(exist_ok=True)
subprocess.run([*compiler, '-std=c++17', '-DBOARD_WAVESHARE_206', '-I'+str(root/'firmware/include'),
                str(root/'tests/waveshare_206.cpp'), '-o', str(output)], check=True)
subprocess.run([str(output)], check=True)
print('PASS Waveshare 2.06 rotation, FT3168 touch and pin contracts')
