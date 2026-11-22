"""Exercise the production 1.85B flush with a simulated asynchronous SPI transfer."""
import argparse
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work/185b-display-test'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
driver = (root / 'firmware/src/board_waveshare_185b.cpp').read_text(encoding='utf-8')
flush = driver.split('// DISPLAY_FLUSH_BEGIN:', 1)[1].split('\n', 1)[1].split('// DISPLAY_FLUSH_END')[0]
complete = driver[driver.index('bool IRAM_ATTR flushComplete'):driver.index('esp_err_t lcdCommand')]
staged_mode = next(line for line in driver.splitlines() if line.startswith('bool waveshare185bUsesStagedDisplay()'))
wearable = (root / 'firmware/src/wearable.cpp').read_text(encoding='utf-8')
rotated_flush = wearable[wearable.index('static void rotatedFlush('):wearable.index('static void applyRotation()')]
configure_orientation = wearable[wearable.index('bool configureOrientation('):wearable.index('static void reply(')]
allocate_start = wearable.index('  auto* driver=lv_disp_get_default()->driver;', wearable.index('void wearableBegin()'))
allocate_rotation = wearable[allocate_start:wearable.index('  if(rotationBuffer)', allocate_start)]
source = r'''
#define BOARD_WAVESHARE_185B
#include "wearable.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
using lv_color_t = uint16_t;
using esp_err_t = int;
using esp_lcd_panel_io_handle_t = void*;
struct esp_lcd_panel_io_event_data_t {};
using lv_coord_t = int16_t;
struct lv_disp_draw_buf_t {size_t size;} drawBuffer;
struct lv_disp_drv_t {uint8_t rotated=0; lv_disp_draw_buf_t* draw_buf=&drawBuffer;} displayDriver;
struct lv_disp_t {lv_disp_drv_t* driver=&displayDriver;} testDisplay;
lv_disp_t* lv_disp_get_default() {return &testDisplay;}
struct lv_area_t {int x1,y1,x2,y2;};
#define IRAM_ATTR
constexpr int SCREEN_RES=360, ESP_OK=0;
size_t dmaTilePixels=360*40;
std::vector<uint16_t> tile(dmaTilePixels), panel(360*360);
lv_color_t* dmaTile=tile.data();
bool stagedDisplay=true;
int panelIo=1, flushes=0, flushErrors=0, displayRegions=0, ready=0;
void lv_disp_flush_ready(lv_disp_drv_t*) { ++ready; }
''' + complete + r'''
int commands=0, failCommand=0, colors=0, failColor=0;
lv_area_t win{};
const uint16_t* inflight=nullptr;
std::vector<uint16_t> expected;
void drain() {
    if (!inflight) return;
    assert(std::equal(expected.begin(),expected.end(),inflight)); // still DMA-owned
    size_t i=0;
    for(int y=win.y1;y<=win.y2;++y)
        for(int x=win.x1;x<=win.x2;++x) panel.at(y*360+x)=inflight[i++];
    inflight=nullptr;
    flushComplete(nullptr,nullptr,nullptr);
}
esp_err_t lcdCommand(uint8_t command,const void* data,size_t size) {
    if (++commands==failCommand) return -1; // fail before a lifetime barrier
    drain();
    auto bytes=static_cast<const uint8_t*>(data);
    assert(size==4);
    int lo=(bytes[0]<<8)|bytes[1],hi=(bytes[2]<<8)|bytes[3];
    if(command==0x2a){win.x1=lo;win.x2=hi;} else {assert(command==0x2b);win.y1=lo;win.y2=hi;}
    return ESP_OK;
}
esp_err_t esp_lcd_panel_io_tx_color(int,int command,const void* pixels,size_t size) {
    assert(!inflight && command==0x32002c00 && size<=dmaTilePixels*2);
    assert(size==size_t(win.x2-win.x1+1)*(win.y2-win.y1+1)*2);
    if(++colors==failColor)return -1;
    inflight=static_cast<const uint16_t*>(pixels);
    if(stagedDisplay)assert(inflight==dmaTile);
    expected.assign(inflight,inflight+size/2);
    return ESP_OK;
}
''' + flush + r'''
static lv_color_t* rotationBuffer=nullptr;
static void (*physicalFlush)(lv_disp_drv_t*,const lv_area_t*,lv_color_t*)=flush;
uint8_t displayRotation=0;
void applyRotation() {displayDriver.rotated=displayRotation;}
struct {void putUChar(const char*,uint8_t){}} prefs;
constexpr uint32_t MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2, MALLOC_CAP_INTERNAL=4, MALLOC_CAP_DMA=8;
std::vector<uint16_t> rotationStorage;
void* heap_caps_malloc(size_t bytes,uint32_t caps) {
    const uint32_t required=stagedDisplay ? MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT : MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA;
    assert(caps==required);
    assert(bytes==(stagedDisplay ? 360*360 : dmaTilePixels)*sizeof(lv_color_t));
    rotationStorage.assign(bytes/2+2,0x7bad);
    return rotationStorage.data()+1;
}
''' + staged_mode + r'''
void allocateRotation() {
''' + allocate_rotation + r'''
}
''' + rotated_flush + configure_orientation + r'''
uint16_t value(int x,int y){return uint16_t((x*3271+y*73)^0xb520);}
void rotationTest(int degrees,lv_area_t area) {
    assert(configureOrientation(degrees));
    auto expectedPanel=panel;
    std::vector<uint16_t> source;
    for(int y=area.y1;y<=area.y2;++y)for(int x=area.x1;x<=area.x2;++x){
        const uint16_t pixel=value(x,y);
        source.push_back(pixel);
        // Physical panel destination for the requested clockwise orientation.
        int px=x,py=y;
        if(degrees==90){px=359-y;py=x;}
        if(degrees==180){px=359-x;py=359-y;}
        if(degrees==270){px=y;py=359-x;}
        expectedPanel[py*360+px]=pixel;
    }
    const int calls=ready;
    rotatedFlush(&displayDriver,&area,source.data());
    if(stagedDisplay){
        assert(ready==calls+1 && inflight==dmaTile);
        std::fill(source.begin(),source.end(),uint16_t(0x33cc));
        std::fill_n(rotationBuffer,drawBuffer.size,uint16_t(0x4488));
    }else{
        assert(ready==calls);
        assert(inflight==(degrees==0 ? source.data() : rotationBuffer));
    }
    drain(); assert(ready==calls+1 && panel==expectedPanel);
    assert(rotationStorage.front()==0x7bad && rotationStorage.back()==0x7bad);
}
void areaTest(lv_area_t area) {
    auto before=panel;
    std::vector<uint16_t> source;
    for(int y=area.y1;y<=area.y2;++y)
        for(int x=area.x1;x<=area.x2;++x)source.push_back(value(x,y));
    int calls=ready;
    flush(&displayDriver,&area,source.data());
    assert(inflight && ready==calls+1);
    std::fill(source.begin(),source.end(),uint16_t(0)); // LVGL immediately reuses PSRAM
    drain();
    assert(ready==calls+1);
    for(int y=0;y<360;++y)for(int x=0;x<360;++x){
        bool changed=x>=area.x1&&x<=area.x2&&y>=area.y1&&y<=area.y2;
        assert(panel[y*360+x]==(changed?value(x,y):before[y*360+x]));
    }
}
int main() {
    areaTest({7,13,342,319}); // packed LVGL rectangle, partial last tile
    areaTest({359,359,359,359});areaTest({257,0,257,359});areaTest({0,0,359,359});
    assert(flushErrors==0);
    lv_area_t full{0,0,359,359};
    std::vector<uint16_t> image(360*360,0xbeef);
    int calls=ready;
    failCommand=commands+3; // second row-window fails with prior DMA in flight
    flush(&displayDriver,&full,image.data());
    assert(ready==calls+1 && inflight && flushErrors==1);
    std::fill(image.begin(),image.end(),uint16_t(0xeca5));
    flush(&displayDriver,&full,image.data()); // must drain before staging again
    drain(); assert(ready==calls+2 && flushErrors==1);
    failColor=colors+2;
    flush(&displayDriver,&full,image.data()); assert(!inflight && ready==calls+3 && flushErrors==2);
    stagedDisplay=false;
    lv_area_t partial{11,77,19,82};
    calls=ready;
    flush(&displayDriver,&partial,tile.data());
    assert(inflight==tile.data() && ready==calls);
    drain(); assert(ready==calls+1);
    failCommand=commands+1;
    flush(&displayDriver,&partial,tile.data()); assert(!inflight && ready==calls+2 && flushErrors==3);
    flush(&displayDriver,&full,tile.data()); assert(!inflight && ready==calls+3 && flushErrors==4);
    for(bool staged:{true,false}){
        stagedDisplay=staged;
        drawBuffer.size=staged ? 360*360 : dmaTilePixels;
        allocateRotation();
        for(int degrees:{0,90,180,270}){
            rotationTest(degrees,{0,354,317,359}); // non-square and adjacent to two edges
            rotationTest(degrees,{350,4,359,4});
            rotationTest(degrees,{29,63,88,147});
            if(staged)rotationTest(degrees,full);
            else rotationTest(degrees,{0,113,359,152}); // full-size fallback tile
        }
    }
    puts("PASS 1.85B production flush and rotation: full/partial rectangles at 0/90/180/270, pixel order, memory capabilities, DMA lifetime, direct fallback, command/color fault recovery");
}
'''
cpp = work / 'flush.cpp'
exe = work / ('flush.exe' if sys.platform == 'win32' else 'flush')
cpp.write_text(source, encoding='utf-8')
subprocess.run(compiler + ['-std=c++17','-UNDEBUG','-I'+str(root/'firmware/include'),str(cpp),'-o',str(exe)], check=True)
subprocess.run([str(exe)], check=True)
