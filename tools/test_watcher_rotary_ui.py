"""Exercise production Watcher rotary/touch arbitration through native LVGL."""
from pathlib import Path
import argparse
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work/watcher-rotary-tests'
work.mkdir(parents=True, exist_ok=True)
p = argparse.ArgumentParser()
p.add_argument('--lvgl-library', type=Path, help='Optional native LVGL built with the emitted lv_conf.h')
args = p.parse_args()
zig = [sys.executable, '-m', 'ziglang']
lv = root / 'hardware/spotpear/lib/lvgl'
config = '''#ifndef LV_CONF_H
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
''' + ''.join(f'#define LV_FONT_MONTSERRAT_{n} 1\n' for n in [8,10,12,14,16,18,20,22,24,26,28,32,40,48]) + '#endif\n'
(work / 'lv_conf.h').write_text(config)
includes = ['-I'+str(work), '-I'+str(lv), '-I'+str(root/'firmware/include'), '-DLV_CONF_INCLUDE_SIMPLE']
library = args.lvgl_library or work/'lvgl.lib'
if args.lvgl_library:
    assert (library.parent/'lv_conf.h').read_text() == config, 'Native LVGL configuration mismatch'
if not library.exists():
    sources = [str(path) for path in (lv/'src').rglob('*.c')]
    flags = ['-lc','-O','ReleaseFast','--name','lvgl',*includes,'-femit-bin='+str(library),'-cflags','-std=c99','--',*sources]
    response = work/'lvgl.rsp'
    response.write_text(' '.join('"'+flag.replace('\\','/')+'"' for flag in flags))
    subprocess.run(zig+['build-lib','@'+str(response)],check=True,cwd=root)

# Only substitute the platform header; all navigation/group/input code below
# is the actual production translation unit and LVGL input processing.
production = (root/'firmware/src/rotary_ui.cpp').read_text().replace('#include "app.h"','#include "test_app.h"')
(work/'rotary_ui.cpp').write_text(production)
(work/'test_app.h').write_text(r'''
#pragma once
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <lvgl.h>
uint32_t now=0;
uint32_t millis(){return now;}
struct SerialStub {template<class... T>void printf(const char* format,T... values){std::printf(format,values...);}};
SerialStub Serial;
template<class T,class A,class B>T constrain(T n,A low,B high){return std::clamp(n,T(low),T(high));}
void uiKey(bool);
''')
source = r'''
#define BOARD_WATCHER
#include <cassert>
#include "rotary_ui.cpp"
unsigned menuRequests=0,keyRequests=0,clicksA=0,clicksB=0,clicksC=0;
bool wakeScreen=false,physicalDown=false;
lv_point_t physicalPoint{30,100};
lv_obj_t* scope=nullptr;
const dragon::UiPalette& uiColors(){static dragon::UiPalette palette{};return palette;}
lv_obj_t* uiRotaryScope(){return scope;}
bool uiRotaryWake(){bool result=wakeScreen;wakeScreen=false;return result;}
bool uiRotaryOpenMenu(){++menuRequests;return true;}
// Node screen (ui.cpp): while a node runs the knob never reaches LVGL; a hold is an exit request.
bool nodeRunning=false;unsigned nodeClicks=0,nodeHolds=0;
bool uiRotaryNode(bool click,bool back){if(!nodeRunning)return false;if(back)++nodeHolds;else if(click)++nodeClicks;return true;}
void uiKey(bool){++keyRequests;}
void pointerRead(lv_indev_drv_t*,lv_indev_data_t* data){
 rotaryUiTouch(physicalDown);data->point=physicalPoint;
 data->state=physicalDown?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
}
void frame(unsigned elapsed=35){now+=elapsed;lv_tick_inc(elapsed);rotaryUiLoop();lv_timer_handler();}
lv_obj_t* makeButton(lv_obj_t* parent,int x,unsigned* clicks){
 auto* b=lv_btn_create(parent);lv_obj_set_pos(b,x,80);lv_obj_set_size(b,80,50);
 lv_obj_add_event_cb(b,[](lv_event_t* e){++*static_cast<unsigned*>(lv_event_get_user_data(e));},LV_EVENT_CLICKED,clicks);
 return b;
}
int main(){
 lv_init();static lv_color_t pixels[412*32];static lv_disp_draw_buf_t buf;
 lv_disp_draw_buf_init(&buf,pixels,nullptr,412*32);lv_disp_drv_t display;
 lv_disp_drv_init(&display);display.hor_res=display.ver_res=412;display.draw_buf=&buf;
 display.flush_cb=[](lv_disp_drv_t* driver,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(driver);};
 lv_disp_drv_register(&display);scope=lv_scr_act();lv_obj_clear_flag(scope,LV_OBJ_FLAG_SCROLLABLE);
 lv_indev_drv_t pointer;lv_indev_drv_init(&pointer);pointer.type=LV_INDEV_TYPE_POINTER;pointer.read_cb=pointerRead;lv_indev_drv_register(&pointer);
 auto* a=makeButton(scope,10,&clicksA);auto* b=makeButton(scope,110,&clicksB);auto* c=makeButton(scope,210,&clicksC);
 rotaryUiBegin();frame();auto* g=static_cast<lv_group_t*>(lv_obj_get_group(a));assert(g&&lv_group_get_focused(g)==a);
 rotaryUiInput(1,false,false);frame();assert(lv_group_get_focused(g)==b&&lv_obj_has_state(b,LV_STATE_FOCUS_KEY));
 rotaryUiInput(1,false,false);frame();assert(lv_group_get_focused(g)==c);
 rotaryUiInput(-1,false,false);frame();assert(lv_group_get_focused(g)==b);
 assert(clicksA+clicksB+clicksC==0); // A turn only changes focus.
 rotaryUiInput(8,false,false);frame();assert(lv_group_get_focused(g)==c);
 for(unsigned i=0;i<30;++i)frame();assert(lv_group_get_focused(g)==c); // No backlog drifts after stopping.
 rotaryUiInput(1,false,false);rotaryUiInput(-1,false,false);frame();assert(lv_group_get_focused(g)==b);
 rotaryUiInput(1,false,false);rotaryUiInput(0,true,false);frame();
 assert(lv_group_get_focused(g)==b&&clicksB==1&&!lv_obj_has_state(b,LV_STATE_PRESSED));
 for(unsigned i=0;i<20;++i)frame();assert(clicksB==1); // Synthetic click always releases in the same read pass.
 rotaryUiInput(1,false,false);frame(250);assert(lv_group_get_focused(g)==c); // Real measured slow frames retain a detent.
 rotaryUiInput(-1,false,false);frame(351);assert(lv_group_get_focused(g)==c); // USB/long stalls do not replay stale turns.
 rotaryUiInput(-1,true,true);frame();assert(menuRequests==1&&keyRequests==0&&clicksB==1&&clicksC==0);
 wakeScreen=true;rotaryUiInput(1,true,false);frame();assert(lv_group_get_focused(g)==c&&clicksC==0); // Wake consumes selection.
 // Pointer runs before keypad, just as on hardware. A real touch cancels an
 // already queued wheel click, focuses its own target and clears wheel styling.
 physicalPoint={130,100};physicalDown=true;rotaryUiInput(0,true,false);frame();
 assert(lv_group_get_focused(g)==b&&clicksB==1&&!lv_obj_has_state(c,LV_STATE_FOCUS_KEY));
 rotaryUiInput(-4,true,true);frame();assert(menuRequests==1&&lv_group_get_focused(g)==b);
 physicalDown=false;frame();assert(clicksB==2); // One ordinary touchscreen click.
 rotaryUiInput(1,false,false);frame(149);assert(lv_group_get_focused(g)==b);
 rotaryUiInput(1,false,false);frame(1);assert(lv_group_get_focused(g)==b); // Still inside last release cooldown at capture.
 rotaryUiInput(1,false,false);frame();assert(lv_group_get_focused(g)==c);
 // Rebuild cancels a pending selection and cannot apply it to new controls.
 rotaryUiInput(0,true,false);rotaryUiReset();lv_obj_clean(scope);
 a=makeButton(scope,10,&clicksA);b=makeButton(scope,110,&clicksB);frame();g=static_cast<lv_group_t*>(lv_obj_get_group(a));
 assert(clicksA==0&&clicksB==2&&lv_group_get_focused(g)==a);
 rotaryUiInput(1,false,false);frame();assert(lv_group_get_focused(g)==b);
 // A top-layer modal scopes the wheel to that dialog, then restores the page.
 auto* modal=lv_msgbox_create(nullptr,"Confirm","Test",nullptr,true);frame();
 assert(lv_group_get_obj_count(g)==1&&lv_group_get_focused(g)==lv_msgbox_get_close_btn(modal));
 rotaryUiInput(6,false,false);frame();assert(lv_group_get_focused(g)==lv_msgbox_get_close_btn(modal));
 lv_msgbox_close(modal);frame();assert(lv_group_get_obj_count(g)==2);
 // BISCUIT NODE / JCMK Node: turns, clicks and holds go to the node screen, not to focus or buttons,
 // and a wheel move queued just before the node started is dropped.
 {auto* before=lv_group_get_focused(g);const unsigned clicked=clicksA+clicksB,menus=menuRequests;rotaryUiInput(1,false,false);nodeRunning=true;
  rotaryUiInput(1,false,false);rotaryUiInput(0,true,false);rotaryUiInput(0,false,true);frame();for(unsigned i=0;i<10;++i)frame();
  assert(lv_group_get_focused(g)==before&&clicksA+clicksB==clicked&&menuRequests==menus&&nodeClicks==1&&nodeHolds==1);
  nodeRunning=false;rotaryUiInput(1,false,false);frame();assert(lv_group_get_focused(g)!=before);}
 rotaryUiDiagnostics();
 puts("PASS production Watcher rotary UI with native LVGL: single detents, burst coalescing, reversal, no backlog, click/hold/wake, slow-frame expiry, real touch priority, cooldown, rebuild, modal scope and the node screen");
}
'''
cpp,exe=work/'watcher.cpp',work/'watcher.exe'
cpp.write_text(source)
subprocess.run(zig+['c++','-std=c++17','-O1','-UNDEBUG',*includes,str(cpp),str(library),'-o',str(exe)],check=True,cwd=root)
subprocess.run([str(exe)],check=True,cwd=root)
subprocess.run(zig+['c++','-std=c++17','-O1','-DBOARD_CROWPANEL_21',*includes,'-c',str(work/'rotary_ui.cpp'),'-o',str(work/'crowpanel.obj')],check=True,cwd=root)
print('PASS CrowPanel rotary conditional path still compiles')
