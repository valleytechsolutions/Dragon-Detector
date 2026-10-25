"""Render the actual VectorPotential drawing code through LVGL at all panel sizes."""
from pathlib import Path
import subprocess
import sys
from PIL import Image

root = Path(__file__).resolve().parents[1]
work = root / ".work/vectorpotential"
stub = work / "native"
stub.mkdir(parents=True, exist_ok=True)
config = """#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_USE_LOG 0
#define LV_USE_SNAPSHOT 1
#define LV_TICK_CUSTOM 0
""" + "".join(f"#define LV_FONT_MONTSERRAT_{n} 1\n" for n in [8,10,12,14,16,18,20,22,24,26,28,32,40,48]) + "#endif\n"
(stub / "lv_conf.h").write_text(config)
(stub / "esp_heap_caps.h").write_text("#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 0\n#define MALLOC_CAP_8BIT 0\ninline void* heap_caps_malloc(size_t n,int){return malloc(n);}\n")
lv = root / "hardware/spotpear/lib/lvgl"
zig = [sys.executable, "-m", "ziglang"]
includes = ["-I"+str(stub), "-I"+str(lv), "-I"+str(root/"firmware/include"), "-DLV_CONF_INCLUDE_SIMPLE"]
lib = stub / "lvgl.lib"
shared = root / ".work/can-commander/native"
if (shared/"lv_conf.h").is_file() and (shared/"lv_conf.h").read_text(encoding='utf-8') == config and (shared/"lvgl.lib").is_file():
    lib = shared / "lvgl.lib"
if not lib.exists():
    sources = [str(p) for p in (lv/"src").rglob("*.c") if "demos" not in p.parts]
    args = ["-lc","-O","ReleaseFast","--name","lvgl",*includes,"-femit-bin="+str(lib),"-cflags","-std=c99","--",*sources]
    (stub/"lvgl.rsp").write_text(" ".join('"'+a.replace("\\","/")+'"' for a in args))
    subprocess.run(zig+["build-lib","@"+str(stub/"lvgl.rsp")], check=True, cwd=root)
renderer = (root/"firmware/src/radar_renderer.inc").read_text(encoding='utf-8')
helpers = renderer[renderer.index("static void line("):renderer.index('#include "lpl_radar.inc"')]
art = (root/"firmware/src/cyber_runner.inc").read_text(encoding='utf-8')
art = art[art.index("static void themeStroke"):art.index("static void boneSweep")]
panel = (root/"firmware/src/capsule_theme.inc").read_text(encoding='utf-8')
panel = panel[panel.index("static void capsulePanel"):panel.index("static void capsuleShell")]
source = """#include <lvgl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "board_pins.h"
#include "wearable.h"
#define S(v) (((v)*SCREEN_RES+((v)>=0?180:-180))/360)
static constexpr int CENTER=SCREEN_RES/2;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
bool prepareThemeLogos();
""" + helpers + art + panel + '\n#include "' + str(root/"firmware/src/vectorpotential_theme.inc").replace("\\","/") + '"\n' + r"""
static bool watch=true,valid=true;
static dragon::Calendar date{2026,10,20,10,24,36};
static std::vector<unsigned char> pixels(SCREEN_RES*SCREEN_RES*3);
static void flush(lv_disp_drv_t* display,const lv_area_t* area,lv_color_t* buffer){
 for(int y=area->y1;y<=area->y2;++y)for(int x=area->x1;x<=area->x2;++x){auto c=lv_color_to32(*buffer++);auto n=(y*SCREEN_RES+x)*3;pixels[n]=(c>>16)&255;pixels[n+1]=(c>>8)&255;pixels[n+2]=c&255;}
 lv_disp_flush_ready(display);
}
static void draw(lv_event_t* e){auto* ctx=lv_event_get_draw_ctx(e);
 if(watch)vpWatch(ctx,date,valid);
 else{vpRadarField(ctx);
 capsuleText(ctx,"WI-FI + BLE / 0 SIGNALS",56,66,248,VpSmall,0xe4faff);
 capsuleText(ctx,"TAP A SIGNAL",70,280,220,VpSmall,0xe4faff);
 capsulePanel(ctx,90,301,90,32,0x0b1b25,0x477b8c);
 capsuleText(ctx,"TARGETS",90,311,90,VpSmall,0xe4faff);
 capsulePanel(ctx,187,301,78,32,0x0b1b25,0x477b8c);
 capsuleText(ctx,"TOOLS",187,311,78,VpSmall,0xe4faff);
 }
}
int main(int argc,char** argv){
 lv_init();static std::vector<lv_color_t> buffer(SCREEN_RES*SCREEN_RES);lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,buffer.data(),nullptr,buffer.size());
 lv_disp_drv_t display;lv_disp_drv_init(&display);display.hor_res=SCREEN_RES;display.ver_res=SCREEN_RES;display.draw_buf=&buf;display.flush_cb=flush;lv_disp_drv_register(&display);
 if(!prepareThemeLogos())return 2;
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0),0);
 auto* obj=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(obj);lv_obj_set_size(obj,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(obj,draw,LV_EVENT_DRAW_MAIN,nullptr);
 for(int i=0;i<5;++i){watch=i!=4;valid=i!=3;
 date.hour=i==1?3:10;date.minute=i==2?0:i==1?40:8;date.second=36;
 lv_obj_invalidate(obj);lv_refr_now(nullptr);char path[512];snprintf(path,sizeof(path),"%s/%d-vp-%d-%s.ppm",argv[1],SCREEN_RES,i,watch?"watch":"radar");
 FILE* out=fopen(path,"wb");if(!out)return 3;fprintf(out,"P6\n%d %d\n255\n",SCREEN_RES,SCREEN_RES);fwrite(pixels.data(),1,pixels.size(),out);fclose(out);
 }
 return 0;
}
"""
(stub/"render.cpp").write_text(source)
for size,flag in [(240,"BOARD_RADAR_LITE"),(360,None),(466,"BOARD_STOPWATCH"),(480,"BOARD_CROWPANEL_21")]:
    exe=stub/f"render-{size}.exe"
    subprocess.run(zig+["c++","-std=c++17","-O2",*includes,*(["-D"+flag] if flag else []),str(stub/"render.cpp"),str(root/"firmware/src/radar_logo.cpp"),str(lib),"-o",str(exe)],check=True,cwd=root)
    subprocess.run([str(exe),str(work)],check=True,cwd=root)
    print("Rendered VectorPotential:",size,flush=True)
for p in work.glob("*.ppm"):
    Image.open(p).save(p.with_suffix(".png"))
print("Actual theme drawing previews ready; radar status and controls are illustrative.")
