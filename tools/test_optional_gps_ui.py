"""Run the optional-GPS board's production menu/actions with a tiny UI harness."""
from pathlib import Path
import argparse
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work' / 'optional-gps-ui'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']


def preprocess(name):
    source = (root / 'firmware/src' / name).read_text()
    source = '#include "board_pins.h"\n' + re.sub(r'^#include[^\n]*', '', source, flags=re.M)
    path = work / name
    path.write_text(source)
    return subprocess.check_output(compiler + ['-E', '-P', '-DBOARD_WAVESHARE_185B',
                                   '-I' + str(root / 'firmware/include'), str(path)], text=True)


def block(source, signature, full=False):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start if full else brace:end]


ui, recon = preprocess('ui.cpp'), preprocess('recon_ui.cpp')
main_menu = block(ui, 'if(menuTab==0)')
tools_menu = block(recon, 'else if(view==Tools)')
start_click = block(ui, 'if(id==7109)')
route = block(recon, 'void reconPage(', full=True)
input_released = block(recon, 'bool gpsMenuInputReleased(', full=True)
source = r'''
#include <algorithm>
#include <cassert>
#include <string>
#include <vector>
using String=std::string;
std::vector<std::string> labels;
bool ready=false,owned=false,menuWardriveAvailable=false,toolsWardriveAvailable=false;
bool wardriveAvailable(){return ready;}bool wardriveBusy(){return owned;}
struct {bool enabled=false;} config;
auto gpsExternalConfig(){return config;}
void* root=nullptr;void* body=nullptr;
void button(void*,const char* s,int,int,int,int,int){labels.push_back(s);}
void row(const char* title,const char*,unsigned,int){labels.push_back(title);}
enum View {Modes,Tools,Wardrive,Uploads,GpsSetup,Confirm,BleTools,Library,Attacks,Recipes,Missions,Network,Csi,Portal,Statistics,Probes,Filters,Capture};
enum class AttackMode {IBeacon};AttackMode chosen;
View requested=Modes;bool pageRequested=false;
enum Page {Recon};unsigned navigations=0,starts=0;
void go(Page){++navigations;}bool wardriveStart(){++starts;return true;}
struct Input {struct {int state=0;} proc;Input* next=nullptr;};
Input* firstInput=nullptr;Input* lv_indev_get_next(Input* input){return input?input->next:firstInput;}
constexpr int LV_INDEV_STATE_PRESSED=1;
''' + input_released + '\n' + route + '\n'
source += 'void drawMain()' + main_menu + '\n'
source += 'void drawTools(){unsigned n=0;' + tools_menu + '}\n'
source += 'void clickStart()' + start_click + '\n'
source += r'''
bool has(const char* text){return std::find(labels.begin(),labels.end(),text)!=labels.end();}
void checkMenus(bool available){
 ready=menuWardriveAvailable=available;labels.clear();drawMain();
 assert(has("START WARDRIVE")==available&&has("Modes")&&has("BLE Lab"));
 labels.clear();drawTools();assert(has("GPS wardrive")==available&&has("GPS setup")&&has("Capture studio"));
 assert(toolsWardriveAvailable==available);
}
int main(){
 checkMenus(false);checkMenus(true);checkMenus(false);
 // Direct USB routing and a stale on-screen Start button obey the same gate.
 pageRequested=false;reconPage("wardrive");assert(pageRequested&&requested==GpsSetup);
 config.enabled=true;clickStart();assert(requested==GpsSetup&&starts==0&&navigations==1);
 ready=true;clickStart();assert(requested==Wardrive&&starts==1&&navigations==2);
 ready=false;owned=true;reconPage("wardrive");assert(requested==Wardrive);
 owned=false;reconPage("gps-setup");assert(requested==GpsSetup);
 // Redrawing for receiver hot-plug waits until every input has released.
 assert(gpsMenuInputReleased());Input touch,button;touch.next=&button;firstInput=&touch;
 assert(gpsMenuInputReleased());touch.proc.state=1;assert(!gpsMenuInputReleased());
 touch.proc.state=0;button.proc.state=1;assert(!gpsMenuInputReleased());
 button.proc.state=0;assert(gpsMenuInputReleased());
 puts("PASS production optional-GPS main/toolkit visibility, attach/detach, stale Start, direct routing, active-run access and released-input refresh");
}
'''
cpp, exe = work / 'ui.cpp', work / 'ui.exe'
cpp.write_text(source)
subprocess.run(compiler + ['-UNDEBUG', '-std=c++17', '-O1', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
