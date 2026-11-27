"""Exercise real P4 passive GAP window handling with a deterministic event queue."""
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = {
    "checks.c": """
#include <assert.h>
#include <string.h>
#include "p4_ble_passive.h"
#include "esp_gap_ble_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
static gapfn callback;
static bool run=true;
static int setting,start,stop,mode,waits;
static int64_t now;
int64_t esp_timer_get_time(void){return now;}
int esp_bluedroid_init(void){return 0;}
int esp_bluedroid_enable(void){return 0;}
int esp_ble_gap_register_callback(gapfn fn){callback=fn;return 0;}
int esp_ble_gap_set_scan_params(esp_ble_scan_params_t *p){
 assert(p->scan_type==BLE_SCAN_TYPE_PASSIVE);++setting;
 esp_ble_gap_cb_param_t a={0};callback(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT,&a);return 0;
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t g,EventBits_t bits){return *g|=bits;}
EventBits_t xEventGroupClearBits(EventGroupHandle_t g,EventBits_t bits){return *g&=~bits;}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g,EventBits_t bits,bool c,bool a,unsigned ms){
 (void)c;(void)a;
 if (*g&bits) return *g&bits;
 now+=1000*ms;
 if(mode==2 && ms==50){
   if (++waits==1) run=false;
   if (waits==3) {
      esp_ble_gap_cb_param_t p={0};p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_CMPL_EVT;
      callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
   }
 }
 return *g&bits;
}
static bool wanted(void){return run;}
int esp_ble_gap_stop_scanning(void){
 ++stop;esp_ble_gap_cb_param_t p={0};
 if(mode==7)return 0;
 if(mode==6)p.scan_stop_cmpl.status=1;
 callback(ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT,&p);return 0;
}
int esp_ble_gap_start_scanning(unsigned seconds){
 ++start;assert(seconds==2);esp_ble_gap_cb_param_t p={0};callback(ESP_GAP_BLE_SCAN_START_COMPLETE_EVT,&p);
 if(mode==2 || mode==5)return 0;
 if(mode==8){p.scan_stop_cmpl.status=1;callback(ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT,&p);}
 if(mode==4){
  p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_RES_EVT;p.scan_rst.rssi=-70;
  uint8_t iBeacon[27]={26,0xff,0x4c,0,2,0x15};
  memcpy(p.scan_rst.ble_adv,iBeacon,27);p.scan_rst.adv_data_len=27;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
  p.scan_rst.ble_adv[1]=0xfe;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
  uint8_t eddy[24]={23,0x16,0xaa,0xfe,0,0};
  memcpy(p.scan_rst.ble_adv,eddy,24);p.scan_rst.adv_data_len=24;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
  p.scan_rst.ble_adv[4]=0x40;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
 }
 if(mode==0){
   p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_RES_EVT;p.scan_rst.rssi=-48;p.scan_rst.adv_data_len=4;
   memcpy(p.scan_rst.ble_adv,(uint8_t[]){3,9,'O','K'},4);
   p.scan_rst.ble_evt_type=ESP_BLE_EVT_NON_CONN_ADV;
   for(unsigned i=0;i<30;++i){p.scan_rst.bda[5]=i;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);}
   p.scan_rst.ble_evt_type=ESP_BLE_EVT_CONN_DIR_ADV;
   p.scan_rst.bda[5]=0;p.scan_rst.rssi=-60;p.scan_rst.ble_adv[0]=30;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
   p.scan_rst.ble_evt_type=ESP_BLE_EVT_SCAN_RSP;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
 }
 now+=mode==0?1800000:2100000;
 p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_CMPL_EVT;callback(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
 if(mode==3)run=false;
 return 0;
}
int main(void){
 p4_ble_snapshot_t out={0};assert(p4_ble_listen_init()==0&&setting==1);
 assert(p4_ble_listen_window(&out,wanted)==0&&out.count==24&&out.reports==31);
 assert(out.listen_ms==1800);
 assert(out.sightings[0].reports==2&&out.sightings[0].rssi==-60);
 assert(out.sightings[0].connectable&&!out.sightings[1].connectable);
 assert(out.sightings[0].name_length==2&&!memcmp(out.sightings[0].name,"OK",2));
 mode=1;assert(p4_ble_listen_window(&out,wanted)==0&&!out.count&&!out.reports&&out.listen_ms==2000);
 mode=2;assert(p4_ble_listen_window(&out,wanted)==ESP_ERR_INVALID_STATE&&stop==0&&!out.count&&!out.listen_ms);
 run=true;mode=3;assert(p4_ble_listen_window(&out,wanted)==ESP_ERR_INVALID_STATE&&stop==0);
 assert(p4_ble_listen_window(&out,wanted)==ESP_ERR_INVALID_STATE&&start==4);
 run=true;mode=4;assert(p4_ble_listen_window(&out,wanted)==ESP_OK&&out.count==1&&out.reports==4);
 assert(out.ibeacon_reports==1&&out.eddystone_reports==1);
 // Missing natural completion enters recovery. A second mode switch and the
 // clock both require the same STOP + fresh parameter acknowledgement barrier.
 mode=5;assert(p4_ble_listen_window(&out,wanted)==ESP_ERR_TIMEOUT&&!out.count);
 assert(!strcmp(p4_ble_window_stage(),"window_end"));
 mode=7;assert(p4_ble_listen_quiesce()==ESP_ERR_TIMEOUT && stop==1 && setting==1);
 assert(!strcmp(p4_ble_window_stage(),"recover_stop"));
 mode=6;assert(p4_ble_listen_window(&out,wanted)==ESP_OK && stop==2 && setting==2);
 assert(p4_ble_listen_quiesce()==ESP_OK && stop==2);
 mode=8;assert(p4_ble_listen_window(&out,wanted)==ESP_OK && stop==2);
 // Repeated transitions should discard the old window and never stop a new one.
 for(unsigned i=0;i<64;++i){
  mode=2;waits=0;assert(p4_ble_listen_window(&out,wanted)==ESP_ERR_INVALID_STATE);
  assert(!out.count && !out.reports && stop==2 && p4_ble_listen_quiesce()==ESP_OK);
  run=true;mode=0;assert(p4_ble_listen_window(&out,wanted)==ESP_OK && out.count==24);
 }
}
""",
    "esp_bt_main.h": """
#include "esp_err.h"
esp_err_t esp_bluedroid_init(void),esp_bluedroid_enable(void);
""",
    "esp_err.h": """
#pragma once
typedef int esp_err_t;
enum{ESP_OK,ESP_FAIL,ESP_ERR_TIMEOUT,ESP_ERR_INVALID_ARG,ESP_ERR_INVALID_STATE,ESP_ERR_NO_MEM};
""",
    "esp_gap_ble_api.h": """
#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef int esp_bt_status_t,esp_gap_ble_cb_event_t;
enum{ESP_BT_STATUS_SUCCESS,ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT,ESP_GAP_BLE_SCAN_START_COMPLETE_EVT,
ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT,ESP_GAP_BLE_SCAN_RESULT_EVT,ESP_GAP_SEARCH_INQ_CMPL_EVT,ESP_GAP_SEARCH_INQ_RES_EVT,
ESP_BLE_EVT_SCAN_RSP,ESP_BLE_EVT_CONN_ADV,ESP_BLE_EVT_CONN_DIR_ADV,ESP_BLE_EVT_NON_CONN_ADV,BLE_SCAN_TYPE_PASSIVE,BLE_ADDR_TYPE_PUBLIC,BLE_SCAN_FILTER_ALLOW_ALL,BLE_SCAN_DUPLICATE_DISABLE,
ESP_BLE_AD_TYPE_NAME_SHORT=8,ESP_BLE_AD_TYPE_NAME_CMPL=9};
typedef struct{int scan_type,own_addr_type,scan_filter_policy,scan_interval,scan_window,scan_duplicate;} esp_ble_scan_params_t;
typedef struct{struct{int status;} scan_param_cmpl,scan_start_cmpl,scan_stop_cmpl;
 struct{int search_evt,rssi,adv_data_len,ble_evt_type,ble_addr_type;uint8_t bda[6],ble_adv[31];}scan_rst;}esp_ble_gap_cb_param_t;
typedef void(*gapfn)(esp_gap_ble_cb_event_t,esp_ble_gap_cb_param_t*);
esp_err_t esp_ble_gap_register_callback(gapfn),esp_ble_gap_set_scan_params(esp_ble_scan_params_t*);
esp_err_t esp_ble_gap_start_scanning(unsigned),esp_ble_gap_stop_scanning(void);
""",
    "esp_timer.h": """
#include <stdint.h>
int64_t esp_timer_get_time(void);
""",
    "freertos/event_groups.h": """
#pragma once
typedef unsigned EventBits_t,StaticEventGroup_t;
typedef StaticEventGroup_t* EventGroupHandle_t;
EventBits_t xEventGroupSetBits(EventGroupHandle_t,EventBits_t);
EventBits_t xEventGroupClearBits(EventGroupHandle_t,EventBits_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t,EventBits_t,bool,bool,unsigned);
static inline EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t* g){return g;}
""",
    "freertos/FreeRTOS.h": """
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BIT0 1u
#define BIT1 2u
#define BIT2 4u
#define BIT3 8u
#define BIT4 16u
typedef unsigned portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(x) (void)(x)
#define taskEXIT_CRITICAL(x) (void)(x)
#define pdFALSE false
#define pdMS_TO_TICKS(x) (x)
""",
    "sdkconfig.h": """
#define CONFIG_BT_BLUEDROID_ENABLED 1
#define CONFIG_ESP_EXT_CONN_BT_ENABLE 1
""",
}

with TemporaryDirectory() as tmp:
    work = Path(tmp)
    for name, source in FIXTURES.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source)
    exe = work / ("gap.exe" if sys.platform == "win32" else "gap")
    subprocess.run([sys.executable, "-m", "ziglang", "cc", "-std=c17", "-Wall", "-Werror", "-UNDEBUG",
                    "-DBOARD_WAVESHARE_P4_KNOB", "-I", str(work), "-I", str(ROOT / "firmware/include"),
                    str(ROOT / "firmware/src/p4_ble_passive.c"), str(work / "checks.c"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("Passive GAP cancellation, recovery, independent acknowledgement and report checks passed.")
