"""Compare Dial on-demand artwork with native LVGL indexed-image rendering."""
from pathlib import Path
import subprocess
import sys
from PIL import Image

root = Path(__file__).resolve().parents[1]
work = root / ".work/m5dial-render"
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

source = r"""
#define BOARD_M5DIAL
#define BOARD_RADAR_LITE
#include <lvgl.h>
#include <cassert>
#include <vector>
#include <cstdio>
#include <cstring>
#include "RADAR_SOURCE"
static std::vector<uint8_t> pixels(240*240*3);
static void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* b){
 for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x){auto c=lv_color_to32(*b++);int n=(y*240+x)*3;pixels[n]=c>>16;pixels[n+1]=c>>8;pixels[n+2]=c;}
 lv_disp_flush_ready(d);
}
int main(int argc,char** argv){
 lv_init();assert(prepareThemeLogos());static lv_color_t tile[240*24];static lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,tile,nullptr,240*24);
 lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=d.ver_res=240;d.draw_buf=&buffer;d.flush_cb=flush;lv_disp_drv_register(&d);
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x123456),0);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
 auto* image=lv_img_create(lv_scr_act());unsigned count=0;
 for(const auto& item:packedThemeLogos){
   lv_img_set_src(image,item.image);lv_obj_center(image);lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);auto actual=pixels;
   char path[512];snprintf(path,sizeof(path),"%s/dial-art-%02u.ppm",argv[1],count);FILE* f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");fwrite(pixels.data(),1,pixels.size(),f);fclose(f);
   std::vector<uint8_t> raw(item.image->data_size);assert(dragon::unpackBlock(item.bytes,item.length,raw.data(),raw.size()));
   lv_img_dsc_t baseline=*item.image;baseline.header.cf=item.format;baseline.data=raw.data();lv_img_set_src(image,&baseline);lv_obj_center(image);lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
   if(actual!=pixels){snprintf(path,sizeof(path),"%s/dial-baseline-%02u.ppm",argv[1],count);f=fopen(path,"wb");fprintf(f,"P6\n240 240\n255\n");fwrite(pixels.data(),1,pixels.size(),f);fclose(f);fprintf(stderr,"Dial image %u rendering mismatch\n",count);return 1;}
   lv_img_cache_invalidate_src(nullptr);++count;
 }
 printf("PASS %u Dial artwork renders match native indexed-image output at 240x240 with 24-row tiles\n",count);
}
""".replace('RADAR_SOURCE',str(root/'firmware/src/radar_logo.cpp').replace('\\','/'))
cpp=stub/'compare.cpp';cpp.write_text(source);exe=stub/'compare.exe'
subprocess.run(zig+['c++','-std=c++17','-O2','-UNDEBUG',*includes,str(cpp),str(lib),'-o',str(exe)],check=True,cwd=root)
subprocess.run([str(exe),str(work)],check=True,cwd=root)
images=[Image.open(p).convert('RGB') for p in sorted(work.glob('dial-art-*.ppm'))]
montage=Image.new('RGB',(240*8,240*((len(images)+7)//8)),(9,16,22))
for i,img in enumerate(images):montage.paste(img,(i%8*240,i//8*240))
montage.save(work/'dial-art-validation.png')
