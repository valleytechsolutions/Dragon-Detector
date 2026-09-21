#include "app.h"
#include <lvgl.h>
#include <Preferences.h>
extern const lv_img_dsc_t vts_logo;
String badgePath;
bool badgeSlideshow=false;
static uint8_t* pixels=nullptr;
static lv_img_dsc_t descriptor;
static lv_obj_t *picture=nullptr,*hint=nullptr;
static uint32_t changed=0;
static Preferences prefs;
void badgeInit(){prefs.begin("dd-badge",false);badgePath=prefs.getString("image","");badgeSlideshow=prefs.getBool("slide",false);}
void badgeUnload(){picture=hint=nullptr;lv_img_cache_invalidate_src(&descriptor);if(pixels){free(pixels);pixels=nullptr;}}
bool badgeLoad(const String& path){
  if(!sdReady || !path.startsWith("/badges/") || path.indexOf("..")>=0)return false;
  xSemaphoreTake(sdMutex,portMAX_DELAY);File f=LittleFS.open(path);uint8_t h[8]{};
  bool ok=f && f.size()==8+466*466*2 && f.read(h,8)==8 && !memcmp(h,"DDB1",4) && h[4]==0xd2 && h[5]==1 && h[6]==0xd2 && h[7]==1;
  uint8_t* next=ok?(uint8_t*)ps_malloc(466*466*2):nullptr;
  ok=next && f.read(next,466*466*2)==466*466*2;f.close();xSemaphoreGive(sdMutex);
  if(!ok){free(next);return false;}
  if(picture)lv_img_set_src(picture,&vts_logo);
  if(pixels)free(pixels);pixels=next;descriptor={};descriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
  descriptor.header.w=466;descriptor.header.h=466;descriptor.data_size=466*466*2;descriptor.data=pixels;
  badgePath=path;if(prefs.getString("image","")!=path)prefs.putString("image",path);
  if(picture){lv_img_cache_invalidate_src(&descriptor);lv_img_set_src(picture,&descriptor);lv_img_set_zoom(picture,256);lv_obj_center(picture);}
  changed=millis();return true;
}
void badgeNext(){
  String paths[16];int n=0;
  if(sdReady){xSemaphoreTake(sdMutex,portMAX_DELAY);File d=LittleFS.open("/badges");File f=d.openNextFile();
    while(f && n<16){if(!f.isDirectory() && String(f.name()).endsWith(".rgb565"))paths[n++]=String("/badges/")+f.name();f.close();f=d.openNextFile();}f.close();d.close();xSemaphoreGive(sdMutex);}
  for(int i=0;i<n;++i)for(int j=i+1;j<n;++j)if(paths[j]<paths[i])std::swap(paths[i],paths[j]);
  int at=-1;for(int i=0;i<n;++i)if(paths[i]==badgePath)at=i;
  for(int i=1;i<=n;++i)if(badgeLoad(paths[(at+i)%n]))return;
}
void badgeView(void* parent){
  picture=lv_img_create((lv_obj_t*)parent);lv_img_set_src(picture,&vts_logo);lv_obj_center(picture);
  lv_obj_add_flag(picture,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(picture,[](lv_event_t*){badgeNext();},LV_EVENT_SHORT_CLICKED,nullptr);
  lv_obj_add_event_cb(picture,[](lv_event_t*){badgeSlideshow=!badgeSlideshow;prefs.putBool("slide",badgeSlideshow);changed=millis();
    lv_label_set_text(hint,badgeSlideshow?"Slideshow ON  /  10 seconds":"Slideshow OFF  /  tap for next");lv_obj_clear_flag(hint,LV_OBJ_FLAG_HIDDEN);},LV_EVENT_LONG_PRESSED,nullptr);
  if(!badgeLoad(badgePath))badgeNext();
  hint=lv_label_create((lv_obj_t*)parent);lv_label_set_text(hint,"Tap: next   Hold: slideshow\nYellow: radar   Blue: tools");
  lv_obj_set_style_text_font(hint,&lv_font_montserrat_16,0);lv_obj_set_style_text_color(hint,lv_color_white(),0);
  lv_obj_set_style_text_align(hint,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_bg_color(hint,lv_color_black(),0);lv_obj_set_style_bg_opa(hint,LV_OPA_80,0);
  lv_obj_align(hint,LV_ALIGN_BOTTOM_MID,0,-55);changed=millis();
}
void badgeLoop(){if(hint && millis()-changed>3000)lv_obj_add_flag(hint,LV_OBJ_FLAG_HIDDEN);if(badgeSlideshow && millis()-changed>10000){changed=millis();badgeNext();}}
