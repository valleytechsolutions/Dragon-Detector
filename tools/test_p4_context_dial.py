"""Exercise the production launcher/player dial routing without hardware."""
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
work=root/'.work/p4-context-dial'
work.mkdir(parents=True,exist_ok=True)
ui=(root/'firmware/src/ui.cpp').read_text()
start=ui.index('bool uiP4DialInput(')
end=ui.index('\nvoid uiP4ReadPointer',start)
source=r'''
#include <cassert>
#include <algorithm>
#include <cstdio>
enum Page{Menu,Player,Clock,Radar};Page page=Menu,pending=Menu;
bool modal=false;bool uiSideKeyModalOpen(){return modal;}
struct {int moves=0,opens=0;void activate(){++opens;}void turn(int n){moves+=n;}} p4Launcher;
struct Audio{int volume=50;};Audio audio;
Audio audioStatus(){return audio;}void audioVolume(int n){audio.volume=std::clamp(n,0,100);}
int playbackAction=0,ticks=0;void voiceAction(int id){playbackAction=id;}void voicePlayerTick(){++ticks;}
''' + ui[start:end] + r'''
int main(){
 assert(uiP4DialInput(1,false)&&p4Launcher.moves==1&&!p4Launcher.opens);
 assert(uiP4DialInput(4,true)&&p4Launcher.opens==1&&p4Launcher.moves==1);
 modal=true;assert(!uiP4DialInput(1,true)&&p4Launcher.opens==1);modal=false;
 pending=Player;assert(!uiP4DialInput(1,false)&&p4Launcher.moves==1);page=Player;
 assert(uiP4DialInput(10,false)&&audio.volume==52&&ticks==1);
 assert(uiP4DialInput(-10,false)&&audio.volume==50&&ticks==2);
 assert(uiP4DialInput(0,true)&&playbackAction==193&&ticks==2);
 audio.volume=100;assert(uiP4DialInput(1,false)&&audio.volume==100);
 audio.volume=0;assert(uiP4DialInput(-1,false)&&audio.volume==0);
 page=pending=Clock;assert(!uiP4DialInput(1,false));
 page=pending=Radar;assert(!uiP4DialInput(0,true));
 puts("PASS production P4 context dial: launcher turn/select, playback volume/pause, bounds, modal and pending-page guards");
}
'''
(work/'dial.cpp').write_text(source)
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-UNDEBUG',str(work/'dial.cpp'),'-o',str(work/'dial.exe')],check=True)
subprocess.run([str(work/'dial.exe')],check=True)
