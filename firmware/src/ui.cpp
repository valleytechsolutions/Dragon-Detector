#include "app.h"
#include "wardrive.h"
#include "personal_theme.h"
#include "notebook.h"
#include "vendor_registry.h"
#include "ble_lab.h"
#include "recon.h"
#include "recipes.h"
#include "radar_colors.h"
#include "board_pins.h"
#include "radar_layout.h"
#include "radar_interaction.h"
#include "notifications.h"
#include "ui_theme.h"
#include "rotary_ui.h"
#include <lvgl.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <Preferences.h>
#include <time.h>

extern void scr_lvgl_init();
extern void set_brightness(uint8_t);
extern const lv_img_dsc_t vts_logo;
extern const lv_img_dsc_t radar_logo;
extern lv_img_dsc_t radar_logo_valleytech,radar_logo_biscuit,radar_logo_marauder,radar_logo_toool,radar_logo_church,watch_logo_church,radar_logo_ci,watch_logo_ci,watch_logo_mcnally;
extern lv_img_dsc_t watch_logo_lpl;
extern lv_img_dsc_t radar_logo_awok,watch_logo_awok,radar_logo_baph,watch_logo_baph,radar_logo_wdgwars,watch_logo_wdgwars,radar_logo_n3rdsec,watch_logo_n3rdsec,radar_logo_halehound,watch_logo_halehound,watch_logo_halehound_splash,radar_logo_waveshare,watch_logo_waveshare,radar_logo_m5stack,watch_logo_m5stack,radar_logo_lab5,watch_logo_lab5,radar_logo_sasquach,watch_logo_sasquach,radar_logo_nyan,watch_logo_nyan,radar_logo_dmac,watch_logo_dmac,radar_logo_seeed,watch_logo_seeed,radar_logo_dagnazty,watch_logo_dagnazty,radar_logo_deflock,watch_logo_deflock;
// The layout is authored for the 360 px round panel. S() maps it onto the
// board's panel; label/button/row helpers apply it, so callers pass 360 px units.
#if SCREEN_RES<=240
// Keep one-pixel strokes and tiny history samples visible after downscaling.
#define S(v) (((v)*SCREEN_RES+((v)>=0?180:-180))/360)
#else
#define S(v) ((v)*SCREEN_RES/360)
#endif
#if SCREEN_RES<=240
#define F12 (&lv_font_montserrat_8)
#define F14 (&lv_font_montserrat_10)
#define F16 (&lv_font_montserrat_12)
#define F20 (&lv_font_montserrat_14)
#define FCALLOUT (&lv_font_montserrat_8)
#define FTIMER (&lv_font_montserrat_22)
#elif SCREEN_RES>=466
#define F12 (&lv_font_montserrat_16)
#define F14 (&lv_font_montserrat_18)
#define F16 (&lv_font_montserrat_20)
#define F20 (&lv_font_montserrat_26)
#define FCALLOUT (&lv_font_montserrat_14)
#define FTIMER (&lv_font_montserrat_40)
#else
#define F12 (&lv_font_montserrat_12)
#define F14 (&lv_font_montserrat_14)
#define F16 (&lv_font_montserrat_16)
#define F20 (&lv_font_montserrat_20)
// Two lines must fit the 26px inner height of a 360px-panel callout.
#define FCALLOUT (&lv_font_montserrat_10)
#define FTIMER (&lv_font_montserrat_32)
#endif
static const int CENTER=SCREEN_RES/2;
enum Page {Boot,Radar,Menu,Devices,Details,Alerts,Channels,Files,Audio,Settings,About,Badge,Hunt,Detectors,History,HistoryDetail,Rules,Controls,Display,Diagnostics,Activity,RadarStyle,Stopwatch,Laps,Clock,ClockEdit,Orientation,FamilyColors,BleLab,Recon,Notebook,NotebookProbes,NotebookBle,NotebookDetail,ClockSettings,ClockWheels,Startup};
static unsigned notebookPage=0,notebookRows=0;
static dragon::DeviceKey notebookKeys[10];
static dragon::notebook::Advert notebookSelection;
static Page page=Boot,pending=Boot;
static lv_obj_t *root,*body,*statusLabel,*audioLabel,*audioMeter,*radar,*micBadge;
static bool openRadioTools=false;
static int menuTab=0,filter=0,selected=-1,filePage=0,brightness=100,historyPage=0;
static bool radarLabels=true;
static dragon::RadarTapGesture radarGesture;
static lv_obj_t* radarSummary=nullptr;
static uint8_t radarTheme=1;
static bool covert(){return radarTheme==9;}
static bool lpu(){return radarTheme==28;}
static bool lpl(){return radarTheme==26;}
static bool sparrows(){return radarTheme==32;}
static bool dmac(){return radarTheme==21;}
static bool locksport(){return radarTheme==6||radarTheme==8||covert()||dmac()||lpl()||lpu()||sparrows()||(radarTheme==dragon::PersonalThemeId&&(personalTheme().flags&8));}
static bool locksportDark(){return locksport()&&radarTheme!=6;}
static uint32_t locksportBg(){return uiColors().bg;}
static uint32_t locksportInk(){return uiColors().ink;}
static uint32_t locksportAccent(){return uiColors().accent;}
#include "radar_theme_catalog.h"
static HistoryRecord historySelection;
static int devicePage=0,deviceRows=0,deviceTotal=0;
static dragon::DeviceKey deviceKeys[10];
static lv_obj_t* diagnosticsLabel=nullptr;
static lv_obj_t *activityLabel=nullptr,*radarActivity=nullptr;
static uint32_t radarPrepared=0;
static uint32_t radarFrameAt=0;
static int radarOrder[MAX_CONTACTS],radarCount=0;
static dragon::DeviceKey historyKeys[10],controlKey,ruleKeys[RULE_LIMIT];
static uint8_t historyFamilies[10]{};
static uint8_t controlChannel=0;
static int historyRows=0;
static lv_obj_t *huntValue=nullptr,*huntMeta=nullptr,*huntGauge=nullptr,*huntGraph=nullptr,*notice=nullptr;
static dragon::DeviceKey noticeKey;static bool noticeHasKey=false,noticeWatch=false;static uint32_t noticeAt=0;
static unsigned noticePriority=0;
static dragon::NoticeQueue noticeQueue;
static uint8_t selectedMac[6]{};static bool selectedBle=false;static uint8_t selectedType=0;
static void selectContact(int i){selected=i;memcpy(selectedMac,contacts[i].mac,6);selectedBle=contacts[i].ble;selectedType=contacts[i].addressType;}
static bool selectionValid(){return selected>=0 && contacts[selected].used && !memcmp(selectedMac,contacts[selected].mac,6) && selectedBle==contacts[selected].ble && selectedType==contacts[selected].addressType;}
static lv_obj_t* trackingLabel=nullptr;
static uint32_t pageAt=0,lastUpdate=0,lastInteraction=0;
static uint32_t inputActions=0;
static int lastInputAction=-1;
static int outputBrightness=-1;
static Preferences settings;
static lv_obj_t *timerValue=nullptr,*timerState=nullptr,*timerLap=nullptr,*timerStartButton=nullptr,*timerSecondary=nullptr,*timerSplit=nullptr,*clockValue=nullptr,*clockDate=nullptr,*editValue=nullptr;
static int editOffset=0;
static lv_obj_t* clockPrompt=nullptr;
static lv_obj_t* resetBox=nullptr;
#if BOARD_HAS_SD
static String directory="/";
struct FileRow {String path;bool dir;uint32_t size;};static FileRow files[96];static int fileCount=0;static bool filesTruncated=false;
#endif
struct Dot {int x,y,index;dragon::DeviceKey key;int labelSlot=-1;};static Dot dots[MAX_CONTACTS];static int dotCount=0;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
static lv_obj_t* label(lv_obj_t* parent,const char* text,int x,int y,int w=0,const lv_font_t* font=F14,uint32_t c=uiColors().ink){
  auto* o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_pos(o,S(x),S(y));if(w)lv_obj_set_width(o,S(w));lv_obj_set_style_text_font(o,uiFont(font),0);lv_obj_set_style_text_color(o,color(c),0);return o;
}
static void go(Page p){
if(wardriveBusy()&&p!=Recon){wardriveStop();reconPage("wardrive");pending=Recon;return;}
if(p==Radar){recipeStop();reconStop();bleLabStop();}
#ifndef BOARD_RADAR_LITE
if(page==Recon&&p!=Recon)reconLeave();
#endif
if(page==BleLab&&p!=BleLab)bleLabStop();pending=p;}
static void action(lv_event_t* e);
static lv_obj_t* button(lv_obj_t* parent,const char* text,int x,int y,int w,int h,int id){
  auto* b=lv_btn_create(parent);lv_obj_set_pos(b,S(x),S(y));lv_obj_set_size(b,S(w),S(h));lv_obj_set_style_pad_all(b,S(8),0);lv_obj_set_style_bg_color(b,color(uiColors().surface),0);lv_obj_set_style_radius(b,uiAscii()?0:S(13),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,color(uiColors().border),0);lv_obj_set_style_shadow_width(b,0,0);
  lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* t=label(b,text,0,0,w-16);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(t);return b;
}
static void dismissNotice(){if(notice){lv_obj_del(notice);notice=nullptr;}}
static lv_obj_t* circleLogo(lv_obj_t* parent,const lv_img_dsc_t* source,int y){
  auto* clip=lv_obj_create(parent);lv_obj_remove_style_all(clip);lv_obj_set_size(clip,source->header.w,source->header.h);lv_obj_align(clip,LV_ALIGN_TOP_MID,0,S(y));lv_obj_set_style_radius(clip,LV_RADIUS_CIRCLE,0);lv_obj_set_style_clip_corner(clip,true,0);lv_obj_clear_flag(clip,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  auto* image=lv_img_create(clip);lv_img_set_src(image,source);lv_obj_center(image);lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE);
  return clip;
}
static void locksportColors(lv_obj_t* object){
  if(lv_obj_check_type(object,&lv_label_class))lv_obj_set_style_text_color(object,color(uiColors().ink),0);
  if(lv_obj_check_type(object,&lv_btn_class)||lv_obj_check_type(object,&lv_msgbox_class)){
    lv_obj_set_style_bg_color(object,color(uiColors().surface),0);lv_obj_set_style_border_color(object,color(uiColors().accent),0);
  }
  if(lv_obj_check_type(object,&lv_btnmatrix_class)){
    lv_obj_set_style_bg_color(object,color(locksportBg()),LV_PART_ITEMS);lv_obj_set_style_text_color(object,color(locksportAccent()),LV_PART_ITEMS);
  }
  for(unsigned i=0;i<lv_obj_get_child_cnt(object);++i)locksportColors(lv_obj_get_child(object,i));
}
void uiNotify(const char* text,const char* detail,const dragon::DeviceKey* key,bool watched){
  dragon::DetectionNotice item;strlcpy(item.title,text,sizeof(item.title));strlcpy(item.detail,detail,sizeof(item.detail));item.watched=watched;item.at=millis();item.hasKey=key!=nullptr;if(key){item.key=*key;int i=findContact(*key);if(i>=0)item.family=contacts[i].match.family;}
  for(unsigned f=0;f<unsigned(Family::Count);++f)if(!strcmp(text,dragon::familyName(Family(f)))){item.family=Family(f);break;}
  if(clockPrompt||page==Boot||page==Startup||page==BleLab||page==Stopwatch||page==Clock||page==ClockSettings||page==ClockWheels||page==ClockEdit||(notice&&noticePriority>=item.priority())){noticeQueue.push(item);return;}
  dismissNotice();noticeAt=millis();noticeWatch=watched;noticeHasKey=key!=nullptr;if(key)noticeKey=*key;
  noticePriority=item.priority();uint32_t accent=uiColors().accent;
  if(detector.wakeAlert)lastInteraction=millis();
  notice=lv_obj_create(lv_layer_top());lv_obj_set_size(notice,S(260),S(108));lv_obj_set_pos(notice,S(50),S(218));lv_obj_clear_flag(notice,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(notice,color(uiColors().bg),0);lv_obj_set_style_border_color(notice,color(accent),0);lv_obj_set_style_border_width(notice,2,0);lv_obj_set_style_radius(notice,uiAscii()?0:S(16),0);lv_obj_set_style_pad_all(notice,0,0);
  auto* heading=label(notice,text,10,7,240,F16,accent);lv_obj_set_style_text_align(heading,LV_TEXT_ALIGN_CENTER,0);
  String safe=detector.privacy?String("Detection observed. Open evidence for details."):String(detail);
  auto* message=label(notice,safe.c_str(),12,31,236,F12);lv_obj_set_height(message,S(30));lv_label_set_long_mode(message,LV_LABEL_LONG_DOT);
  if(key)button(notice,"HUNT",14,69,100,30,90);
  button(notice,"DISMISS",key?124:64,69,122,30,89);
}
static void title(const char* text,const char* subtitle=nullptr){
  auto* t=label(root,text,55,39,250,F20);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
  if(subtitle){auto* s=label(root,subtitle,55,67,250,F12,uiColors().muted);lv_obj_set_style_text_align(s,LV_TEXT_ALIGN_CENTER,0);}
  button(root,LV_SYMBOL_HOME,149,307,62,33,1);
}
static void legible(lv_obj_t* o){lv_obj_set_style_bg_color(o,color(uiColors().surface),0);lv_obj_set_style_bg_opa(o,LV_OPA_80,0);lv_obj_set_style_radius(o,7,0);}
static lv_obj_t* scrollBody(int y=91,int h=211){
  auto* o=lv_obj_create(root);lv_obj_set_pos(o,S(44),S(y));lv_obj_set_size(o,S(272),S(h));lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_scroll_dir(o,LV_DIR_VER);return o;
}
static void row(lv_obj_t* parent,const char* top,const char* bottom,int index,int id,uint32_t c=uiColors().ink){
  auto* b=button(parent,"",1,index*59,256,54,id);auto* heading=label(b,top,0,0,231,F14,c);auto* detail=label(b,bottom,0,22,231,F12,uiColors().muted);
  if(uiColors().light&&c!=uiColors().ink){lv_obj_set_style_bg_color(heading,color(dragon::RadarLabelBackground),0);lv_obj_set_style_bg_opa(heading,LV_OPA_COVER,0);}
  lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);lv_obj_set_height(heading,S(19));lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);lv_obj_set_height(detail,S(17));
}
static void centered(lv_obj_t* o){lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);}
static void durationText(char* out,size_t length,uint64_t us){
  uint64_t cs=us/10000;unsigned h=cs/360000;
  if(h)snprintf(out,length,"%u:%02u:%02u.%02u",h,unsigned(cs/6000%60),unsigned(cs/100%60),unsigned(cs%100));
  else snprintf(out,length,"%02u:%02u.%02u",unsigned(cs/6000),unsigned(cs/100%60),unsigned(cs%100));
}
static lv_obj_t* timerOrbit=nullptr;
static void updateTimerOrbit(uint64_t elapsed);
static void updateTimer(){
  if(!timerValue)return;uint64_t elapsed=stopwatch.elapsed(stopwatchNow());char text[48],lap[90],split[40];durationText(text,sizeof(text),elapsed);lv_label_set_text(timerValue,text);
  lv_obj_set_style_text_font(timerValue,elapsed>=360000000000ull?uiFont(F20):uiAscii()?&dragon_ascii_digits:FTIMER,0);
  updateTimerOrbit(elapsed);
  lv_label_set_text(timerState,locksport()?(stopwatch.running?"P I C K I N G":stopwatch.finished?"O P E N E D":stopwatch.elapsed(0)?"P A U S E D":"R E A D Y"):(stopwatch.running?"M E A S U R I N G":"R E A D Y"));
  lv_label_set_text(lv_obj_get_child(timerStartButton,0),stopwatch.running?"PAUSE":stopwatch.elapsed(0)?"RESUME":"START");
  lv_label_set_text(lv_obj_get_child(timerSecondary,0),stopwatch.running?(locksport()?"OPEN":"LAP"):"RESET");
  if(timerSplit){if(stopwatch.running)lv_obj_clear_state(timerSplit,LV_STATE_DISABLED);else lv_obj_add_state(timerSplit,LV_STATE_DISABLED);}
  if(locksport()){
    bool enabled=stopwatch.running||stopwatch.elapsed(0);
    if(enabled)lv_obj_clear_state(timerSecondary,LV_STATE_DISABLED);else lv_obj_add_state(timerSecondary,LV_STATE_DISABLED);
    lv_obj_set_style_text_color(lv_obj_get_child(timerSecondary,0),color(!enabled&&locksportDark()?uiColors().muted:uiColors().onAccent),0);
  }
  if(auto* l=stopwatch.newest()){durationText(split,sizeof(split),l->split);snprintf(lap,sizeof(lap),"%s %lu   %s",locksport()?"SPLIT":"LAP",l->number,split);}else strcpy(lap,locksport()?(stopwatch.running?"OPEN stops this attempt":stopwatch.elapsed(0)?"Resume when ready":"Start when your pick is ready"):"No laps yet");lv_label_set_text(timerLap,lap);
}
static void resetStopwatch(){
  if(stopwatch.running||!stopwatch.elapsed(stopwatchNow())||resetBox)return;
  static const char* choices[]={"Keep","Reset",""};
  resetBox=uiMessageBox(root,locksport()?"NEW ATTEMPT?":"RESET STOPWATCH?",locksport()?"Clear this attempt and its splits?":"Clear elapsed time and all laps?",choices,true);lv_obj_set_width(resetBox,S(260));lv_obj_center(resetBox);
  lv_obj_add_event_cb(resetBox,[](lv_event_t* e){auto* box=lv_event_get_current_target(e);if(lv_msgbox_get_active_btn(box)==1)stopwatch.reset();lv_msgbox_close(box);resetBox=nullptr;updateTimer();},LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_add_event_cb(resetBox,[](lv_event_t*){resetBox=nullptr;},LV_EVENT_DELETE,nullptr);
}
#include "clock_settings.inc"
static void radarPick(lv_draw_ctx_t* ctx,float radians);
static void radarAwokArt(lv_draw_ctx_t* ctx);
static void radarBaphArt(lv_draw_ctx_t* ctx);
static void radarWdgArt(lv_draw_ctx_t* ctx);
static void radarGhostArt(lv_draw_ctx_t* ctx);
static void radarN3rdArt(lv_draw_ctx_t* ctx);
static void radarHaleArt(lv_draw_ctx_t* ctx);
static void radarWaveArt(lv_draw_ctx_t* ctx);
static void radarM5Art(lv_draw_ctx_t* ctx);
static void radarLabArt(lv_draw_ctx_t* ctx);
static void radarSasquachArt(lv_draw_ctx_t* ctx);
static void radarNyanArt(lv_draw_ctx_t* ctx);
static void radarSeeedArt(lv_draw_ctx_t* ctx);
static void radarDagArt(lv_draw_ctx_t* ctx);
static void radarDeflockArt(lv_draw_ctx_t* ctx);
static void radarPigArt(lv_draw_ctx_t* ctx);
static void radarPigSweep(lv_draw_ctx_t* ctx,float angle);
static void asciiMark(lv_draw_ctx_t* ctx,const char* text,int x,int y,uint32_t ink);
#include "radar_renderer.inc"
#include "watch_faces.inc"
static void radarPick(lv_draw_ctx_t* ctx,float radians){
  watchPick(ctx,radians/WatchTau+.25f,pickMcTickler,pickMcTicklerTriangles,true,1.16f);
}
static void selectRadarTheme(uint8_t selected){
  if(selected==dragon::PersonalThemeId&&!personalThemeReady())return;
  radarTheme=selected;dismissNotice();uiApplyTheme(radarTheme);settings.putUChar("radar-theme",radarTheme);
  if(radarTheme==dragon::PersonalThemeId){watchFace=dragon::PersonalWatchId;settings.putUChar("watch-face",watchFace);return;}
  if(radarTheme==0){watchFace=0;settings.putUChar("watch-face",watchFace);}
  if(locksport()){watchFace=sparrows()?35:lpu()?31:lpl()?29:dmac()?24:covert()?12:locksportDark()?11:9;settings.putUChar("watch-face",watchFace);}
  else if(radarTheme==7){watchFace=10;settings.putUChar("watch-face",watchFace);}
  else if(radarTheme>=10){watchFace=radarTheme+3;settings.putUChar("watch-face",watchFace);}
}
static void huntDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);auto* obj=lv_event_get_target(e);lv_area_t area;lv_obj_get_coords(obj,&area);
  for(int i=0;i<4;++i)line(ctx,area.x1,area.y1+S(i*14),area.x2,area.y1+S(i*14),uiColors().border);
  if(trackedContact<0)return;auto& trace=contacts[trackedContact].trace;
  uint32_t now=millis();
  for(unsigned i=0;i<48;++i){int current=trace.recent(i,now);if(current==dragon::SignalHistory::Gap)continue;int b=constrain((-current-25)*42/80,0,42);
    // Sparse advertisers may never fill adjacent buckets. Keep each received
    // sample visible without drawing across the missing reception between them.
    disc(ctx,area.x1+S(i*240/47),area.y1+S(b),S(1),uiColors().accent);
    int previous=i?trace.recent(i-1,now):dragon::SignalHistory::Gap;if(previous==dragon::SignalHistory::Gap)continue;int a=constrain((-previous-25)*42/80,0,42);
    line(ctx,area.x1+S((i-1)*240/47),area.y1+S(a),area.x1+S(i*240/47),area.y1+S(b),uiColors().accent,2);
  }
}
static void radarTap(lv_event_t*){lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);int nearest=S(18)*S(18),idx=-1;for(int i=0;i<dotCount;++i){int d=(dots[i].x-p.x)*(dots[i].x-p.x)+(dots[i].y-p.y)*(dots[i].y-p.y);if(d<nearest && contactKey(contacts[dots[i].index])==dots[i].key){nearest=d;idx=dots[i].index;}}
  if(idx<0)for(int i=0;i<dotCount;++i)if(dots[i].labelSlot>=0&&contactKey(contacts[dots[i].index])==dots[i].key){auto b=dragon::radarLabelSlot(dots[i].labelSlot);if(p.x>=S(b.x)&&p.x<=S(b.x+b.w)&&p.y>=S(b.y)&&p.y<=S(b.y+b.h)){idx=dots[i].index;break;}}
  if(idx>=0){
    const auto key=contactKey(contacts[idx]);
    bool open=radarGesture.tap(key,millis());
    radarSelectedKey=key;radarHasSelection=true;radarSelectedAt=millis();
    if(open){selectContact(idx);go(Details);}else lv_obj_invalidate(radar);
  }else{radarGesture.clear();radarHasSelection=false;lv_obj_invalidate(radar);}}
#if BOARD_HAS_SD
static void loadFiles(){
  fileCount=0;filesTruncated=false;if(!sdReady)return;
  xSemaphoreTake(sdMutex,portMAX_DELAY);File dir=CardFS.open(directory);
  if(dir&&dir.isDirectory()){File f=dir.openNextFile();while(f){if(fileCount>=96){filesTruncated=true;f.close();break;}String name=f.name();String path=directory=="/"?"/"+name:directory+"/"+name;files[fileCount++]={path,f.isDirectory(),uint32_t(f.size())};f.close();f=dir.openNextFile();}}dir.close();xSemaphoreGive(sdMutex);
}
#endif
#include "startup_logo.inc"
static void drawStartup(){showStartupLogo(root);}
static void build(){
  // BLE Lab owns its complete screen and returns before the shared mic label
  // is rebuilt. Never retain a label from the page that is about to be freed.
  micBadge=nullptr;radarSummary=nullptr;radarGesture.clear();
  timerOrbit=nullptr;timerValue=timerState=timerLap=timerStartButton=timerSecondary=timerSplit=clockValue=clockDate=editValue=nullptr;
  badgeUnload();page=pending;pageAt=millis();statusLabel=audioLabel=audioMeter=radar=trackingLabel=huntValue=huntMeta=huntGauge=huntGraph=diagnosticsLabel=activityLabel=radarActivity=nullptr;radarPrepared=0;
  setRadioStandby(page==Clock||page==Stopwatch||page==Laps||page==ClockSettings||page==ClockWheels||page==ClockEdit);
  lv_obj_clean(root);lv_obj_set_style_bg_color(root,color(uiColors().bg),0);
  #ifndef BOARD_RADAR_LITE
  if(page==BleLab){bleLabView(root);return;}
  if(page==Recon){reconView(root,openRadioTools);return;}
#endif
  if(page==Startup){dismissNotice();drawStartup();return;}
  else if(page==Badge){lv_obj_set_style_bg_color(root,color(0x000000),0);badgeView(root);
  }else if(page==Radar){
    radar=lv_obj_create(root);lv_obj_remove_style_all(radar);lv_obj_set_pos(radar,0,0);lv_obj_set_size(radar,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(radar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(radar,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(radar,radarDraw,LV_EVENT_DRAW_MAIN,nullptr);lv_obj_add_event_cb(radar,radarTap,LV_EVENT_CLICKED,nullptr);
    if(radarTheme==8||radarTheme==14||dmac()||radarTheme==22||radarTheme==23)circleLogo(root,radarLogo(radarTheme),17);
    else if(radarTheme!=13&&radarTheme!=24&&radarTheme!=32&&radarTheme!=CanCommanderTheme){auto* logo=lv_img_create(root);lv_img_set_src(logo,radarLogo(radarTheme));lv_obj_align(logo,LV_ALIGN_TOP_MID,0,S(17));lv_obj_clear_flag(logo,LV_OBJ_FLAG_CLICKABLE);if(radarTheme==10||radarTheme==3){lv_obj_set_style_img_recolor(logo,color(radarTheme==3?0xe1e8ec:0xffffff),0);lv_obj_set_style_img_recolor_opa(logo,LV_OPA_COVER,0);}}
    const auto& theme=radarPalette(radarTheme);
    statusLabel=label(root,"LISTENING",56,66,248,F12,theme.text);lv_obj_set_style_text_align(statusLabel,LV_TEXT_ALIGN_CENTER,0);legible(statusLabel);lv_obj_set_style_bg_color(statusLabel,color(theme.panel),0);lv_obj_set_style_bg_opa(statusLabel,LV_OPA_60,0);
    radarActivity=label(root,"",75,84,210,F12,0xffbc76);lv_obj_set_style_text_align(radarActivity,LV_TEXT_ALIGN_CENTER,0);legible(radarActivity);lv_obj_add_flag(radarActivity,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(radarActivity,action,LV_EVENT_CLICKED,(void*)116);
    radarSummary=label(root,"",65,244,194,F12,theme.text);
    lv_obj_set_height(radarSummary,S(34));lv_obj_set_style_text_align(radarSummary,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_bg_color(radarSummary,color(dragon::RadarLabelBackground),0);lv_obj_set_style_bg_opa(radarSummary,LV_OPA_COVER,0);
    lv_obj_set_style_radius(radarSummary,S(5),0);lv_obj_add_flag(radarSummary,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(radarSummary,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(radarSummary,action,LV_EVENT_CLICKED,(void*)159);
    trackingLabel=label(root,"TAP A SIGNAL",70,280,220,F12,theme.text);lv_obj_set_style_text_align(trackingLabel,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(trackingLabel,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(trackingLabel,action,LV_EVENT_CLICKED,(void*)152);
    auto* targets=button(root,"TARGETS",90,301,90,32,96);
    auto* tools=button(root,"TOOLS",187,301,78,32,2);
    auto* quiet=button(root,detector.quiet?LV_SYMBOL_MUTE:LV_SYMBOL_VOLUME_MAX,270,257,38,34,97);
    for(auto* b:{targets,tools,quiet}){lv_obj_set_style_bg_color(b,color(theme.panel),0);lv_obj_set_style_bg_opa(b,LV_OPA_80,0);lv_obj_set_style_border_color(b,color(theme.ring),0);lv_obj_set_style_border_opa(b,LV_OPA_60,0);if(locksport()||radarTheme==31)lv_obj_set_style_text_color(lv_obj_get_child(b,0),color(theme.text),0);}
  }else if(page==Menu){
    title("DRAGON TOOLS");const char* tabs[]={"RECON","UTILITIES","SYSTEM"};
    for(int i=0;i<3;++i){auto* b=button(root,tabs[i],47+i*89,76,86,32,10+i);if(menuTab==i)lv_obj_set_style_bg_color(b,color(uiColors().raised),0);}
    #ifndef BOARD_RADAR_LITE
    if(menuTab==0){button(root,"Modes",57,115,119,40,7100);button(root,"Attacks",184,115,119,40,7103);button(root,"Wi-Fi",57,162,119,40,20);button(root,"Bluetooth",184,162,119,40,21);button(root,"History",57,209,119,40,60);button(root,"Hunt",184,209,119,40,57);button(root,"Radio tools",57,257,119,39,7102);button(root,"BLE Lab",184,257,119,39,7000);}
#else
    if(menuTab==0){button(root,"Wi-Fi",57,119,119,49,20);button(root,"Bluetooth",184,119,119,49,21);button(root,"History",57,176,119,49,60);button(root,"Hunt",184,176,119,49,57);button(root,"Detectors",57,233,119,45,61);button(root,"Activity",184,233,119,45,116);}
    #endif
    if(menuTab==1){button(root,dmac()?"Locksport\ncounter":locksport()?"Locksport\ntimer":"Stopwatch",57,119,119,49,130);button(root,"Clock",184,119,119,49,133);button(root,"My badge",57,176,119,49,50);
#if BOARD_HAS_SD
    button(root,"SD files",184,176,119,49,32);
#else
    button(root,"Saved files",184,176,119,49,7104);
#endif
    button(root,"Activity",79,242,202,43,116);}

    if(menuTab==2){button(root,"Settings",60,126,240,48,40);button(root,"Recon notebook",60,184,240,48,153);button(root,"Radio health",79,243,202,44,99);}
  }else if(page==Devices){
    title(filter==0?"RADAR TARGETS":filter==1?"WI-FI CONTACTS":"BLE CONTACTS","Axon first / trackers last");body=scrollBody(88,180);
    int order[MAX_CONTACTS];deviceTotal=0;uint32_t now=millis();
    for(size_t i=0;i<MAX_CONTACTS;++i){auto& c=contacts[i];if(!c.used || !c.packets || (filter==0&&!radarVisible(c,now)) || (filter==1&&c.ble)||(filter==2&&!c.ble)||(filter!=0&&now-c.seen>=60000))continue;order[deviceTotal++]=i;}
    std::sort(order,order+deviceTotal,[now](int a,int b){return preferContact(contacts[a],contacts[b],now);});
    devicePage=constrain(devicePage,0,deviceTotal?(deviceTotal-1)/10:0);deviceRows=0;
    for(int k=devicePage*10;k<deviceTotal&&deviceRows<10;++k){auto& c=contacts[order[k]];deviceKeys[deviceRows]=contactKey(c);
      char mac[18],sub[96];displayMac(c.mac,mac);auto flags=deviceRules(contactKey(c));
      snprintf(sub,sizeof(sub),"%d dBm / %lus / %s%s",c.rssi,(unsigned long)((now-c.seen)/1000),flags&RuleWatch?"WATCH / ":flags&RuleIgnore?"IGNORED / ":"",dragon::familyName(visibleFamily(c,now,detector.families)));
      row(body,*c.name?displayName(c.name).c_str():mac,sub,deviceRows,6000+deviceRows,familyColor(visibleFamily(c,now,detector.families)));++deviceRows;
    }
    if(!deviceRows)label(body,"Listening...\nNo recent targets",10,30,230);
    button(root,"<",68,274,40,31,114);char pages[30];snprintf(pages,sizeof(pages),"%d/%d",devicePage+1,std::max(1,(deviceTotal+9)/10));label(root,pages,118,280,60,F12);
    button(root,">",180,274,40,31,115);button(root,LV_SYMBOL_REFRESH,230,274,50,31,25);
  }else if(page==Details){
    title("DEVICE DETAILS");body=scrollBody(88,169);if(selectionValid()){auto& c=contacts[selected];char mac[18];displayMac(c.mac,mac);
      String s=(*c.name?displayName(c.name):String(mac))+"\n"+mac+"\n\n"+dragon::familyName(c.match.family)+"\n"+dragon::confidenceName(c.match.confidence)+"\n"+(detector.privacy?"Evidence hidden by privacy mode":c.match.evidence)+"\n\n"+String(c.rssi)+" dBm  |  "+(c.ble?"BLE":String("CH ")+c.channel)+"\n"+c.signal.quality()+" / "+c.signal.direction()+"\nLast seen "+String((millis()-c.seen)/1000)+"s ago\nFrames: "+String(c.packets);
      if(trafficFresh(c,millis()))s+="\n\nTraffic: "+String(dragon::familyName(c.trafficFamily))+"\n"+(detector.privacy?String("Evidence hidden"):String(c.trafficEvidence))+"\nHunt follows this observed address, not a proven attacker.";
      if(c.drone.valid && !detector.privacy){if(c.drone.hasId)s+="\nRemote ID: "+String(c.drone.id);if(c.drone.hasLocation){s+="\nAircraft: "+String(c.drone.latitude,6)+", "+String(c.drone.longitude,6)+"\nPosition age: "+String((millis()-c.drone.locationAt)/1000)+"s";if(c.drone.altitude>-1000)s+="\nAltitude WGS84: "+String(c.drone.altitude,1)+" m";if(c.drone.speed<255)s+="\nSpeed: "+String(c.drone.speed,1)+" m/s";}if(c.drone.hasOperator)s+="\nOperator location: "+String(c.drone.operatorLatitude,6)+", "+String(c.drone.operatorLongitude,6)+"\nOperator position age: "+String((millis()-c.drone.operatorAt)/1000)+"s";s+="\nBroadcast data; not authenticated.";}
      s+="\n\nIdentity and activity unverified. Signal strength is not distance.";
      if(const char* vendor=dragon::registeredVendor(c.mac,c.ble,c.addressType))s+="\n\nIEEE address registrant:\n"+String(vendor)+"\nRegistry ownership is not product identification.";
      int evidenceY=0;
#ifndef BOARD_RADAR_LITE
      if(c.ble){button(body,"BLE TOOLKIT",8,0,240,36,7001);button(body,"GATT CLONE TOOLS",8,44,240,36,7002);evidenceY=92;}
#endif
      label(body,s.c_str(),8,evidenceY,240);
      button(root,"Controls",52,263,124,37,56);
      button(root,"Hunt signal",184,263,124,37,52);
    }else label(body,"Contact expired. Return to the radar and select again.",8,0,240);
  }else if(page==Hunt){
    title("DRAGON HUNT",reconRadio()?"Selected survey mode owns this radio":"Target priority / both radios scanning");
    if(trackedContact<0){label(root,"Choose a target, then HUNT.\nWalk and compare the signal.\nStronger readings move inward.",65,125,234,F14);button(root,"Nearby contacts",75,230,210,44,21);}
    else {
      auto& c=contacts[trackedContact];char mac[18];displayMac(c.mac,mac);auto* name=label(root,*c.name?displayName(c.name).c_str():mac,65,92,230,F14,uiColors().accent);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_CENTER,0);
      huntValue=label(root,"LISTENING",70,119,220,F20,uiColors().accent);lv_obj_set_style_text_align(huntValue,LV_TEXT_ALIGN_CENTER,0);
      huntGauge=lv_bar_create(root);lv_obj_set_pos(huntGauge,S(70),S(151));lv_obj_set_size(huntGauge,S(220),S(14));lv_bar_set_range(huntGauge,-105,-25);lv_obj_set_style_bg_color(huntGauge,color(uiColors().raised),0);lv_obj_set_style_bg_color(huntGauge,color(uiColors().accent),LV_PART_INDICATOR);
      huntMeta=label(root,"Waiting for target",62,172,240,F12,uiColors().muted);lv_obj_set_style_text_align(huntMeta,LV_TEXT_ALIGN_CENTER,0);
      huntGraph=lv_obj_create(root);lv_obj_remove_style_all(huntGraph);lv_obj_set_pos(huntGraph,S(60),S(219));lv_obj_set_size(huntGraph,S(240),S(43));lv_obj_add_event_cb(huntGraph,huntDraw,LV_EVENT_DRAW_MAIN,nullptr);
      button(root,"Mark",62,272,76,31,58);button(root,"Target",144,272,76,31,59);button(root,"Stop",226,272,72,31,54);
    }
  }else if(page==Notebook){
    title("RECON NOTEBOOK","Scanning stays active");body=scrollBody();auto& t=notebookCounters;
    row(body,"BLE advertisement inspector","Payload / company / services / changes",0,155);
    row(body,"Probe-request inventory","Requested Wi-Fi names / raw SSID export",1,154);
    row(body,"Refresh snapshot","USB flasher downloads both CSV tables",2,153);
    String s=String("Wi-Fi frames: ")+t.frames+"\nBeacons: "+t.beacons+"\nProbes: "+t.probes+"\nDisconnect: "+t.disconnect+"\nEAPOL: "+t.eapol+"\nM1 / M2 / M3 / M4: "+t.keyMessages[1]+" / "+t.keyMessages[2]+" / "+t.keyMessages[3]+" / "+t.keyMessages[4]+"\nPMKID observations: "+t.pmkid+"\nSAE commit: "+t.sae+"\nRetries: "+t.retries+"\n\nIEEE vendor prefixes: "+dragon::vendorRegistryCount()+"\nUncached BLE frames: "+notebookRejected+"\n\nCounts are observations, not proof of a valid handshake. Tables retain up to 96 entries each in RAM.";
    label(body,s.c_str(),7,190,240,F12);
  }else if(page==NotebookProbes||page==NotebookBle){
    bool ble=page==NotebookBle;title(ble?"BLE INSPECTOR":"PROBE INVENTORY","Refresh updates this snapshot");body=scrollBody(88,178);
    unsigned order[dragon::notebook::Capacity],count=0;for(unsigned i=0;i<dragon::notebook::Capacity;++i)if(ble?(notebookAdverts&&notebookAdverts[i].used):(notebookProbes&&notebookProbes[i].used))order[count++]=i;
    auto seen=[&](unsigned i){return ble?notebookAdverts[i].seen:notebookProbes[i].seen;};uint32_t now=millis();std::sort(order,order+count,[&](unsigned a,unsigned b){return uint32_t(now-seen(a))<uint32_t(now-seen(b));});
    notebookPage=std::min<unsigned>(notebookPage,count?(count-1)/10:0);notebookRows=0;
    for(unsigned j=notebookPage*10;j<count&&notebookRows<10;++j){unsigned i=order[j];char mac[18],sub[128];
      if(ble){auto& a=notebookAdverts[i];displayMac(a.mac,mac);auto d=dragon::lab::inspectAdvertisement(a.data,a.length,a.boundary);snprintf(sub,sizeof(sub),"%d dBm / %lus / %lu changes",a.rssi,(now-a.seen)/1000,a.changes);auto& key=notebookKeys[notebookRows];memcpy(key.mac,a.mac,6);key.radio=1;key.addressType=a.type;String top=String(mac)+" / "+d.protocol;row(body,top.c_str(),sub,notebookRows,8000+notebookRows);}
      else {auto& a=notebookProbes[i];displayMac(a.mac,mac);char name[33];dragon::safeText(name,sizeof(name),a.ssid,a.length);snprintf(sub,sizeof(sub),"%s / CH%u / %d dBm / %lu frames",mac,a.channel,a.rssi,a.count);row(body,a.length?displayName(name).c_str():"Wildcard probe",sub,notebookRows,0);}
      ++notebookRows;
    }
    if(!count)label(body,"Listening...\nNo observations yet.",8,20,238);
    button(root,"<",65,274,40,31,156);String p=String(notebookPage+1)+"/"+std::max(1u,(count+9)/10);label(root,p.c_str(),114,280,60,F12);button(root,">",180,274,40,31,157);button(root,LV_SYMBOL_REFRESH,232,274,48,31,158);
  }else if(page==NotebookDetail){
    title("ADVERTISEMENT","Latest complete received payload");body=scrollBody();auto& a=notebookSelection;auto d=dragon::lab::inspectAdvertisement(a.data,a.length,a.boundary);char mac[18];displayMac(a.mac,mac);
    String s=String(mac)+"\nAddress type: "+a.type+"\n"+a.rssi+" dBm / "+a.count+" observations\nAdvert changes: "+a.changes+"\n\n"+d.protocol+"\nService: "+(d.service?d.service:"Unresolved / not advertised")+"\nCompany: "+(d.company==0xffff?String("Not advertised"):String(dragon::lab::companyName(d.company))+" ("+String(d.company,HEX)+")")+"\nAppearance: "+d.appearance;
    if(d.hasPower)s+="\nAdvertised TX power: "+String(d.power)+" dBm";
    if(const char* v=dragon::registeredVendor(a.mac,true,a.type))s+="\nAddress registrant: "+String(v);
    s+="\n\n"+String(a.length)+" bytes / "+a.boundary+" advertisement bytes\n";
    if(detector.privacy)s+="Payload hidden by privacy mode";else for(unsigned i=0;i<a.length;++i){char b[4];snprintf(b,4,"%02x ",a.data[i]);s+=b;if(i%8==7)s+='\n';}
    label(body,s.c_str(),8,0,240,F12);
  }else if(page==Activity){
    title("RADIO ACTIVITY","Observed traffic / possible attacks");body=scrollBody(88,178);activityLabel=label(body,"Listening...",5,0,246,F12);
    button(root,"Events",63,272,105,32,22);button(root,"Channels",184,272,113,32,23);
  }else if(page==Detectors){
    title("DETECTION FILTERS","Priority devices alert; trackers stay as dots");body=scrollBody();
    row(body,detector.minimum==Confidence::Low?"Other alerts: all matches":detector.minimum==Confidence::Medium?"Other alerts: medium +":"Other alerts: strong only","Axon / cameras / glasses / drones / Flipper bypass",0,62);
    row(body,axonPriority?"Axon priority: ON":"Axon priority: OFF","First hit; every AXEOFF / Axon match",1,151);
    for(unsigned i=2;i<unsigned(Family::Count);++i){auto f=Family(i);String text=String(detector.enabled(f)?"ON  ":"OFF  ")+dragon::familyName(f);row(body,text.c_str(),"Tap to enable or disable this family",i,4000+i);}
  }else if(page==Controls){
    title("TARGET CONTROLS");body=scrollBody();char mac[18];displayMac(controlKey.mac,mac);uint8_t flags=deviceRules(controlKey);
    row(body,mac,"Rules match this radio/address only",0,0);
    row(body,flags&RuleWatch?"Watch: ON":"Watch: OFF","Priority alert when this address is observed",1,65);
    row(body,flags&RuleIgnore?"Ignore: ON":"Ignore: OFF","Mute alerts and hide from radar",2,66);
    row(body,flags&RuleSnooze?"Snoozed until restart":"Snooze until restart","Tap to toggle temporary silence",3,67);
    row(body,"Hunt this address","Random-address rotation can lose the target",4,68);
  }else if(page==Rules){
    title("WATCH / IGNORE LIST","Tap an entry to manage its rules");body=scrollBody();int n=0;
    for(size_t i=0;i<RULE_LIMIT;++i){uint8_t flags;dragon::DeviceKey key;if(!ruleAt(i,key,flags))continue;ruleKeys[n]=key;char mac[18];displayMac(key.mac,mac);String s=String(flags&RuleWatch?"WATCH ":"")+(flags&RuleIgnore?"IGNORED ":"")+(flags&RuleSnooze?"SNOOZED":"");row(body,mac,s.c_str(),n,5000+n);++n;}
    if(!n)label(body,"No saved rules.\nOpen any target's Controls\nto watch, ignore, or snooze it.",6,28,244);
  }else if(page==History){
    title("DETECTION HISTORY",historyReady?"Stored locally / most recent first":"Flash history unavailable");body=scrollBody(88,180);
    int order[HISTORY_LIMIT];for(size_t i=0;i<historyCount;++i)order[i]=i;
    std::sort(order,order+historyCount,[](int a,int b){return int32_t(historyRecords[a].sequence-historyRecords[b].sequence)>0;});
    historyPage=constrain(historyPage,0,int(historyCount?((historyCount-1)/10):0));historyRows=0;
    for(size_t k=historyPage*10;k<historyCount&&historyRows<10;++k){auto& r=historyRecords[order[k]];historyKeys[historyRows]=r.key;historyFamilies[historyRows]=r.family;char mac[18],sub[90];displayMac(r.key.mac,mac);snprintf(sub,sizeof(sub),"%s / peak %d dBm / %lu visits",dragon::familyName(Family(r.family)),r.peak,(unsigned long)r.encounters);row(body,*r.name?displayName(r.name).c_str():mac,sub,historyRows,3000+historyRows,familyColor(Family(r.family)));++historyRows;}
    if(!historyCount)label(body,detector.history?"No observations saved yet.\nFirst matches are recorded.":"Persistent history is disabled.\nEnable it in Settings.",6,28,244);
    button(root,"<",70,274,40,31,110);button(root,"Events",116,274,84,31,22);button(root,">",206,274,40,31,111);button(root,"Clear",252,274,55,31,112);
  }else if(page==HistoryDetail){
    title("SAVED EVIDENCE");body=scrollBody(88,169);auto& r=historySelection;char mac[18];displayMac(r.key.mac,mac);
    String s=displayName(r.name)+"\n"+mac+"\n\n"+dragon::familyName(Family(r.family))+"\n"+dragon::confidenceName(Confidence(r.confidence))+"\n"+(detector.privacy?"Evidence hidden":r.evidence)+"\nPeak "+String(r.peak)+" dBm / last "+String(r.rssi)+" dBm\nEncounters: "+String(r.encounters);
    if(r.lastEpoch){time_t epoch=r.lastEpoch;struct tm tm;gmtime_r(&epoch,&tm);char date[40];strftime(date,sizeof(date),"\nLast seen: %Y-%m-%d\n%H:%M:%S UTC",&tm);s+=date;}else s+="\nLast seen: "+String(r.lastUptime/1000)+"s into recorded boot\nClock was not synchronized.";
    label(body,s.c_str(),8,0,240);button(root,"Controls",52,263,124,37,69);button(root,"Hunt signal",184,263,124,37,70);
  }else if(page==Alerts){
    title("OBSERVATIONS","Pattern evidence, not proof of intent");body=scrollBody();uint32_t count=std::min<uint32_t>(alertTotal,MAX_ALERTS);
    if(!count)label(body,"No alerts observed.\nScan gaps and silent devices\ncan hide activity.",6,25,246);
    for(uint32_t i=0;i<count;++i){auto& a=alerts[(alertTotal-1-i)%MAX_ALERTS];String s=String(a.at/1000)+"s  "+a.title+"\n"+(detector.privacy?"Details hidden by privacy mode":a.detail);auto* l=label(body,s.c_str(),4,i*110,244,F12);lv_obj_set_height(l,S(104));}
  }else if(page==Channels){
    title("CHANNEL ACTIVITY","Frames / second of observed dwell time");body=scrollBody();for(int ch=1;ch<=11;++ch){char s[96];float rate=channelDwellMs[ch]?channelFrames[ch]*1000.0/channelDwellMs[ch]:0;snprintf(s,sizeof(s),"CH %02d     %.1f f/s     %lus",ch,rate,(unsigned long)(channelDwellMs[ch]/1000));row(body,s,channelLock==ch?"Priority channel; full sweeps continue":"Tap to prioritize this channel",ch-1,200+ch);}
    button(root,"Auto",215,307,52,31,200);

#if BOARD_HAS_SD
  }else if(page==Files){
    title("SD CARD",directory.c_str());loadFiles();body=scrollBody();
    if(!sdReady)label(body,"SD card unavailable.\nInsert FAT32 card and restart.",5,20,244);
    else{row(body,".. / Parent",fileCount?"Tap a folder or text file":"This folder is empty",0,39);for(int i=0;i<fileCount;++i){String name=files[i].path.substring(files[i].path.lastIndexOf('/')+1);String sub=files[i].dir?"Folder":String(files[i].size/1024)+" KB";row(body,name.c_str(),sub.c_str(),i+1,2000+i);}if(filesTruncated)label(body,"First 96 entries shown",5,(fileCount+1)*59,235);}

#endif
  }else if(page==Audio){
    title("AUDIO / HUNT",audioReady()?"Device melodies & signal feedback":"Speaker unavailable");body=scrollBody();int n=0;
    if(!audioReady()){label(body,"No speaker on this board.\nRadar, visual alerts and\nRSSI hunting remain available.",6,12,244);}
    else {
    row(body,alertSound?"Alert tones: ON":"Alert tones: OFF","Unique RTTTL melody per detector family",n++,78);
    String volume=String("Volume: ")+audioStatus().volume+"%";row(body,volume.c_str(),"Tap: 0 / 10 / 25 / 50 / 75 / 100%",n++,85);
    row(body,huntSound?"Hunt feedback: ON":"Hunt feedback: OFF","Pitch & cadence follow signal strength",n++,86);
    String focus=audioFocus<0?"Audio focus: OFF":String("Focus: ")+dragon::familyName(Family(audioFocus));row(body,focus.c_str(),"Off / Axon / ALPR + Flock + Raven / Glasses",n++,87);
    row(body,"Preview Axon alert","Plays once at the selected volume",n++,88);
    row(body,"Preview glasses alert","Short, simple chime",n++,94);
    row(body,"Customize with USB web UI","RTTTL editor beside Badge Studio",n++,95);
    row(body,"Stop current sound","Quiet mode mutes automatic feedback",n++,34);
    }
  }else if(page==Settings){
    title("SETTINGS");body=scrollBody();int n=0;
    row(body,"Time","Set hours, minutes and AM / PM",n++,160);
    row(body,"Date","Set month, day and year",n++,162);
    row(body,"Clock sync & time zone","USB sync / local UTC offset",n++,163);
    row(body,"Orientation","Choose a saved fixed angle",n++,135);
    row(body,"Display & timeouts","Brightness, dimming and screen off",n++,71);
    row(body,"Theme & radar labels",radarThemeNames[radarTheme],n++,117);
    row(body,"Audio & hunt tones","Volume, target focus and RTTTL alerts",n++,31);
    row(body,"Detection filters","Radar families / audible alert threshold",n++,61);
    row(body,activeBle?"BLE scan: ACTIVE":"BLE scan: PASSIVE",activeBle?"Requests scan-response names; transmits":"Listen only; tap for scan-response names",n++,98);
    row(body,"Radio health","Queue, memory and reception diagnostics",n++,99);
    row(body,"Recon notebook","Passive BLE / Wi-Fi details and USB export",n++,153);
    row(body,"Watch / ignore list","Manage saved and snoozed addresses",n++,64);
    String quiet=detector.autoQuiet?String("Auto quiet: after ")+detector.autoQuiet:"Auto quiet: off";row(body,quiet.c_str(),"Re-alert at +7 dB; resets after 30 minutes",n++,72);
    row(body,detector.quiet?"Quiet mode: ON":"Quiet mode: OFF","Hide normal popups; continue history",n++,73);
    row(body,detector.history?"Flash history: ON":"Flash history: OFF","512 unique records / USB CSV export",n++,74);
    row(body,detector.privacy?"Display privacy: ON":"Display privacy: OFF","Mask addresses, names and coordinates",n++,75);
    row(body,candidatesOnly?"Radar: candidates":"Radar: all contacts","Show filtered candidates or nearby traffic",n++,53);
    row(body,"Detection: always on","Hunt, badge and menus keep both radios scanning",n++,99);
    row(body,"Signal smoothing",detector.smoothingMs==600?"Fast / 0.6 s":detector.smoothingMs==1800?"Balanced / 1.8 s":"Steady / 4 s",n++,76);
    row(body,foxTransmit()?"Fox beacon: TRANSMITTING":"Fox beacon: OFF","Opt in to a public Dragon training beacon",n++,77);
#ifdef BOARD_STOPWATCH
    row(body,alertVibration?"Vibration: ON":"Vibration: OFF","Built-in StopWatch motor",n++,79);
#endif
    row(body,"Capture studio","Record Wi-Fi / BLE to internal flash",n++,7105);
    row(body,"Saved captures","Internal flash files / USB download",n++,7104);
#if BOARD_HAS_SD
    row(body,loggingEnabled&&sdReady?"SD event logging: ON":"SD event logging: OFF",sdReady?sessionPath.c_str():"Requires a mounted SD card",n++,43);
#ifdef BOARD_AMOLED_175
    row(body,"Capture studio",sdReady?"Wi-Fi + BLE / SD streaming / Stop saves":"RAM capture / Stop then Save",n++,7105);
#else
    row(body,captureEnabled?"SD PCAP: recording":"SD PCAP: stopped",sdReady?"Raw 802.11 / 16 MB per capture":"Requires a mounted SD card",n++,24);
#endif
#endif
    row(body,"Boot to radar","Clear the saved badge startup choice",n++,55);
  }else if(page==Stopwatch){
    dismissNotice();lv_obj_set_style_bg_color(root,color(locksport()?locksportBg():uiColors().bg),0);buildChronographFrame();
    if(locksport()){
      if(radarTheme!=dragon::PersonalThemeId&&!covert()&&!dmac()&&!lpl()&&!lpu()&&!sparrows()){circleLogo(root,&radar_logo_toool,20);centered(label(root,"LOCKSPORT TIMER",48,74,264,F20,locksportAccent()));}
      button(root,LV_SYMBOL_HOME,149,307,62,33,1);
    }else if(radarTheme!=dragon::PersonalThemeId&&radarTheme!=CanCommanderTheme)title("CHRONOGRAPH","Precision stopwatch");
    timerState=label(root,"",50,locksport()?106:98,260,F12,locksport()?locksportAccent():uiColors().accent);centered(timerState);
    timerValue=label(root,"00:00.00",33,locksport()?136:130,294,FTIMER,locksport()?locksportInk():uiColors().ink);centered(timerValue);
    timerLap=label(root,"",50,184,260,F14,uiColors().muted);centered(timerLap);
    timerStartButton=button(root,"START",55,220,122,47,131);timerSecondary=button(root,"LAP",184,220,122,47,132);
    for(auto* b:{timerStartButton,timerSecondary}){lv_obj_set_style_bg_color(b,color(uiColors().surface),0);lv_obj_set_style_border_color(b,color(uiColors().border),0);}
    if(locksport()){timerSplit=button(root,"SPLIT",65,278,72,28,168);button(root,"LOG",144,278,72,28,134);}else button(root,"LAPS",70,278,70,28,134);button(root,"TOOLS",223,278,72,28,2);
    for(unsigned i=0;i<lv_obj_get_child_cnt(root);++i){auto* b=lv_obj_get_child(root,i);if(lv_obj_check_type(b,&lv_btn_class)){lv_obj_set_style_bg_color(b,color(locksport()?(uiColors().surface):uiColors().surface),0);lv_obj_set_style_border_color(b,color(uiColors().border),0);lv_obj_set_style_text_color(lv_obj_get_child(b,0),color(locksport()?locksportInk():uiColors().ink),0);}}
    if(locksport()){lv_obj_set_style_bg_color(timerSecondary,color(uiColors().accent),0);lv_obj_set_style_text_color(lv_obj_get_child(timerSecondary,0),color(uiColors().onAccent),0);}
    if(locksportDark())for(auto* b:{timerSecondary,timerSplit}){
      lv_obj_set_style_bg_color(b,color(uiColors().surface),LV_STATE_DISABLED);
      lv_obj_set_style_color_filter_opa(b,LV_OPA_TRANSP,LV_STATE_DISABLED);
      lv_obj_set_style_opa(b,LV_OPA_50,LV_STATE_DISABLED);
    }
    updateTimer();
  }else if(page==Laps){
    if(locksport()){lv_obj_set_style_bg_color(root,color(locksportBg()),0);buildChronographFrame();}
    title(locksport()?"PICKING SPLITS":"LAPS","Latest 50 retained / split + total");body=scrollBody(91,178);
    char top[80],sub[70],split[40],total[40];
    for(size_t i=0;i<stopwatch.retained();++i){auto* lap=stopwatch.newest(i);durationText(split,sizeof(split),lap->split);durationText(total,sizeof(total),lap->total);snprintf(top,sizeof(top),"%s %lu    %s",locksport()?"SPLIT":"LAP",lap->number,split);snprintf(sub,sizeof(sub),"Total %s",total);row(body,top,sub,i,130);}
    if(!stopwatch.retained())label(body,locksport()?"START begins your attempt.\nSPLIT records a milestone.\nOPEN records and stops.\nReset clears this attempt.":"Start the timer, then tap LAP.\nLaps exclude paused time.\nTimer and laps reset at reboot.",8,22,240);
    button(root,"Back to timer",91,272,178,32,130);
    if(locksport())locksportColors(root);
  }else if(page==Clock){
    buildWatch();
  }else if(page==ClockSettings){
    buildClockSync();
  }else if(page==ClockWheels){
    buildClockWheels();
  }else if(page==ClockEdit){
    buildClockZone();
  }else if(page==Orientation){
    title("ORIENTATION","Saved angle / touch follows the display");body=scrollBody();int n=0;
    for(int r=0;r<4;++r){char text[50];snprintf(text,sizeof(text),"%s%d degrees",((4-displayRotation)&3)==r?LV_SYMBOL_OK "  ":"",r*90);row(body,text,"Clockwise / retained after restart",n++,144+r);}
  }else if(page==FamilyColors){
    title("DEVICE COLORS","Same family color in every theme");body=scrollBody();
    for(unsigned i=0;i<unsigned(Family::Count);++i)row(body,dragon::familyName(Family(i)),"Dot, leader and label",i,0,dragon::familyColor(Family(i)));
  }else if(page==RadarStyle){
    title("THEME & RADAR","Menus / utilities / radar");body=scrollBody();int n=0;
    for(unsigned i=0;i<RadarThemeCount;++i){if(i==dragon::PersonalThemeId&&!personalThemeReady())continue;String name=String(radarTheme==i?LV_SYMBOL_OK "  ":"")+radarThemeNames[i];row(body,name.c_str(),radarThemeDescriptions[i],n++,9000+i);}
    row(body,"Device color legend","Unique family colors in every theme",n++,150);
    row(body,radarLabels?"Device labels: ON":"Device labels: OFF",radarLabels?"Priority tags + tapped dot / all dots visible":"All dots together / tap for evidence",n++,121);
    row(body,radarRecent?"Recent signals: 2 minutes":"Recent signals: OFF","Stale dots show last-seen age, not live RSSI",n++,124);
  }else if(page==Display){
    title("DISPLAY");body=scrollBody();String b=String("Brightness: ")+brightness+"%";row(body,b.c_str(),"25 / 50 / 75 / 100%",0,44);
    String dim=detector.dimSeconds?String("Dim after ")+detector.dimSeconds+" seconds":"Auto dim: NEVER";row(body,dim.c_str(),"Never / 15 / 30 / 60 / 120 / 300 s",1,80);
    String level=String("Dim brightness: ")+detector.idleBrightness+"%";row(body,level.c_str(),"5 / 8 / 15 / 25%",2,81);
    String off=detector.offSeconds?String("Screen off after ")+detector.offSeconds+" seconds":"Screen off: NEVER";row(body,off.c_str(),"Never / 30 / 60 / 120 / 300 / 600 s",3,82);
    row(body,detector.wakeAlert?"Wake on alert: ON":"Wake on alert: OFF","Touch or button also wakes the display",4,83);
    row(body,"Restore always-on defaults","100% brightness; no dimming or timeout",5,84);
  }else if(page==Diagnostics){
    title("RADIO HEALTH","Live reception and resource counters");body=scrollBody();diagnosticsLabel=label(body,"Reading...",6,0,244,F12);
  }else if(page==About){
    title("SIGNATURES & LIMITS");body=scrollBody();
    label(body,"DRAGON DETECTOR " DRAGON_VERSION "\nDetection / hunt edition\n\nGreen radar / signal proximity.\nAngles are decorative.\nNo distance or direction.\n\nAll contacts on one radar.\nAxon gets the primary tag.\nCameras / glasses / drones alert.\nTrackers stay silent as dots.\nTap a dot for its summary.\nDouble-tap for details / BLE / Hunt\nwith both radios listening.\n\nAirTag / Find My is a family\nmatch, not model or owner ID.\nBiscuit Pro / Ultra cannot be\ndistinguished from adverts.\nNo proof of malicious intent.\n\nHold badge: tools.\nAlways on at 100% by default.\nDisplay timeouts in Settings.\nUSB Badge Studio on GitHub.\n\nBy Your Pal Kal / Valleytech\nOriginal code: MIT.\nFlock Noir / Colonel Panic\nOUI Spy / Unified Blue\nMarauder: Just Call Me Koko\nSquachWatch feature reference\nOpenDroneID: Apache-2.0\nFull notices in CREDITS.md",5,0,244,F14);
  }
  micBadge=label(root,"MIC REC",137,17,86,F12,0xff8585);lv_obj_set_style_text_align(micBadge,LV_TEXT_ALIGN_CENTER,0);legible(micBadge);lv_obj_add_flag(micBadge,LV_OBJ_FLAG_HIDDEN);
}
static void action(lv_event_t* e){
  ++inputActions;lastInputAction=int(intptr_t(lv_event_get_user_data(e)));
  lastInteraction=millis();
  int id=(intptr_t)lv_event_get_user_data(e);
  if(id>=9000 && id<9000+int(RadarThemeCount)){selectRadarTheme(id-9000);go(RadarStyle);page=Boot;return;}
#if BOARD_HAS_SD
  if(id>=2000 && id<2096){int i=id-2000;if(i>=fileCount)return;if(files[i].dir){directory=files[i].path;go(Files);page=Boot;}else{String ext=files[i].path;ext.toLowerCase();if(ext.endsWith(".mp3")||ext.endsWith(".wav")){audioPlay(files[i].path);go(Audio);}else{
      xSemaphoreTake(sdMutex,portMAX_DELAY);File f=CardFS.open(files[i].path);String text;while(f && f.available() && text.length()<2048){char ch=f.read();text+=(ch=='\n'||ch=='\t'||(ch>=32&&ch<127))?ch:'.';}f.close();xSemaphoreGive(sdMutex);
      lv_obj_t* box=uiMessageBox(root,"FILE PREVIEW",text.c_str(),nullptr,true);lv_obj_set_size(box,S(260),S(225));lv_obj_center(box);
    }}return;}
#endif
  if(id>=6000 && id<6000+deviceRows){int i=findContact(deviceKeys[id-6000]);if(i>=0){selectContact(i);go(Details);}else{uiNotify("Target expired","Refresh the list or reopen it from History.",nullptr,false);}return;}
  if(id>=1000 && id<1000+(int)MAX_CONTACTS){selectContact(id-1000);go(Details);return;}
  if(id>=3000&&id<3000+historyRows){for(size_t i=0;i<historyCount;++i)if(historyRecords[i].key==historyKeys[id-3000]&&historyRecords[i].family==historyFamilies[id-3000]){historySelection=historyRecords[i];go(HistoryDetail);return;}go(History);page=Boot;return;}
  if(id>=4002&&id<4000+int(Family::Count)){detector.families^=1u<<(id-4000);detectorSave();go(Detectors);page=Boot;return;}
  if(id>=5000&&id<5000+int(RULE_LIMIT)){controlKey=ruleKeys[id-5000];controlChannel=0;go(Controls);return;}
  if(id>=8000&&id<8010){unsigned row=id-8000;if(row<notebookRows&&notebookAdverts){auto& k=notebookKeys[row];for(unsigned i=0;i<dragon::notebook::Capacity;++i){auto& a=notebookAdverts[i];if(a.used&&a.type==k.addressType&&!memcmp(a.mac,k.mac,6)){notebookSelection=a;go(NotebookDetail);return;}}}go(NotebookBle);return;}
  if(id>=201 && id<=211){trackedContact=-1;channelLock=id-200;setRadio(true);go(Channels);page=Boot;return;}
#ifndef BOARD_RADAR_LITE
  if(id==7104||id==7105){reconPage(id==7104?"files":"captures");go(Recon);return;}
  if(id==7100||id==7102){openRadioTools=id==7102;go(Recon);return;}
  if(id==7103){reconPage("attacks");go(Recon);return;}
  if(id==7000){go(BleLab);return;}
  if(id==7001||id==7002){if(selectionValid()&&contacts[selected].ble){auto& c=contacts[selected];
    if(millis()-c.seen>=30000)uiNotify("Device is stale","Return to radar and select a fresh signal before connecting.",nullptr,false);
    else if(bleLabOpenDevice(c.mac,c.addressType,c.name,id==7002))go(BleLab);
    else uiNotify("Radio tools busy","Stop the current session and wait for saving.",nullptr,false);
  }else uiNotify("Device expired","Return to radar and select the device again.",nullptr,false);return;}
#endif
  switch(id){
    case 150:go(FamilyColors);break;
    case 161:editOffset=clockOffsetMinutes;go(ClockEdit);break;
    case 160:clockTab(0);break;
    case 162:clockTab(1);break;
    case 163:clockTab(2);break;
    case 164:go(Settings);break;
    case 171:configureClockOffset(editOffset);clockTab(2,true);break;
    case 130:go(Stopwatch);break;
    case 131:if(!resetBox){stopwatch.toggle(stopwatchNow());updateTimer();}break;
    case 132:if(!resetBox){if(stopwatch.running){if(locksport())stopwatch.finish(stopwatchNow());else stopwatch.lap(stopwatchNow());}else resetStopwatch();updateTimer();}break;
    case 168:if(!resetBox&&stopwatch.running){stopwatch.lap(stopwatchNow());updateTimer();}break;
    case 133:go(Clock);break;
    case 134:go(Laps);break;
    case 135:go(Orientation);break;
    case 136:clockTab(0,true);break;
    case 137:case 138:editOffset=constrain(editOffset+(id==138?15:-15),-720,840);updateClockZone();break;
    case 141:saveClockTab();break;
    case 144:case 145:case 146:case 147:configureOrientation((id-144)*90);go(Orientation);page=Boot;break;
    case 1:go(Radar);break;case 2:go(Menu);break;
    case 10:case 11:case 12:menuTab=id-10;go(Menu);page=Boot;break;
    case 20:filter=1;devicePage=0;go(Devices);break;case 21:filter=2;devicePage=0;go(Devices);break;case 22:go(Alerts);break;case 23:go(Channels);break;
    case 25:page=Boot;go(Devices);break;
#if BOARD_HAS_SD
    case 24:setCapture(!captureEnabled);go(Settings);page=Boot;break;
#endif
    case 31:go(Audio);break;
#if BOARD_HAS_SD
    case 30:directory="/music";go(Files);break;case 32:directory="/";go(Files);break;
#endif
    case 33:audioRecord();break;case 34:audioStop();break;case 35:audioVolume(audioStatus().volume-10);break;case 36:audioVolume(audioStatus().volume+10);break;
#if BOARD_HAS_SD
    case 39:directory=directory.substring(0,directory.lastIndexOf('/'));if(directory.isEmpty())directory="/";go(Files);page=Boot;break;
#endif
    case 40:go(Settings);break;case 41:go(About);break;case 42:setRadio(true);pending=page;page=Boot;break;
#if BOARD_HAS_SD
    case 43:loggingEnabled=!loggingEnabled;pending=page;page=Boot;break;
#endif
    case 44:brightness=brightness==100?25:brightness+25;settings.putUChar("brightness",brightness);outputBrightness=-1;pending=page;page=Boot;break;
    case 45:if(selectionValid() && !contacts[selected].ble && contacts[selected].channel){trackedContact=-1;channelLock=contacts[selected].channel;setRadio(true);go(Channels);}break;
#if BOARD_HAS_SD
    case 46:setCapture(!captureEnabled);pending=Details;page=Boot;break;
#endif
    case 50:go(Badge);break;
    case 52:if(selectionValid())trackContact(selected);go(Hunt);break;
    case 53:candidatesOnly=!candidatesOnly;settings.putBool("candidates",candidatesOnly);pending=page;page=Boot;break;
    case 54:trackContact(-1);go(Radar);break;
    case 55:badgeCommand("badge boot off");go(Radar);break;
    case 56:if(selectionValid()){controlKey=contactKey(contacts[selected]);controlChannel=contacts[selected].channel;go(Controls);}break;
    case 57:go(Hunt);break;
    case 58:if(trackedContact>=0&&millis()-contacts[trackedContact].seen<10000)contacts[trackedContact].trace.mark=contacts[trackedContact].rssi;break;
    case 59:if(trackedContact>=0){selectContact(trackedContact);go(Details);}break;
    case 60:historyPage=0;go(History);break;
    case 61:go(Detectors);break;
    case 151:axonPriority=!axonPriority;detectorSave();go(Detectors);page=Boot;break;
    case 62:detector.minimum=detector.minimum==Confidence::Strong?Confidence::Low:Confidence(unsigned(detector.minimum)+1);detectorSave();go(Detectors);page=Boot;break;
    case 64:go(Rules);break;
    case 65:case 66:case 67:if(!toggleRule(controlKey,id==65?RuleWatch:id==66?RuleIgnore:RuleSnooze)){uiNotify("Rule not saved","Rule list is full or storage failed. Remove an existing rule and retry.",nullptr,false);}go(Controls);page=Boot;break;
    case 68:huntDevice(controlKey,controlChannel);go(Hunt);break;
    case 69:controlKey=historySelection.key;controlChannel=historySelection.channel;go(Controls);break;
    case 70:huntDevice(historySelection.key,historySelection.channel);go(Hunt);break;
    case 71:go(Display);break;
    case 72:detector.autoQuiet=detector.autoQuiet==0?5:detector.autoQuiet==5?10:0;detectorSave();go(Settings);page=Boot;break;
    case 73:detector.quiet=!detector.quiet;detectorSave();go(Settings);page=Boot;break;
    case 74:detector.history=!detector.history;detectorSave();go(Settings);page=Boot;break;
    case 75:detector.privacy=!detector.privacy;detectorSave();go(Settings);page=Boot;break;
    case 76:detector.smoothingMs=detector.smoothingMs==600?1800:detector.smoothingMs==1800?4000:600;detectorSave();go(Settings);page=Boot;break;
    case 77:setFoxTransmit(!foxTransmit());go(Settings);page=Boot;break;
    case 85:{const int levels[]={0,10,25,50,75,100};int next=0;for(int v:levels)if(v>audioStatus().volume){next=v;break;}audioVolume(next);go(Audio);page=Boot;break;}
    case 86:huntSound=!huntSound;audioSave();go(Audio);page=Boot;break;
    case 87:audioFocus=audioFocus<0?int(Family::Axon):audioFocus==int(Family::Axon)?int(Family::Flock):audioFocus==int(Family::Flock)?int(Family::Meta):-1;audioSave();go(Audio);page=Boot;break;
    case 88:audioCommand("tone play 3");break;
    case 94:audioCommand("tone play 4");break;
    case 95:uiNotify("USB TONE EDITOR","Open Detector tools on the Dragon Detector web flasher.",nullptr,false);break;
    case 78:alertSound=!alertSound;audioSave();audioStop();go(Audio);page=Boot;break;
    case 79:alertVibration=!alertVibration;detectorSave();go(Settings);page=Boot;break;
    case 80:{const uint16_t choices[]={0,15,30,60,120,300};size_t i=0;while(i<5&&choices[i]!=detector.dimSeconds)++i;detector.dimSeconds=choices[(i+1)%6];detectorSave();go(Display);page=Boot;break;}
    case 81:detector.idleBrightness=detector.idleBrightness==5?8:detector.idleBrightness==8?15:detector.idleBrightness==15?25:5;detectorSave();go(Display);page=Boot;break;
    case 82:{const uint16_t choices[]={0,30,60,120,300,600};size_t i=0;while(i<5&&choices[i]!=detector.offSeconds)++i;detector.offSeconds=choices[(i+1)%6];detectorSave();go(Display);page=Boot;break;}
    case 83:detector.wakeAlert=!detector.wakeAlert;detectorSave();go(Display);page=Boot;break;
    case 84:brightness=100;settings.putUChar("brightness",brightness);detector.dimSeconds=detector.offSeconds=0;detectorSave();outputBrightness=-1;go(Display);page=Boot;break;
    case 89:dismissNotice();break;
    case 90:if(noticeHasKey){auto key=noticeKey;dismissNotice();huntDevice(key);go(Hunt);}break;
    case 91:if(noticeHasKey){if(!(deviceRules(noticeKey)&RuleSnooze))toggleRule(noticeKey,RuleSnooze);dismissNotice();}break;
    case 96:filter=0;devicePage=0;go(Devices);break;
    case 97:detector.quiet=!detector.quiet;detectorSave();audioStop();go(Radar);page=Boot;break;
    case 98:setActiveBle(!activeBle);go(Settings);page=Boot;break;
    case 99:go(Diagnostics);break;
    case 153:go(Notebook);page=Boot;break;
    case 154:notebookPage=0;go(NotebookProbes);break;
    case 155:notebookPage=0;go(NotebookBle);break;
    case 156:if(notebookPage)--notebookPage;pending=page;page=Boot;break;
    case 157:++notebookPage;pending=page;page=Boot;break;
    case 158:pending=page;page=Boot;break;
    case 114:if(devicePage>0)--devicePage;go(Devices);page=Boot;break;
    case 115:if((devicePage+1)*10<deviceTotal)++devicePage;go(Devices);page=Boot;break;
    case 116:go(Activity);break;
    case 117:go(RadarStyle);break;
    case 159:if(radarHasSelection){int index=findContact(radarSelectedKey);if(index>=0){selectContact(index);go(Details);}}break;
    case 152:if(radarPrimary>=0&&contacts[radarPrimary].used){selectContact(radarPrimary);go(Details);}else if(trackedContact>=0)go(Hunt);break;
    case 121:radarLabels=!radarLabels;settings.putBool("radar-labels",radarLabels);go(RadarStyle);page=Boot;break;
    case 124:radarRecent=!radarRecent;settings.putBool("radar-recent",radarRecent);go(RadarStyle);page=Boot;break;
    case 110:if(historyPage>0)--historyPage;go(History);page=Boot;break;
    case 111:if(size_t((historyPage+1)*10)<historyCount)++historyPage;go(History);page=Boot;break;
    case 112:{static const char* buttons[]={"Cancel","Clear",""};auto* box=uiMessageBox(root,"CLEAR HISTORY?","Erase stored detections on this device? Active scanning can create new records.",buttons,true);lv_obj_set_width(box,S(260));lv_obj_center(box);lv_obj_add_event_cb(box,[](lv_event_t* event){auto* b=lv_event_get_current_target(event);if(lv_msgbox_get_active_btn(b)==1)historyClear();lv_msgbox_close(b);go(History);page=Boot;},LV_EVENT_VALUE_CHANGED,nullptr);break;}
    case 200:trackContact(-1);pending=page;page=Boot;break;
  }
}
bool uiTouch(bool pressed,int,int){
  static bool consumeWake=false;
  if(pressed && outputBrightness==0)consumeWake=true;
  if(pressed)lastInteraction=millis();
  if(consumeWake){if(!pressed)consumeWake=false;return false;}
  // Navigation uses visible Tools/Home buttons. Leave drags to LVGL lists.
  return true;
}
#ifdef BOARD_CROWPANEL_21
lv_obj_t* uiRotaryScope(){return resetBox?resetBox:clockPrompt?clockPrompt:notice?notice:root;}
bool uiRotaryWake(){bool wasOff=outputBrightness==0;lastInteraction=millis();return wasOff;}
#endif
void uiShow(const String& name){if(name=="startup"){go(Startup);page=Boot;return;}if(name=="radar")recipeStop();
#ifndef BOARD_RADAR_LITE
  if(name=="modes"||name=="tools"){openRadioTools=name=="tools";go(Recon);return;}
  if(name=="findmy-beeper"){bleLabBeeperPage();go(BleLab);return;}
  if(name=="lab"){go(BleLab);return;}
  if(name=="wardrive"||name=="ble-attacks"||name=="ibeacon"||name=="attacks"||name=="missions"||name=="recipes"||name=="network"||name=="csi"||name=="portal"||name=="analysis"||name=="probes"||name=="filters"||name=="captures"){reconPage(name);go(Recon);return;}
#endif
  if(name=="files"){
#if BOARD_HAS_SD
    go(Files);
#else
    reconPage("files");go(Recon);
#endif
    return;
  }
  if(name=="utilities"){menuTab=1;go(Menu);return;}
  if(name=="notebook"){go(Notebook);return;}if(name=="notebook-ble"){notebookPage=0;go(NotebookBle);return;}if(name=="notebook-probes"){notebookPage=0;go(NotebookProbes);return;}
  if(name=="device-colors"){go(FamilyColors);return;}
  if(name=="clock-settings"||name=="clock-edit"||name=="time-settings"){clockTab(0,true);return;}
  if(name=="date-settings"){clockTab(1,true);return;}if(name=="clock-sync"){clockTab(2,true);return;}
  if(name=="stopwatch"){go(Stopwatch);return;}if(name=="clock"){go(Clock);return;}if(name=="orientation"){go(Orientation);return;}if(name=="laps"){go(Laps);return;}
  if(name=="radar-style"){go(RadarStyle);return;}
  lastInteraction=millis();
  if(name=="activity"){go(Activity);return;}
  if(name=="badge")go(Badge);else if(name=="radar")go(Radar);else if(name=="menu")go(Menu);else if(name=="wifi"){filter=1;go(Devices);}else if(name=="ble"){filter=2;go(Devices);}else if(name=="audio")go(Audio);else if(name=="alerts")go(Alerts);else if(name=="channels")go(Channels);else if(name=="about")go(About);else if(name=="settings")go(Settings);else if(name=="hunt")go(Hunt);else if(name=="detectors")go(Detectors);else if(name=="history")go(History);else if(name=="display")go(Display);else if(name=="health")go(Diagnostics);else if(name=="targets"){filter=0;devicePage=0;go(Devices);}
}
void uiPersonalThemeChanged(){selectRadarTheme(dragon::PersonalThemeId);go(Clock);page=Boot;}
void uiPersonalThemeRemoved(){if(radarTheme==dragon::PersonalThemeId)selectRadarTheme(1);if(watchFace==dragon::PersonalWatchId){watchFace=0;settings.putUChar("watch-face",watchFace);}go(Radar);page=Boot;}
void uiBegin(){
  if(!prepareThemeLogos())Serial.println("THEME_LOGOS PSRAM allocation failed / fallback logos");
  settings.begin("dd-ui",false);
  watchFace=settings.getUChar("watch-face",0);if(!watchFaceAllowed(watchFace)){watchFace=0;settings.putUChar("watch-face",watchFace);}
  if(!settings.getBool("panel-v04",false)){settings.putUChar("brightness",100);settings.putBool("panel-v04",true);}
  brightness=settings.getUChar("brightness",100);if(brightness<25||brightness>100)brightness=100;candidatesOnly=settings.getBool("candidates",true);
  if(!settings.getBool("radar-v07",false)){settings.putBool("radar-labels",true);settings.putUChar("radar-theme",1);settings.putBool("radar-v07",true);}
  radarLabels=settings.getBool("radar-labels",true);radarRecent=settings.getBool("radar-recent",true);radarTheme=settings.getUChar("radar-theme",1);if(radarTheme>=RadarThemeCount||(radarTheme==dragon::PersonalThemeId&&!personalThemeReady()))radarTheme=1;
  scr_lvgl_init();uiApplyTheme(radarTheme);set_brightness(brightness);lastInteraction=millis();root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,color(uiColors().bg),0);
#ifndef BOARD_RADAR_LITE
  pinMode(BTN_PIN,INPUT_PULLUP);
#endif
  drawStartup();
  pageAt=millis();
}
void uiLoop(){
  uint32_t idle=millis()-lastInteraction;
  int level=detector.offSeconds&&idle>=uint32_t(detector.offSeconds)*1000?0:detector.dimSeconds&&idle>=uint32_t(detector.dimSeconds)*1000?detector.idleBrightness:brightness;
  if(level!=outputBrightness){outputBrightness=level;set_brightness(level);}
  if(notice&&!noticeWatch&&millis()-noticeAt>=6000)dismissNotice();
  if(!clockPrompt&&!notice&&page!=Boot&&page!=Startup&&page!=BleLab&&page!=Recon&&page!=Stopwatch&&page!=Clock&&page!=ClockSettings&&page!=ClockWheels&&page!=ClockEdit){dragon::DetectionNotice next;if(noticeQueue.take(millis(),next)&&detector.enabled(next.family)&&(!detector.quiet||next.watched)&&(!next.hasKey||!(deviceRules(next.key)&(RuleIgnore|RuleSnooze))))uiNotify(next.title,next.detail,next.hasKey?&next.key:nullptr,next.watched);}
  if(((page==Boot&&pending==Boot)||(page==Startup&&pending==Startup)) && millis()-pageAt>2200)pending=Radar;
  if(page!=pending)build();
  clockSetupPrompt();
#ifndef BOARD_RADAR_LITE
  if(page==BleLab)bleLabViewLoop();
  if(page==Recon)reconViewLoop();
#endif
  if(millis()-lastUpdate>(page==Stopwatch?50:100)){lastUpdate=millis();if(micBadge){if(audioStatus().mode==AudioMode::Recording)lv_obj_clear_flag(micBadge,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(micBadge,LV_OBJ_FLAG_HIDDEN);}
    if(page==Stopwatch)updateTimer();
    if(page==Clock)watchTick();
    if(page==ClockSettings)updateClockStatus();
    if(page==Radar && radar){
      radarFrameAt=millis();
      if(radarSummary){int index=radarHasSelection&&millis()-radarSelectedAt<10000?findContact(radarSelectedKey):-1;
        if(index>=0){const auto& c=contacts[index];char summary[140];
          snprintf(summary,sizeof(summary),"%s\n%d dBm / double-tap: details",radarFamilyName(c.match.family),c.rssi);
          lv_label_set_text(radarSummary,summary);lv_obj_set_style_text_color(radarSummary,color(familyColor(c.match.family)),0);lv_obj_clear_flag(radarSummary,LV_OBJ_FLAG_HIDDEN);
        }else lv_obj_add_flag(radarSummary,LV_OBJ_FLAG_HIDDEN);
      }
      bool deauth=detector.enabled(Family::Deauth)&&trafficStatus.disconnectActive(millis()),spam=detector.enabled(Family::BleSpam)&&trafficStatus.bleActive(millis());
      lv_label_set_text(radarActivity,deauth&&spam?"DEAUTH + BLE BURST  >":deauth?"DEAUTH ACTIVITY  >":spam?"BLE SPAM CANDIDATE  >":"");
      if(deauth||spam)lv_obj_clear_flag(radarActivity,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(radarActivity,LV_OBJ_FLAG_HIDDEN);
      if(trackedContact>=0 && trackingLabel){auto& t=contacts[trackedContact];uint32_t age=millis()-t.seen;String line=age>10000?"SIGNAL LOST / HUNT":String("HUNT / ")+t.signal.direction();lv_label_set_text(trackingLabel,line.c_str());}
      else if(trackingLabel){char primary[64];if(radarPrimary>=0){auto& c=contacts[radarPrimary];snprintf(primary,sizeof(primary),"%s / %d dBm",radarFamilyName(c.match.family),c.rssi);lv_obj_set_style_text_color(trackingLabel,color(familyColor(c.match.family)),0);}else{strlcpy(primary,reconMode()==dragon::ReconMode::Radar?"TAP A DOT FOR ITS TAG":dragon::reconModes[unsigned(reconMode())].name,sizeof(primary));lv_obj_set_style_text_color(trackingLabel,color(radarPalette(radarTheme).text),0);}lv_label_set_text(trackingLabel,primary);}
      if(locksport()&&trackingLabel){if(trackedContact>=0)lv_obj_set_style_text_color(trackingLabel,color(familyColor(contacts[trackedContact].match.family)),0);lv_obj_set_style_bg_color(trackingLabel,color((radarPrimary>=0||trackedContact>=0)?dragon::RadarLabelBackground:locksportBg()),0);lv_obj_set_style_bg_opa(trackingLabel,LV_OPA_COVER,0);lv_obj_set_style_radius(trackingLabel,S(5),0);}
      lv_obj_invalidate(radar);int count=0;for(auto& c:contacts)if(radarVisible(c,millis()))++count;char s[96];int n=snprintf(s,sizeof(s),"%s %s / %d %s",radioEnabled?(blePhase?"BLE":"WI-FI"):"PAUSED",captureEnabled?"PCAP":"RX",count,count==1?"target":"targets");
      int battery=batteryPercent();if(battery>=0)snprintf(s+n,sizeof(s)-n,"  /  %s%d%%",batteryCharging()?LV_SYMBOL_CHARGE " ":"",battery);
#ifdef BOARD_XIAO_ROUND
      else snprintf(s+n,sizeof(s)-n," / %d%%",brightness);
#endif
      lv_label_set_text(statusLabel,s);}
    if(page==Diagnostics&&diagnosticsLabel){
      char text[360];snprintf(text,sizeof(text),"Wi-Fi frames: %lu\nBLE adverts: %lu\nQueue drops: %lu / peak %u\nMax processing: %lu us\nMax queue age: %lu ms\nContact slots: %u\nEvictions: %lu\nCache hits/misses: %lu/%lu\nFree heap: %u KB\nFree PSRAM: %u KB\nBLE: %s\n\nCounters restart at boot.\nHigher queue age means delayed\nRSSI and scan results.",wifiFrames,bleFrames,queueDrops,radioQueueHigh,radioMaxBatchUs,radioMaxLagMs,unsigned(MAX_CONTACTS),contactEvictions,contactCacheHits,contactCacheMisses,ESP.getFreeHeap()/1024,ESP.getFreePsram()/1024,activeBle?"Active / requests names":"Passive / listen only");lv_label_set_text(diagnosticsLabel,text);
    }
    if(page==Activity&&activityLabel){
      char text[600],wifiMeta[100],bleMeta[120];auto& t=trafficStatus;unsigned recent=recentDisconnects();
      if(t.disconnectAlerts){snprintf(wifiMeta,sizeof(wifiMeta),"Last: CH %u / %s\nReason: %s",t.channel,t.disconnectActive(millis())?"RECENT":"older burst",t.protectedFrame?"protected / unavailable":"");if(!t.protectedFrame)snprintf(wifiMeta+strlen(wifiMeta),sizeof(wifiMeta)-strlen(wifiMeta),"%u",t.reason);}else strcpy(wifiMeta,"No bursts observed.");
      if(t.bleAlerts)snprintf(bleMeta,sizeof(bleMeta),"%s / %s\n%u addresses / %u variants",dragon::bleBurstName(t.bleGroup),t.bleActive(millis())?"RECENT":"older burst",t.bleAddresses,t.bleVariants);else strcpy(bleMeta,"No bursts observed.");
      snprintf(text,sizeof(text),"WI-FI DEAUTH / DISASSOC\n%lu frames / %lu burst alerts\n%u%s frames in last 3s\n%s\n\nBLE SPAM CANDIDATES\n%lu burst alerts\n%s\n\nThresholds: 6 disconnects / 3s;\n12 BLE addresses / 5s, or\n24 pairing variants / 5s.\nRetries/identical adverts filtered.\nScan gaps can hide activity.\nCrowding or normal disconnects\ncan resemble attacks.\nEvents retain the evidence.",t.disconnectFrames,t.disconnectAlerts,recent,recent==64?"+":"",wifiMeta,t.bleAlerts,bleMeta);
      lv_label_set_text(activityLabel,text);
    }
    if(page==Hunt&&trackedContact>=0&&huntValue){auto& c=contacts[trackedContact];uint32_t age=millis()-(c.signal.count?c.signal.last:c.seen);bool stale=age>=10000||!c.signal.count;
      char value[52],meta[128];if(stale)snprintf(value,sizeof(value),"%s",c.packets?"SIGNAL LOST":"SEARCHING");else snprintf(value,sizeof(value),"%d dBm  %s",c.rssi,c.signal.trend>0?LV_SYMBOL_UP:c.signal.trend<0?LV_SYMBOL_DOWN:"-");lv_label_set_text(huntValue,value);lv_bar_set_value(huntGauge,stale?-105:c.rssi,LV_ANIM_OFF);
      if(stale)snprintf(meta,sizeof(meta),"Last heard %lus ago\n%s",(unsigned long)(age/1000),c.ble?"Listening for BLE target":huntSweep(c,millis())?"Searching Wi-Fi channels 1-11":"Listening on saved channel");
      else if(c.trace.mark>-127)snprintf(meta,sizeof(meta),"%s / peak %d dBm\n%+d dB from mark / %s",c.signal.direction(),c.trace.peak,c.rssi-c.trace.mark,c.signal.quality());
      else snprintf(meta,sizeof(meta),"%s / peak %d dBm\n%s / MARK to compare",c.signal.direction(),c.trace.peak,c.signal.quality());
      if(!stale){size_t n=strlen(meta);snprintf(meta+n,sizeof(meta)-n,"\nRaw %d dBm / last %lus",c.signal.raw,(unsigned long)(age/1000));}
      lv_label_set_text(huntMeta,meta);lv_obj_invalidate(huntGraph);
    }
    if(page==Audio && audioLabel){auto s=audioStatus();String text=String(s.mode==AudioMode::Recording?"RECORDING":s.mode==AudioMode::Playing?"PLAYING":s.mode==AudioMode::Error?"ERROR":"READY")+"  "+s.seconds+"s  Vol "+s.volume+"%\n"+s.message;lv_label_set_text(audioLabel,text.c_str());lv_bar_set_value(audioMeter,s.peak,LV_ANIM_OFF);}
  }
#ifdef BOARD_CROWPANEL_21
  rotaryUiLoop();
#endif
  lv_timer_handler();
}

void uiKey(bool home){lastInteraction=millis();go(home?Radar:page==Menu?Badge:Menu);}
bool uiStopwatchKey(bool start,uint64_t at){
  if(page!=Stopwatch||pending!=Stopwatch)return false;lastInteraction=millis();
  if(resetBox)return true;
  if(!at)at=stopwatchNow();if(start)stopwatch.toggle(at);else if(stopwatch.running){if(locksport())stopwatch.finish(at);else stopwatch.lap(at);}else resetStopwatch();
  updateTimer();return true;
}
bool uiCommand(const String& cmd){
  if(cmd=="clock pig"){
    if(watchFace!=27||page!=Clock){Serial.println("CLOCK_PIG_ERROR select the Pig watch");return true;}
    dragon::Calendar d;clockLocal(d);Serial.printf("CLOCK_PIG x=%d target=%d look=%d reacting=%u time=%02d:%02d:%02d valid=%u\n",int(std::lround(pigX)),pigTarget,pigLook,pigReacting,d.hour,d.minute,d.second,wallTime()!=0);return true;
  }
  if(cmd=="clock pig left"||cmd=="clock pig right"){
    if(watchFace!=27||page!=Clock){Serial.println("CLOCK_PIG_ERROR select the Pig watch");return true;}
    pigReact(cmd.endsWith("left")?144:216);Serial.println("CLOCK_PIG_OK");return true;
  }
  if(cmd=="clock face"){Serial.printf("CLOCK_FACE index=%u name=%s\n",watchFace,watches[watchFace].name);return true;}
  if(cmd.startsWith("clock face ")){String value=cmd.substring(11);unsigned number=0;bool valid=value.length()>0&&value.length()<=2;for(unsigned i=0;i<value.length();++i){if(value[i]<'0'||value[i]>'9'){valid=false;break;}number=number*10+unsigned(value[i]-'0');}if(valid&&watchFaceAllowed(number)){watchFace=number;settings.putUChar("watch-face",watchFace);pending=Clock;page=Boot;Serial.println("CLOCK_FACE_OK");}else Serial.println("CLOCK_FACE_ERROR");return true;}

  if(cmd=="ui layout"){lv_obj_update_layout(root);Serial.printf("UI_LAYOUT count=%lu page=%u\n",lv_obj_get_child_cnt(root),unsigned(page));for(unsigned i=0;i<lv_obj_get_child_cnt(root);++i){auto* o=lv_obj_get_child(root,i);lv_area_t a;lv_obj_get_coords(o,&a);Serial.printf("UI_WIDGET %u x=%d y=%d w=%d h=%d hidden=%u opa=%u children=%lu type=%s\n",i,a.x1,a.y1,a.x2-a.x1+1,a.y2-a.y1+1,lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN),lv_obj_get_style_opa(o,0),lv_obj_get_child_cnt(o),lv_obj_check_type(o,&lv_label_class)?"label":lv_obj_check_type(o,&lv_btn_class)?"button":"object");}Serial.println("UI_LAYOUT_END");return true;}

  if(cmd=="ui status"){Serial.printf("UI_STATUS page=%u actions=%lu last_action=%d\n",unsigned(page),inputActions,lastInputAction);return true;}
  if(cmd=="radar status"){Serial.printf("RADAR_SETTINGS theme=%u labels=%u recent=%u pages=1 plotted=%u primary=%d\n",radarTheme,radarLabels,radarRecent,dotCount,radarPrimary);return true;}
  if(cmd=="radar scope all"||cmd=="radar scope candidates"){candidatesOnly=cmd.endsWith("candidates");settings.putBool("candidates",candidatesOnly);radarPrepared=0;Serial.printf("RADAR_SCOPE %s\n",candidatesOnly?"candidates":"all");return true;}
  if(cmd=="radar labels on"||cmd=="radar labels off"){radarLabels=cmd.endsWith("on");settings.putBool("radar-labels",radarLabels);Serial.printf("RADAR_LABELS %u\n",radarLabels);return true;}
  if(cmd=="radar recent on"||cmd=="radar recent off"){radarRecent=cmd.endsWith("on");settings.putBool("radar-recent",radarRecent);Serial.printf("RADAR_RECENT %u\n",radarRecent);return true;}
  if(cmd=="radar next"||cmd=="radar previous"){Serial.println("RADAR_ALL_CONTACTS no pages");return true;}
  if(cmd.startsWith("radar theme ")){for(unsigned i=0;i<RadarThemeCount;++i)if(cmd.substring(12)==radarThemeCommands[i]){if(i==dragon::PersonalThemeId&&!personalThemeReady()){Serial.println("RADAR_THEME_ERROR upload a personal theme first");return true;}selectRadarTheme(i);go(Radar);page=Boot;Serial.printf("RADAR_THEME %u\n",radarTheme);return true;}Serial.println("RADAR_THEME_ERROR unknown theme");return true;}
  if(cmd=="display status"){Serial.printf("DISPLAY_SETTINGS brightness=%d output=%d dim=%u off=%u dim_level=%u wake=%u\n",brightness,outputBrightness,detector.dimSeconds,detector.offSeconds,detector.idleBrightness,detector.wakeAlert);return true;}
  if(cmd.startsWith("display set ")){
    unsigned b=0,dim=0,off=0,level=0;char extra;
    if(sscanf(cmd.c_str(),"display set %u %u %u %u %c",&b,&dim,&off,&level,&extra)!=4||b<25||b>100||dim>600||off>600||level>25){Serial.println("DISPLAY_ERROR");return true;}
    brightness=b;detector.dimSeconds=dim;detector.offSeconds=off;detector.idleBrightness=level;settings.putUChar("brightness",brightness);detectorSave();lastInteraction=millis();outputBrightness=-1;
    if(page==Display){pending=Display;page=Boot;}Serial.println("DISPLAY_OK");return true;
  }
  return false;
}
