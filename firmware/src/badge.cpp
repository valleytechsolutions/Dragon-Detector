// MIT. DDB1 is an 8-byte header followed by big-endian RGB565 pixels.
#include "app.h"
#include "board_pins.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <lvgl.h>
extern const lv_img_dsc_t vts_logo;
static bool ready=false;
static uint8_t* pixels=nullptr;
static lv_img_dsc_t image;
static lv_obj_t* picture=nullptr;
static Preferences prefs;
static bool validHeader(const uint8_t* h){return !memcmp(h,"DDB1",4) && (h[4]|h[5]<<8)==SCREEN_RES && (h[6]|h[7]<<8)==SCREEN_RES;}
static bool load(){
  if(!ready)return false;File f=LittleFS.open("/badges/active.rgb565");uint8_t h[8];
  size_t size=SCREEN_RES*SCREEN_RES*2;
  if(!f || f.size()!=size+8 || f.read(h,8)!=8 || !validHeader(h)){f.close();return false;}
  uint8_t* next=(uint8_t*)ps_malloc(size);
  bool ok=next && f.read(next,size)==size;f.close();if(!ok){free(next);return false;}
  lv_img_cache_invalidate_src(&image);if(pixels)free(pixels);pixels=next;image={};
  image.header.cf=LV_IMG_CF_TRUE_COLOR;image.header.w=SCREEN_RES;image.header.h=SCREEN_RES;image.data_size=size;image.data=pixels;
  if(picture)lv_img_set_src(picture,&image);return true;
}
void badgeInit(){
  prefs.begin("dd-badge",false);
  // Never format on a mount error: factory images initialize the filesystem.
  ready=LittleFS.begin(false,"/littlefs",8,"spiffs");
  if(ready){LittleFS.mkdir("/badges");
    if(!LittleFS.exists("/badges/active.rgb565") && LittleFS.exists("/badges/previous.rgb565"))LittleFS.rename("/badges/previous.rgb565","/badges/active.rgb565");
    LittleFS.remove("/badges/upload.part");
  }
  Serial.printf("BADGE storage=%s resolution=%d\n",ready?"OK":"MISSING",SCREEN_RES);
  if(prefs.getBool("boot",false))uiShow("badge");
}
void badgeUnload(){picture=nullptr;lv_img_cache_invalidate_src(&image);if(pixels){free(pixels);pixels=nullptr;}}
void badgeView(void* parent){
  picture=lv_img_create((lv_obj_t*)parent);lv_img_set_src(picture,&vts_logo);load();lv_obj_center(picture);
  lv_obj_add_flag(picture,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(picture,[](lv_event_t*){uiShow("menu");},LV_EVENT_LONG_PRESSED,nullptr);
}
void badgeLoop(){
  // Small slow shift and idle dimming (in UI) reduce sustained AMOLED exposure.
  static uint32_t shifted=0;if(picture && millis()-shifted>60000){shifted=millis();int d=int((shifted/60000)%5)-2;lv_obj_align(picture,LV_ALIGN_CENTER,d,-d);}
}
bool badgeCommand(const String& c){
  if(c=="badge info"){Serial.printf("BADGE_INFO 0.3.1 %d %s %s\n",SCREEN_RES,ready?"READY":"NO_STORAGE",BOARD_NAME);return true;}
  if(c=="badge show"){uiShow("badge");return true;}
  if(c=="badge boot on" || c=="badge boot off"){prefs.putBool("boot",c.endsWith("on"));Serial.println("BADGE_OK");return true;}
  if(!c.startsWith("badge upload "))return false;
  unsigned long length=0,checksum=0;char extra;
  if(sscanf(c.c_str(),"badge upload %lu %lx %c",&length,&checksum,&extra)!=2 || length!=8+SCREEN_RES*SCREEN_RES*2 || !ready || LittleFS.totalBytes()-LittleFS.usedBytes()<length+16384){Serial.println("BADGE_ERROR size or storage");return true;}
  LittleFS.remove("/badges/upload.part");File f=LittleFS.open("/badges/upload.part",FILE_WRITE);
  if(!f){Serial.println("BADGE_ERROR open");return true;}
  bool enabled=radioEnabled;setRadio(false);Serial.println("BADGE_READY");
  uint8_t buffer[1024],header[8]{};size_t received=0;uint32_t crc=0xffffffffu,last=millis();bool ok=true;
  while(received<length && millis()-last<5000){
    size_t available=Serial.available();if(!available){delay(1);continue;}
    size_t n=Serial.readBytes(buffer,std::min(sizeof(buffer),std::min(available,size_t(length-received))));
    if(received<8)memcpy(header+received,buffer,std::min(n,8-received));
    crc=dragon::crc32(buffer,n,crc);if(f.write(buffer,n)!=n){ok=false;break;}
    received+=n;last=millis();Serial.printf("BADGE_ACK %u\n",unsigned(received));
  }
  f.flush();f.close();ok=ok && received==length && (crc^0xffffffffu)==checksum && validHeader(header);
  if(ok){
    LittleFS.remove("/badges/previous.rgb565");
    bool had=LittleFS.exists("/badges/active.rgb565");
    if(had)ok=LittleFS.rename("/badges/active.rgb565","/badges/previous.rgb565");
    if(ok)ok=LittleFS.rename("/badges/upload.part","/badges/active.rgb565");
    if(!ok && had && !LittleFS.exists("/badges/active.rgb565"))LittleFS.rename("/badges/previous.rgb565","/badges/active.rgb565");
  }
  if(!ok)LittleFS.remove("/badges/upload.part");
  setRadio(enabled);Serial.println(ok?"BADGE_OK":"BADGE_ERROR transfer or checksum");
  if(ok){prefs.putBool("boot",true);load();uiShow("badge");}return true;
}
