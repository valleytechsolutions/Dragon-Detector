"""Run native C++ parser/tracking tests. Install ziglang==0.13.0, or pass --cxx g++."""
import argparse, pathlib, subprocess, sys
from check_coverage import generate
generate()
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx');p.add_argument('--sanitize',action='store_true');args=p.parse_args()
out=root/'.work'/'core_test';out.parent.mkdir(exist_ok=True)
if sys.platform=='win32':out=out.with_suffix('.exe')
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
cc=[args.cxx.replace('g++','gcc').replace('clang++','clang')] if args.cxx else [sys.executable,'-m','ziglang','cc']
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
lib=root/'lib/opendroneid/src'
obj=root/'.work'/'opendroneid.o'
subprocess.run(cc+flags+['-std=c11','-DODID_DISABLE_PRINTF','-I'+str(lib),'-c',str(lib/'opendroneid.c'),'-o',str(obj)],check=True,cwd=root)
subprocess.run(compiler+flags+['-UNDEBUG','-std=c++17','-I'+str(root/'firmware/include'),'-I'+str(root/'.work'),'-I'+str(lib),str(root/'tests/core_test.cpp'),str(root/'firmware/src/attack_frames.cpp'),str(obj),'-o',str(out)],check=True,cwd=root)
subprocess.run([str(out)],check=True,cwd=root)
