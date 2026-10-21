"""Exercise Dial touch/encoder and production indexed-art decoder without hardware."""
from pathlib import Path
import argparse,csv,subprocess,sys
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
work=root/'.work/m5dial-tests';work.mkdir(parents=True,exist_ok=True)
(work/'esp_heap_caps.h').write_text('#pragma once\n#include <cstdlib>\n')
(work/'lvgl.h').write_text(r"""#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
using lv_coord_t=int16_t;
using lv_res_t=int;
constexpr int LV_RES_OK=0,LV_RES_INV=1,LV_IMG_CF_TRUE_COLOR_ALPHA=5,LV_IMG_CF_RAW_ALPHA=2,LV_IMG_CF_INDEXED_1BIT=7,LV_IMG_CF_INDEXED_2BIT=8,LV_IMG_CF_INDEXED_4BIT=9,LV_IMG_CF_INDEXED_8BIT=10;
struct lv_color_t{uint16_t full;};
inline lv_color_t lv_color_make(uint8_t r,uint8_t g,uint8_t b){uint16_t n=((r>>3)<<11)|((g>>2)<<5)|(b>>3);return {uint16_t((n>>8)|(n<<8))};}
struct lv_img_header_t{unsigned cf,always_zero,reserved,w,h;};
struct lv_img_dsc_t{lv_img_header_t header;uint32_t data_size;const uint8_t* data;};
struct lv_img_decoder_t{};
struct lv_img_decoder_dsc_t{const void* src=nullptr;void* user_data=nullptr;const uint8_t* img_data=nullptr;};
inline lv_img_decoder_t* lv_img_decoder_create(){static lv_img_decoder_t d;return &d;}
inline void lv_img_cache_set_size(int){}
template<class T> void lv_img_decoder_set_info_cb(lv_img_decoder_t*,T){}
template<class T> void lv_img_decoder_set_open_cb(lv_img_decoder_t*,T){}
template<class T> void lv_img_decoder_set_read_line_cb(lv_img_decoder_t*,T){}
template<class T> void lv_img_decoder_set_close_cb(lv_img_decoder_t*,T){}
""")
source=r"""
#define BOARD_M5DIAL
#define BOARD_RADAR_LITE
#include "board_pins.h"
#include "dial_touch.h"
#include "rotary_input.h"
#include "contacts.h"
#include <cassert>
#include <vector>
#include <cstdio>
#include "RADAR_SOURCE"
static_assert(SCREEN_RES==240&&BOARD_HAS_AUDIO==1&&BOARD_HAS_SD==0);
static_assert(DIAL_SDA==11&&DIAL_SCL==12&&DIAL_HOLD==46&&DIAL_BUTTON==42);
static_assert(DIAL_ENCODER_A==41&&DIAL_ENCODER_B==40&&DIAL_BUZZER==3&&MAX_CONTACTS==32);
int main(){
 dragon::DialTouch touch;uint8_t down[]={1,0,119,0,220};assert(dragon::decodeDialTouch(down,5,touch)&&touch.pressed&&touch.x==119&&touch.y==220);
 uint8_t move[]={1,0x80,220,0,239};assert(dragon::decodeDialTouch(move,5,touch)&&touch.pressed&&touch.x==220);
 uint8_t release[]={1,0x40,0xff,0xf,0xff};assert(dragon::decodeDialTouch(release,5,touch)&&!touch.pressed&&touch.x==220);
 uint8_t empty[]={0,0,0,0,0};assert(dragon::decodeDialTouch(empty,5,touch)&&!touch.pressed);
 for(auto invalid:std::vector<std::vector<uint8_t>>{{1,0,240,0,100},{1,1,0,0,0},{1,0,1,1,0},{2,0,0,0,0},{1,0xc0,0,0,0}})assert(!dragon::decodeDialTouch(invalid.data(),5,touch));
 assert(!dragon::decodeDialTouch(nullptr,5,touch)&&!dragon::decodeDialTouch(down,4,touch));
 dragon::RotaryDecoder encoder;encoder.reset(3);int steps=0;for(uint8_t state:{2,0,1,3})steps+=encoder.update(state);assert(steps==1||steps==-1);int reverse=0;for(uint8_t state:{1,0,2,3})reverse+=encoder.update(state);assert(reverse==-steps);
 encoder.reset(3);assert(!encoder.update(0)&&!encoder.update(3));
 assert(prepareThemeLogos()&&prepareThemeLogos());size_t maxBytes=0,images=0;
 for(const auto& item:packedThemeLogos){
   lv_img_header_t h{};assert(dialArtInfo(nullptr,item.image,&h)==LV_RES_OK&&h.cf==LV_IMG_CF_TRUE_COLOR_ALPHA);
   lv_img_decoder_dsc_t d{};d.src=item.image;assert(dialArtOpen(nullptr,&d)==LV_RES_OK&&d.user_data&&!d.img_data);
   unsigned bits=item.format==LV_IMG_CF_INDEXED_4BIT?4:item.format==LV_IMG_CF_INDEXED_8BIT?8:0;assert(bits);
   unsigned stride=(h.w*bits+7)/8,palette=(1u<<bits)*4;auto* raw=(const uint8_t*)d.user_data;
   std::vector<uint8_t> line(h.w*3+2,0xab);
   for(unsigned y=0;y<h.h;++y){
     assert(dialArtLine(nullptr,&d,0,y,h.w,line.data()+1)==LV_RES_OK&&line.front()==0xab&&line.back()==0xab);
     for(unsigned x=0;x<h.w;++x){unsigned i=bits==8?raw[palette+y*stride+x]:((raw[palette+y*stride+x/2]>>(x%2?0:4))&15);auto* b=raw+i*4;
       uint16_t expected=((b[2]>>3)<<11)|((b[1]>>2)<<5)|(b[0]>>3);assert(line[1+x*3]==(expected>>8)&&line[2+x*3]==(expected&255)&&line[3+x*3]==b[3]);}
     if(h.w>7){uint8_t part[21];assert(dialArtLine(nullptr,&d,3,y,7,part)==LV_RES_OK&&!memcmp(part,line.data()+1+9,21));}
   }
   assert(dialArtLine(nullptr,&d,-1,0,1,line.data())==LV_RES_INV&&dialArtLine(nullptr,&d,0,h.h,1,line.data())==LV_RES_INV&&dialArtLine(nullptr,&d,h.w,0,1,line.data())==LV_RES_INV);
   dialArtClose(nullptr,&d);assert(!d.user_data);++images;if(item.image->data_size>maxBytes)maxBytes=item.image->data_size;
 }
 lv_img_header_t h;int unrelated;assert(dialArtInfo(nullptr,&unrelated,&h)==LV_RES_INV);
 assert(images>=40&&maxBytes<=65536);
 printf("PASS Dial input, pins, %u artwork scanline decodes; peak art %u bytes; contact pool %u bytes\n",unsigned(images),unsigned(maxBytes),unsigned(MAX_CONTACTS*sizeof(Contact)));
}
""".replace('RADAR_SOURCE',str(root/'firmware/src/radar_logo.cpp').replace('\\','/'))
cpp=work/'dial.cpp';exe=work/('dial.exe' if sys.platform=='win32' else 'dial');cpp.write_text(source)
subprocess.run(compiler+['-std=c++17','-O2','-UNDEBUG','-I'+str(work),'-I'+str(root/'firmware/include'),'-I'+str(root/'lib/opendroneid/src'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
rows=[(int(r[3],0),int(r[4],0)) for r in csv.reader((root/'hardware/m5stack-dial/partitions.csv').read_text().splitlines()) if r and not r[0].startswith('#')]
assert all(a+n<=b for (a,n),(b,m) in zip(rows,rows[1:])) and max(a+n for a,n in rows)==8*1024*1024
print('PASS Dial 8 MB partition boundaries')
