"""Run native C++ parser/tracking tests. Install ziglang==0.13.0, or pass --cxx g++."""
import argparse, pathlib, subprocess, sys
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
out=root/'.work'/'core_test';out.parent.mkdir(exist_ok=True)
if sys.platform=='win32':out=out.with_suffix('.exe')
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
subprocess.run(compiler+['-std=c++17','-O2','-I'+str(root/'firmware/include'),str(root/'tests/core_test.cpp'),'-o',str(out)],check=True,cwd=root)
subprocess.run([str(out)],check=True,cwd=root)
