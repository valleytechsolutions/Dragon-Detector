"""Build and execute the actual P4 node adapter against a simulated radio."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
with TemporaryDirectory(prefix='dd-p4-node-') as directory:
    binary = Path(directory)/'test.exe'
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-UNDEBUG',
                    '-I'+str(root/'firmware/include'),str(root/'tests/p4_node_radio_test.cpp'),
                    '-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
