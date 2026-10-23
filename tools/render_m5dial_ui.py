"""Render production Dial pages and exercise menu/input routes with native LVGL."""
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
static uint32_t tick=0;
inline uint32_t millis(){return tick;}
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
#include "wearable.h"
#include "personal_theme.h"
#include <cassert>
#include <cstdio>
using dragon::Family;
static Contact entries[MAX_CONTACTS];
static ContactStore contacts{entries};
static dragon::DetectorSettings detector;
static dragon::TrafficStatus trafficStatus;
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
static void audioSave(){} static void audioStop(){}
static void macText(const uint8_t* m,char* out){snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);}
void set_brightness(uint8_t){} void dialInputStatus(){}
void uiKey(bool);
''')
ui = (root/'firmware/src/ui_lite.cpp').read_text().replace('#include "app.h"', '#include "test_app.h"')
source = r'''
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
 for(unsigned theme:{0,3,9,23,28,29,31,35,39,40}){uiShow("clock");radarTheme=theme;refreshLiteTheme();tick+=200;uiLoop();char label[40];snprintf(label,sizeof(label),"clock-theme-%02u",theme);render(argv[1],label);}
 uiShow("radar");assert(!standby&&!dialMenuVisible());
 puts("PASS production Dial pages, ring/button menu navigation, timer, radio standby and matching tile/full-frame rendering");
}
'''
cpp = work/'ui.cpp'
cpp.write_text(source)
exe = work/'ui.exe'
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',
               '-DBOARD_M5DIAL','-DBOARD_RADAR_LITE','-DLV_CONF_INCLUDE_SIMPLE',
               *['-I'+str(p) for p in (work,native,root/'hardware/spotpear/lib/lvgl',root/'firmware/include',root/'firmware/src',root/'lib/opendroneid/src')],
               str(cpp),str(root/'firmware/src/radar_logo.cpp'),str(library),'-o',str(exe)],check=True)
subprocess.run([str(exe),str(work)],check=True)
paths=sorted(work.glob('*.ppm'))
montage=Image.new('RGB',(240*4,264*((len(paths)+3)//4)),(15,19,22))
for i,path in enumerate(paths):
    mask=Image.new('L',(240,240));ImageDraw.Draw(mask).ellipse((0,0,239,239),fill=255)
    montage.paste(Image.open(path).convert('RGB'),(i%4*240,i//4*264),mask)
    ImageDraw.Draw(montage).text((i%4*240+8,i//4*264+242),path.stem,fill='white')
montage.save(work/'dial-pages.png')
