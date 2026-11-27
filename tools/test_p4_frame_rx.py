"""Check real P4 receive-window cleanup and a device-captured RX descriptor."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
with TemporaryDirectory(prefix='dd-p4-frame-') as directory:
    work = Path(directory)
    (work/'freertos').mkdir()
    (work/'esp_wifi.h').write_text('''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_INVALID_ARG,ESP_ERR_INVALID_STATE,WIFI_SECOND_CHAN_NONE};
enum {WIFI_PROMIS_FILTER_MASK_MGMT=1,WIFI_PROMIS_FILTER_MASK_DATA=4};
typedef void (*wifi_promiscuous_cb_t)(void*,int);
typedef struct {unsigned filter_mask;} wifi_promiscuous_filter_t;
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t*);
esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t);
esp_err_t esp_wifi_set_promiscuous(bool);
esp_err_t esp_wifi_set_channel(uint8_t,int);
#ifdef __cplusplus
}
#endif
''')
    (work/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdMS_TO_TICKS(ms) (ms)\n')
    (work/'freertos/task.h').write_text('''
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void vTaskDelay(unsigned);
#ifdef __cplusplus
}
#endif
''')
    (work/'test.cpp').write_text(r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include "p4_frame_rx.h"
#include "p4_wifi_metadata.h"
unsigned elapsed=0,until=10000,tunes=0,clears=0,stops=0,failure=0;
bool enabled=false;wifi_promiscuous_cb_t callback=nullptr;
bool wanted(){return elapsed<until;}
void receive(void*,int){}
extern "C" {
void vTaskDelay(unsigned ms){assert(ms==10);elapsed+=ms;}
esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t* f){assert(f->filter_mask==5);return failure==1?2:0;}
esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb){if(!cb){++clears;callback=nullptr;return 0;}if(failure==2)return 2;callback=cb;return 0;}
esp_err_t esp_wifi_set_promiscuous(bool on){if(!on){++stops;enabled=false;return failure==5?2:0;}if(failure==3)return 2;enabled=true;return 0;}
esp_err_t esp_wifi_set_channel(uint8_t channel,int secondary){assert(secondary==WIFI_SECOND_CHAN_NONE);assert(channel==++tunes);return failure==4?2:0;}
}
void reset(){elapsed=tunes=clears=stops=failure=0;until=10000;enabled=false;callback=nullptr;}
int main(){
  assert(p4_read_frame_window(nullptr,wanted)==ESP_ERR_INVALID_ARG);
  assert(p4_read_frame_window(receive,nullptr)==ESP_ERR_INVALID_ARG);
  reset();until=0;assert(p4_read_frame_window(receive,wanted)==ESP_ERR_INVALID_STATE);assert(!tunes&&!stops);
  reset();assert(p4_read_frame_window(receive,wanted)==ESP_OK);assert(elapsed==2200&&tunes==11&&stops==1&&clears==1&&!enabled&&!callback);
  reset();until=55;assert(p4_read_frame_window(receive,wanted)==ESP_OK);assert(elapsed==60&&tunes==1&&stops==1&&clears==1&&!callback);
  for(unsigned error=1;error<=5;++error){reset();failure=error;assert(p4_read_frame_window(receive,wanted)==ESP_ERR_INVALID_STATE);assert(stops==1&&clears==1&&!enabled&&!callback);}
  // Real channel-1 beacon: the SDK's P4 bitfield reads byte 9 (zero).
  const uint8_t captured[]={0xcd,0x20,0xec,0xc1,0,0,0,0,0xa0,0,1,0xc3,0xd7,0x87,0xa0,0,0x82,0x0e,7,0x2f,0,0,0xf0,8,0xec,0xc1,0x1e,0};
  assert(dragon::p4::wifiPrimaryChannel(captured)==1);
  uint8_t descriptor[29]{};memcpy(descriptor+1,captured,28);descriptor[11]=0xab;
  assert(dragon::p4::wifiPrimaryChannel(descriptor+1)==11);
  puts("PASS P4 frame RX: full sweep, cancellation, failure cleanup and captured ESP32 channel metadata");
}
''')
    includes = ['-I'+str(work), '-I'+str(root/'firmware/include')]
    subprocess.run([sys.executable,'-m','ziglang','cc','-std=c11','-DBOARD_WAVESHARE_P4_KNOB',*includes,
                    '-c',str(root/'firmware/src/p4_frame_rx.c'),'-o',str(work/'rx.o')],check=True)
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-UNDEBUG',*includes,
                    str(work/'test.cpp'),str(work/'rx.o'),'-o',str(work/'test.exe')],check=True)
    subprocess.run([str(work/'test.exe')],check=True)
