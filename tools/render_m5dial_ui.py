"""Render production Dial pages and exercise menu/input routes with native LVGL.

The same ui_lite.cpp is then built as the one-button waveshare_lite and its MODES
page and node screen are driven through Tap / Double / Hold."""
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
work = root / '.work/m5dial-ui-render'
work.mkdir(parents=True, exist_ok=True)
native = root / '.work/m5dial-render/native'
library = native / 'lvgl.lib'
shared = root / '.work/can-commander/native'
if not library.exists() and (shared/'lvgl.lib').exists() and (native/'lv_conf.h').exists() and (shared/'lv_conf.h').read_bytes() == (native/'lv_conf.h').read_bytes():
    library = shared/'lvgl.lib'
if not library.exists():
    subprocess.run([sys.executable, str(root/'tools/render_m5dial.py')], check=True)
    if not library.exists():
        library = shared/'lvgl.lib'

(work/'Arduino.h').write_text(r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
class String: public std::string {
 public: using std::string::string;
 String(const std::string& s):std::string(s){}
 bool startsWith(const char* s)const{return rfind(s,0)==0;}
 String substring(size_t n)const{return substr(n);}
};
template<class T,class A,class B> T constrain(T n,A low,B high){return std::clamp(n,T(low),T(high));}
inline uint32_t tick=0; // one clock for the UI and the node screen
inline uint32_t millis(){return tick;}
inline void* ps_malloc(size_t){return nullptr;} // no PSRAM on the Dial (getFreePsram 0): the radar effects take their no-table path
struct SerialStub{template<class... T>void printf(T...){} template<class T>void println(T){}};
static SerialStub Serial;
struct EspStub{unsigned getFreeHeap(){return 140000;}unsigned getFreePsram(){return 0;}unsigned getMinFreeHeap(){return 130000;}};
static EspStub ESP;
''')
(work/'Preferences.h').write_text(r'''
#pragma once
#include <map>
#include <string>
struct Preferences {
 std::map<std::string,unsigned> values;
 bool begin(const char*,bool){return true;}
 unsigned getUChar(const char* key,unsigned fallback){auto i=values.find(key);return i==values.end()?fallback:i->second;}
 unsigned putUChar(const char* key,unsigned value){values[key]=value;return 1;}
};
''')
(work/'esp_heap_caps.h').write_text(r'''
#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#define MALLOC_CAP_INTERNAL 0
inline void* heap_caps_malloc(size_t n,int){return malloc(n);}
inline unsigned heap_caps_get_largest_free_block(int){return 120000;}
''')
(work/'test_app.h').write_text(r'''
#pragma once
#include "Arduino.h"
#include "board_pins.h"
#include "contacts.h"
#include "traffic.h"
#include "nearby.h"
#include "wearable.h"
#include "personal_theme.h"
#include <cassert>
#include <cstdio>
using dragon::Family;
static Contact entries[MAX_CONTACTS];
static ContactStore contacts{entries};
static dragon::DetectorSettings detector;
static void setAdultMode(bool enabled){detector.families=dragon::adultOptInMask(detector.families,enabled);}
static dragon::TrafficStatus trafficStatus;
static dragon::NearbyRadios nearby;
static dragon::Stopwatch stopwatch;
static bool radioEnabled=true,blePhase=true,candidatesOnly=false,radarRecent=false,alertSound=true,rtcAvailable=true,standby=false;
static uint32_t wifiFrames=12830,bleFrames=9540,queueDrops=0;
static unsigned wifiChannel=6;
static int trackedContact=-1,clockOffsetMinutes=-240;
static uint32_t epoch=1792769736;
static dragon::Calendar date{2026,10,23,10,24,36};
static dragon::PersonalTheme custom;
bool personalThemeReady(){return false;}
const dragon::PersonalTheme& personalTheme(){return custom;}
const dragon::UiPalette& themeUiColors(unsigned n){return dragon::UiPalettes[n];}
const lv_img_dsc_t* personalThemeLogo(bool){return nullptr;}
void personalThemeClock(lv_draw_ctx_t*,const dragon::Calendar&,bool){}
void personalThemeSweep(lv_draw_ctx_t*,float){}
static void setRadioStandby(bool enabled){standby=enabled;radioEnabled=!enabled;}
static void clockLocal(dragon::Calendar& out){out=date;}
static uint32_t wallTime(){return epoch;}
static bool configureClock(uint32_t value){epoch=value;return true;}
static uint64_t stopwatchNow(){return uint64_t(tick)*1000;}
static bool radarVisible(const Contact& c,uint32_t now){return c.used&&now-c.seen<120000;}
static dragon::DeviceKey contactKey(const Contact& c){return keyOf(c);}
static int findContact(const dragon::DeviceKey& key){for(int i=0;i<MAX_CONTACTS;++i)if(contacts[i].used&&keyOf(contacts[i])==key)return i;return -1;}
static void trackContact(int i){trackedContact=i;}
#include "audio_levels.h"
static unsigned soundVolume=100;
static dragon::AudioLevel audioLevel(){return dragon::audioLevelFor(alertSound,soundVolume);}
static void audioSetLevel(dragon::AudioLevel level){soundVolume=dragon::audioLevelVolume(level);alertSound=soundVolume!=0;}
static void audioSave(){} static void audioStop(){}
static void macText(const uint8_t* m,char* out){snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);}
void set_brightness(uint8_t){} void dialInputStatus(){}
void uiKey(bool);
// Node session double: the production node screen (node_ui.cpp) is linked in.
#include "node_mode.h"
static bool nodeRunning=false,nodeRefuses=false;
static NodeKind nodeRunningKind=NodeKind::None;
static unsigned nodeStops=0;
static dragon::node::Status nodeLink;
bool nodeBusy(){return nodeRunning;}
NodeKind nodeKind(){return nodeRunning?nodeRunningKind:NodeKind::None;}
bool nodeStart(NodeKind kind){if(nodeRefuses||stopwatch.running)return false;nodeRunning=true;nodeRunningKind=kind;return true;}
void nodeStop(){if(nodeRunning)++nodeStops;nodeRunning=false;}
const char* nodeError(){return stopwatch.running?"Stop the stopwatch first":"";}
const dragon::node::Status& nodeStatus(){return nodeLink;}
bool nodeBleReports(){return nodeRunningKind==NodeKind::Jcmk;}
bool nodeKeyed(){return false;}
''')
ui = (root/'firmware/src/ui_lite.cpp').read_text().replace('#include "app.h"', '#include "test_app.h"')
harness = r'''
#include <vector>
''' + ui + r'''
static std::vector<uint8_t> pixels(240*240*3);
static lv_disp_drv_t display;
static lv_disp_draw_buf_t drawBuffer;
static lv_color_t tile[240*24],full[240*240];
static void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* b){for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x){auto c=lv_color_to32(*b++);auto n=(y*240+x)*3;pixels[n]=c>>16;pixels[n+1]=c>>8;pixels[n+2]=c;}lv_disp_flush_ready(d);}
void scr_lvgl_init(){lv_init();lv_disp_draw_buf_init(&drawBuffer,tile,nullptr,240*24);lv_disp_drv_init(&display);display.hor_res=display.ver_res=240;display.draw_buf=&drawBuffer;display.flush_cb=flush;lv_disp_drv_register(&display);}
static void render(const char* folder,const char* name){
 lv_obj_invalidate(root);lv_refr_now(nullptr);auto tiled=pixels;
 lv_disp_draw_buf_init(&drawBuffer,full,nullptr,240*240);lv_obj_invalidate(root);lv_refr_now(nullptr);
 assert(pixels==tiled && "24-row display tiles must match a full-frame render");lv_disp_draw_buf_init(&drawBuffer,tile,nullptr,240*24);
 char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);FILE* f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");fwrite(pixels.data(),1,pixels.size(),f);fclose(f);
}
// First descendant of a class, or the label showing exactly this text (once laid out,
// so a long-mode label shows its full text).
static lv_obj_t* findObject(lv_obj_t* o,const lv_obj_class_t* type,const char* text=nullptr){
 lv_obj_update_layout(o);
 for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i){
   auto* child=lv_obj_get_child(o,i);
   if(lv_obj_check_type(child,type)&&(!text||!strcmp(lv_label_get_text(child),text)))return child;
   if(auto* found=findObject(child,type,text))return found;
 }
 return nullptr;
}
'''
dial_main = r'''
int main(int argc,char** argv){
 for(int i=0;i<8;++i){auto& c=contacts[i];c.used=true;c.seen=2500;c.packets=5;c.match.family=i==0?Family::Axon:i==1?Family::Camera:Family::Tracker;c.matchHits=1;c.mac[5]=i+1;c.rssi=-42-i*6;c.signal.count=1;c.signal.smooth=c.rssi;c.radarX=cosf(i*.9f);c.radarY=sinf(i*.9f);snprintf(c.name,sizeof(c.name),"Example device %d",i+1);}
 uiBegin();tick=3000;uiLoop();lv_refr_now(nullptr);tick+=200;uiLoop();render(argv[1],"01-radar");assert(dotCount==8&&radarPrimary==0);
 auto selected=dots[0];dialRadarDown(selected.x,selected.y);dialRadarRelease();assert(radarHasSelection&&radarSelectedKey==selected.key);tick+=200;uiLoop();render(argv[1],"01a-selected");assert(!lv_obj_has_flag(dialTargetPanel,LV_OBJ_FLAG_HIDDEN));
 lv_event_send(lv_obj_get_child(dialTargetPanel,2),LV_EVENT_SHORT_CLICKED,nullptr);assert(trackedContact==selected.index);lv_event_send(lv_obj_get_child(dialTargetPanel,2),LV_EVENT_SHORT_CLICKED,nullptr);assert(trackedContact==-1);
 dialRotate(1);assert(litePage==LitePage::Radar&&dialTargetChoice==1);dialButton(dragon::LitePress::Tap);assert(trackedContact==selected.index);dialButton(dragon::LitePress::Tap);assert(trackedContact==-1);dialRotate(1);dialButton(dragon::LitePress::Tap);assert(!radarHasSelection);dialRadarDown(selected.x,selected.y);dialRadarRelease();
 dialRadarDown(120,180);dialRadarRelease();assert(!radarHasSelection&&lv_obj_has_flag(dialTargetPanel,LV_OBJ_FLAG_HIDDEN));
 dialRadarDown(selected.x,selected.y);dialRadarRelease();dialRadarDown(selected.x,selected.y);dialRadarRelease();assert(!radarHasSelection);
 dialRadarDown(selected.x,selected.y);dialRadarRelease();lv_event_send(lv_obj_get_child(dialTargetPanel,1),LV_EVENT_SHORT_CLICKED,nullptr);assert(litePage==LitePage::Signals&&!radarHasSelection);uiShow("radar");
 uiNotify("Example camera detected","",nullptr,false);tick+=200;uiLoop();render(argv[1],"01b-notification");assert(!lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN));lv_event_send(notice,LV_EVENT_SHORT_CLICKED,nullptr);assert(lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN));
 unsigned originalTheme=radarTheme;dialButton(dragon::LitePress::Tap);assert(dialMenu==DialMenu::Main&&radarTheme==originalTheme);render(argv[1],"02-menu");
 dialRotate(1);dialButton(dragon::LitePress::Tap);assert(litePage==LitePage::Clock&&standby);tick+=200;uiLoop();render(argv[1],"03-clock");
 dialButton(dragon::LitePress::Tap);dialRotate(1);dialRotate(1);dialButton(dragon::LitePress::Tap);assert(dialMenu==DialMenu::Tools);render(argv[1],"04-tools");dialButton(dragon::LitePress::Tap);assert(litePage==LitePage::Timer&&standby);dialButton(dragon::LitePress::Tap);assert(stopwatch.running);tick+=68230;uiLoop();render(argv[1],"05-timer");dialButton(dragon::LitePress::Tap);assert(!stopwatch.running);
 dialButton(dragon::LitePress::Hold);assert(litePage==LitePage::Radar&&!standby);dialRotate(-1);assert(litePage==LitePage::Status);tick+=200;uiLoop();render(argv[1],"06-status");
 contacts[0].seen=tick;dialOpen(keyOf(contacts[0]));tick+=200;uiLoop();render(argv[1],"07-signal");lv_event_send(pageValue,LV_EVENT_SHORT_CLICKED,nullptr);assert(trackedContact==0);lv_event_send(pageValue,LV_EVENT_SHORT_CLICKED,nullptr);assert(trackedContact==-1);
 lv_event_send(dialMenuButton,LV_EVENT_SHORT_CLICKED,nullptr);assert(dialMenu==DialMenu::Main);dialChoice=4;dialButton(dragon::LitePress::Tap);assert(dialMenu==DialMenu::Settings);render(argv[1],"08-settings");
 for(unsigned theme:{0,3,9,23,28,29,31,35,39}){uiShow("clock");radarTheme=theme;refreshLiteTheme();tick+=200;uiLoop();char label[40];snprintf(label,sizeof(label),"clock-theme-%02u",theme);render(argv[1],label);}
 uiShow("radar");assert(!standby&&!dialMenuVisible());
 // Tools > Modes: BISCUIT NODE and JCMK Node own the whole panel until two exit
 // requests: EXIT taps or front-button holds. A tap or a double press of the
 // front button only shows how to leave, so a bump cannot end a drive.
 using P=dragon::LitePress;radarTheme=1;refreshLiteTheme();dialClearSelection();const uint32_t children=lv_obj_get_child_cnt(root);const LitePage page=litePage;
 liteButtonEvent(P::Tap);assert(dialMenu==DialMenu::Main);dialRotate(1);dialRotate(1);liteButtonEvent(P::Tap);assert(dialMenu==DialMenu::Tools);
 for(int i=0;i<3;++i)dialRotate(1);liteButtonEvent(P::Tap);assert(dialMenu==DialMenu::Modes&&dialChoice==0);render(argv[1],"09-modes");
 liteButtonEvent(P::Tap);assert(nodeKind()==NodeKind::Biscuit&&liteNodeHost&&nodeScreenShown()&&!dialMenuVisible());
 assert(lv_obj_get_index(liteNodeHost)==lv_obj_get_child_cnt(root)-1);lv_obj_update_layout(root);assert(lv_obj_get_width(lv_obj_get_child(liteNodeHost,0))==240);
 nodeLink.linked=nodeLink.registered=true;nodeLink.core=dragon::node::Core::Biscuit;nodeLink.wifi=1284;nodeLink.sent=1302;nodeLink.channel=6;nodeLink.planCount=11;for(uint8_t i=0;i<11;++i)nodeLink.plan[i]=i+1;
 tick+=250;uiLoop();render(argv[1],"10-biscuit-node");assert(findObject(liteNodeHost,&lv_label_class,"1,284"));
 dialRotate(1);dialRotate(-1);assert(litePage==page&&!dialMenuVisible()&&liteNodeHost);
 uiNotify("Example camera detected","",nullptr,false);assert(lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN));
 for(const char* name:{"radar","clock","stopwatch","menu","tools","settings","modes","startup","node"}){uiShow(name);assert(nodeBusy()&&liteNodeHost&&!dialMenuVisible()&&!liteStartup&&litePage==page);}
 const bool radio=standby;setLitePage(LitePage::Clock);assert(standby==radio);setLitePage(page); // no radio change while a node runs
 liteButtonEvent(P::Tap);assert(nodeBusy());tick+=250;uiLoop();assert(findObject(liteNodeHost,&lv_label_class,"TAP EXIT TO LEAVE"));render(argv[1],"11-node-hint");
 for(int i=0;i<3;++i){liteButtonEvent(P::Double);liteButtonEvent(P::Tap);tick+=600;uiLoop();}assert(nodeBusy()&&nodeStops==0);
 tick+=3100;uiLoop();assert(!findObject(liteNodeHost,&lv_label_class,"TAP EXIT TO LEAVE"));   // the hint goes again
 liteButtonEvent(P::Hold);assert(nodeBusy());tick+=250;uiLoop();assert(findObject(liteNodeHost,&lv_label_class,"TAP CONFIRM TO LEAVE"));render(argv[1],"11-node-confirm");
 liteButtonEvent(P::Hold);assert(nodeBusy());                      // too soon after the first: a bounce, not a second request
 tick+=5100;uiLoop();liteButtonEvent(P::Hold);assert(nodeBusy()); // the confirmation lapsed, so this hold only arms again
 liteButtonEvent(P::Double);liteButtonEvent(P::Tap);assert(nodeBusy()); // short presses never confirm
 tick+=600;liteButtonEvent(P::Hold);assert(!nodeBusy()&&nodeStops==1&&liteNodeHost); // stopped; the screen leaves on the next pass
 liteButtonEvent(P::Tap);dialRotate(1);assert(!dialMenuVisible()&&litePage==page);
 uiLoop();assert(!liteNodeHost&&!nodeScreenShown()&&dialMenu==DialMenu::Modes&&dialChoice==0&&lv_obj_get_child_cnt(root)==children);
 dialRotate(1);liteButtonEvent(P::Tap);assert(nodeKind()==NodeKind::Jcmk&&liteNodeHost);nodeLink.ble=211;tick+=250;uiLoop();render(argv[1],"12-jcmk-node");
 auto* exitButton=findObject(liteNodeHost,&lv_btn_class);assert(exitButton);
 lv_event_send(exitButton,LV_EVENT_CLICKED,nullptr);assert(nodeBusy());lv_event_send(exitButton,LV_EVENT_CLICKED,nullptr);assert(nodeBusy()); // a double tap is one request
 tick+=600;lv_event_send(exitButton,LV_EVENT_CLICKED,nullptr);assert(!nodeBusy()&&nodeStops==2&&liteNodeHost);
 uiLoop();assert(!liteNodeHost&&dialMenu==DialMenu::Modes&&dialChoice==1);
 // EXIT on the screen, then a front-button hold, also leaves.
 liteButtonEvent(P::Tap);assert(nodeKind()==NodeKind::Jcmk&&liteNodeHost);tick+=250;uiLoop();
 lv_event_send(findObject(liteNodeHost,&lv_btn_class),LV_EVENT_CLICKED,nullptr);assert(nodeBusy());tick+=1000;uiLoop();
 liteButtonEvent(P::Double);assert(nodeBusy());liteButtonEvent(P::Hold);assert(!nodeBusy()&&nodeStops==3);uiLoop();assert(!liteNodeHost&&dialMenu==DialMenu::Modes&&dialChoice==1);
 // A refusal stays on Modes with the reason.
 stopwatch.toggle(stopwatchNow());dialRotate(-1);liteButtonEvent(P::Tap);assert(!nodeBusy()&&!liteNodeHost&&dialMenu==DialMenu::Modes&&!strcmp(lv_label_get_text(dialMenuNote),"Stop the stopwatch first"));
 render(argv[1],"13-modes-refused");stopwatch.toggle(stopwatchNow());nodeRefuses=true;dialRotate(1);dialRotate(-1);liteButtonEvent(P::Tap);assert(!strcmp(lv_label_get_text(dialMenuNote),"Could not start the node"));nodeRefuses=false;
 // USB: start, switch in place, stop; a node that ends on its own; a resume over the startup splash.
 dialMenuOpen(DialMenu::Brightness);const int brightness=dialBrightness;dialMenuRotate(-1);
 assert(nodeStart(NodeKind::Biscuit));uiShow("node");assert(liteNodeHost&&!dialMenuVisible()&&dialBrightness==brightness&&findObject(liteNodeHost,&lv_label_class,"BISCUIT NODE"));
 assert(nodeStart(NodeKind::Jcmk));uiShow("node");assert(findObject(liteNodeHost,&lv_label_class,"JCMK Node")&&!findObject(liteNodeHost,&lv_label_class,"BISCUIT NODE"));
 nodeStop();uiShow("modes");assert(!liteNodeHost&&dialMenu==DialMenu::Modes&&dialChoice==1);
 assert(nodeStart(NodeKind::Biscuit));uiLoop();assert(liteNodeHost&&!dialMenuVisible());
 nodeRunning=false;tick+=250;uiLoop();assert(liteNodeHost);uiLoop();assert(!liteNodeHost&&dialMenu==DialMenu::Modes&&dialChoice==0);
 uiShow("startup");assert(liteStartup&&!dialMenuVisible());assert(nodeStart(NodeKind::Biscuit));uiShow("node");
 assert(lv_obj_get_index(liteNodeHost)>lv_obj_get_index(liteStartup));tick+=2300;uiLoop();assert(!liteStartup&&liteNodeHost);
 nodeStop();uiShow("radar");assert(!standby&&!dialMenuVisible()&&!liteNodeHost&&lv_obj_get_child_cnt(root)==children);
 // Vector Potential: page headers use the watch-size mark, clear of the page title.
 radarTheme=VectorPotentialTheme;refreshLiteTheme();setLitePage(LitePage::Status);tick+=200;uiLoop();lv_obj_update_layout(root);
 {lv_area_t l,t;lv_obj_get_coords(logo,&l);lv_obj_get_coords(pageTitle,&t);assert(lv_img_get_src(logo)==&watch_logo_vp&&!lv_obj_has_flag(logo,LV_OBJ_FLAG_HIDDEN)&&l.y2<t.y1);}
 render(argv[1],"14-status-vp");setLitePage(LitePage::Radar);assert(lv_obj_has_flag(logo,LV_OBJ_FLAG_HIDDEN));radarTheme=1;refreshLiteTheme();
 puts("PASS production Dial pages, ring/button menu navigation, timer, radio standby, Tools > Modes node screen (EXIT or hold to leave, taps and double presses never leave, refusal, USB, resume), Vector Potential headers and matching tile/full-frame rendering");
}
'''
lite_main = r'''
int main(int argc,char** argv){
 // One BOOT button: a hold always moves to the next page; on MODES a tap picks the node and
 // a double tap starts it. On the node screen only holds count: the first arms, the second leaves.
 using P=dragon::LitePress;
 uiBegin();tick=3000;uiLoop();tick+=200;uiLoop();assert(litePage==LitePage::Radar&&!liteStartup&&!standby);const uint32_t children=lv_obj_get_child_cnt(root);
 for(int i=0;i<5;++i)liteButtonEvent(P::Hold);assert(litePage==LitePage::Settings);tick+=100;uiLoop();assert(!strcmp(lv_label_get_text(pageHint),"TAP TO TOGGLE / HOLD MODES"));
 liteButtonEvent(P::Hold);assert(litePage==LitePage::Modes&&!standby);tick+=100;uiLoop();render(argv[1],"lite-01-modes");
 assert(!strcmp(lv_label_get_text(pageTitle),"MODES")&&strstr(lv_label_get_text(pageValue),LV_SYMBOL_RIGHT "  BISCUIT NODE  " LV_SYMBOL_LEFT)&&!strcmp(lv_label_get_text(pageHint),"TAP NEXT / 2x START"));
 const unsigned theme=radarTheme;liteButtonEvent(P::Tap);assert(liteModeChoice==1&&radarTheme==theme);tick+=100;uiLoop();
 assert(strstr(lv_label_get_text(pageValue),LV_SYMBOL_RIGHT "  JCMK Node  " LV_SYMBOL_LEFT)&&strstr(lv_label_get_text(pageSub),"JCMK"));render(argv[1],"lite-02-modes-jcmk");
 liteButtonEvent(P::Tap);assert(liteModeChoice==0);
 // A refusal stays on MODES and shows the reason for five seconds.
 stopwatch.toggle(stopwatchNow());liteButtonEvent(P::Double);assert(!nodeBusy()&&!liteNodeHost&&litePage==LitePage::Modes);
 tick+=100;uiLoop();assert(!strcmp(lv_label_get_text(pageSub),"Stop the stopwatch first"));render(argv[1],"lite-03-modes-refused");
 tick+=5000;uiLoop();assert(strstr(lv_label_get_text(pageSub),"Biscuit"));stopwatch.toggle(stopwatchNow());
 liteButtonEvent(P::Double);assert(nodeKind()==NodeKind::Biscuit&&liteNodeHost&&!findObject(liteNodeHost,&lv_btn_class)&&findObject(liteNodeHost,&lv_label_class,"HOLD BUTTON TO EXIT"));
 nodeLink.linked=nodeLink.registered=true;nodeLink.core=dragon::node::Core::Biscuit;nodeLink.wifi=48213;nodeLink.sent=50111;nodeLink.channel=3;nodeLink.planCount=11;for(uint8_t i=0;i<11;++i)nodeLink.plan[i]=i+1;
 tick+=250;uiLoop();render(argv[1],"lite-04-biscuit-node");
 liteButtonEvent(P::Tap);liteButtonEvent(P::Double);assert(nodeBusy()&&litePage==LitePage::Modes&&liteModeChoice==0&&radarTheme==theme);
 for(const char* name:{"radar","clock","settings","startup","modes","node"}){uiShow(name);assert(nodeBusy()&&liteNodeHost&&!liteStartup&&litePage==LitePage::Modes);}
 uiNotify("Example camera detected","",nullptr,false);assert(lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN));
 liteButtonEvent(P::Tap);liteButtonEvent(P::Double);assert(nodeBusy());   // short presses only show how to leave
 liteButtonEvent(P::Hold);assert(nodeBusy());tick+=250;uiLoop();assert(findObject(liteNodeHost,&lv_label_class,"HOLD AGAIN TO LEAVE"));render(argv[1],"lite-05-node-confirm");
 tick+=600;liteButtonEvent(P::Hold);assert(!nodeBusy()&&nodeStops==1&&liteNodeHost);liteButtonEvent(P::Hold);
 uiLoop();assert(!liteNodeHost&&litePage==LitePage::Modes&&liteModeChoice==0&&lv_obj_get_child_cnt(root)==children);
 liteButtonEvent(P::Tap);liteButtonEvent(P::Double);assert(nodeKind()==NodeKind::Jcmk);nodeLink.ble=5120;tick+=250;uiLoop();render(argv[1],"lite-06-jcmk-node");
 nodeStop();uiShow("modes");assert(!liteNodeHost&&litePage==LitePage::Modes&&liteModeChoice==1);
 // A node started over the clock (radio standby) leaves the radio alone, then hands it back on the way to MODES.
 liteButtonEvent(P::Hold);liteButtonEvent(P::Hold);assert(litePage==LitePage::Clock&&standby);
 assert(nodeStart(NodeKind::Biscuit));uiShow("node");setLitePage(LitePage::Radar);assert(standby);setLitePage(LitePage::Clock);
 liteButtonEvent(P::Hold);tick+=600;liteButtonEvent(P::Hold);uiLoop();assert(litePage==LitePage::Modes&&!standby&&liteModeChoice==0);
 // Resume after a restart: the node screen sits above the startup splash.
 uiShow("startup");assert(liteStartup);assert(nodeStart(NodeKind::Jcmk));uiShow("node");
 assert(lv_obj_get_index(liteNodeHost)>lv_obj_get_index(liteStartup));tick+=2300;uiLoop();assert(!liteStartup&&liteNodeHost);
 nodeStop();uiShow("radar");assert(litePage==LitePage::Radar&&!standby&&!liteNodeHost&&lv_obj_get_child_cnt(root)==children);
 // Vector Potential: MODES keeps its title clear of the theme mark.
 radarTheme=VectorPotentialTheme;refreshLiteTheme();setLitePage(LitePage::Modes);tick+=100;uiLoop();lv_obj_update_layout(root);
 {lv_area_t l,t;lv_obj_get_coords(logo,&l);lv_obj_get_coords(pageTitle,&t);assert(lv_img_get_src(logo)==&watch_logo_vp&&!lv_obj_has_flag(logo,LV_OBJ_FLAG_HIDDEN)&&l.y2<t.y1);}
 render(argv[1],"lite-07-modes-vp");setLitePage(LitePage::Radar);assert(lv_obj_has_flag(logo,LV_OBJ_FLAG_HIDDEN));radarTheme=theme;refreshLiteTheme();
 puts("PASS production Lite MODES page (tap/double/hold), node screen hold-to-exit, refusals, radio hand-back, resume over the splash, Vector Potential header and matching tile/full-frame rendering");
}
'''
def build(name, main, defines, folder):
    folder.mkdir(parents=True, exist_ok=True)
    for old in folder.glob('*.ppm'):
        old.unlink()
    cpp = work/(name+'.cpp')
    cpp.write_text(harness + main)
    exe = work/(name+'.exe')
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',*defines,'-DLV_CONF_INCLUDE_SIMPLE',
                   *['-I'+str(p) for p in (work,native,root/'hardware/spotpear/lib/lvgl',root/'firmware/include',root/'firmware/src',root/'lib/opendroneid/src')],
                   str(cpp),*[str(root/'firmware/src'/source) for source in ('radar_logo.cpp','node_ui.cpp','node_logo.cpp')],str(library),'-o',str(exe)],check=True)
    subprocess.run([str(exe),str(folder)],check=True)
    paths=sorted(folder.glob('*.ppm'))
    montage=Image.new('RGB',(240*4,264*((len(paths)+3)//4)),(15,19,22))
    for i,path in enumerate(paths):
        mask=Image.new('L',(240,240));ImageDraw.Draw(mask).ellipse((0,0,239,239),fill=255)
        montage.paste(Image.open(path).convert('RGB'),(i%4*240,i//4*264),mask)
        ImageDraw.Draw(montage).text((i%4*240+8,i//4*264+242),path.stem,fill='white')
    montage.save(folder/(name+'-pages.png'))

build('dial', dial_main, ['-DBOARD_M5DIAL','-DBOARD_RADAR_LITE'], work)
build('lite', lite_main, ['-DBOARD_RADAR_LITE'], work/'lite')
