"""Render the production rectangular fields and clocks, with tile/full comparisons.

Uses the native LVGL library built by render_node_screen.py. Node pages have their
own complete widget/input harness in render_node_screen.py.
"""
from pathlib import Path
import subprocess
import sys
from PIL import Image

root = Path(__file__).resolve().parents[1]
work = root / '.work/waveshare206-render'
work.mkdir(parents=True, exist_ok=True)
native = root / '.work/node-render/native'
library = native / 'liblvgl.a'
if not library.exists():
    subprocess.run([sys.executable, str(root/'tools/render_node_screen.py')], check=True)
lv = root / 'hardware/spotpear/lib/lvgl'
renderer = (root/'firmware/src/radar_renderer.inc').read_text(encoding='utf-8')
helpers = renderer[renderer.index('static void line('):renderer.index('#ifdef BOARD_WAVESHARE_206')]
catalog = (root/'firmware/include/radar_theme_catalog.h').read_text(encoding='utf-8')
palettes = '#include "radar_theme_catalog.h"\n'
watch = (root/'firmware/src/watch_faces.inc').read_text(encoding='utf-8')
watch_palettes = watch[watch.index('struct WatchPalette'):watch.index('static constexpr uint8_t WatchFaces')]
source = r'''
#include <lvgl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "wearable.h"
#include "rectangular_layout.h"
#include "ui_palette.h"
#include "personal_theme_format.h"
#define F12 (&lv_font_montserrat_16)
#define F16 (&lv_font_montserrat_20)
#define F20 (&lv_font_montserrat_26)
extern "C" {LV_FONT_DECLARE(dragon_watch_digits);}
#define FWATCH (&dragon_watch_digits)
static unsigned radarTheme=1,watchFace=0;
static float pigX=180;static unsigned pigStep=0;static int pigLook=0;static bool pigReacting=false;
extern "C" {LV_FONT_DECLARE(dragon_ascii_small);}

static uint32_t radarSweepAt=2100;
static int batteryPercent(){return 87;}
static bool batteryCharging(){return false;}
static dragon::PersonalTheme custom;
const dragon::PersonalTheme& personalTheme(){return custom;}
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
''' + helpers + palettes + watch_palettes + r'''
bool personalThemeReady(){return true;}
const dragon::UiPalette& themeUiColors(unsigned i){return dragon::UiPalettes[i];}
static lv_img_dsc_t personalLogo;
const lv_img_dsc_t* personalThemeLogo(bool){return &personalLogo;}
#include "rectangular_draw.inc"
#include "rectangular_watch.inc"
static int width=410,height=502;
static bool clockPage=false;
static std::vector<unsigned char> pixels;
static void flush(lv_disp_drv_t* display,const lv_area_t* area,lv_color_t* buffer){
 for(int y=area->y1;y<=area->y2;++y)for(int x=area->x1;x<=area->x2;++x){assert(x>=0&&x<width&&y>=0&&y<height);auto c=lv_color_to32(*buffer++);auto n=(y*width+x)*3;pixels[n]=(c>>16)&255;pixels[n+1]=(c>>8)&255;pixels[n+2]=c&255;}
 lv_disp_flush_ready(display);
}
static void draw(lv_event_t* e){auto* ctx=lv_event_get_draw_ctx(e);
 if(clockPage)rectangularWatch(ctx,watches[watchFace],dragon::Calendar{2026,11,26,10,24,36},true);
 else{
  rectangularRadarField(ctx,2100);const auto p=radarPalette(radarTheme);
  panelText(ctx,radarThemeNames[radarTheme],18,16,width-88,F20,p.text);
  panelText(ctx,"WI-FI + BLE / 6 SIGNALS",18,50,width-36,F12,p.text);
  panelText(ctx,"NEARBY / 3 DEVICES",18,73,width-36,F12,p.text);
  for(int i=0;i<6;++i){auto b=dragon::rectangularRadarLabel(i,width,height);panelBox(ctx,b.x,b.y,b.w,b.h,0x020c0c,p.ring,5);panelText(ctx,i%2?"Camera vendor\n-68 dBm":"Axon / TASER\n-55 dBm",b.x,b.y,b.w,&lv_font_montserrat_14,0xeaf9dd);}
  for(int i=0;i<12;++i){auto pt=dragon::rectangularRadarPoint(100+i*5,std::cos(i*.52f),std::sin(i*.52f),width,height);disc(ctx,pt.x,pt.y,5,i%2?0xffad68:0x60e4fa);}
  panelText(ctx,"Tap a signal for details",18,height-96,width-36,F12,p.text);
 }
 const auto p=clockPage?watches[watchFace]:WatchPalette{"",0x020c0c,0xeaf9dd,0,0x37694c,false};
 for(int i=0;i<2;++i){const int x=18+i*(width/2-6),y=height-(clockPage?48:56),w=(width-48)/2;panelBox(ctx,x,y,w,clockPage?36:42,p.bg,p.muted);panelText(ctx,i?"TOOLS":clockPage?"SETTINGS":"TARGETS",x,y+9,w,&lv_font_montserrat_16,p.ink);}
}
int main(int argc,char** argv){
 lv_init();const bool logosReady=prepareThemeLogos();assert(logosReady);width=std::atoi(argv[2]);height=std::atoi(argv[3]);pixels.resize(width*height*3);
 static std::vector<uint8_t> logoPixels(123*123*3);for(int y=0;y<123;++y)for(int x=0;x<123;++x){auto c=lv_color_hex((x/12+y/12)%2?0x68e2c1:0x72aaff);auto* p=&logoPixels[(y*123+x)*3];memcpy(p,&c,2);p[2]=(x>8&&x<114&&y>8&&y<114)?255:0;}
 personalLogo.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;personalLogo.header.w=personalLogo.header.h=123;personalLogo.data=logoPixels.data();personalLogo.data_size=logoPixels.size();
 static std::vector<lv_color_t> buffer(width*height);lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,buffer.data(),nullptr,buffer.size());
 lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=width;driver.ver_res=height;driver.draw_buf=&buf;driver.flush_cb=flush;auto* display=lv_disp_drv_register(&driver);
 auto* obj=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(obj);lv_obj_set_size(obj,width,height);lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(obj,draw,LV_EVENT_DRAW_MAIN,nullptr);
 for(int mode=0;mode<2;++mode){clockPage=mode;unsigned count=mode?sizeof(watches)/sizeof(watches[0]):sizeof(radarPalettes)/sizeof(radarPalettes[0]);
  for(unsigned theme=0;theme<count;++theme){if(mode?!watches[theme].name:!dragon::uiThemeActive(theme))continue;watchFace=radarTheme=theme;
   display->driver->draw_buf->size=width*height;lv_obj_invalidate(obj);lv_refr_now(display);auto full=pixels;
   display->driver->draw_buf->size=width*32;lv_obj_invalidate(obj);lv_refr_now(display);// RGB565 alpha rounding can differ by one level at a clipped stroke edge.
   unsigned differences=0;for(size_t n=0;n<pixels.size();n+=3){bool changed=false;for(int c=0;c<3;++c){const int delta=std::abs(int(pixels[n+c])-int(full[n+c]));assert(delta<=(c==1?4:8));changed|=delta!=0;}differences+=changed;}assert(differences<unsigned(width+height));
   char path[512];snprintf(path,sizeof(path),"%s/%dx%d-%s-%02u.ppm",argv[1],width,height,mode?"clock":"radar",theme);FILE* out=fopen(path,"wb");assert(out);fprintf(out,"P6\n%d %d\n255\n",width,height);fwrite(pixels.data(),1,pixels.size(),out);fclose(out);
  }
 }
 clockPage=true;watchFace=dragon::PersonalWatchId;
 for(int option=0;option<5;++option){std::vector<unsigned char> previous;
  const int count=option==0?3:option==1?4:option==2?3:option==3?4:2;
  for(int value=0;value<count;++value){custom={};
   if(option==0)custom.layout=value;if(option==1)custom.hands=value;
   if(option==2)custom.font=value;if(option==3)custom.markers=value;
   if(option==4)custom.flags=value?0:7;
   lv_obj_invalidate(obj);lv_refr_now(display);assert(previous.empty()||previous!=pixels);previous=pixels;
   char path[512];snprintf(path,sizeof(path),"%s/%dx%d-personal-%d-%d.ppm",argv[1],width,height,option,value);FILE* out=fopen(path,"wb");assert(out);fprintf(out,"P6\n%d %d\n255\n",width,height);fwrite(pixels.data(),1,pixels.size(),out);fclose(out);
  }
 }
 clockPage=false;radarTheme=dragon::PersonalThemeId;std::vector<unsigned char> previous;
 for(unsigned sweep=0;sweep<3;++sweep){custom.sweep=sweep;lv_obj_invalidate(obj);lv_refr_now(display);assert(previous.empty()||previous!=pixels);previous=pixels;}
 puts("PASS native rectangular clocks and radar fields; full and 32-row tiles agree within one RGB565 level");
}
'''
(work/'render.cpp').write_text(source, encoding='utf-8')
zig = [sys.executable, '-m', 'ziglang']
(work/'esp_heap_caps.h').write_text('#pragma once\n#include <cstdlib>\n#define MALLOC_CAP_SPIRAM 0\n#define MALLOC_CAP_8BIT 0\ninline void* heap_caps_malloc(size_t n,int){return malloc(n);}\ninline void heap_caps_free(void* p){free(p);}\n')
includes = ['-I'+str(p) for p in (work, native, lv, root/'firmware/include', root/'firmware/src')]
font = work/'watch-font.o'
subprocess.run(zig+['cc', '-DBOARD_WAVESHARE_206', '-DLV_CONF_INCLUDE_SIMPLE', *includes,
                   '-c', str(root/'firmware/src/watch_digits.c'), '-o', str(font)], check=True)
subprocess.run(zig+['cc', '-DBOARD_WAVESHARE_206', '-DLV_CONF_INCLUDE_SIMPLE', *includes, '-c', str(root/'firmware/src/ascii_fonts.c'), '-o', str(work/'ascii.o')], check=True)
exe = work/'render.exe'
subprocess.run(zig+['c++', '-DBOARD_WAVESHARE_206', '-DSCREEN_RES=410', '-std=c++17', '-UNDEBUG', '-O2', '-DLV_CONF_INCLUDE_SIMPLE', *includes,
                   str(work/'render.cpp'), str(font), str(root/'firmware/src/radar_logo.cpp'), str(root/'firmware/src/vlad_art.cpp'), str(work/'ascii.o'), str(library), '-o', str(exe)], check=True)
for width,height in ((410,502),(502,410)):
    subprocess.run([str(exe),str(work),str(width),str(height)], check=True)
for path in work.glob('*.ppm'):
    Image.open(path).save(path.with_suffix('.png'))
print('Previews:', work)
