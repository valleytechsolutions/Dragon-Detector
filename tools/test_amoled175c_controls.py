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
#include "amoled175c_button.h"
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
bool recording=false,saving=false,reconOwned=false,bleOwned=false,mission=false,radioEnabled=true;
bool reconRecording(){return recording;}bool reconStorageBusy(){return saving;}
bool reconOwnsRadio(){return reconOwned;}bool bleLabOwnsRadio(){return bleOwned;}bool missionActive(){return mission;}
enum class AudioMode {Idle,Recording};struct AppAudioStatus {AudioMode mode=AudioMode::Idle;} audio;
AppAudioStatus audioStatus(){return audio;}void audioStop(){}void setRadio(bool on){radioEnabled=on;}
const char* dialogTitle=nullptr;
lv_obj_t* uiMessageBox(lv_obj_t*,const char* title,const char*,const char**,bool){dialogTitle=title;return &marker;}
void lv_obj_set_width(lv_obj_t*,int){}void lv_obj_center(lv_obj_t*){}int S(int value){return value;}
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
void uiAmoled175cSleep(unsigned seconds=0);
'''
for signature in ['static void go(', 'bool uiStopwatchKey(',
                  'static bool uiSideKeyHasMessageBox(', 'static bool uiSideKeyModalOpen(',
                  'void uiAmoled175cSleep(', 'void uiAmoled175cKey(']:
    source += '\n' + function(ui, signature)
source += r'''
constexpr int BTN_PIN=0,LOW=0,HIGH=1,GPIO_NUM_3=3,ESP_OK=0,TP_INT=42,AUDIO_MUTE_PIN=46;
constexpr int ESP_SLEEP_WAKEUP_EXT0=0,ESP_SLEEP_WAKEUP_ALL=1;
bool bootDown=false,powerReady=true,wakeDown=false,touchReady=true;
int digitalRead(int pin){assert(pin==BTN_PIN||pin==3);return pin==3?wakeDown:(bootDown?LOW:HIGH);}
struct Power {bool press=false;void getIrqStatus(){}bool isPekeyShortPressIrq(){return press;}void clearIrqStatus(){press=false;}} power;
struct SerialMock {void println(const char*){}void flush(){}template<class... T>void printf(const char*,T...){}} Serial;
void* panel=reinterpret_cast<void*>(1);void* panelIo=reinterpret_cast<void*>(2);
bool wakeError=false,timerError=false,displayError=false,commandError=false;
unsigned sleepCalls=0;uint64_t timerUs=0;
enum Call {Wake,Timer,DisplayOff,DisplayOn,SleepIn,TouchDetach,TouchSleep,Mute,WifiStop,BleStop,EnterSleep,DisableExt0,DisableAll};
std::vector<Call> calls;
int esp_sleep_enable_ext0_wakeup(int pin,int level){assert(pin==3&&level==HIGH);calls.push_back(Wake);return wakeError?-1:ESP_OK;}
int esp_sleep_enable_timer_wakeup(uint64_t us){calls.push_back(Timer);timerUs=us;return timerError?-1:ESP_OK;}
int esp_sleep_disable_wakeup_source(int source){assert(source==ESP_SLEEP_WAKEUP_EXT0||source==ESP_SLEEP_WAKEUP_ALL);calls.push_back(source==ESP_SLEEP_WAKEUP_EXT0?DisableExt0:DisableAll);return ESP_OK;}
int esp_lcd_panel_disp_on_off(void* handle,bool on){assert(handle==panel);calls.push_back(on?DisplayOn:DisplayOff);return displayError?-1:ESP_OK;}
int esp_lcd_panel_io_tx_param(void* handle,int command,const void* data,size_t len){assert(handle==panelIo&&command==0x02001000&&!data&&!len);calls.push_back(SleepIn);return commandError?-1:ESP_OK;}
struct Touch {void sleep(){calls.push_back(TouchSleep);}} touch;
void detachInterrupt(int pin){assert(pin==TP_INT);calls.push_back(TouchDetach);}
void digitalWrite(int pin,int level){assert(pin==AUDIO_MUTE_PIN&&level==LOW);calls.push_back(Mute);}
unsigned long wallTime(){return 1794171896UL;}
int esp_wifi_stop(){calls.push_back(WifiStop);return ESP_OK;}
struct NimBLEDevice {static void deinit(bool clear){assert(!clear);calls.push_back(BleStop);}};
void esp_deep_sleep_start(){calls.push_back(EnterSleep);++sleepCalls;}
void delay(unsigned ms){now+=ms;}
void uiShow(const char* name){assert(!strcmp(name,"menu"));go(Menu);}
void uiKey(bool home){assert(home);go(Radar);}
''' + function(board, 'bool amoled175cSleep(') + function(board, 'void boardLoop(') + r'''
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
 // PWR is Start/Pause; a BOOT tap is Lap. Holding cannot discard a running timer.
 visible(Stopwatch);pwr();assert(stopwatch.running);poll(1000);
 boot();assert(stopwatch.running&&stopwatch.lapCount==1);
 bootDown=true;poll();poll();unsigned laps=stopwatch.lapCount;
 for(int i=0;i<50;++i)poll();assert(stopwatch.lapCount==laps&&laps==1&&sleepCalls==0);
 bootDown=false;poll();poll();assert(sleepCalls==0&&stopwatch.lapCount==1);
 assert(!strcmp(dialogTitle,"STILL ACTIVE")&&radioEnabled&&stopwatch.running);
 poll(3000);assert(sleepCalls==0);
 pwr();assert(!stopwatch.running&&pending==Stopwatch);
 auto elapsed=stopwatch.elapsed(stopwatchNow());poll(1000);assert(stopwatch.elapsed(stopwatchNow())==elapsed);
 // After pausing, one hold requests sleep on release, without a lap/reset action.
 bootDown=true;poll();poll();poll(2000);assert(sleepCalls==0);
 bootDown=false;poll();poll();assert(sleepCalls==1&&stopwatch.lapCount==1&&resets==0);
 poll(3000);assert(sleepCalls==1);radioEnabled=true; // Fake deep sleep returns; hardware reboots on wake.
 pwr();assert(stopwatch.running); // Resume preserves earlier laps.
 // BOOT bounce shorter than debounce never records another lap.
 bootDown=true;poll(5);bootDown=false;poll(5);poll(50);assert(stopwatch.lapCount==1);
 // Locksport's secondary action finishes the attempt; paused BOOT requests reset.
 locksportTheme=true;poll(1000);boot();assert(!stopwatch.running&&stopwatch.lapCount==2);
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
 visible(BleLab);unsigned stopped=bleStops;pwr();assert(bleStops==stopped+1&&pending==Menu);
 visible(Recon);driveBusy=true;pwr();assert(pending==Recon&&driveEnds==1);driveBusy=false;
 uploadBusy=true;pwr();assert(pending==Recon&&uploadCancels==1);uploadBusy=false;
 // Sleep never interrupts an active writer, radio session, or off-screen timer.
 visible(Radar);unsigned sleeps=sleepCalls;
 for(bool* busy:{&recording,&saving,&reconOwned,&bleOwned,&mission,&driveBusy,&uploadBusy}){
   *busy=true;uiAmoled175cSleep();assert(sleepCalls==sleeps&&!strcmp(dialogTitle,"STILL ACTIVE"));*busy=false;
 }
 audio.mode=AudioMode::Recording;uiAmoled175cSleep();assert(sleepCalls==sleeps);audio.mode=AudioMode::Idle;
 stopwatch.running=true;uiAmoled175cSleep();assert(sleepCalls==sleeps&&!strcmp(dialogTitle,"STILL ACTIVE"));stopwatch.running=false;
 clockPrompt=&marker;uiAmoled175cSleep();assert(sleepCalls==sleeps);clockPrompt=nullptr;
 pending=Clock;uiAmoled175cSleep();assert(sleepCalls==sleeps);pending=Radar;
 powerReady=false;uiAmoled175cSleep();assert(sleepCalls==sleeps&&radioEnabled&&!strcmp(dialogTitle,"SLEEP UNAVAILABLE"));powerReady=true;
 // Debounce must not turn a 1999 ms hold into sleep; subtraction survives rollover.
 using Event=dragon::Amoled175cButtonEvent;
 for(uint32_t start:{100u,UINT32_MAX-1500u}){
   for(uint32_t held:{1999u,2000u}){
     dragon::Amoled175cButton b;b.update(false,start-80);b.update(false,start-40);
     assert(b.update(true,start)==Event::None&&b.update(true,start+35)==Event::None);
     assert(b.update(false,start+held)==Event::None);
     assert(b.update(false,start+held+35)==(held==2000?Event::Sleep:Event::Tap));
     assert(b.update(false,start+held+100)==Event::None);
   }
 }
 assert(lastInteraction==now);
 // Fail before screen/radio teardown if a wake source cannot be established.
 calls.clear();wakeDown=true;assert(!amoled175cSleep(0)&&calls.empty());wakeDown=false;
 panel=nullptr;assert(!amoled175cSleep(0)&&calls.empty());panel=reinterpret_cast<void*>(1);
 panelIo=nullptr;assert(!amoled175cSleep(0)&&calls.empty());panelIo=reinterpret_cast<void*>(2);
 wakeError=true;assert(!amoled175cSleep(0)&&(calls==std::vector<Call>{Wake}));wakeError=false;
 calls.clear();timerError=true;assert(!amoled175cSleep(10)&&(calls==std::vector<Call>{Wake,Timer,DisableExt0}));timerError=false;
 calls.clear();displayError=true;assert(!amoled175cSleep(0)&&(calls==std::vector<Call>{Wake,DisplayOff,DisableAll}));displayError=false;
 calls.clear();commandError=true;assert(!amoled175cSleep(0)&&(calls==std::vector<Call>{Wake,DisplayOff,SleepIn,DisplayOn,DisableAll}));commandError=false;
 // Failed UI sleep restores the previous radio setting, including Clock's standby.
 visible(Clock);radioEnabled=false;wakeError=true;uiAmoled175cSleep();assert(!radioEnabled&&sleepCalls==sleeps&&!strcmp(dialogTitle,"SLEEP UNAVAILABLE"));wakeError=false;
 // Production shutdown sequence keeps the CPU supply on and tears down peripherals.
 calls.clear();assert(amoled175cSleep(0));
 assert((calls==std::vector<Call>{Wake,DisplayOff,SleepIn,TouchDetach,TouchSleep,Mute,WifiStop,BleStop,EnterSleep}));
 calls.clear();assert(amoled175cSleep(1234)&&timerUs==1234000000ULL);
 assert((calls==std::vector<Call>{Wake,Timer,DisplayOff,SleepIn,TouchDetach,TouchSleep,Mute,WifiStop,BleStop,EnterSleep}));
 calls.clear();touchReady=false;assert(amoled175cSleep(0));
 assert((calls==std::vector<Call>{Wake,DisplayOff,SleepIn,TouchDetach,Mute,WifiStop,BleStop,EnterSleep}));
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
print('PASS 1.75C navigation, timing, sleep routing, wake/display failures, debounce/rollover, timer/storage/modal guards; non-C unchanged')
