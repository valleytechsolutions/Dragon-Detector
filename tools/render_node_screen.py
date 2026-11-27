"""Render the BISCUIT NODE / JCMK Node screen with native LVGL at every panel size.

Builds LVGL 8.3 from hardware/spotpear/lib/lvgl with the host C compiler once,
then compiles firmware/src/node_ui.cpp and node_logo.cpp per panel class against
small stubs and writes PNGs to .work/node-render/. Each board is also checked for
text that would spill outside the round glass.

Run: python tools/render_node_screen.py
"""
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
work = root / ".work/node-render"
native = work / "native"
native.mkdir(parents=True, exist_ok=True)
lv = root / "hardware/spotpear/lib/lvgl"

config = """#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_USE_LOG 0
#define LV_TICK_CUSTOM 0
""" + "".join(f"#define LV_FONT_MONTSERRAT_{n} 1\n" for n in [8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 32, 40, 48]) + "#endif\n"
conf = native / "lv_conf.h"
if not conf.exists() or conf.read_text() != config:
    conf.write_text(config)
    for o in native.glob("*.o"):
        o.unlink()
includes = ["-I" + str(native), "-I" + str(lv), "-DLV_CONF_INCLUDE_SIMPLE"]
library = native / "liblvgl.a"
if not library.exists():
    objects = []
    sources = [p for p in (lv / "src").rglob("*.c")]
    procs = []
    for i, source in enumerate(sources):
        obj = native / f"lv{i}.o"
        objects.append(str(obj))
        procs.append(subprocess.Popen(["cc", "-O1", "-w", "-c", str(source), "-o", str(obj), *includes]))
        if len(procs) >= 8:
            for p in procs:
                assert p.wait() == 0
            procs = []
    for p in procs:
        assert p.wait() == 0
    subprocess.run(["ar", "rcs", str(library), *objects], check=True)

(native / "Arduino.h").write_text(r"""
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
class String: public std::string { public: using std::string::string; String(const std::string& s):std::string(s){} bool startsWith(const char* s)const{return rfind(s,0)==0;} };
extern uint32_t hostMillis;
inline uint32_t millis(){return hostMillis;}
struct SerialStub{template<class... T>void printf(T...){} template<class T>void println(T){}};
static SerialStub Serial;
""")

harness = r"""
#include "node_ui.h"
#include "node_mode.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t hostMillis=100000;
static dragon::node::Status status;
static NodeKind kind=NodeKind::Biscuit;
static bool busy=true,ble=true,keyed=false;
bool nodeBusy(){return busy;}
NodeKind nodeKind(){return kind;}
void nodeStop(){busy=false;}
const dragon::node::Status& nodeStatus(){return status;}
bool nodeBleReports(){return ble;}
bool nodeKeyed(){return keyed;}
static std::vector<uint16_t> frame;static int size=0,height=0;
static void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* p){
  for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)frame[y*size+x]=(p++)->full;
  lv_disp_flush_ready(d);
}
static void save(const char* path){FILE* f=fopen(path,"wb");fwrite(frame.data(),2,frame.size(),f);fclose(f);}
// Every label, image and button must sit inside the round glass with a small margin.
static int outside(lv_obj_t* screen,const char* out,const char* name,const char* where){
  int problems=0;std::vector<lv_obj_t*> stack{screen};
  while(!stack.empty()){
    lv_obj_t* o=stack.back();stack.pop_back();
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)stack.push_back(lv_obj_get_child(o,i));
    if(!lv_obj_check_type(o,&lv_label_class)&&!lv_obj_check_type(o,&lv_img_class)&&!lv_obj_check_type(o,&lv_btn_class))continue;
    lv_area_t a;lv_obj_get_coords(o,&a);
    if(lv_obj_check_type(o,&lv_label_class)){
      // Shrink to the drawn text, not the label box.
      lv_point_t sz;const char* t=lv_label_get_text(o);
      lv_txt_get_size(&sz,t,lv_obj_get_style_text_font(o,0),lv_obj_get_style_text_letter_space(o,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
      const int cx=(a.x1+a.x2)/2;a.x1=cx-sz.x/2;a.x2=cx+sz.x/2;a.y2=a.y1+sz.y;
      if(sz.x>lv_area_get_width(&a)+2||sz.x>lv_obj_get_width(o)){printf("CLIPPED %s %s%s '%s' %d>%d\n",out,name,where,t,int(sz.x),int(lv_obj_get_width(o)));++problems;}
    }
#ifdef BOARD_WAVESHARE_206
    if(a.x1<0||a.y1<0||a.x2>=size||a.y2>=height){printf("OUTSIDE rectangle %s %s\n",out,name);++problems;}
    continue;
#endif
    const float r=size/2.f,margin=size*0.02f;
    for(int corner=0;corner<4;++corner){
      const float x=(corner&1?a.x2:a.x1)-r+0.5f,y=(corner&2?a.y2:a.y1)-r+0.5f;
      if(x*x+y*y>(r-margin)*(r-margin)&&lv_obj_get_width(o)<size){printf("OUTSIDE %s %s%s corner %d (%d,%d)-(%d,%d)\n",out,name,where,corner,a.x1,a.y1,a.x2,a.y2);++problems;break;}
    }
  }
  return problems;
}
// The label whose text contains part, if any.
static lv_obj_t* labelWith(lv_obj_t* o,const char* part){
  for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i){
    lv_obj_t* c=lv_obj_get_child(o,i);
    if(lv_obj_check_type(c,&lv_label_class)&&strstr(lv_label_get_text(c),part))return c;
    if(lv_obj_t* found=labelWith(c,part))return found;
  }
  return nullptr;
}
// How many labels contain part.
static int labelsWith(lv_obj_t* o,const char* part){
  int n=0;
  for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i){
    lv_obj_t* c=lv_obj_get_child(o,i);
    if(lv_obj_check_type(c,&lv_label_class)&&strstr(lv_label_get_text(c),part))++n;
    n+=labelsWith(c,part);
  }
  return n;
}
int main(int argc,char** argv){
  size=atoi(argv[1]);const char* out=argv[2];const bool button=atoi(argv[3]);const char* confirm=argc>4&&*argv[4]?argv[4]:nullptr;
  const char* keyHint=argc>5&&*argv[5]?argv[5]:nullptr;const char* keyHintShort=argc>6&&*argv[6]?argv[6]:nullptr;
  height=size;
#ifdef BOARD_WAVESHARE_206
  height=502;
#endif
  lv_init();frame.assign(size*height,0);
  static lv_disp_draw_buf_t buf;static std::vector<lv_color_t> pixels(size*height);
  lv_disp_draw_buf_init(&buf,pixels.data(),nullptr,size*height);
  static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=size;drv.ver_res=height;drv.flush_cb=flush;drv.draw_buf=&buf;drv.full_refresh=1;
  lv_disp_drv_register(&drv);
  lv_obj_t* screen=lv_scr_act();lv_obj_set_style_bg_color(screen,lv_color_hex(0x203040),0);
  // The options each board passes (ui.cpp, lite_node.inc).
  NodeScreenOptions options;options.exitButton=button;options.exitHint=button?nullptr:"HOLD BUTTON TO EXIT";options.confirmHint=confirm;options.keyHint=keyHint;options.keyHintShort=keyHintShort;
  // After a short key press the status line says how to leave, in the shortest wording at least.
  const char* hintText=keyHintShort?keyHintShort:button?"TAP EXIT":"HOLD";
  using dragon::node::Core;
  // scanned: the channel of the last scan before a visit home (Report on channel 6).
  struct Case{const char* name;NodeKind kind;bool linked,registered;Core core,other;uint32_t wifi,ble,sent;uint8_t channel,scanned;bool arm,keyed;const char* expect;const char* also;const char* forbid;bool waiting=false,hint=false;};
  const Case cases[]={
    {"biscuit-linked",NodeKind::Biscuit,true,true,Core::Biscuit,Core::None,12384,0,13110,6,4,false,false,"LINKED","CH 4",nullptr},
    {"biscuit-waiting",NodeKind::Biscuit,true,false,Core::Biscuit,Core::None,0,0,0,6,0,false,false,"WAITING",nullptr,"LINKED",true},
    {"biscuit-waiting-after",NodeKind::Biscuit,true,true,Core::Biscuit,Core::None,1840,0,1902,6,0,false,false,"WAITING","SENT 1,902","LINKED",true},
    {"biscuit-search",NodeKind::Biscuit,false,false,Core::None,Core::None,0,0,0,6,0,false,false,"SEARCHING",nullptr,nullptr},
    {"biscuit-secure",NodeKind::Biscuit,false,false,Core::None,Core::BiscuitSecure,214,0,214,6,0,false,false,"EXTENDED MODE",nullptr,nullptr},
    {"jcmk-linked",NodeKind::Jcmk,true,true,Core::Jcmk,Core::None,8742,1903,10840,11,0,false,false,"LINKED","BLE 1,903","PIGLET"},
    {"jcmk-piglet",NodeKind::Jcmk,true,true,Core::Piglet,Core::None,8742,1903,10840,11,0,false,false,"PIGLET CORE","SENT 10,840","BLE"},
    {"jcmk-wrong",NodeKind::Jcmk,false,false,Core::None,Core::Biscuit,0,0,0,6,0,false,false,"USE BISCUIT NODE",nullptr,nullptr},
    {"jcmk-secure-biscuit",NodeKind::Jcmk,false,false,Core::None,Core::BiscuitSecure,0,0,0,6,0,false,false,"USE BISCUIT NODE",nullptr,"EXTENDED"},
    {"jcmk-keyed-piglet",NodeKind::Jcmk,false,false,Core::None,Core::Piglet,0,0,0,6,0,false,true,"CLEAR NODE KEY",nullptr,"USE JCMK"},
    {"jcmk-confirm",NodeKind::Jcmk,true,true,Core::Jcmk,Core::None,999999,99999,1099998,3,0,true,false,confirm?confirm:button?"TAP CONFIRM TO LEAVE":"PRESS AGAIN TO LEAVE",nullptr,nullptr},
    {"biscuit-key-hint",NodeKind::Biscuit,true,true,Core::Biscuit,Core::None,12384,0,13110,6,4,false,false,hintText,nullptr,"LINKED",false,true},
  };
  int problems=0;
  for(const Case& c:cases){
    kind=c.kind;busy=true;keyed=c.keyed;status={};status.linked=c.linked;status.registered=c.registered;status.waiting=c.waiting;status.core=c.core;
    status.other=c.other;status.wifi=c.wifi;status.ble=c.ble;status.sent=c.sent;status.channel=c.channel;
    status.planCount=11;for(uint8_t i=0;i<11;++i)status.plan[i]=i+1;status.phase=c.linked?dragon::node::Phase::Scan:dragon::node::Phase::Search;
    lv_obj_clean(screen);nodeScreenOpen(screen,options);
    status.lastScanned=c.linked?(c.scanned?c.scanned:c.channel):0;
    if(c.scanned)status.phase=dragon::node::Phase::Report;
    if(c.arm)nodeScreenExitPress();
    if(c.hint)nodeScreenKeyHint();
    hostMillis+=1000;nodeScreenLoop();
    lv_obj_invalidate(screen);lv_refr_now(nullptr);
    char path[512];snprintf(path,sizeof(path),"%s-%s.raw",out,c.name);save(path);
    // The status says what the case is about, in the wording that fits this panel.
    if(!labelWith(screen,c.expect)){printf("WRONG %s %s: no '%s'\n",out,c.name,c.expect);++problems;}
    // Lite's fixed line under the logo already reads HOLD BUTTON TO EXIT, so the hint must be a second label: the status line.
    if(c.hint&&labelsWith(screen,c.expect)<(options.exitHint&&strstr(options.exitHint,c.expect)?2:1)){printf("WRONG %s %s: the status line does not show '%s'\n",out,c.name,c.expect);++problems;}
    if(c.also&&!labelWith(screen,c.also)){printf("WRONG %s %s: no '%s'\n",out,c.name,c.also);++problems;}
    if(c.forbid&&labelWith(screen,c.forbid)){printf("WRONG %s %s: shows '%s'\n",out,c.name,c.forbid);++problems;}
    problems+=outside(screen,out,c.name,"");
    // The same at every burn-in drift step: the content layer (EXIT included) moves.
    lv_obj_t* node=lv_obj_get_child(screen,0);lv_obj_t* content=nullptr;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(node);++i)if(!content||lv_obj_get_child_cnt(lv_obj_get_child(node,i))>lv_obj_get_child_cnt(content))content=lv_obj_get_child(node,i);
    static const int8_t drift[][2]={{2,-2},{-2,2},{2,2},{-2,-2},{0,3},{0,-3},{3,0},{-3,0}};
    for(auto& d:drift){
      const float k=size/466.f;lv_obj_set_pos(content,lv_coord_t(lroundf(d[0]*k)),lv_coord_t(lroundf(d[1]*k)));lv_obj_update_layout(screen);
      char where[32];snprintf(where,sizeof(where)," drift(%d,%d)",d[0],d[1]);problems+=outside(screen,out,c.name,where);
    }
  }
  return problems?1:0;
}
"""
(work / "harness.cpp").write_text(harness)

# Name, panel, defines, on-screen EXIT, and the confirm and key hints the board
# passes (ui.cpp for CrowPanel and Watcher, lite_node.inc for the Waveshare Lite).
boards = [
    ("410x502",410,"-DBOARD_WAVESHARE_206",True,"","",""),
    ("466", 466, "-DBOARD_AMOLED_175C", True, "", "", ""),
    ("480", 480, "-DBOARD_CROWPANEL_21", True, "", "HOLD KNOB TO EXIT", "HOLD KNOB"),
    ("412", 412, "-DBOARD_WATCHER", True, "", "HOLD KNOB TO EXIT", "HOLD KNOB"),
    ("360", 360, "-DBOARD_WAVESHARE_185B", True, "", "", ""),
    ("240", 240, "-DBOARD_XIAO_ROUND", True, "", "", ""),
    ("240-dial", 240, "-DBOARD_M5DIAL -DBOARD_RADAR_LITE", True, "", "", ""),
    ("240-lite", 240, "-DBOARD_RADAR_LITE", False, "HOLD AGAIN TO LEAVE", "", ""),
]
problems = 0
for name, size, define, button, confirm, key_hint, key_hint_short in boards:
    exe = work / f"node-{name}"
    res = {410: 410, 466: 466, 480: 466, 412: 412, 360: 360, 240: 240}[size]
    cmd = ["c++", "-std=c++17", "-O1", "-w", *define.split(), f"-DSCREEN_RES={res}", *includes,
           "-I" + str(root / "firmware/include"), str(work / "harness.cpp"), str(root / "firmware/src/node_ui.cpp"),
           str(root / "firmware/src/node_logo.cpp"), str(library), "-o", str(exe)]
    (native / "board_pins.h").write_text("#pragma once\n")
    subprocess.run(cmd, check=True)
    for old in work.glob(f"{name}-*.png"):
        old.unlink()
    result = subprocess.run([str(exe), str(size), str(work / name), "1" if button else "0", confirm, key_hint, key_hint_short], capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end="")
        problems += result.stdout.count("\n")
    if result.returncode and not result.stdout:
        print(result.stderr, end="")
        problems += 1
    for raw in sorted(work.glob(f"{name}-*.raw")):
        data = raw.read_bytes()
        img = Image.new("RGB", (size, 502 if name=="410x502" else size))
        px = img.load()
        for i in range(size * (502 if name=="410x502" else size)):
            v = data[2 * i] | data[2 * i + 1] << 8
            px[i % size, i // size] = ((v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31)
        mask = Image.new("L", (size, 502 if name=="410x502" else size), 0)
        if name=="410x502":
            ImageDraw.Draw(mask).rectangle((0,0,size-1,501),fill=255)
        else:
            ImageDraw.Draw(mask).ellipse((0,0,size-1,size-1),fill=255)
        out = Image.new("RGB", (size, 502 if name=="410x502" else size), (40, 40, 40))
        out.paste(img, (0, 0), mask)
        out.save(raw.with_suffix(".png"))
        raw.unlink()

# The knob boards (CrowPanel 2.1, SenseCAP Watcher): the real rotary_ui.cpp and
# node_ui.cpp. A knob click must never reach EXIT, so clicks only show how to
# leave; a knob hold is an exit request and a second one leaves.
ui_source = (root / "firmware/src/ui.cpp").read_text()
hook = ui_source[ui_source.index("bool uiRotaryNode(bool click,bool back){"):]
hook = hook[:hook.index("\n}\n")]
for part in ('if(!nodeBusy())return false;', 'if(back)nodeKey("knob_hold",true);else if(click)nodeKey("knob_click");', 'return true;'):
    if part not in hook:
        print(f"ui.cpp uiRotaryNode changed; update the knob check: {part}")
        problems += 1
(native / "app.h").write_text(r"""
#pragma once
#include <Arduino.h>
#include <lvgl.h>
template<class T> T constrain(T v,T lo,T hi){return v<lo?lo:v>hi?hi:v;}
void uiKey(bool home);
""")
knob_harness = r"""
#include "node_ui.h"
#include "node_mode.h"
#include "rotary_ui.h"
#include "ui_theme.h"
#include <cstdio>
#include <cstring>
#include <vector>
uint32_t hostMillis=100000;
static dragon::node::Status status;
static bool busy=true;
bool nodeBusy(){return busy;}
NodeKind nodeKind(){return NodeKind::Biscuit;}
void nodeStop(){busy=false;}
const dragon::node::Status& nodeStatus(){return status;}
bool nodeBleReports(){return false;}
bool nodeKeyed(){return false;}
static unsigned keys=0;
lv_obj_t* uiRotaryScope(){return lv_scr_act();}
bool uiRotaryWake(){return false;}
#ifdef ROTARY_TOUCH_UI
bool uiRotaryOpenMenu(){++keys;return true;}
#endif
void uiKey(bool){++keys;}
const dragon::UiPalette& uiColors(){static dragon::UiPalette p{0,0x202020,0x303030,0xffffff,0x909090,0x00c0ff,0x404040,0,false};return p;}
// ui.cpp's uiRotaryNode, with nodeKey() on a node screen that is shown (its body is checked above).
bool uiRotaryNode(bool click,bool back){
  if(!nodeBusy())return false;
  if(back)nodeScreenExitPress(NodeExitInput::Hold);else if(click)nodeScreenKeyHint();
  return true;
}
static void flush(lv_disp_drv_t* d,const lv_area_t*,lv_color_t*){lv_disp_flush_ready(d);}
static void run(uint32_t ms){for(uint32_t t=0;t<ms;t+=10){hostMillis+=10;lv_tick_inc(10);rotaryUiLoop();lv_timer_handler();nodeScreenLoop();}}
static bool shows(lv_obj_t* o,const char* part){
  if(lv_obj_check_type(o,&lv_label_class)&&strstr(lv_label_get_text(o),part))return true;
  for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)if(shows(lv_obj_get_child(o,i),part))return true;
  return false;
}
#define CHECK(c) do{if(!(c)){printf("KNOB %s line %d: %s\n",BOARD,__LINE__,#c);++problems;}}while(0)
int main(){
  int problems=0;
  lv_init();static lv_disp_draw_buf_t buf;static std::vector<lv_color_t> px(480*40);
  lv_disp_draw_buf_init(&buf,px.data(),nullptr,480*40);static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=480;drv.ver_res=480;drv.flush_cb=flush;drv.draw_buf=&buf;lv_disp_drv_register(&drv);
  NodeScreenOptions o;o.exitButton=true;o.keyHint="HOLD KNOB TO EXIT";o.keyHintShort="HOLD KNOB";nodeScreenOpen(lv_scr_act(),o);
  rotaryUiBegin();run(200);
  for(int i=0;i<12;++i){rotaryUiInput(0,true,false);run(i%2?300:700);}   // single and paired knob clicks
  CHECK(busy);CHECK(shows(lv_scr_act(),"HOLD KNOB"));CHECK(!shows(lv_scr_act(),"CONFIRM"));
  rotaryUiInput(3,false,false);rotaryUiInput(-2,false,false);run(300);CHECK(busy);  // turning does nothing
  rotaryUiInput(0,false,true);run(200);CHECK(busy);CHECK(shows(lv_scr_act(),"TAP CONFIRM"));   // a hold asks
  rotaryUiInput(0,true,false);run(200);rotaryUiInput(0,true,false);run(800);CHECK(busy);       // clicks still do not confirm
  rotaryUiInput(0,false,true);run(200);CHECK(!busy);                                           // a second hold leaves
  CHECK(keys==0);
  return problems?1:0;
}
"""
(work / "knob.cpp").write_text(knob_harness)
for name, define in (("crowpanel", "-DBOARD_CROWPANEL_21"), ("watcher", "-DBOARD_WATCHER"), ("p4knob", "-DBOARD_WAVESHARE_P4_KNOB")):
    exe = work / f"knob-{name}"
    (native / "board_pins.h").write_text("#pragma once\n")
    subprocess.run(["c++", "-std=c++17", "-O1", "-w", define, f'-DBOARD="{name}"', "-DSCREEN_RES=466", *includes,
                    "-I" + str(root / "firmware/include"), str(work / "knob.cpp"), str(root / "firmware/src/rotary_ui.cpp"),
                    str(root / "firmware/src/node_ui.cpp"), str(root / "firmware/src/node_logo.cpp"), str(library), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(result.stdout, end="")
    if result.returncode:
        problems += max(1, result.stdout.count("\n"))
print(f"Rendered node screens to {work}; {problems} layout problem(s)")
sys.exit(1 if problems else 0)
