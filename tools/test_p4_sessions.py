"""Exercise P4 session storage and the shared observation recorder with synthetic data."""
from pathlib import Path
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as out:
    temp=Path(out)
    code=(root/"firmware/src/missions.cpp").read_text()
    # Exercise the exact observation encoder, bounds and counters used on-device.
    symbols=[("struct Buffer {","const char* files[]"),("bool append(","bool flush("),("void observe(","bool exportFile(")]
    blocks=[code[code.index(start):code.index(end,code.index(start))] for start,end in symbols]
    (temp/"mission_record_core.inc").write_text("constexpr unsigned MaxSeen=512,MaxRelations=128;\n"+"\n".join(blocks))
    exe=temp/("p4-sessions.exe" if sys.platform=="win32" else "p4-sessions")
    subprocess.run([sys.executable,"-m","ziglang","c++","-std=c++17",
        "-I"+str(temp),"-I"+str(root/"firmware/include"),"-I"+str(root/"firmware/src"),
        str(root/"tests/p4_session_test.cpp"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("PASS P4 recon sessions, RCN1 records, privacy, timer, ownership, card mismatch and retry.")
