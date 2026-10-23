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
names = re.search(r'static const char\* radarThemeNames\[\]=\{.*?\};', catalog).group()
source = r'''
#include <lvgl.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "wearable.h"
#include "ui_palette.h"
#define constrain(v,a,b) std::clamp(int(v),int(a),int(b))
namespace dragon {constexpr unsigned PersonalThemeId=34;}
NAMES
constexpr unsigned RadarThemeCount=41;
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
static void audioSave(){} static void audioStop(){}
static void refreshLiteTheme(){++themeChanges;}
static bool liteLocksport(){return radarTheme==6;}
enum class LitePage{Radar,Clock,Timer,Signals,Status};
static LitePage litePage=LitePage::Radar;
static void setLitePage(LitePage p){litePage=p;}
struct Prefs {unsigned getUChar(const char*,unsigned n){return savedBrightness;}void putUChar(const char*,unsigned n){savedBrightness=n;}};
static Prefs litePrefs;
static lv_obj_t* root;
#include "MENU_SOURCE"
static std::vector<uint8_t> pixels(240*240*3);
static void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* b){for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x){auto c=lv_color_to32(*b++);auto n=(y*240+x)*3;pixels[n]=c>>16;pixels[n+1]=c>>8;pixels[n+2]=c;}lv_disp_flush_ready(d);}
static void render(const char* folder,const char* name){lv_obj_invalidate(root);lv_refr_now(nullptr);char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);FILE* f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");fwrite(pixels.data(),1,pixels.size(),f);fclose(f);}
int main(int argc,char** argv){
 lv_init();static lv_color_t tile[240*24];lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,tile,nullptr,240*24);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=d.ver_res=240;d.draw_buf=&buffer;d.flush_cb=flush;lv_disp_drv_register(&d);root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);dialMenuCreate();
 dialMenuOpen(DialMenu::Main);render(argv[1],"01-menu");unsigned theme=radarTheme;
 dialMenuRotate(1);dialMenuSelect();assert(!dialMenuVisible()&&litePage==LitePage::Clock&&radarTheme==theme);
 dialMenuOpen(DialMenu::Main);dialChoice=2;dialMenuSelect();assert(dialMenu==DialMenu::Tools);render(argv[1],"02-tools");dialChoice=0;dialMenuSelect();assert(litePage==LitePage::Timer&&!dialMenuVisible());
 dialMenuOpen(DialMenu::Settings);render(argv[1],"03-settings");dialMenuSelect();assert(dialMenu==DialMenu::Time);render(argv[1],"04-time");dialChoice=1;dialMenuSelect();dialMenuRotate(1);assert(dialEditDate.minute==46);dialMenuSelect();dialChoice=3;dialMenuSelect();assert(savedEpoch==dragon::dateEpoch({2026,10,23,15,46,12})+240*60);
 dialMenuOpen(DialMenu::Date);dialEditDate={2028,1,31,1,1,1};dialChoice=1;dialMenuSelect();dialMenuRotate(1);assert(dialEditDate.month==2&&dialEditDate.day==29);dialMenuSelect();render(argv[1],"05-date");dialChoice=3;dialMenuSelect();assert(savedEpoch==dragon::dateEpoch({2028,2,29,15,45,12})+240*60);
 dialMenuOpen(DialMenu::Date);dialChoice=3;rtcWrite=false;dialMenuSelect();assert(dialMenu==DialMenu::Date&&strstr(lv_label_get_text(dialMenuNote),"failed"));rtcWrite=true;
 dialMenuOpen(DialMenu::Brightness);dialMenuRotate(-1);assert(outputBrightness==95);dialMenuBack();assert(outputBrightness==100&&savedBrightness==100);dialMenuOpen(DialMenu::Brightness);dialMenuRotate(-1);dialMenuSelect();assert(savedBrightness==95&&outputBrightness==95);
 dialMenuOpen(DialMenu::Themes);assert(dialMenuCount()==40);for(int i=0;i<40;++i){assert(dialThemeAt(i)!=34);dialMenuRotate(1);}assert(radarTheme==theme&&themeChanges==0);dialChoice=9;dialMenuRefresh();render(argv[1],"06-themes");dialMenuSelect();assert(themeChanges==1&&!dialMenuVisible());
 radarTheme=23;dialMenuOpen(DialMenu::Settings);render(argv[1],"07-light-settings");dialChoice=3;bool sound=alertSound;dialMenuSelect();assert(alertSound!=sound);
 lv_obj_update_layout(root);for(auto* row:dialMenuRows){lv_area_t a;lv_obj_get_coords(row,&a);for(int x:{a.x1,a.x2})for(int y:{a.y1,a.y2})assert((x-120)*(x-120)+(y-120)*(y-120)<=120*120);}
 dialMenuBack();assert(dialMenu==DialMenu::Main);dialMenuBack();assert(!dialMenuVisible());
 puts("PASS Dial menu routes, explicit theme selection, time/date saves, RTC failures, leap dates, brightness save/cancel and round-screen bounds");
}
'''.replace('NAMES', names).replace('MENU_SOURCE', (root/'firmware/src/dial_menu.inc').as_posix())
cpp = work/'menus.cpp'
cpp.write_text(source)
exe = work/'menus.exe'
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG',
               '-I'+str(native),'-I'+str(root/'hardware/spotpear/lib/lvgl'),
               '-I'+str(root/'firmware/include'),'-DLV_CONF_INCLUDE_SIMPLE',str(cpp),str(library),'-o',str(exe)],check=True)
subprocess.run([str(exe),str(work)],check=True)
paths=sorted(work.glob('*.ppm'))
montage=Image.new('RGB',(240*4,264*2),(15,19,22))
for i,path in enumerate(paths):
    frame=Image.open(path).convert('RGB')
    mask=Image.new('L',(240,240));ImageDraw.Draw(mask).ellipse((0,0,239,239),fill=255)
    montage.paste(frame,(i%4*240,i//4*264),mask)
    ImageDraw.Draw(montage).text((i%4*240+8,i//4*264+242),path.stem,fill='white')
montage.save(work/'dial-menus.png')
