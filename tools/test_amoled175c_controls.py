"""Exercise the production AMOLED side-button routing and screen cycle."""
from pathlib import Path
import argparse
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work' / '175c-controls-tests'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
ui = (root / 'firmware/src/ui.cpp').read_text()
board = (root / 'firmware/src/board_amoled175c.cpp').read_text()


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


enum_start = ui.index('enum Page {')
source = r'''
#include "theme_button.h"
#include "wearable.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
''' + ui[enum_start:ui.index(';', enum_start) + 1] + r'''
Page page=Radar,pending=Radar;int menuTab=2;uint32_t now=0,lastInteraction=0;
uint32_t millis(){return now;}uint64_t stopwatchNow(){return uint64_t(now)*1000;}
struct lv_obj_t {bool hidden=false,box=false;std::vector<lv_obj_t*> children;};
lv_obj_t screen,top,marker;
lv_obj_t* root=&screen;lv_obj_t* resetBox=nullptr;lv_obj_t* clockPrompt=nullptr;lv_obj_t* notice=nullptr;
constexpr int LV_OBJ_FLAG_HIDDEN=1;int lv_msgbox_class=1;
bool lv_obj_has_flag(lv_obj_t* object,int){return object->hidden;}
bool lv_obj_check_type(lv_obj_t* object,int*){return object->box;}
unsigned lv_obj_get_child_cnt(lv_obj_t* object){return object->children.size();}
lv_obj_t* lv_obj_get_child(lv_obj_t* object,unsigned i){return object->children.at(i);}
lv_obj_t* lv_layer_top(){return &top;}
bool uploadBusy=false,driveBusy=false,locksportTheme=false;
unsigned uploadCancels=0,driveEnds=0,recipeStops=0,reconStops=0,bleStops=0,reconLeaves=0;
bool wardriveUploadBusy(){return uploadBusy;}bool wardriveBusy(){return driveBusy;}
void wardriveUploadCancel(){++uploadCancels;}void reconEndWardrive(){++driveEnds;}
void reconPage(const char*){}void recipeStop(){++recipeStops;}
void reconStop(){++reconStops;}void bleLabStop(){++bleStops;}void reconLeave(){++reconLeaves;}
dragon::Stopwatch stopwatch;
unsigned resets=0,updates=0;
bool locksport(){return locksportTheme;}
void resetStopwatch(){++resets;resetBox=&marker;}
void updateTimer(){++updates;}bool uiStopwatchKey(bool start,uint64_t at=0);
'''
for signature in ['static void go(', 'bool uiStopwatchKey(',
                  'static bool uiSideKeyHasMessageBox(', 'static bool uiSideKeyModalOpen(',
                  'void uiAmoled175cKey(']:
    source += '\n' + function(ui, signature)
source += r'''
constexpr int BTN_PIN=0,LOW=0;
bool bootDown=false,powerReady=true;
int digitalRead(int pin){assert(pin==BTN_PIN);return bootDown?LOW:1;}
struct Power {bool press=false;void getIrqStatus(){}bool isPekeyShortPressIrq(){return press;}void clearIrqStatus(){press=false;}} power;
void uiShow(const char* name){assert(!strcmp(name,"menu"));go(Menu);}
void uiKey(bool home){assert(home);go(Radar);}
''' + function(board, 'void boardLoop(') + r'''
void visible(Page next,int tab=0){page=pending=next;menuTab=tab;}
void render(){page=pending;}
void poll(unsigned advance=50){now+=advance;boardLoop();}
void pwr(){power.press=true;poll();}
void boot(){bootDown=true;poll();poll();bootDown=false;poll();poll();}
int main(){
#ifdef BOARD_AMOLED_175C
 // The download strap may still be held at startup. It is not a lap or navigation press.
 bootDown=true;for(int i=0;i<100;++i)poll();assert(pending==Radar);
 bootDown=false;poll();poll();
 // Drive the actual board loop: PWR cycles four screens, from the visible screen.
 pwr();assert(pending==Menu&&menuTab==0);render();
 pwr();assert(pending==Badge);render();pwr();assert(pending==Clock);render();
 pwr();assert(pending==Radar&&recipeStops==1&&reconStops==1&&bleStops==1);render();
 visible(Menu,2);pwr();assert(pending==Badge); // Touch selected a different Tools tab.
 visible(Badge);pwr();assert(pending==Clock);
 visible(Clock);pwr();assert(pending==Radar);
 visible(Settings);pwr();assert(pending==Menu&&menuTab==0);
 visible(Menu,2);boot();assert(page==Boot&&pending==Menu&&menuTab==0);render();
 visible(Clock);boot();assert(pending==Menu);render();
 // Ignore a second input while the first screen transition is still pending.
 visible(Radar);pwr();pwr();assert(page==Radar&&pending==Menu);render();
 // PWR is Start/Pause; BOOT is Lap, including exactly one event for a long hold.
 visible(Stopwatch);pwr();assert(stopwatch.running);poll(1000);
 boot();assert(stopwatch.running&&stopwatch.lapCount==1);
 bootDown=true;poll();poll();unsigned laps=stopwatch.lapCount;
 for(int i=0;i<50;++i)poll();assert(stopwatch.lapCount==laps&&laps==2);
 bootDown=false;poll();poll();pwr();assert(!stopwatch.running&&pending==Stopwatch);
 auto elapsed=stopwatch.elapsed(stopwatchNow());poll(1000);assert(stopwatch.elapsed(stopwatchNow())==elapsed);
 pwr();assert(stopwatch.running); // Resume preserves earlier laps.
 // BOOT bounce shorter than debounce never records another lap.
 bootDown=true;poll(5);bootDown=false;poll(5);poll(50);assert(stopwatch.lapCount==2);
 // Locksport's secondary action finishes the attempt; paused BOOT requests reset.
 locksportTheme=true;poll(1000);boot();assert(!stopwatch.running&&stopwatch.lapCount==3);
 boot();assert(resets==1&&resetBox);pwr();assert(!stopwatch.running&&resets==1);resetBox=nullptr;
 // Other dialogs must not trigger a timer action or hide a confirmation.
 for(lv_obj_t** modal:{&clockPrompt,&notice}){
   *modal=&marker;pwr();boot();assert(!stopwatch.running&&pending==Stopwatch&&resets==1);*modal=nullptr;
 }
 lv_obj_t container,dialog;dialog.box=true;container.children.push_back(&dialog);screen.children.push_back(&container);
 visible(Radar);pwr();boot();assert(pending==Radar);screen.children.clear();
 top.children.push_back(&dialog);pwr();assert(pending==Radar);top.children.clear();
 dialog.hidden=true;screen.children.push_back(&dialog);pwr();assert(pending==Menu);screen.children.clear();
 // Navigation retains the existing cleanup and storage-operation guards.
 visible(Recon);pwr();assert(reconLeaves==1&&pending==Menu);
 visible(BleLab);pwr();assert(bleStops==3&&pending==Menu);
 visible(Recon);driveBusy=true;pwr();assert(pending==Recon&&driveEnds==1);driveBusy=false;
 uploadBusy=true;pwr();assert(pending==Recon&&uploadCancels==1);uploadBusy=false;
 assert(lastInteraction==now);
#else
 // The 16 MB non-C profile keeps its original controls.
 poll();visible(Stopwatch);boot();assert(stopwatch.running);poll(1000);pwr();assert(stopwatch.running&&stopwatch.lapCount==1);
 boot();assert(!stopwatch.running);visible(Radar);boot();assert(pending==Menu);render();pwr();assert(pending==Radar);
#endif
}
'''
path = work / 'controls.cpp'
path.write_text(source)
for variant in ['BOARD_AMOLED_175C', 'BOARD_AMOLED_175']:
    exe = work / (variant + ('.exe' if sys.platform == 'win32' else ''))
    subprocess.run(compiler + ['-std=c++17', '-O1', '-UNDEBUG', '-D' + variant,
                              '-I' + str(root / 'firmware/include'), str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS 1.75C physical button mapping, four-screen cycle, timer/locksport, debounce, modal guards; non-C controls unchanged')
