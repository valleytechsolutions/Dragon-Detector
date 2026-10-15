#include "app.h"
#include "radar_colors.h"
#include "radar_layout.h"
#include <lvgl.h>
#include <math.h>
#include <esp_heap_caps.h>
#define S(v) (((v)*SCREEN_RES+((v)>=0?180:-180))/360)
#define FCALLOUT (&lv_font_montserrat_12)
extern void scr_lvgl_init();
extern const lv_img_dsc_t radar_logo;
static constexpr int CENTER=SCREEN_RES/2;
struct RadarPalette {uint32_t top,bottom,grid,ring,dot,sweep,text,panel;};
static constexpr RadarPalette radarPalettes[]={
  {0x20b970,0x0c9553,0x095134,0xe0e5cd,0xffec50,0xb9ff72,0xfffae2,0x064a2e},
  {0x061f1b,0x020c0c,0x0c3027,0x286c52,0xc0ff65,0x45efad,0xdbf9c3,0x0b2419}
};
static constexpr unsigned radarTheme=1;
static constexpr bool radarLabels=true;
static uint32_t radarPrepared=0;
static int radarOrder[MAX_CONTACTS],radarCount=0;
static uint32_t radarFrameAt=0;
struct Dot {int x,y,index;dragon::DeviceKey key;int labelSlot=-1;};
static Dot dots[MAX_CONTACTS];static int dotCount=0;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
static lv_img_dsc_t liteBackdrop;
// Paint immutable instrument geometry once. Keep the internal display tile free
// for moving content; this cache costs 115 KB of the otherwise idle PSRAM.
static void prepareLiteBackdrop(lv_obj_t* root){
  auto* pixels=(lv_color_t*)heap_caps_malloc(SCREEN_RES*SCREEN_RES*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("LITE_BACKGROUND cache unavailable");return;}
  auto* canvas=lv_canvas_create(root);lv_canvas_set_buffer(canvas,pixels,SCREEN_RES,SCREEN_RES,LV_IMG_CF_TRUE_COLOR);lv_canvas_fill_bg(canvas,color(0x020b0b),LV_OPA_COVER);
  lv_draw_rect_dsc_t field;lv_draw_rect_dsc_init(&field);field.radius=LV_RADIUS_CIRCLE;field.bg_color=color(0x061f1b);field.bg_grad.dir=LV_GRAD_DIR_VER;field.bg_grad.stops[0].color=field.bg_color;field.bg_grad.stops[1].color=color(0x020c0c);
  lv_canvas_draw_rect(canvas,32,32,176,176,&field);
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.width=1;arc.color=color(0x153e32);
  for(int r:{28,56,84})lv_canvas_draw_arc(canvas,CENTER,CENTER,r,0,359,&arc);
  arc.color=color(0x245f4c);lv_canvas_draw_arc(canvas,CENTER,CENTER,117,0,359,&arc);
  arc.color=color(0x307b5d);for(int a=12;a<360;a+=90)lv_canvas_draw_arc(canvas,CENTER,CENTER,92,a,a+56,&arc);
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.width=1;d.color=color(0x0b2a23);
  lv_point_t horizontal[]={{24,120},{216,120}},vertical[]={{120,64},{120,180}};
  lv_canvas_draw_line(canvas,horizontal,2,&d);lv_canvas_draw_line(canvas,vertical,2,&d);
  for(int deg=0;deg<360;deg+=5){float a=deg*.0174532925f;int inner=deg%30==0?108:113;d.color=color(deg%30==0?0x44866a:0x1c483b);
    lv_point_t p[]={{lv_coord_t(CENTER+lroundf(cosf(a)*inner)),lv_coord_t(CENTER+lroundf(sinf(a)*inner))},{lv_coord_t(CENTER+lroundf(cosf(a)*116)),lv_coord_t(CENTER+lroundf(sinf(a)*116))}};lv_canvas_draw_line(canvas,p,2,&d);
  }
  liteBackdrop=*lv_canvas_get_img(canvas);lv_obj_del(canvas);
}
#include "radar_renderer.inc"
static lv_obj_t *root,*radar,*status,*activity,*pageLabel,*notice;
static uint32_t noticeAt=0,lastFrame=0,frames=0;
static lv_obj_t* text(const char* value,int y,int width,const lv_font_t* font,uint32_t rgb){
  auto* o=lv_label_create(root);lv_label_set_text(o,value);lv_obj_set_size(o,width,LV_SIZE_CONTENT);lv_obj_set_pos(o,(SCREEN_RES-width)/2,y);
  lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,color(rgb),0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
  lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);return o;
}
void uiBegin(){
  candidatesOnly=true;radarRecent=true;scr_lvgl_init();root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,color(0x020c08),0);
  prepareLiteBackdrop(root);
  radar=lv_obj_create(root);lv_obj_remove_style_all(radar);lv_obj_set_pos(radar,0,0);lv_obj_set_size(radar,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(radar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_clear_flag(radar,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(radar,radarDraw,LV_EVENT_DRAW_MAIN,nullptr);
  auto* logo=lv_img_create(root);lv_img_set_src(logo,&radar_logo);lv_obj_align(logo,LV_ALIGN_TOP_MID,0,9);
  status=text("STARTING RADIO",44,184,&lv_font_montserrat_10,0xdbf9c3);
  activity=text("",57,172,&lv_font_montserrat_10,0xffbc76);
  pageLabel=text("",193,150,&lv_font_montserrat_10,0x9acbb7);
  notice=text("",207,132,&lv_font_montserrat_10,0xffe78a);
  lv_obj_set_height(notice,13);lv_obj_add_flag(activity,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
}
void uiLoop(){
  const uint32_t now=millis();
  // Leave time between full radar draws to drain reception queues on the S3R2.
  if(uint32_t(now-lastFrame)>=83){lastFrame=radarFrameAt=now;++frames;lv_obj_invalidate(radar);
    char value[80];snprintf(value,sizeof(value),"%s RX   %02d SIGNALS   100%%",blePhase?"BLE":"WI-FI",radarCount);lv_label_set_text(status,value);
    const bool deauth=trafficStatus.disconnectActive(now),spam=trafficStatus.bleActive(now);
    lv_label_set_text(activity,deauth&&spam?"DEAUTH + BLE BURST":deauth?"DEAUTH ACTIVITY":spam?"BLE SPAM CANDIDATE":"");
    if(deauth||spam)lv_obj_clear_flag(activity,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(activity,LV_OBJ_FLAG_HIDDEN);
    snprintf(value,sizeof(value),"%s",radarCount?"RSSI  /  STRONGER INWARD":"SCANNING  /  BLE + WI-FI");
    lv_label_set_text(pageLabel,value);
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
  if(c=="ui status"){Serial.printf("UI_STATUS page=radar lite=1 frames=%lu backdrop=%u frame_ms=83\n",frames,liteBackdrop.data?1:0);return true;}
  if(c=="radar status"){Serial.printf("RADAR_SETTINGS theme=1 labels=1 recent=1 pages=1 plotted=%u primary=%d\n",dotCount,radarPrimary);return true;}
  if(c=="display status"){Serial.println("DISPLAY_SETTINGS brightness=100 output=100 dim=0 off=0 dim_level=0 wake=0");return true;}
  return false;
}
