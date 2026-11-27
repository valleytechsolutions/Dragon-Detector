"""Exercise and render the production P4 launcher with native LVGL."""
from pathlib import Path
import re
import subprocess
import sys
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
work = root / '.work/p4-launcher-tests'
work.mkdir(parents=True, exist_ok=True)
native = root / '.work/watcher-rotary-tests'
if not (native/'lvgl.lib').exists():
    subprocess.run([sys.executable, str(root/'tools/test_watcher_rotary_ui.py'), '--p4'], check=True)
(work/'lv_conf.h').write_bytes((native/'lv_conf.h').read_bytes())
bindings = (root/'firmware/src/p4_ui_bindings.inc').read_text()
catalog = bindings[bindings.index('static const p4ui::LauncherItem'):bindings.index('\n};')+3]
titles = re.findall(r'^\s*\{"([^"]+)"', catalog, re.M)
featured = [titles.index(title) for title in ('Biscuit Node', 'JCMK Node', 'Music', 'Voice Recorder')]
featured_indices = ', '.join(str(index) for index in featured)
source = r'''
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "p4_launcher.h"
#include "p4_menu.h"
''' + catalog + r'''
constexpr unsigned featured[]={''' + featured_indices + r'''};
unsigned opened=0;int lastAction=0;
void open(int id){++opened;lastAction=id;}
void frame(unsigned ms=35){lv_tick_inc(ms);lv_timer_handler();}
void capture(lv_obj_t* root,const char* path){
  frame(220);auto* shot=lv_snapshot_take(root,LV_IMG_CF_TRUE_COLOR);assert(shot);
  assert(shot->header.w==466&&shot->header.h==466);const size_t bytes=466*466*sizeof(lv_color_t);
  FILE* out=fopen(path,"wb");assert(out);assert(fwrite(shot->data,1,bytes,out)==bytes);fclose(out);lv_snapshot_free(shot);
}
int main(){
  lv_init();static lv_color_t pixels[466*32];static lv_disp_draw_buf_t buf;
  lv_disp_draw_buf_init(&buf,pixels,nullptr,466*32);lv_disp_drv_t driver;lv_disp_drv_init(&driver);
  driver.hor_res=driver.ver_res=466;driver.draw_buf=&buf;
  driver.flush_cb=[](lv_disp_drv_t* d,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(d);};lv_disp_drv_register(&driver);
  auto* root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(root,0,0);
  p4ui::Launcher launcher;constexpr unsigned count=sizeof(p4LauncherItems)/sizeof(*p4LauncherItems);
  for(unsigned theme:{12u,20u,6u}){
    auto palette=dragon::UiPalettes[theme];lv_obj_set_style_bg_color(root,lv_color_hex(palette.bg),0);
    launcher.create(root,palette,p4LauncherItems,count,open);frame();
    for(unsigned i=0;i<count;++i){
      unsigned index=launcher.current();assert(!launcher.turn(0));assert(launcher.current()==index);
      bool captureCard=theme==12;for(auto selected:featured)captureCard|=index==selected;
      if(captureCard){char file[64];snprintf(file,sizeof(file),"theme-%u-item-%u.raw",theme,index);capture(root,file);}
      launcher.activate();assert(lastAction==p4LauncherItems[index].action);
      assert(launcher.turn(1));assert(launcher.current()==(index+1)%count);frame();
    }
    assert(launcher.current()==0);assert(launcher.turn(-1)&&launcher.current()==count-1);
    assert(launcher.turn(1)&&launcher.current()==0);
    frame(250);launcher.awake(false);assert(lv_anim_count_running()==0);
    launcher.awake(true);assert(lv_anim_count_running()==1);
    // Reversals replace transitions; deleting the screen cancels its animations.
    for(int i=0;i<100;++i){launcher.turn(1);launcher.turn(-1);}
    assert(launcher.current()==0);assert(launcher.turn(100)&&launcher.current()==100%count);assert(launcher.turn(-100)&&launcher.current()==0);assert(lv_anim_count_running()<=3);launcher.forget();lv_obj_clean(root);frame(2500);
    assert(!launcher.turn(1));unsigned before=opened;launcher.activate();assert(opened==before);
  }
  assert(opened==3*count);puts("PASS P4 launcher: every route, wrap/reversal, multi-detent input and bounded animations, activation, three themes and screen deletion");
}
'''
(work/'launcher.cpp').write_text(source)
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O1','-UNDEBUG','-DLV_CONF_INCLUDE_SIMPLE',
    '-DBOARD_WAVESHARE_P4_KNOB','-I'+str(work),'-I'+str(root/'hardware/spotpear/lib/lvgl'),'-I'+str(root/'firmware/include'),
    str(work/'launcher.cpp'),str(root/'firmware/src/node_logo.cpp'),str(native/'lvgl.lib'),'-o',str(work/'launcher.exe')],check=True)
subprocess.run([str(work/'launcher.exe')],cwd=work,check=True)
sheet=Image.new('RGB',(466*4,496*3),'#101018')
draw=ImageDraw.Draw(sheet)
for row,theme in enumerate([12,20,6]):
    for col,index in enumerate(featured):
        raw=(work/f'theme-{theme}-item-{index}.raw').read_bytes()
        assert len(raw)==466*466*2
        rgb=bytearray()
        for i in range(0,len(raw),2):
            v=(raw[i]<<8)|raw[i+1]
            rgb.extend((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
        image=Image.frombytes('RGB',(466,466),bytes(rgb))
        mask=Image.new('L',(466,466));ImageDraw.Draw(mask).ellipse((0,0,465,465),fill=255)
        sheet.paste(image,(col*466,row*496),mask)
        draw.text((col*466+12,row*496+474),f'Theme {theme} / {titles[index]}',fill='white')
sheet.save(work/'launcher-preview.png')
print(work/'launcher-preview.png')

# Contact sheet verifies every custom icon and its caption at native layout.
all_icons=Image.new('RGB',(233*4,233*((len(titles)+3)//4)),'#101018')
for index in range(len(titles)):
    raw=(work/f'theme-12-item-{index}.raw').read_bytes()
    rgb=bytearray()
    for i in range(0,len(raw),2):
        v=(raw[i]<<8)|raw[i+1]
        rgb.extend((((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
    all_icons.paste(Image.frombytes('RGB',(466,466),bytes(rgb)).resize((233,233)),((index%4)*233,(index//4)*233))
all_icons.save(work/'all-icons.png')
