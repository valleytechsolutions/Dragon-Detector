#include "app.h"
#include "radar_colors.h"
#include "radar_layout.h"
#include <lvgl.h>
#include <math.h>
#include <esp_heap_caps.h>
#define S(v) (((v)*SCREEN_RES+((v)>=0?180:-180))/360)
#define FCALLOUT (&lv_font_montserrat_12)
extern void scr_lvgl_init();
#include "radar_theme_catalog.h"
#include <Preferences.h>
static constexpr int CENTER=SCREEN_RES/2;
static unsigned radarTheme=1;
static Preferences litePrefs;
static bool prefsReady=false,themeDirty=false;
static uint32_t themeChangedAt=0;
static constexpr bool radarLabels=true;
static uint32_t radarPrepared=0;
static int radarOrder[MAX_CONTACTS],radarCount=0;
static uint32_t radarFrameAt=0;
struct Dot {int x,y,index;dragon::DeviceKey key;int labelSlot=-1;};
static Dot dots[MAX_CONTACTS];static int dotCount=0;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
static lv_img_dsc_t liteBackdrop;
// Repaint instrument geometry only at startup or theme changes. Keep the internal display tile free
// for moving content; this cache costs 115 KB of the otherwise idle PSRAM.
static void prepareLiteBackdrop(lv_obj_t* root){
  static auto* pixels=(lv_color_t*)heap_caps_malloc(SCREEN_RES*SCREEN_RES*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("LITE_BACKGROUND cache unavailable");return;}
  const auto& theme=radarPalettes[radarTheme];
  auto* canvas=lv_canvas_create(root);lv_canvas_set_buffer(canvas,pixels,SCREEN_RES,SCREEN_RES,LV_IMG_CF_TRUE_COLOR);lv_canvas_fill_bg(canvas,color(theme.bottom),LV_OPA_COVER);
  lv_draw_rect_dsc_t field;lv_draw_rect_dsc_init(&field);field.radius=LV_RADIUS_CIRCLE;field.bg_color=color(theme.top);field.bg_grad.dir=LV_GRAD_DIR_VER;field.bg_grad.stops[0].color=field.bg_color;field.bg_grad.stops[1].color=color(theme.bottom);
  lv_canvas_draw_rect(canvas,32,32,176,176,&field);
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.width=1;arc.color=color(theme.grid);
  for(int r:{28,56,84})lv_canvas_draw_arc(canvas,CENTER,CENTER,r,0,359,&arc);
  arc.color=color(theme.ring);lv_canvas_draw_arc(canvas,CENTER,CENTER,117,0,359,&arc);
  arc.color=color(theme.ring);for(int a=12;a<360;a+=90)lv_canvas_draw_arc(canvas,CENTER,CENTER,92,a,a+56,&arc);
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.width=1;d.color=color(theme.grid);
  lv_point_t horizontal[]={{24,120},{216,120}},vertical[]={{120,64},{120,180}};
  lv_canvas_draw_line(canvas,horizontal,2,&d);lv_canvas_draw_line(canvas,vertical,2,&d);
  for(int deg=0;deg<360;deg+=5){float a=deg*.0174532925f;int inner=deg%30==0?108:113;d.color=color(deg%30==0?theme.ring:theme.grid);
    lv_point_t p[]={{lv_coord_t(CENTER+lroundf(cosf(a)*inner)),lv_coord_t(CENTER+lroundf(sinf(a)*inner))},{lv_coord_t(CENTER+lroundf(cosf(a)*116)),lv_coord_t(CENTER+lroundf(sinf(a)*116))}};lv_canvas_draw_line(canvas,p,2,&d);
  }
  // Native marks avoid adding full watch assets or animated scene renderers.
  if(radarTheme==13){
    for(int side:{-1,1}){lv_point_t eye[]={{lv_coord_t(CENTER+side*8),29},{lv_coord_t(CENTER+side*31),18}};
      d.color=color(0x5a2680);d.width=9;lv_canvas_draw_line(canvas,eye,2,&d);
      d.color=color(0xcb83ff);d.width=5;lv_canvas_draw_line(canvas,eye,2,&d);
      d.color=color(0xf3dfff);d.width=2;lv_canvas_draw_line(canvas,eye,2,&d);}
  }else if(radarTheme==24){
    lv_draw_label_dsc_t label;lv_draw_label_dsc_init(&label);label.color=color(theme.sweep);label.font=&lv_font_montserrat_10;label.align=LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(canvas,85,7,70,&label," .--. \n(o..o)\n/||||\\");
  }
  liteBackdrop=*lv_canvas_get_img(canvas);lv_obj_del(canvas);lv_img_cache_invalidate_src(&liteBackdrop);
}
#include "radar_renderer.inc"
static lv_obj_t *root,*radar,*status,*activity,*pageLabel,*notice,*logo,*mcnallyLogo;
static lv_obj_t* liteStartup=nullptr;
static uint32_t liteStartupAt=0;
#include "startup_logo.inc"
static void startLiteSplash(){
  if(liteStartup)lv_obj_del(liteStartup);
  liteStartup=lv_obj_create(root);lv_obj_remove_style_all(liteStartup);
  lv_obj_set_size(liteStartup,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(liteStartup,LV_OBJ_FLAG_SCROLLABLE);
  showStartupLogo(liteStartup);liteStartupAt=millis();
}
static uint32_t noticeAt=0,lastFrame=0,frames=0;
static lv_obj_t* text(const char* value,int y,int width,const lv_font_t* font,uint32_t rgb){
  auto* o=lv_label_create(root);lv_label_set_text(o,value);lv_obj_set_size(o,width,LV_SIZE_CONTENT);lv_obj_set_pos(o,(SCREEN_RES-width)/2,y);
  lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,color(rgb),0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
  lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);return o;
}
#include "lite_pages.inc"
void uiBegin(){
  if(!prepareThemeLogos())Serial.println("THEME_LOGOS PSRAM allocation failed / fallback logos");
  prefsReady=litePrefs.begin("lite-ui",false);
  if(prefsReady){unsigned saved=litePrefs.getUChar("theme",1);radarTheme=saved<RadarThemeCount?saved:1;}
  candidatesOnly=true;radarRecent=true;scr_lvgl_init();root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,color(radarPalettes[radarTheme].bottom),0);
  prepareLiteBackdrop(root);
  radar=lv_obj_create(root);lv_obj_remove_style_all(radar);lv_obj_set_pos(radar,0,0);lv_obj_set_size(radar,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(radar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_clear_flag(radar,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(radar,radarDraw,LV_EVENT_DRAW_MAIN,nullptr);
  logo=lv_img_create(root);if(radarLogos[radarTheme])lv_img_set_src(logo,radarLogos[radarTheme]);else lv_obj_add_flag(logo,LV_OBJ_FLAG_HIDDEN);lv_obj_align(logo,LV_ALIGN_TOP_MID,0,9);
  mcnallyLogo=lv_img_create(root);lv_img_set_src(mcnallyLogo,&radar_logo_mcnally);lv_obj_clear_flag(mcnallyLogo,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(mcnallyLogo,LV_OBJ_FLAG_HIDDEN);
  status=text("STARTING RADIO",44,184,&lv_font_montserrat_10,radarPalettes[radarTheme].text);
  activity=text("",57,172,&lv_font_montserrat_10,dragon::UiPalettes[radarTheme].accent);
  pageLabel=text("",193,150,&lv_font_montserrat_10,radarPalettes[radarTheme].text);
  notice=text("",207,132,&lv_font_montserrat_10,dragon::UiPalettes[radarTheme].ink);
  lv_obj_set_height(notice,13);lv_obj_add_flag(activity,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
  createLitePages();
  startLiteSplash();
}
void uiLoop(){
  const uint32_t now=millis();
  if(liteStartup&&uint32_t(now-liteStartupAt)>=2200){lv_obj_del(liteStartup);liteStartup=nullptr;}
  if(themeDirty&&uint32_t(now-themeChangedAt)>=1000){
    if(prefsReady&&litePrefs.putUChar("theme",radarTheme)!=1)Serial.println("LITE_THEME save failed");
    themeDirty=false;
  }
  // Leave time between full radar draws to drain reception queues on the S3R2.
  if(uint32_t(now-lastFrame)>=83){lastFrame=radarFrameAt=now;++frames;
    if(litePage==LitePage::Radar){lv_obj_invalidate(radar);
    char value[80];snprintf(value,sizeof(value),"%s RX   %02d SIGNALS   100%%",blePhase?"BLE":"WI-FI",radarCount);lv_label_set_text(status,value);
    const bool deauth=trafficStatus.disconnectActive(now),spam=trafficStatus.bleActive(now);
    lv_label_set_text(activity,deauth&&spam?"DEAUTH + BLE BURST":deauth?"DEAUTH ACTIVITY":spam?"BLE SPAM CANDIDATE":"");
    if(deauth||spam)lv_obj_clear_flag(activity,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(activity,LV_OBJ_FLAG_HIDDEN);
    snprintf(value,sizeof(value),"%s",themeChangedAt&&uint32_t(now-themeChangedAt)<4000?radarThemeNames[radarTheme]:radarCount?"RSSI  /  STRONGER INWARD":"SCANNING  /  BLE + WI-FI");
    lv_label_set_text(pageLabel,value);
    }
    updateLitePage();
    if(!noticeAt||uint32_t(now-noticeAt)>5000)lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
  }
  lv_timer_handler();
}
void uiNotify(const char* title,const char*,const dragon::DeviceKey*,bool){
  if(!notice||litePage!=LitePage::Radar)return;noticeAt=millis();lv_label_set_text(notice,title);lv_obj_clear_flag(notice,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(pageHint,LV_OBJ_FLAG_HIDDEN);
}
void uiShow(const String& name){if(name=="startup"){startLiteSplash();return;}for(unsigned i=0;i<5;++i)if(name==litePageNames[i]){setLitePage(LitePage(i));return;}}
void uiKey(bool held){
  if(held||!root)return;
  radarTheme=(radarTheme+1)%RadarThemeCount;themeDirty=true;themeChangedAt=millis();
  prepareLiteBackdrop(root);lv_obj_set_style_bg_color(root,color(radarPalettes[radarTheme].bottom),0);
  lv_obj_set_style_text_color(status,color(radarPalettes[radarTheme].text),0);
  lv_obj_set_style_text_color(pageLabel,color(radarPalettes[radarTheme].text),0);
  lv_obj_set_style_text_color(activity,color(dragon::UiPalettes[radarTheme].accent),0);
  lv_obj_set_style_text_color(notice,color(dragon::UiPalettes[radarTheme].ink),0);
  if(radarLogos[radarTheme]){lv_img_set_src(logo,radarLogos[radarTheme]);lv_obj_clear_flag(logo,LV_OBJ_FLAG_HIDDEN);}
  else lv_obj_add_flag(logo,LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(logo,LV_ALIGN_TOP_MID,0,9);setLitePage(litePage);lv_obj_invalidate(root);
  Serial.printf("LITE_THEME theme=%u name=%s\n",radarTheme,radarThemeCommands[radarTheme]);
}
bool uiStopwatchKey(bool,uint64_t){return false;}
bool uiCommand(const String& c){
  if(c=="ui status"){Serial.printf("UI_STATUS page=%s lite=1 frames=%lu backdrop=%u frame_ms=83\n",litePageNames[unsigned(litePage)],frames,liteBackdrop.data?1:0);return true;}
  if(c=="radar status"){Serial.printf("RADAR_SETTINGS theme=%u labels=1 recent=1 pages=1 plotted=%u primary=%d\n",radarTheme,dotCount,radarPrimary);return true;}
  if(c=="display status"){Serial.println("DISPLAY_SETTINGS brightness=100 output=100 dim=0 off=0 dim_level=0 wake=0");return true;}
  return false;
}
