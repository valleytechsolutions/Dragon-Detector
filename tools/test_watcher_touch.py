"""Compile the actual Watcher SPD2010 driver against a scripted I2C transport."""
from pathlib import Path
import argparse, subprocess, sys
root=Path(__file__).resolve().parents[1]
work=root/'.work/watcher-touch-tests'; work.mkdir(parents=True,exist_ok=True)
p=argparse.ArgumentParser();p.add_argument('--cc');args=p.parse_args()
compiler=[args.cc] if args.cc else [sys.executable,'-m','ziglang','cc']
stub=r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_STATE 0x103
#define CONFIG_ESP_LCD_TOUCH_MAX_POINTS 10
#define CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS 0
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_RETURN_ON_FALSE(a,e,...) do{if(!(a))return (e);}while(0)
#define ESP_RETURN_ON_ERROR(a,...) do{esp_err_t e_=(a);if(e_!=ESP_OK)return e_;}while(0)
#define ESP_GOTO_ON_FALSE(a,e,label,...) do{if(!(a)){ret=(e);goto label;}}while(0)
#define ESP_GOTO_ON_ERROR(a,label,...) do{esp_err_t e_=(a);if(e_!=ESP_OK){ret=e_;goto label;}}while(0)
typedef struct {int owner;} portMUX_TYPE;
#define portMUX_FREE_VAL 0
#define portENTER_CRITICAL(...) ((void)0)
#define portEXIT_CRITICAL(...) ((void)0)
typedef unsigned TickType_t;
#define pdMS_TO_TICKS(n) (n)
static inline void vTaskDelay(unsigned n){(void)n;}
static inline void esp_rom_delay_us(unsigned n){(void)n;}
typedef int gpio_num_t;
typedef void (*gpio_isr_t)(void*);
#define GPIO_NUM_NC -1
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_INTR_POSEDGE 1
#define GPIO_INTR_NEGEDGE 2
#define BIT64(n) (1ull<<(n))
typedef struct {int mode,intr_type;uint64_t pin_bit_mask;} gpio_config_t;
static inline int gpio_config(const gpio_config_t* p){(void)p;return ESP_OK;}
static inline int gpio_reset_pin(int n){(void)n;return ESP_OK;}
static inline int gpio_set_level(int n,int v){(void)n;(void)v;return ESP_OK;}
static inline int gpio_isr_handler_remove(int n){(void)n;return ESP_OK;}
typedef struct fake_io *esp_lcd_panel_io_handle_t;
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t,int,const void*,size_t);
esp_err_t esp_lcd_panel_io_rx_param(esp_lcd_panel_io_handle_t,int,void*,size_t);
'''
(work/'stubs.h').write_text(stub)
for header in ['sdkconfig.h','esp_err.h','esp_log.h','esp_check.h','esp_system.h','esp_lcd_panel_io.h','driver/gpio.h','driver/i2c.h','freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h']:
 f=work/header;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#include "stubs.h"\n')
source=r'''
#include "stubs.h"
#include <assert.h>
#include <stdio.h>
#include "esp_lcd_touch_spd2010.h"
esp_err_t esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_handle_t tp,esp_lcd_touch_interrupt_callback_t cb){(void)tp;(void)cb;return ESP_OK;}
#include "esp_lcd_touch_spd2010.c"
struct fake_io {int unused;};
struct read_step {uint16_t reg;size_t size;uint8_t bytes[64];esp_err_t result;};
static struct read_step reads[32];static unsigned nreads,read_index,acks,starts,hdps,hdp_status_reads;
static uint16_t selected;static bool fail_ack;
static void script_reset(void){nreads=read_index=acks=starts=hdps=hdp_status_reads=0;selected=0;fail_ack=false;}
static void queue_read(uint16_t reg,const uint8_t* data,size_t count,esp_err_t result){
 assert(nreads<32&&count<=64);struct read_step *s=&reads[nreads++];s->reg=reg;s->size=count;memcpy(s->bytes,data,count);s->result=result;
}
static void status(uint8_t lo,uint8_t hi,uint16_t len){uint8_t b[]={lo,hi,len&255,len>>8};queue_read(0x20,b,4,ESP_OK);}
static void hdp_state(uint8_t st,uint16_t next){uint8_t b[]={0,0,next&255,next>>8,0,st,0,0};queue_read(0x2fc,b,8,ESP_OK);}
static void point(uint8_t weight){uint8_t b[]={0,0,0,0,0,100,200,0,weight,0};queue_read(0x300,b,10,ESP_OK);}
static void done(void){assert(read_index==nreads);}
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int cmd,const void* data,size_t size){
 assert(io&&cmd==-1&&(size==2||size==4));const uint8_t *p=data;selected=p[0]|p[1]<<8;
 if(size==4){if(selected==2){++acks;if(fail_ack)return ESP_FAIL;}else if(selected==4){++starts;}}
 return ESP_OK;
}
esp_err_t esp_lcd_panel_io_rx_param(esp_lcd_panel_io_handle_t io,int cmd,void* data,size_t size){
 assert(io&&cmd==-1&&read_index<nreads);struct read_step *s=&reads[read_index++];assert(s->reg==selected&&s->size==size);
 if(selected==0x300)++hdps;if(selected==0x2fc)++hdp_status_reads;memcpy(data,s->bytes,size);return s->result;
}
int main(void){
 struct fake_io io={0};esp_lcd_touch_config_t cfg={0};cfg.rst_gpio_num=cfg.int_gpio_num=GPIO_NUM_NC;cfg.x_max=cfg.y_max=412;
 esp_lcd_touch_handle_t tp=NULL;
 // Reject floating version replies and unwind the allocated touch handle.
 for(int fill=0;fill<2;++fill){script_reset();uint8_t v[18];memset(v,fill?255:0,18);queue_read(0x26,v,18,ESP_OK);assert(esp_lcd_touch_new_i2c_spd2010(&io,&cfg,&tp)==ESP_ERR_INVALID_RESPONSE);assert(tp==NULL);done();}
 script_reset();uint8_t version[18]={0};version[4]=1;memcpy(version+10,"2010SPD",7);queue_read(0x26,version,18,ESP_OK);assert(esp_lcd_touch_new_i2c_spd2010(&io,&cfg,&tp)==ESP_OK);done();
 bool report=false;uint16_t x=0,y=0,strength=0;uint8_t count=0;
 // The observed all-FF fault must never be treated as a BIOS handshake or packet.
 script_reset();status(255,255,65535);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_INVALID_RESPONSE&&!report);assert(!acks&&!starts);done();
 // Handshake and idle status are healthy but do not fabricate a release.
 script_reset();status(0,0x40,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&!report);assert(acks==1&&starts==1);done();
 script_reset();status(0,0x20,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&!report);assert(acks==1&&!starts);done();
 script_reset();status(0,0,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&!report);done();
 // Coordinates are exposed only after the complete valid report is acknowledged.
 script_reset();status(1,8,10);point(25);hdp_state(0x82,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&report);assert(tp->get_xy(tp,&x,&y,&strength,&count,1)&&count==1&&x==100&&y==200&&strength==25);assert(acks==1);done();
 script_reset();status(0,0,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&!report);assert(!tp->get_xy(tp,&x,&y,&strength,&count,1)&&count==0);done();
 // Weight-zero and empty point reports are explicit releases.
 script_reset();status(1,8,10);point(0);hdp_state(0x82,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&report);assert(tp->get_xy(tp,&x,&y,&strength,&count,1)&&strength==0);done();
 script_reset();status(1,8,4);uint8_t empty[4]={0};queue_read(0x300,empty,4,ESP_OK);hdp_state(0x82,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&report);assert(!tp->get_xy(tp,&x,&y,&strength,&count,1));done();
 script_reset();status(0,8,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&report);assert(acks==1);done();
 // Corrupt lengths, IDs, unknown HDP status and failed acknowledgements never publish.
 script_reset();status(1,8,65535);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_INVALID_RESPONSE&&!report);assert(acks==1&&!hdps);done();
 script_reset();status(1,8,10);uint8_t badid[10]={0,0,0,0,0x80};queue_read(0x300,badid,10,ESP_OK);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_INVALID_RESPONSE&&!report);assert(acks==1);done();
 script_reset();status(1,8,10);point(25);hdp_state(0x55,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_INVALID_RESPONSE&&!report);assert(acks==1);done();
 script_reset();status(1,8,10);point(25);hdp_state(0x82,0);fail_ack=true;assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_FAIL&&!report);assert(!tp->get_xy(tp,&x,&y,&strength,&count,1));done();
 // A failed status read does not send a command; a failed HDP read drops one report.
 script_reset();uint8_t ioerr[4]={0};queue_read(0x20,ioerr,4,ESP_FAIL);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_FAIL&&!report);assert(!acks&&!starts);done();
 script_reset();status(1,8,10);uint8_t broken[10]={0};queue_read(0x300,broken,10,ESP_FAIL);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_FAIL&&!report);assert(acks==1);done();
 script_reset();status(1,8,10);point(25);hdp_state(0,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_INVALID_RESPONSE&&!report);assert(acks==1&&hdps==1);done();
 script_reset();status(1,8,9);uint8_t odd[9]={0};queue_read(0x300,odd,9,ESP_OK);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_INVALID_RESPONSE&&!report);assert(acks==1);done();
 // A stuck report is bounded to four drain checks plus one recovery ACK.
 script_reset();status(1,8,10);point(25);for(int i=0;i<4;++i){hdp_state(0,10);point(0);}assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_ERR_TIMEOUT&&!report);assert(hdp_status_reads==4&&hdps==5&&acks==1);done();
 // Normal drain and a gesture report remain supported.
 script_reset();status(1,8,10);point(25);hdp_state(0,10);point(0);hdp_state(0x82,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&report);done();
 script_reset();status(2,8,7);uint8_t gesture[7]={0,0,0,0,0xf6,0,1};queue_read(0x300,gesture,7,ESP_OK);hdp_state(0x82,0);assert(esp_lcd_touch_spd2010_read_sample(tp,&report)==ESP_OK&&!report);done();
 assert(esp_lcd_touch_spd2010_read_sample(NULL,&report)==ESP_ERR_INVALID_ARG);tp->del(tp);
 puts("PASS actual SPD2010 driver: floating responses, startup, packet bounds, contact/release semantics, corrupt reports and bounded drain recovery");
}
'''
file=work/'driver_test.c';file.write_text(source)
exe=work/'driver_test.exe'
subprocess.run(compiler+['-std=c11','-O1','-UNDEBUG','-I'+str(work),'-I'+str(root/'hardware/seeed-watcher/lib/WatcherDisplay/src'),str(file),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
