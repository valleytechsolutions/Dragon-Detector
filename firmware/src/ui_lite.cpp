#include "app.h"
#include "radar_colors.h"
#include "radar_layout.h"
#include <lvgl.h>
#include <math.h>
#define S(v) (((v)*SCREEN_RES+((v)>=0?180:-180))/360)
#define FCALLOUT (&lv_font_montserrat_8)
extern void scr_lvgl_init();
extern const lv_img_dsc_t radar_logo;
static constexpr int CENTER=SCREEN_RES/2;
struct RadarPalette {uint32_t top,bottom,grid,ring,dot,sweep,text,panel;};
static constexpr RadarPalette radarPalettes[]={
  {0x20b970,0x0c9553,0x095134,0xe0e5cd,0xffec50,0xb9ff72,0xfffae2,0x064a2e},
  {0x09291c,0x020c08,0x153d28,0x286c42,0xc0ff65,0x52dc84,0xdbf9c3,0x0b2419}
};
static constexpr unsigned radarTheme=1;
static constexpr bool radarLabels=true;
static unsigned radarPage=0;
static uint32_t radarPrepared=0;
static int radarOrder[MAX_CONTACTS],radarCount=0;
static uint32_t radarFrameAt=0;
struct Dot {int x,y,index;dragon::DeviceKey key;int labelSlot=-1;};
static Dot dots[dragon::RadarPageSize];static int dotCount=0;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
#include "radar_renderer.inc"
static lv_obj_t *root,*radar,*status,*activity,*pageLabel,*notice;
static uint32_t noticeAt=0,lastFrame=0,frames=0;
static lv_obj_t* text(const char* value,int y,int width,const lv_font_t* font,uint32_t rgb){
  auto* o=lv_label_create(root);lv_label_set_text(o,value);lv_obj_set_size(o,width,LV_SIZE_CONTENT);lv_obj_set_pos(o,(SCREEN_RES-width)/2,y);
  lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,color(rgb),0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_set_style_bg_color(o,color(dragon::RadarLabelBackground),0);lv_obj_set_style_bg_opa(o,LV_OPA_90,0);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);return o;
}
void uiBegin(){
  candidatesOnly=true;radarRecent=true;scr_lvgl_init();root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,color(0x020c08),0);
  radar=lv_obj_create(root);lv_obj_remove_style_all(radar);lv_obj_set_pos(radar,0,0);lv_obj_set_size(radar,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(radar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_clear_flag(radar,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(radar,radarDraw,LV_EVENT_DRAW_MAIN,nullptr);
  auto* logo=lv_img_create(root);lv_img_set_src(logo,&radar_logo);lv_obj_align(logo,LV_ALIGN_TOP_MID,0,9);
  status=text("STARTING RADIO",44,176,&lv_font_montserrat_8,0xdbf9c3);
  activity=text("",57,154,&lv_font_montserrat_8,0xffbc76);
  pageLabel=text("",178,156,&lv_font_montserrat_10,0xdbf9c3);
  notice=text("",191,154,&lv_font_montserrat_10,0xffe78a);
  lv_obj_set_height(notice,13);lv_obj_add_flag(activity,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
  text("LITE / ALWAYS SCANNING",211,142,&lv_font_montserrat_8,0xdbf9c3);
}
void uiLoop(){
  const uint32_t now=millis();
  // Leave time between full radar draws to drain reception queues on the S3R2.
  if(uint32_t(now-lastFrame)>=125){lastFrame=radarFrameAt=now;++frames;lv_obj_invalidate(radar);
    char value[80];snprintf(value,sizeof(value),"%s RX / %d targets / 100%%",blePhase?"BLE":"WI-FI",radarCount);lv_label_set_text(status,value);
    const bool deauth=trafficStatus.disconnectActive(now),spam=trafficStatus.bleActive(now);
    lv_label_set_text(activity,deauth&&spam?"DEAUTH + BLE BURST":deauth?"DEAUTH ACTIVITY":spam?"BLE SPAM CANDIDATE":"");
    if(deauth||spam)lv_obj_clear_flag(activity,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(activity,LV_OBJ_FLAG_HIDDEN);
    const unsigned pages=dragon::radarPages(radarCount);
    if(pages>1)snprintf(value,sizeof(value),"TARGETS %u / %u",radarPage+1,pages);
    else snprintf(value,sizeof(value),"%s",radarCount?"RSSI / STRONGER = INNER":"LISTENING FOR SIGNATURES");
    lv_obj_set_style_text_font(pageLabel,pages>1?&lv_font_montserrat_10:&lv_font_montserrat_8,0);lv_label_set_text(pageLabel,value);
    if(!noticeAt||uint32_t(now-noticeAt)>5000)lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
  }
  lv_timer_handler();
}
void uiNotify(const char* title,const char*,const dragon::DeviceKey*,bool){
  if(!notice)return;noticeAt=millis();lv_label_set_text(notice,title);lv_obj_clear_flag(notice,LV_OBJ_FLAG_HIDDEN);
}
void uiShow(const String&){} void uiKey(bool){}
bool uiStopwatchKey(bool,uint64_t){return false;}
bool uiCommand(const String& c){
  if(c=="ui status"){Serial.printf("UI_STATUS page=radar lite=1 frames=%lu\n",frames);return true;}
  if(c=="radar status"){Serial.printf("RADAR_SETTINGS theme=1 labels=1 recent=1 page=%u pages=%u plotted=%u auto_page_ms=7000\n",radarPage+1,dragon::radarPages(radarCount),dotCount);return true;}
  if(c=="display status"){Serial.println("DISPLAY_SETTINGS brightness=100 output=100 dim=0 off=0 dim_level=0 wake=0");return true;}
  return false;
}
