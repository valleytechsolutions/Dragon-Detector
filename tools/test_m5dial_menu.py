"""Exercise and render the production Dial menus with native LVGL."""
from pathlib import Path
import re
import subprocess
import sys
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
work = root / '.work/m5dial-menu-tests'
work.mkdir(parents=True, exist_ok=True)
native = root / '.work/m5dial-render/native'
library = native / 'lvgl.lib'
if not library.exists():
    subprocess.run([sys.executable, str(root/'tools/render_m5dial.py')], check=True)
    if not library.exists():
        library = root / '.work/can-commander/native/lvgl.lib'
catalog = (root/'firmware/include/radar_theme_catalog.h').read_text()
names = '\n'.join(re.search(r'static const char\* '+name+r'\[\]=\{.*?\};', catalog).group() for name in ['radarThemeNames','radarThemeCommands'])
count = re.search(r'static constexpr unsigned RadarThemeCount=.*?;', catalog).group()
active = re.search(r'static bool radarThemeActive\(unsigned i\)\{.*?\}', catalog).group()
node_header = (root/'firmware/include/node_mode.h').read_text()
nodes = '\n'.join(re.search(pattern, node_header).group() for pattern in [r'enum class NodeKind[^;]*;', r'constexpr const char\* NodeBiscuitName=[^;]*;', r'constexpr const char\* NodeJcmkName=[^;]*;'])
source = r'''
#include <lvgl.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "wearable.h"
#include "ui_palette.h"
#include "audio_levels.h"
#define constrain(v,a,b) std::clamp(int(v),int(a),int(b))
namespace dragon {constexpr unsigned PersonalThemeId=34;}
NAMES
COUNT
ACTIVE
static unsigned radarTheme=1,themeChanges=0,savedBrightness=100,outputBrightness=100;
static bool prefsReady=true,alertSound=true,rtcWrite=true;
static int clockOffsetMinutes=-240;
static uint32_t savedEpoch=1;
static dragon::Calendar liveDate{2026,10,23,15,45,12};
static const auto& themeUiColors(unsigned theme){return dragon::UiPalettes[theme];}
static lv_color_t color(uint32_t rgb){return lv_color_hex(rgb);}
static uint32_t wallTime(){return savedEpoch;}
static void clockLocal(dragon::Calendar& d){d=liveDate;}
static bool configureClock(uint32_t epoch){if(!rtcWrite)return false;savedEpoch=epoch;return true;}
static void set_brightness(uint8_t n){outputBrightness=n;}
static unsigned soundVolume=100;
static dragon::AudioLevel audioLevel(){return dragon::audioLevelFor(alertSound,soundVolume);}
static void audioSetLevel(dragon::AudioLevel level){soundVolume=dragon::audioLevelVolume(level);alertSound=soundVolume!=0;}
static void refreshLiteTheme(){++themeChanges;}
static bool liteLocksport(){return radarTheme==6;}
enum class Family{Adult};struct Detector{bool adult=false;bool enabled(Family){return adult;}};static Detector detector;static void setAdultMode(bool enabled){detector.adult=enabled;}
enum class LitePage{Radar,Clock,Timer,Signals,Status};
static LitePage litePage=LitePage::Radar;
static void setLitePage(LitePage p){litePage=p;}
NODES
// Tools > Modes: the production session and screen are exercised by render_m5dial_ui.py.
static NodeKind startedNode=NodeKind::None;
static const char* nodeRefusal=nullptr;
static void dialMenuClose();
static bool liteNodeBegin(NodeKind kind){if(nodeRefusal)return false;startedNode=kind;dialMenuClose();return true;}
static const char* liteNodeError(){return nodeRefusal;}
struct Prefs {unsigned getUChar(const char*,unsigned n){return savedBrightness;}void putUChar(const char*,unsigned n){savedBrightness=n;}};
static Prefs litePrefs;
// Tools > BadUSB (badusb.cpp). The production runtime is exercised by test_badusb.py.
static uint32_t millis(){return 0;}
// macOS and glibc 2.38+ declare strlcpy; a renamed mock avoids clashing with either.
static size_t testStrlcpy(char* out,const char* in,size_t cap){size_t n=strlen(in);if(cap){size_t k=std::min(n,cap-1);memcpy(out,in,k);out[k]=0;}return n;}
#define strlcpy testStrlcpy
struct BadUsbStatus {bool connected,running,selected;unsigned line;const char* name;const char* message;const char* medium;bool keyboardMode;};
static bool usbSupported=true,usbKeyboard=false,usbRunning=false,usbSelected=false;
static unsigned usbRuns=0,usbStops=0,usbExits=0;
static bool badusbSupported(){return usbSupported;}
static unsigned badusbCount(){return 2;}
static const char* badusbName(unsigned i){return i==0?"demo.txt":i==1?"hello.txt":"";}
static void badusbRefresh(){}
static bool badusbSelect(unsigned i){if(usbRunning||i>=2)return false;usbSelected=true;return true;}
static bool badusbRun(){++usbRuns;if(!usbKeyboard)return false;usbRunning=true;return true;} // serial mode only asks for the keyboard restart
static void badusbStop(){if(usbRunning)++usbStops;usbRunning=false;}
static bool badusbKeyboardMode(){return usbKeyboard;}
static void badusbExitKeyboard(){++usbExits;usbRunning=false;}
static const char* badusbPreview(){return "STRING hello";}
static BadUsbStatus badusbStatus(){return {usbKeyboard,usbRunning,usbSelected,0,usbSelected?"demo.txt":"","Ready","Internal /badusb",usbKeyboard};}
static lv_obj_t* root;
static lv_obj_t* notice=nullptr;
static uint32_t noticeAt=0;
#include "MENU_SOURCE"
static std::vector<uint8_t> pixels(240*240*3);
static void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* b){for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x){auto c=lv_color_to32(*b++);auto n=(y*240+x)*3;pixels[n]=c>>16;pixels[n+1]=c>>8;pixels[n+2]=c;}lv_disp_flush_ready(d);}
static void render(const char* folder,const char* name){lv_obj_invalidate(root);lv_refr_now(nullptr);char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);FILE* f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");fwrite(pixels.data(),1,pixels.size(),f);fclose(f);}
int main(int argc,char** argv){
 lv_init();static lv_color_t tile[240*24];lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,tile,nullptr,240*24);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=d.ver_res=240;d.draw_buf=&buffer;d.flush_cb=flush;lv_disp_drv_register(&d);root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);dialMenuCreate();
 dialMenuOpen(DialMenu::Main);render(argv[1],"01-menu");unsigned theme=radarTheme;
 dialMenuRotate(1);dialMenuSelect();assert(!dialMenuVisible()&&litePage==LitePage::Clock&&radarTheme==theme);
 dialMenuOpen(DialMenu::Main);dialChoice=2;dialMenuSelect();assert(dialMenu==DialMenu::Tools);render(argv[1],"02-tools");dialChoice=0;dialMenuSelect();assert(litePage==LitePage::Timer&&!dialMenuVisible());
 auto rowText=[](int row){return std::string(lv_label_get_text(dialMenuLabels[row]));};
 dialMenuOpen(DialMenu::Tools);assert(dialMenuCount()==6);for(int i=0;i<3;++i)dialMenuRotate(1);assert(dialChoice==3&&rowText(1)=="Device status"&&rowText(2)=="Modes"&&rowText(3)=="BadUSB"&&rowText(4)=="Back");render(argv[1],"02a-tools-modes");
 dialMenuSelect();assert(dialMenu==DialMenu::Modes&&dialMenuCount()==3&&dialChoice==0&&std::string(lv_label_get_text(dialMenuTitle))=="MODES");
 assert(rowText(0)=="BISCUIT NODE"&&rowText(1)=="JCMK Node"&&rowText(2)=="Back"&&lv_obj_has_flag(dialMenuRows[3],LV_OBJ_FLAG_HIDDEN));render(argv[1],"02b-modes");
 nodeRefusal="Wi-Fi could not switch to node mode";dialMenuSelect();assert(dialMenu==DialMenu::Modes&&startedNode==NodeKind::None&&!strcmp(lv_label_get_text(dialMenuNote),nodeRefusal));
 assert(lv_label_get_long_mode(dialMenuNote)==LV_LABEL_LONG_SCROLL_CIRCULAR);render(argv[1],"02c-modes-refused");nodeRefusal=nullptr;
 dialMenuSelect();assert(startedNode==NodeKind::Biscuit&&!dialMenuVisible());
 dialMenuOpen(DialMenu::Modes);dialMenuRotate(1);dialMenuSelect();assert(startedNode==NodeKind::Jcmk&&!dialMenuVisible());
 dialMenuOpen(DialMenu::Modes);dialMenuRotate(-1);assert(dialChoice==2);dialMenuSelect();assert(dialMenu==DialMenu::Tools&&dialChoice==0);dialMenuBack();assert(dialMenu==DialMenu::Main);
 dialMenuOpen(DialMenu::Modes);dialMenuBack();assert(dialMenu==DialMenu::Tools);dialChoice=5;dialMenuSelect();assert(dialMenu==DialMenu::Main);
 // Tools > BadUSB follows Modes. In USB serial mode RUN only asks for the keyboard restart.
 dialMenuOpen(DialMenu::Tools);dialChoice=4;dialMenuSelect();assert(dialMenu==DialMenu::BadUsb&&usbSelected&&std::string(lv_label_get_text(dialMenuTitle))=="BADUSB");render(argv[1],"02d-badusb");
 dialChoice=2;dialMenuSelect();assert(usbRuns==1&&!usbRunning);dialMenuBack();assert(dialMenu==DialMenu::Tools);
 // A script running in keyboard mode stops when the menu leaves BadUSB, before Modes is reached.
 usbKeyboard=true;dialMenuOpen(DialMenu::BadUsb);dialChoice=2;dialMenuSelect();assert(usbRunning&&usbRuns==2);
 dialMenuOpen(DialMenu::Tools);assert(!usbRunning&&usbStops==1);dialChoice=3;dialMenuSelect();assert(dialMenu==DialMenu::Modes&&usbRuns==2);
 // In keyboard mode Back from BadUSB is EXIT, a restart as USB serial; it never runs a script.
 dialMenuOpen(DialMenu::BadUsb);dialChoice=4;dialMenuSelect();assert(usbExits==1&&usbRuns==2&&!usbRunning);usbKeyboard=false;
 // Profiles without native USB HID (Waveshare Lite, Watcher) list Modes, then Back.
 usbSupported=false;dialMenuOpen(DialMenu::Tools);assert(dialMenuCount()==5);dialChoice=4;dialMenuRefresh();assert(rowText(3)=="Modes"&&rowText(4)=="Back");
 dialChoice=3;dialMenuSelect();assert(dialMenu==DialMenu::Modes);dialMenuBack();dialChoice=4;dialMenuSelect();assert(dialMenu==DialMenu::Main);usbSupported=true;
 dialMenuOpen(DialMenu::Settings);render(argv[1],"03-settings");dialMenuSelect();assert(dialMenu==DialMenu::Time);render(argv[1],"04-time");dialChoice=1;dialMenuSelect();dialMenuRotate(1);assert(dialEditDate.minute==46);dialMenuSelect();dialChoice=3;dialMenuSelect();assert(savedEpoch==dragon::dateEpoch({2026,10,23,15,46,12})+240*60);
 dialMenuOpen(DialMenu::Date);dialEditDate={2028,1,31,1,1,1};dialChoice=1;dialMenuSelect();dialMenuRotate(1);assert(dialEditDate.month==2&&dialEditDate.day==29);dialMenuSelect();render(argv[1],"05-date");dialChoice=3;dialMenuSelect();assert(savedEpoch==dragon::dateEpoch({2028,2,29,15,45,12})+240*60);
 dialMenuOpen(DialMenu::Date);dialChoice=3;rtcWrite=false;dialMenuSelect();assert(dialMenu==DialMenu::Date&&strstr(lv_label_get_text(dialMenuNote),"failed"));rtcWrite=true;
 dialMenuOpen(DialMenu::Brightness);dialMenuRotate(-1);assert(outputBrightness==95);dialMenuBack();assert(outputBrightness==100&&savedBrightness==100);dialMenuOpen(DialMenu::Brightness);dialMenuRotate(-1);dialMenuSelect();assert(savedBrightness==95&&outputBrightness==95);
 dialMenuOpen(DialMenu::Themes);std::vector<unsigned> activeIds;for(unsigned id=0;id<RadarThemeCount;++id)if(id!=34&&id!=40)activeIds.push_back(id);assert(dialMenuCount()==int(activeIds.size()));for(int i=0;i<dialMenuCount();++i){unsigned id=dialThemeAt(i);assert(id==activeIds[i]&&dialThemeLabel(id)[0]);dialMenuRotate(1);}assert(dialThemeAt(-1)==0&&dialThemeAt(dialMenuCount())==0);assert(!dialThemeLabel(40)[0]&&!dialThemeLabel(999)[0]);assert(radarTheme==theme&&themeChanges==0);dialChoice=9;dialMenuRefresh();render(argv[1],"06-themes");dialMenuSelect();assert(themeChanges==1&&!dialMenuVisible());
 radarTheme=23;dialMenuOpen(DialMenu::Settings);render(argv[1],"07-light-settings");dialChoice=3;dialMenuSelect();assert(audioLevel()==dragon::AudioLevel::Silent);dialMenuSelect();assert(audioLevel()==dragon::AudioLevel::Medium);dialMenuSelect();assert(audioLevel()==dragon::AudioLevel::High);
 lv_obj_update_layout(root);for(auto* row:dialMenuRows){lv_area_t a;lv_obj_get_coords(row,&a);for(int x:{a.x1,a.x2})for(int y:{a.y1,a.y2})assert((x-120)*(x-120)+(y-120)*(y-120)<=120*120);}
 dialMenuBack();assert(dialMenu==DialMenu::Main);dialMenuBack();assert(!dialMenuVisible());
 puts("PASS Dial menu routes, Tools > Modes node starts and refusals, Tools > BadUSB beside Modes, explicit theme selection, time/date saves, RTC failures, leap dates, brightness save/cancel and round-screen bounds");
}
'''.replace('NAMES', names).replace('COUNT', count).replace('ACTIVE', active).replace('NODES', nodes).replace('MENU_SOURCE', (root/'firmware/src/dial_menu.inc').as_posix())
cpp = work/'menus.cpp'
cpp.write_text(source)
exe = work/'menus.exe'
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',
               '-I'+str(native),'-I'+str(root/'hardware/spotpear/lib/lvgl'),
               '-I'+str(root/'firmware/include'),'-DLV_CONF_INCLUDE_SIMPLE',str(cpp),str(library),'-o',str(exe)],check=True)
subprocess.run([str(exe),str(work)],check=True)
paths=sorted(work.glob('*.ppm'))
montage=Image.new('RGB',(240*4,264*((len(paths)+3)//4)),(15,19,22))
for i,path in enumerate(paths):
    frame=Image.open(path).convert('RGB')
    mask=Image.new('L',(240,240));ImageDraw.Draw(mask).ellipse((0,0,239,239),fill=255)
    montage.paste(frame,(i%4*240,i//4*264),mask)
    ImageDraw.Draw(montage).text((i%4*240+8,i//4*264+242),path.stem,fill='white')
montage.save(work/'dial-menus.png')
