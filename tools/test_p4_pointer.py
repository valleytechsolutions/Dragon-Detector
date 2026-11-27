"""Check logical touch coordinates and bottom-edge navigation at every rotation."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess,sys
root=Path(__file__).resolve().parents[1]
with TemporaryDirectory(prefix='dd-p4-pointer-') as directory:
    work=Path(directory);exe=work/'test.exe'
    (work/'test.cpp').write_text(r'''
#include <cassert>
#include "p4_pointer.h"
using namespace p4ui;
int main(){
 for(unsigned rotation=0;rotation<4;++rotation){
  auto physical=[&](Point p){return rotatePoint(p,466,4-rotation);};
  auto logical=[&](Point p){return rotatePoint(p,466,rotation);};
  for(int x:{0,65,233,465})for(int y:{0,65,233,465}){
   const auto point=logical(physical({x,y}));assert(point.x==x&&point.y==y);
  }
  EdgePointer edge(466);
  auto first=edge.sample(true,logical(physical({233,415})),0,true);assert(first.count==0);
  auto moved=edge.sample(true,logical(physical({233,310})),100,true);assert(moved.count==0);
  auto released=edge.sample(false,{0,0},150,true);assert(released.gesture==Gesture::SwipeUp&&released.count==0);
  EdgePointer scroll(466);
  first=scroll.sample(true,logical(physical({233,300})),0,true);assert(first.count==1&&first.frames[0].pressed);
  moved=scroll.sample(true,logical(physical({233,140})),100,true);assert(moved.count==1&&moved.frames[0].at.y==140);
  released=scroll.sample(false,{0,0},150,true);assert(released.gesture==Gesture::None&&released.count==1&&!released.frames[0].pressed);
  EdgePointer tap(466);tap.sample(true,logical(physical({233,415})),0,true);
  released=tap.sample(false,{0,0},50,true);assert(released.gesture==Gesture::None&&released.count==2);
  assert(released.frames[0].pressed&&!released.frames[1].pressed&&released.frames[0].at.y==415);
 }
}
''')
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-UNDEBUG','-include','initializer_list',
                    '-I'+str(root/'firmware/include'),str(work/'test.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS P4 pointer: coordinate round trips, scrolling, bottom-edge swipe and replayed taps at 0/90/180/270 degrees')
