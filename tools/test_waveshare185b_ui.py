"""Exercise production 1.85B page cycling and timer/modal/navigation guards."""
from pathlib import Path
import argparse
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work' / 'waveshare185b-ui'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
ui = (root / 'firmware/src/ui.cpp').read_text()


def function(signature):
    start = ui.index(signature)
    brace = ui.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (ui[end] == '{') - (ui[end] == '}')
        end += 1
    return ui[start:end]


enum_start = ui.index('enum Page {')
source = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
''' + ui[enum_start:ui.index(';', enum_start) + 1] + r'''
Page page=Radar,pending=Radar;int menuTab=0;uint32_t lastInteraction=0,now=42;
uint32_t millis(){return now;}uint64_t stopwatchNow(){return uint64_t(now)*1000;}
struct lv_obj_t {bool hidden=false,box=false;std::vector<lv_obj_t*> children;};
lv_obj_t screen,top,marker;
lv_obj_t* root=&screen;lv_obj_t* resetBox=nullptr;lv_obj_t* clockPrompt=nullptr;lv_obj_t* notice=nullptr;
constexpr int LV_OBJ_FLAG_HIDDEN=1;int lv_msgbox_class=1;
bool lv_obj_has_flag(lv_obj_t* object,int flag){assert(flag==LV_OBJ_FLAG_HIDDEN);return object->hidden;}
bool lv_obj_check_type(lv_obj_t* object,int* type){assert(type==&lv_msgbox_class);return object->box;}
unsigned lv_obj_get_child_cnt(lv_obj_t* object){return object->children.size();}
lv_obj_t* lv_obj_get_child(lv_obj_t* object,unsigned i){return object->children.at(i);}
lv_obj_t* lv_layer_top(){return &top;}
const char* vendorState="ready";const char* bleState="ready";
bool sdLookupNamesReady(){return !strcmp(vendorState,"ready")&&!strcmp(bleState,"ready");}
const char* sdResourceVendorState(){return vendorState;}const char* sdResourceBleLabelState(){return bleState;}
std::vector<std::string> labels;unsigned dialogsOpened=0;std::string dialogText;
void button(lv_obj_t*,const char* text,int,int,int,int,int){labels.emplace_back(text);}
lv_obj_t resourceDialog;
lv_obj_t* uiMessageBox(lv_obj_t* parent,const char* title,const char* text,const char**,bool close){
 assert(std::string(title)=="SD RESOURCES"&&close);++dialogsOpened;dialogText=text;
 resourceDialog.box=true;parent->children.push_back(&resourceDialog);return &resourceDialog;
}
constexpr int F16=16,F12=12,LV_DIR_VER=1,LV_OBJ_FLAG_SCROLLABLE=2;
int S(int n){return n;}int uiFont(int font){return font;}
void lv_obj_set_size(lv_obj_t*,int w,int h){assert(w==260&&h==244);}void lv_obj_center(lv_obj_t*){}
lv_obj_t* lv_msgbox_get_title(lv_obj_t* box){return box;}lv_obj_t* lv_msgbox_get_text(lv_obj_t* box){return box;}
lv_obj_t* lv_msgbox_get_content(lv_obj_t* box){return box;}void lv_obj_set_style_text_font(lv_obj_t*,int,int){}
void lv_obj_set_height(lv_obj_t*,int h){assert(h==172);}void lv_obj_set_scroll_dir(lv_obj_t*,int dir){assert(dir==LV_DIR_VER);}
void lv_obj_add_flag(lv_obj_t*,int flag){assert(flag==LV_OBJ_FLAG_SCROLLABLE);}
bool uploadBusy=false,driveBusy=false,locksportTheme=false;
unsigned uploadCancels=0,driveEnds=0,recipeStops=0,reconStops=0,bleStops=0,reconLeaves=0;
std::string reconRequest;
bool wardriveUploadBusy(){return uploadBusy;}bool wardriveBusy(){return driveBusy;}
void wardriveUploadCancel(){++uploadCancels;}void reconEndWardrive(){++driveEnds;}
void reconPage(const char* name){reconRequest=name;}void recipeStop(){++recipeStops;}
void reconStop(){++reconStops;}void bleLabStop(){++bleStops;}void reconLeave(){++reconLeaves;}
struct Timer {bool running=false;unsigned toggles=0,laps=0,finishes=0;uint64_t at=0;
 void toggle(uint64_t tick){at=tick;running=!running;++toggles;}
 void lap(uint64_t tick){at=tick;++laps;}void finish(uint64_t tick){at=tick;running=false;++finishes;}
} stopwatch;
unsigned resetRequests=0,updates=0;
bool locksport(){return locksportTheme;}void resetStopwatch(){++resetRequests;}
void updateTimer(){++updates;}bool uiStopwatchKey(bool start,uint64_t at=0);
'''
for signature in ['static void go(', 'bool uiStopwatchKey(', 'static const char* ui185bResourceState(',
                  'static void build185bSystem(', 'static void show185bResources(', 'static bool ui185bHasMessageBox(',
                  'static bool ui185bModalOpen(', 'bool ui185bTimerVisible(',
                  'void ui185bHome(', 'void ui185bCycle(']:
    source += '\n' + function(signature)
source += r'''
void visible(Page next,int tab=0){page=pending=next;menuTab=tab;}
void render(){page=pending;}
int main(){
 // The normal five-page cycle starts with RECON regardless of an old tab.
 visible(Radar,2);ui185bCycle();assert(pending==Menu&&menuTab==0);render();
 ui185bCycle();assert(page==Boot&&pending==Menu&&menuTab==1);render();
 ui185bCycle();assert(pending==Badge);render();ui185bCycle();assert(pending==Clock);render();
 ui185bCycle();assert(pending==Radar&&recipeStops==1&&reconStops==1&&bleStops==1);render();
 assert(lastInteraction==now);
 // Touch selection determines the next step; no private button index drifts.
 visible(Menu,1);ui185bCycle();assert(pending==Badge);
 visible(Menu,0);ui185bCycle();assert(pending==Menu&&menuTab==1&&page==Boot);
 visible(Menu,2);ui185bCycle();assert(pending==Menu&&menuTab==0&&page==Boot);
 visible(Settings);ui185bCycle();assert(pending==Menu&&menuTab==0);
 visible(Badge);ui185bCycle();assert(pending==Clock);
 visible(Clock);ui185bCycle();assert(pending==Radar);
 visible(Menu,1);ui185bHome();assert(page==Boot&&pending==Menu&&menuTab==0);
 visible(Badge);pending=Clock;ui185bCycle();assert(page==Badge&&pending==Clock); // Transition already requested.
 // A single timer press remains start/pause; captured timestamps still work.
 visible(Stopwatch);assert(ui185bTimerVisible());ui185bCycle();assert(page==Stopwatch&&pending==Stopwatch&&stopwatch.running&&stopwatch.toggles==1);
 ui185bCycle();assert(!stopwatch.running&&stopwatch.toggles==2);
 assert(uiStopwatchKey(true,123456)&&stopwatch.running&&stopwatch.at==123456);
 assert(uiStopwatchKey(false,123600)&&stopwatch.laps==1&&stopwatch.running);
 locksportTheme=true;assert(uiStopwatchKey(false,123700)&&!stopwatch.running&&stopwatch.finishes==1);
 assert(uiStopwatchKey(false,123800)&&resetRequests==1);
 uiStopwatchKey(true,123900);unsigned toggles=stopwatch.toggles;ui185bHome();
 assert(pending==Menu&&menuTab==0&&stopwatch.running&&stopwatch.toggles==toggles);
 visible(Stopwatch);pending=Menu;assert(!ui185bTimerVisible());
 // Prompts, alerts and nested message boxes block both cycle and hold.
 for(lv_obj_t** modal:{&resetBox,&clockPrompt,&notice}){
   visible(Stopwatch);*modal=&marker;assert(!ui185bTimerVisible());ui185bCycle();ui185bHome();
   assert(page==Stopwatch&&pending==Stopwatch&&stopwatch.toggles==toggles);*modal=nullptr;
 }
 lv_obj_t panel,dialog;dialog.box=true;panel.children.push_back(&dialog);screen.children.push_back(&panel);
 visible(Settings);ui185bCycle();ui185bHome();assert(pending==Settings);
 screen.children.clear();top.children.push_back(&dialog);visible(Stopwatch);assert(!ui185bTimerVisible());
 ui185bCycle();ui185bHome();assert(pending==Stopwatch);top.children.clear();
 screen.children.push_back(&dialog);dialog.hidden=true;visible(Settings);ui185bCycle();assert(pending==Menu);screen.children.clear();
 // Leaving a live run still uses go(): stop/cancel first and stay on its page.
 visible(Recon,2);driveBusy=true;ui185bCycle();assert(pending==Recon&&driveEnds==1&&menuTab==0);
 ui185bHome();assert(pending==Recon&&driveEnds==2);driveBusy=false;
 visible(Recon,1);uploadBusy=true;ui185bHome();assert(pending==Recon&&uploadCancels==1&&reconRequest=="wardrive-upload"&&menuTab==0);
 uploadBusy=false;ui185bHome();assert(pending==Menu&&reconLeaves==1);
 // Resource health stays visible in System without creating automatic popups.
 for(const char* state:{"ready","missing SD vendor labels","corrupt checksum","insufficient PSRAM"}){
   vendorState=state;bleState="ready";labels.clear();build185bSystem();
   const char* expected=!strcmp(state,"ready")?"ready":!strncmp(state,"missing",7)?"missing":!strcmp(state,"corrupt checksum")?"invalid":"unavailable";
   assert(labels.size()==5&&labels[2]==std::string("SD resources: ")+expected&&dialogsOpened==0);
 }
 vendorState="corrupt checksum";bleState="missing SD BLE labels";assert(std::string(ui185bResourceState())=="invalid");
 vendorState="ready";assert(std::string(ui185bResourceState())=="missing");
 visible(Menu,2);show185bResources();assert(dialogsOpened==1);
 for(const char* text:{"IEEE names: ready","BLE names: missing SD BLE labels","vendors.ddr + ble-labels.ddb","/dragon/resources/","Restart","Core radar works without lookup names"})assert(dialogText.find(text)!=std::string::npos);
 ui185bCycle();assert(pending==Menu&&menuTab==2); // Explicit information dialog blocks BOOT until closed.
 puts("PASS production 1.85B page/tab cycle, touch resync, held home, timer start/pause and secondary actions, modals, pending transitions and safe save/cancel navigation");
 puts("PASS System resource ready/missing/invalid/unavailable status and explicit install-guide dialog without automatic popups");
}
'''
cpp, exe = work / 'cycle.cpp', work / 'cycle.exe'
cpp.write_text(source)
subprocess.run(compiler + ['-UNDEBUG', '-std=c++17', '-O1', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
