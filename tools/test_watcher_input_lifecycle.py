"""Exercise the actual Watcher touch task lifecycle with scripted bus failures."""
from pathlib import Path
import argparse, subprocess, sys
root=Path(__file__).resolve().parents[1];work=root/'.work/watcher-input-lifecycle';work.mkdir(parents=True,exist_ok=True)
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
board=(root/'firmware/src/board_watcher.cpp').read_text()
def function(signature):
 start=board.index(signature);opening=board.index('{',start);depth=1;i=opening+1
 while depth:
  depth+=(board[i]=='{')-(board[i]=='}');i+=1
 return board[start:i]
functions='\n'.join(function(s) for s in ['bool startTouchBus()','void cancelPhysicalTouch()','void touchTask(void*)'])
source=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <deque>
#include <vector>
#include "rotary_input.h"
using esp_err_t=int;using esp_lcd_touch_handle_t=void*;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_RESPONSE=0x108,INPUT_PULLUP=2,GPIO_NUM_NC=-1,pdTRUE=1;
constexpr unsigned pdMS_TO_TICKS(unsigned n){return n;}
struct esp_lcd_touch_config_t {unsigned x_max=0,y_max=0;int rst_gpio_num=0,int_gpio_num=0;};
struct InputEdge {bool down;uint16_t x,y;uint32_t at;};
struct Sample {int result;bool report;unsigned count,x,y,weight;};
std::deque<Sample> samples;Sample latest{};std::deque<InputEdge> queue;
std::deque<int> initResults;
std::vector<unsigned> initTimes;
uint32_t nowMs=0,stopAt=0;unsigned creates=0,deletes=0,reads=0,begins=0,ends=0,pullups=0,resets=0;
bool beginOK=true,overflow=false;
struct Stop{};
uint32_t millis(){return nowMs;}
uint32_t micros(){return nowMs*1000;}
void vTaskDelay(unsigned n){nowMs+=n;if(n==10&&nowMs>=stopAt)throw Stop{};}
void pinMode(int pin,int mode){assert((pin==38||pin==39)&&mode==INPUT_PULLUP);++pullups;}
struct WireStub {bool begin(int sda,int scl,unsigned hz){assert(sda==39&&scl==38&&hz==100000);++begins;return beginOK;}void end(){++ends;}void setTimeOut(unsigned n){assert(n==20);}} Wire1;
void esp_lcd_touch_del(void* p){assert(p);++deletes;}
int esp_lcd_touch_new_i2c_spd2010(void* transport,const esp_lcd_touch_config_t* cfg,void** out){
 assert(transport&&cfg->x_max==412&&cfg->y_max==412&&cfg->rst_gpio_num==GPIO_NUM_NC&&cfg->int_gpio_num==GPIO_NUM_NC);
 ++creates;initTimes.push_back(nowMs);int r=initResults.empty()?ESP_OK:initResults.front();if(!initResults.empty())initResults.pop_front();
 if(r==ESP_OK)*out=reinterpret_cast<void*>(1);return r;
}
int esp_lcd_touch_spd2010_read_sample(void* tp,bool* report){
 assert(tp);++reads;latest=samples.empty()?Sample{ESP_OK,false,0,0,0,0}:samples.front();if(!samples.empty())samples.pop_front();*report=latest.report;return latest.result;
}
bool esp_lcd_touch_get_coordinates(void* tp,uint16_t*x,uint16_t*y,uint16_t*w,uint8_t*c,int n){assert(tp&&n==1);*x=latest.x;*y=latest.y;*w=latest.weight;*c=latest.count;return *c>0;}
int encoderMux=0;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
int touchQueue=1;
void xQueueReset(int q){assert(q==1);++resets;queue.clear();}
int xQueueSend(int q,const InputEdge* e,int wait){assert(q==1&&wait==0);if(overflow){overflow=false;return 0;}queue.push_back(*e);return pdTRUE;}
void* touch=nullptr;bool touchReady=false,touchCancelPending=false,touchRecoveryRequested=false;
uint32_t touchRecoveries=0,touchFailures=0,touchMaxPollUs=0,touchPolls=0,touchErrors=0,rawContacts=0,touchSamples=0,touchPresses=0;
uint16_t rawX=0,rawY=0,rawStrength=0;
int touchLastError=ESP_OK,detents=0,touchTransport=0;
InputEdge physicalTouch{};
'''+functions+r'''
void reset(){samples.clear();initResults.clear();initTimes.clear();queue.clear();latest={};nowMs=0;stopAt=1;creates=deletes=reads=begins=ends=pullups=resets=0;beginOK=true;overflow=false;touch=nullptr;touchReady=touchCancelPending=touchRecoveryRequested=false;touchRecoveries=touchFailures=touchMaxPollUs=touchPolls=touchErrors=rawContacts=touchSamples=touchPresses=0;rawX=rawY=rawStrength=0;touchLastError=ESP_OK;detents=0;physicalTouch={};}
void run(unsigned until){stopAt=until;try{touchTask(nullptr);assert(false);}catch(const Stop&){} }
int main(){
 // Invalid firmware replies cause bounded retries without touching the main bus.
 reset();initResults={ESP_ERR_INVALID_RESPONSE,ESP_ERR_INVALID_RESPONSE,ESP_ERR_INVALID_RESPONSE};run(4050);
 assert(creates==3&&touchFailures==3&&!touchReady&&!reads);assert(initTimes==std::vector<unsigned>({0,2000,4000}));assert(touchCancelPending);
 // Wire begin failure never calls the driver or polls a missing controller.
 reset();beginOK=false;run(2010);assert(begins==2&&!creates&&!reads&&!touchReady);
 // A valid contact remains held over quiet polls, with one press edge.
 reset();samples={{ESP_OK,true,1,100,200,22},{ESP_OK,false,0,0,0,0},{ESP_OK,false,0,0,0,0}};run(80);
 assert(touchReady&&physicalTouch.down&&touchPresses==1&&queue.size()==1&&queue.front().down);
 // A zero-weight packet explicitly releases; no 60ms delay or duplicated press.
 reset();samples={{ESP_OK,true,1,100,200,22},{ESP_OK,false,0,0,0,0},{ESP_OK,true,1,100,200,0}};run(80);
 assert(!physicalTouch.down&&touchPresses==1&&queue.size()==2&&queue.front().down&&!queue.back().down);
 // One transient read error cancels any in-progress gesture without a release edge.
 reset();samples={{ESP_OK,true,1,100,200,22},{ESP_FAIL,false,0,0,0,0}};run(70);
 assert(!physicalTouch.down&&touchCancelPending&&queue.empty()&&touchErrors==1&&creates==1);
 // A finger still held after a fault is suppressed until its real lift, then rearmed.
 reset();samples={{ESP_OK,true,1,100,200,22},{ESP_FAIL,false,0,0,0,0},{ESP_OK,true,1,101,201,22},{ESP_OK,false,0,0,0,0},{ESP_OK,true,1,101,201,0},{ESP_OK,true,1,102,202,20}};run(110);
 assert(touchPresses==2&&queue.size()==1&&queue.front().down&&physicalTouch.down&&physicalTouch.x==102);
 // Overflow cancels the old gesture rather than injecting a release/click.
 reset();overflow=true;samples={{ESP_OK,true,1,100,200,22},{ESP_OK,true,1,101,201,22},{ESP_OK,true,1,101,201,0},{ESP_OK,true,1,102,202,20}};run(90);
 assert(touchPresses==2&&queue.size()==1&&queue.front().down&&touchCancelPending&&physicalTouch.x==102);
 // Three bad reads wait for the retry budget; a new valid report works after reinit.
 reset();samples={{ESP_ERR_INVALID_RESPONSE,false,0,0,0,0},{ESP_ERR_INVALID_RESPONSE,false,0,0,0,0},{ESP_ERR_INVALID_RESPONSE,false,0,0,0,0},{ESP_OK,true,1,111,222,30}};run(2110);
 assert(creates==2&&deletes==1&&touchErrors==3&&touchReady&&physicalTouch.down&&touchPresses==1&&physicalTouch.x==111&&physicalTouch.y==222);assert(initTimes==std::vector<unsigned>({0,2050}));
 // Initialization can fail, then recover without reboot or stale coordinates.
 reset();initResults={ESP_FAIL,ESP_OK};samples={{ESP_OK,true,1,105,205,18}};run(2070);
 assert(creates==2&&touchFailures==1&&touchReady&&touchPresses==1&&physicalTouch.down);
 puts("PASS actual Watcher touch lifecycle: retry/backoff, failed bus initialization, idle hold, explicit release, fault cancellation and recovery to valid contact");
}
'''
cpp=work/'lifecycle.cpp';cpp.write_text(source);exe=work/'lifecycle.exe'
subprocess.run(compiler+['-std=c++17','-O1','-UNDEBUG','-I'+str(root/'firmware/include'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
