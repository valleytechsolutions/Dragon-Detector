#include "app.h"
#include "board_pins.h"
#include "radar_layout.h"
#include <lvgl.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <Preferences.h>
#include <time.h>

extern void scr_lvgl_init();
extern void set_brightness(uint8_t);
extern const lv_img_dsc_t vts_logo;
extern const lv_img_dsc_t radar_logo;
// The layout is authored for the 360 px round panel. S() maps it onto the
// board's panel; label/button/row helpers apply it, so callers pass 360 px units.
#define S(v) ((v)*SCREEN_RES/360)
#if SCREEN_RES>=466
#define F12 (&lv_font_montserrat_16)
#define F14 (&lv_font_montserrat_18)
#define F16 (&lv_font_montserrat_20)
#define F20 (&lv_font_montserrat_26)
#define FCALLOUT (&lv_font_montserrat_14)
#else
#define F12 (&lv_font_montserrat_12)
#define F14 (&lv_font_montserrat_14)
#define F16 (&lv_font_montserrat_16)
#define F20 (&lv_font_montserrat_20)
// Two lines must fit the 26px inner height of a 360px-panel callout.
#define FCALLOUT (&lv_font_montserrat_10)
#endif
static const int CENTER=SCREEN_RES/2;
enum Page {Boot,Radar,Menu,Devices,Details,Alerts,Channels,Files,Audio,Settings,About,Badge,Hunt,Detectors,History,HistoryDetail,Rules,Controls,Display,Diagnostics,Activity,RadarStyle};
static Page page=Boot,pending=Boot;
static lv_obj_t *root,*body,*statusLabel,*audioLabel,*audioMeter,*radar,*micBadge;
static int menuTab=0,filter=0,selected=-1,filePage=0,brightness=100,historyPage=0;
static bool radarLabels=true;
static uint8_t radarTheme=1;
static unsigned radarPage=0;
static lv_obj_t *radarPrevious=nullptr,*radarNext=nullptr,*radarPageLabel=nullptr;
struct RadarPalette {uint32_t top,bottom,grid,ring,dot,sweep,text,panel;};
static const RadarPalette radarPalettes[]={
  {0x20b970,0x0c9553,0x095134,0xe0e5cd,0xffec50,0xb9ff72,0xfffae2,0x064a2e},
  {0x09291c,0x020c08,0x153d28,0x286c42,0xc0ff65,0x52dc84,0xdbf9c3,0x0b2419},
  {0x19130b,0x070707,0x3e301b,0xa88742,0xffd166,0xe54138,0xffe4a3,0x21190f}
};
static const char* radarThemeNames[]={"Dragon Ball","Green radar","Black / gold / red"};
static HistoryRecord historySelection;
static int devicePage=0,deviceRows=0,deviceTotal=0;
static dragon::DeviceKey deviceKeys[10];
static lv_obj_t* diagnosticsLabel=nullptr;
static lv_obj_t *activityLabel=nullptr,*radarActivity=nullptr;
static uint32_t radarPrepared=0;
static int radarOrder[MAX_CONTACTS],radarCount=0;
static dragon::DeviceKey historyKeys[10],controlKey,ruleKeys[RULE_LIMIT];
static uint8_t historyFamilies[10]{};
static uint8_t controlChannel=0;
static int historyRows=0;
static lv_obj_t *huntValue=nullptr,*huntMeta=nullptr,*huntGauge=nullptr,*huntGraph=nullptr,*notice=nullptr;
static dragon::DeviceKey noticeKey;static bool noticeHasKey=false,noticeWatch=false;static uint32_t noticeAt=0;
static uint8_t selectedMac[6]{};static bool selectedBle=false;static uint8_t selectedType=0;
static void selectContact(int i){selected=i;memcpy(selectedMac,contacts[i].mac,6);selectedBle=contacts[i].ble;selectedType=contacts[i].addressType;}
static bool selectionValid(){return selected>=0 && contacts[selected].used && !memcmp(selectedMac,contacts[selected].mac,6) && selectedBle==contacts[selected].ble && selectedType==contacts[selected].addressType;}
static lv_obj_t* trackingLabel=nullptr;
static uint32_t pageAt=0,lastUpdate=0,lastInteraction=0;
static int outputBrightness=-1;
static Preferences settings;
static String directory="/";
struct FileRow {String path;bool dir;uint32_t size;};static FileRow files[96];static int fileCount=0;static bool filesTruncated=false;
struct Dot {int x,y,index;dragon::DeviceKey key;int labelSlot=-1;};static Dot dots[MAX_CONTACTS];static int dotCount=0;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
static uint32_t familyColor(Family f){switch(f){case Family::Flock:return 0xff6878;case Family::Axon:return 0xf5b565;case Family::Meta:return 0xb8a0ff;case Family::Pineapple:return 0xffff70;case Family::Pwnagotchi:return 0xff86db;case Family::Nyanbox:return 0x80eaff;case Family::Halehound:return 0xff582f;case Family::Tracker:return 0x99abff;case Family::Skimmer:return 0xffbb88;case Family::Ble:return 0x83def0;default:return 0xfcdc58;}}
static lv_obj_t* label(lv_obj_t* parent,const char* text,int x,int y,int w=0,const lv_font_t* font=F14,uint32_t c=0xd9efe0){
  auto* o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_pos(o,S(x),S(y));if(w)lv_obj_set_width(o,S(w));lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,color(c),0);return o;
}
static void go(Page p){pending=p;}
static void action(lv_event_t* e);
static lv_obj_t* button(lv_obj_t* parent,const char* text,int x,int y,int w,int h,int id){
  auto* b=lv_btn_create(parent);lv_obj_set_pos(b,S(x),S(y));lv_obj_set_size(b,S(w),S(h));lv_obj_set_style_pad_all(b,S(8),0);lv_obj_set_style_bg_color(b,color(0x132b22),0);lv_obj_set_style_radius(b,S(13),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,color(0x31543a),0);lv_obj_set_style_shadow_width(b,0,0);
  lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* t=label(b,text,0,0,w-16);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(t);return b;
}
static void dismissNotice(){if(notice){lv_obj_del(notice);notice=nullptr;}}
void uiNotify(const char* text,const char* detail,const dragon::DeviceKey* key,bool watched){
  if(notice && (noticeWatch||!watched))return;
  dismissNotice();noticeAt=millis();noticeWatch=watched;noticeHasKey=key!=nullptr;if(key)noticeKey=*key;
  if(detector.wakeAlert)lastInteraction=millis();
  notice=lv_obj_create(lv_layer_top());lv_obj_set_size(notice,S(276),S(242));lv_obj_center(notice);lv_obj_clear_flag(notice,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(notice,color(0x10291c),0);lv_obj_set_style_border_color(notice,color(watched?0xffcf48:0x92e96d),0);lv_obj_set_style_border_width(notice,2,0);lv_obj_set_style_radius(notice,S(24),0);lv_obj_set_style_pad_all(notice,0,0);
  auto* heading=label(notice,text,18,16,240,F16,0xe6ed7a);lv_obj_set_style_text_align(heading,LV_TEXT_ALIGN_CENTER,0);
  String safe=detector.privacy?String("Detection observed. Open evidence for details."):String(detail);
  auto* message=label(notice,safe.c_str(),18,48,240,F12);lv_obj_set_height(message,S(88));
  if(key){button(notice,"HUNT",18,142,114,37,90);button(notice,"SNOOZE",142,142,114,37,91);}
  button(notice,"DISMISS",68,190,140,36,89);
}
static void title(const char* text,const char* subtitle=nullptr){
  auto* t=label(root,text,55,39,250,F20);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
  if(subtitle){auto* s=label(root,subtitle,55,67,250,F12,0x8caf99);lv_obj_set_style_text_align(s,LV_TEXT_ALIGN_CENTER,0);}
  button(root,LV_SYMBOL_HOME,149,307,62,33,1);
}
static void legible(lv_obj_t* o){lv_obj_set_style_bg_color(o,color(0x092317),0);lv_obj_set_style_bg_opa(o,LV_OPA_80,0);lv_obj_set_style_radius(o,7,0);}
static lv_obj_t* scrollBody(int y=91,int h=211){
  auto* o=lv_obj_create(root);lv_obj_set_pos(o,S(44),S(y));lv_obj_set_size(o,S(272),S(h));lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_scroll_dir(o,LV_DIR_VER);return o;
}
static void row(lv_obj_t* parent,const char* top,const char* bottom,int index,int id,uint32_t c=0xd9efe0){
  auto* b=button(parent,"",1,index*59,256,54,id);auto* heading=label(b,top,0,0,231,F14,c);auto* detail=label(b,bottom,0,22,231,F12,0x88a995);
  lv_label_set_long_mode(heading,LV_LABEL_LONG_DOT);lv_obj_set_height(heading,S(19));lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);lv_obj_set_height(detail,S(17));
}
static void line(lv_draw_ctx_t* ctx,int x1,int y1,int x2,int y2,uint32_t c,int width=1,int opa=255){
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=color(c);d.width=width;d.opa=opa;lv_point_t a{(lv_coord_t)x1,(lv_coord_t)y1},b{(lv_coord_t)x2,(lv_coord_t)y2};lv_draw_line(ctx,&d,&a,&b);
}
static void disc(lv_draw_ctx_t* ctx,int x,int y,int r,uint32_t c,int opa=255){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=LV_RADIUS_CIRCLE;d.bg_color=color(c);d.bg_opa=opa;lv_area_t a{(lv_coord_t)(x-r),(lv_coord_t)(y-r),(lv_coord_t)(x+r),(lv_coord_t)(y+r)};lv_draw_rect(ctx,&d,&a);
}
static void huntDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);auto* obj=lv_event_get_target(e);lv_area_t area;lv_obj_get_coords(obj,&area);
  for(int i=0;i<4;++i)line(ctx,area.x1,area.y1+S(i*14),area.x2,area.y1+S(i*14),0x244832);
  if(trackedContact<0)return;auto& trace=contacts[trackedContact].trace;
  uint32_t now=millis();
  for(unsigned i=0;i<48;++i){int current=trace.recent(i,now);if(current==dragon::SignalHistory::Gap)continue;int b=constrain((-current-25)*42/80,0,42);
    // Sparse advertisers may never fill adjacent buckets. Keep each received
    // sample visible without drawing across the missing reception between them.
    disc(ctx,area.x1+S(i*240/47),area.y1+S(b),S(1),0xb9f65e);
    int previous=i?trace.recent(i-1,now):dragon::SignalHistory::Gap;if(previous==dragon::SignalHistory::Gap)continue;int a=constrain((-previous-25)*42/80,0,42);
    line(ctx,area.x1+S((i-1)*240/47),area.y1+S(a),area.x1+S(i*240/47),area.y1+S(b),0xb9f65e,2);
  }
}
static const char* radarFamilyName(Family f){
  switch(f){case Family::Axon:return "Axon / TASER";case Family::Meta:return "Camera glasses";case Family::Tracker:return "Find My / DULT";case Family::GoogleTag:return "Find Hub";case Family::Skimmer:return "Serial module";case Family::Camera:return "Camera vendor";case Family::Alpr:return "ALPR vendor";case Family::Drone:return "Drone RID";case Family::Deauth:return "Deauth burst";case Family::BleSpam:return "BLE burst";case Family::EvilTwin:return "SSID conflict";default:return dragon::familyName(f);}
}
static void callout(lv_draw_ctx_t* ctx,const Dot& dot,unsigned slot,const char* text,uint32_t c){
  auto b=dragon::radarLabelSlot(slot);int x=S(b.x),y=S(b.y),w=S(b.w),h=S(b.h);
  int edgeX=constrain(dot.x,x,x+w),edgeY=constrain(dot.y,y,y+h);
  line(ctx,dot.x,dot.y,edgeX,edgeY,c,1,190);
  lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.radius=S(4);box.bg_color=color(radarPalettes[radarTheme].bottom);box.bg_opa=LV_OPA_90;
  box.border_color=color(c);box.border_opa=LV_OPA_50;box.border_width=1;
  lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w),(lv_coord_t)(y+h)};lv_draw_rect(ctx,&box,&area);
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(c);d.font=FCALLOUT;
  lv_area_t textArea{(lv_coord_t)(x+S(4)),(lv_coord_t)(y+S(3)),(lv_coord_t)(x+w-S(4)),(lv_coord_t)(y+h-S(3))};
  // Clip locally so an unusually long translated/family label cannot escape.
  lv_area_t clip;if(_lv_area_intersect(&clip,ctx->clip_area,&textArea)){const lv_area_t* saved=ctx->clip_area;ctx->clip_area=&clip;lv_draw_label(ctx,&d,&textArea,text,nullptr);ctx->clip_area=saved;}
}
static void radarDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);const auto& theme=radarPalettes[radarTheme];const uint32_t now=millis();
  disc(ctx,CENTER,CENTER,S(178),theme.grid);disc(ctx,CENTER,CENTER,S(176),theme.ring);
  lv_draw_rect_dsc_t glass;lv_draw_rect_dsc_init(&glass);glass.radius=LV_RADIUS_CIRCLE;
  glass.bg_color=color(theme.top);glass.bg_grad.dir=LV_GRAD_DIR_VER;glass.bg_grad.stops[0].color=glass.bg_color;glass.bg_grad.stops[1].color=color(theme.bottom);
  lv_area_t field{(lv_coord_t)S(9),(lv_coord_t)S(9),(lv_coord_t)(SCREEN_RES-S(9)-1),(lv_coord_t)(SCREEN_RES-S(9)-1)};lv_draw_rect(ctx,&glass,&field);
  static const int reaches[]={77,119,143,157,166,169,166,157,143,119,77};
  for(int n=0;n<11;++n){int axis=CENTER+S(n*30-150),reach=S(reaches[n]);line(ctx,axis,CENTER-reach,axis,CENTER+reach,theme.grid,S(1),180);line(ctx,CENTER-reach,axis,CENTER+reach,axis,theme.grid,S(1),180);}
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(theme.ring);arc.width=S(1);arc.opa=160;lv_point_t center{(lv_coord_t)CENTER,(lv_coord_t)CENTER};
  if(radarTheme)for(int radius=42;radius<169;radius+=42)lv_draw_arc(ctx,&arc,&center,S(radius),0,359);
  arc.width=S(2);lv_draw_arc(ctx,&arc,&center,S(170),0,359);
  // Sweep and pulse are animation, not measurements of bearing or RF activity.
  static uint32_t sweepAt=0;if(radioEnabled)sweepAt=now;
  float angle=(sweepAt%6000)*.00104719755f,dx=cosf(angle),dy=sinf(angle);
  lv_draw_rect_dsc_t trail;lv_draw_rect_dsc_init(&trail);trail.bg_color=color(theme.sweep);
  for(int n=0;n<18;++n){float nextX=dx*.998629535f+dy*.052335956f,nextY=dy*.998629535f-dx*.052335956f;
    lv_point_t wedge[]={{(lv_coord_t)CENTER,(lv_coord_t)CENTER},{(lv_coord_t)(CENTER+int(dx*S(166))),(lv_coord_t)(CENTER+int(dy*S(166)))},{(lv_coord_t)(CENTER+int(nextX*S(166))),(lv_coord_t)(CENTER+int(nextY*S(166)))}};
    trail.bg_opa=58-n*3;lv_draw_triangle(ctx,&trail,wedge);dx=nextX;dy=nextY;
  }
  line(ctx,CENTER,CENTER,CENTER+int(cosf(angle)*S(166)),CENTER+int(sinf(angle)*S(166)),theme.sweep,S(2),200);
  if(!radarPrepared || uint32_t(now-radarPrepared)>=250){
    radarPrepared=now;radarCount=0;
    for(size_t i=0;i<MAX_CONTACTS;++i)if(radarVisible(contacts[i],now))radarOrder[radarCount++]=i;
    std::sort(radarOrder,radarOrder+radarCount,[now](int a,int b){if(a==trackedContact||b==trackedContact)return a==trackedContact&&b!=trackedContact;
      bool staleA=uint32_t(now-contacts[a].seen)>=30000,staleB=uint32_t(now-contacts[b].seen)>=30000;if(staleA!=staleB)return !staleA;return preferContact(contacts[a],contacts[b],now);});
  }
  const unsigned pages=radarLabels?dragon::radarPages(radarCount):1;if(radarPage>=pages)radarPage=pages-1;
  dotCount=0;unsigned first=radarLabels?radarPage*dragon::RadarPageSize:0,last=radarLabels?std::min<unsigned>(first+dragon::RadarPageSize,radarCount):radarCount;
  for(unsigned k=first;k<last;++k){int i=radarOrder[k];auto& c=contacts[i];if(!radarVisible(c,now))continue;
    float target=c.signal.count?dragon::radarRadius(c.signal.smooth):158.f;if(!c.plotRadius)c.plotRadius=target;else c.plotRadius+=(target-c.plotRadius)*.35f;
    auto plot=dragon::radarPlotPoint(c.plotRadius,c.radarX,c.radarY);dots[dotCount++]={S(plot.x),S(plot.y),i,contactKey(c)};
  }
  if(radarLabels){dragon::RadarPoint points[dragon::RadarPageSize];uint8_t preferred[dragon::RadarPageSize];
    static dragon::DeviceKey labelKeys[dragon::RadarPageSize];static uint8_t slots[dragon::RadarPageSize];static int priorCount=0;static uint32_t labelsAt=0;bool changed=priorCount!=dotCount;
    for(int i=0;i<dotCount;++i){points[i]={dots[i].x*360/SCREEN_RES,dots[i].y*360/SCREEN_RES};preferred[i]=255;if(i>=priorCount||!(labelKeys[i]==dots[i].key))changed=true;for(int j=0;j<priorCount;++j)if(labelKeys[j]==dots[i].key)preferred[i]=slots[j];}
    if(changed||uint32_t(now-labelsAt)>=500){dragon::placeRadarLabels(points,dotCount,slots,preferred);labelsAt=now;priorCount=dotCount;for(int i=0;i<dotCount;++i)labelKeys[i]=dots[i].key;}
    for(int i=0;i<dotCount;++i){auto& c=contacts[dots[i].index];Family family=visibleFamily(c,now,detector.families);uint32_t age=now-c.seen;
      char state[24],text[64];if(age>=10000)snprintf(state,sizeof(state),"%s %lus AGO",c.match.confidence==Confidence::Low&&!trafficFresh(c,now)?"CLUE":"SEEN",(unsigned long)(age/1000));else snprintf(state,sizeof(state),"%s",c.match.confidence==Confidence::Low&&!trafficFresh(c,now)?"WEAK CLUE":age>=5000?"FADING":c.rssi>=-55?"STRONG":c.rssi>=-75?"MODERATE":"FAINT");
      dots[i].labelSlot=slots[i];snprintf(text,sizeof(text),"%s\n%s",radarFamilyName(family),state);callout(ctx,dots[i],slots[i],text,trafficFresh(c,now)?0xff7161:age>=30000?0xa49772:theme.dot);
    }
  }
  for(int i=0;i<dotCount;++i){auto& dot=dots[i];auto& c=contacts[dot.index];int x=dot.x,y=dot.y;uint32_t age=now-c.seen;uint8_t opacity=age>=30000?130:dragon::radarOpacity(age);
    if(age>=30000 || (c.match.confidence==Confidence::Low&&!trafficFresh(c,now))){auto clue=arc;clue.color=color(theme.dot);clue.opa=opacity;clue.width=2;lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};
      if(age>=30000)for(int a=0;a<360;a+=90)lv_draw_arc(ctx,&clue,&point,S(6),a,a+48);else lv_draw_arc(ctx,&clue,&point,S(5),0,359);
    }else{unsigned pulse=radioEnabled?unsigned((now+contactHash(contactKey(c)))%1800):900;pulse=pulse>900?1800-pulse:pulse;disc(ctx,x,y,S(9),theme.dot,unsigned(opacity)*(25+pulse/20)/255);disc(ctx,x,y,S(5),theme.dot,opacity);disc(ctx,x-S(1),y-S(1),S(2),0xfff8ab,opacity);}
    if(dot.index==trackedContact || (detector.enabled(c.trafficFamily)&&trafficFresh(c,now))){auto lock=arc;lock.color=color(dot.index==trackedContact?theme.text:0xff5040);lock.opa=220;lock.width=2;lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};lv_draw_arc(ctx,&lock,&point,S(12),0,359);}
  }
  // Decorative center cursor, not a compass. Keep it above the sweep/labels.
  for(int y=-7;y<=6;++y){int half=(y+7)/2;line(ctx,CENTER-S(half),CENTER+S(y),CENTER+S(half),CENTER+S(y),0xff6651,S(1)+1);}
}
static void radarTap(lv_event_t*){lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);int nearest=S(18)*S(18),idx=-1;for(int i=0;i<dotCount;++i){int d=(dots[i].x-p.x)*(dots[i].x-p.x)+(dots[i].y-p.y)*(dots[i].y-p.y);if(d<nearest && contactKey(contacts[dots[i].index])==dots[i].key){nearest=d;idx=dots[i].index;}}
  if(idx<0)for(int i=0;i<dotCount;++i)if(dots[i].labelSlot>=0&&contactKey(contacts[dots[i].index])==dots[i].key){auto b=dragon::radarLabelSlot(dots[i].labelSlot);if(p.x>=S(b.x)&&p.x<=S(b.x+b.w)&&p.y>=S(b.y)&&p.y<=S(b.y+b.h)){idx=dots[i].index;break;}}
  if(idx>=0){selectContact(idx);go(Details);}}
static void loadFiles(){
  fileCount=0;filesTruncated=false;if(!sdReady)return;
  xSemaphoreTake(sdMutex,portMAX_DELAY);File dir=SD_MMC.open(directory);
  if(dir&&dir.isDirectory()){File f=dir.openNextFile();while(f){if(fileCount>=96){filesTruncated=true;f.close();break;}String name=f.name();String path=directory=="/"?"/"+name:directory+"/"+name;files[fileCount++]={path,f.isDirectory(),uint32_t(f.size())};f.close();f=dir.openNextFile();}}dir.close();xSemaphoreGive(sdMutex);
}
static void build(){
  badgeUnload();page=pending;pageAt=millis();statusLabel=audioLabel=audioMeter=radar=trackingLabel=huntValue=huntMeta=huntGauge=huntGraph=diagnosticsLabel=activityLabel=radarActivity=nullptr;radarPrepared=0;
  radarPrevious=radarNext=radarPageLabel=nullptr;lv_obj_clean(root);lv_obj_set_style_bg_color(root,color(0x071510),0);
  if(page==Badge){lv_obj_set_style_bg_color(root,color(0x000000),0);badgeView(root);
  }else if(page==Radar){
    radar=lv_obj_create(root);lv_obj_remove_style_all(radar);lv_obj_set_pos(radar,0,0);lv_obj_set_size(radar,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(radar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(radar,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(radar,radarDraw,LV_EVENT_DRAW_MAIN,nullptr);lv_obj_add_event_cb(radar,radarTap,LV_EVENT_CLICKED,nullptr);
    auto* logo=lv_img_create(root);lv_img_set_src(logo,&radar_logo);lv_obj_align(logo,LV_ALIGN_TOP_MID,0,S(17));lv_obj_clear_flag(logo,LV_OBJ_FLAG_CLICKABLE);
    const auto& theme=radarPalettes[radarTheme];
    statusLabel=label(root,"LISTENING",56,66,248,F12,theme.text);lv_obj_set_style_text_align(statusLabel,LV_TEXT_ALIGN_CENTER,0);legible(statusLabel);lv_obj_set_style_bg_color(statusLabel,color(theme.panel),0);lv_obj_set_style_bg_opa(statusLabel,LV_OPA_60,0);
    radarActivity=label(root,"",75,84,210,F12,0xffbc76);lv_obj_set_style_text_align(radarActivity,LV_TEXT_ALIGN_CENTER,0);legible(radarActivity);lv_obj_add_flag(radarActivity,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(radarActivity,action,LV_EVENT_CLICKED,(void*)116);
    trackingLabel=label(root,"TAP A SIGNAL",70,280,220,F12,theme.text);lv_obj_set_style_text_align(trackingLabel,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(trackingLabel,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(trackingLabel,action,LV_EVENT_CLICKED,(void*)57);
    auto* targets=button(root,"TARGETS",98,301,82,32,96);
    auto* tools=button(root,"TOOLS",187,301,78,32,2);
    auto* quiet=button(root,detector.quiet?LV_SYMBOL_MUTE:LV_SYMBOL_VOLUME_MAX,270,257,38,34,97);
    radarPrevious=button(root,LV_SYMBOL_LEFT,86,258,32,24,122);radarNext=button(root,LV_SYMBOL_RIGHT,230,258,32,24,123);radarPageLabel=label(root,"",123,261,103,F12,theme.text);lv_obj_set_style_text_align(radarPageLabel,LV_TEXT_ALIGN_CENTER,0);
    for(auto* b:{targets,tools,quiet,radarPrevious,radarNext}){lv_obj_set_style_bg_color(b,color(theme.panel),0);lv_obj_set_style_bg_opa(b,LV_OPA_80,0);lv_obj_set_style_border_color(b,color(theme.ring),0);lv_obj_set_style_border_opa(b,LV_OPA_60,0);}
    for(auto* b:{radarPrevious,radarNext}){lv_obj_set_style_pad_all(b,0,0);lv_obj_add_flag(b,LV_OBJ_FLAG_HIDDEN);}lv_obj_add_flag(radarPageLabel,LV_OBJ_FLAG_HIDDEN);
  }else if(page==Menu){
    title("DRAGON TOOLS");const char* tabs[]={"RECON","BADGE","SYSTEM"};
    for(int i=0;i<3;++i){auto* b=button(root,tabs[i],47+i*89,76,86,32,10+i);if(menuTab==i)lv_obj_set_style_bg_color(b,color(0x36582b),0);}
    if(menuTab==0){button(root,"Wi-Fi",57,119,119,49,20);button(root,"Bluetooth",184,119,119,49,21);button(root,"History",57,176,119,49,60);button(root,"Hunt",184,176,119,49,57);button(root,"Detectors",57,233,119,45,61);button(root,"Activity",184,233,119,45,116);}
    if(menuTab==1){button(root,"Show my badge",59,120,242,48,50);button(root,"USB badge editor",59,178,242,48,51);button(root,LV_SYMBOL_DIRECTORY "  SD files",79,242,202,43,32);}

    if(menuTab==2){button(root,"Settings",60,126,240,48,40);button(root,"Signatures & limits",60,184,240,48,41);button(root,radioEnabled?"Pause radio":"Resume radio",79,243,202,44,42);}
  }else if(page==Devices){
    title(filter==0?"RADAR TARGETS":filter==1?"WI-FI CONTACTS":"BLE CONTACTS","Evidence first / strongest signal next");body=scrollBody(88,180);
    int order[MAX_CONTACTS];deviceTotal=0;uint32_t now=millis();
    for(size_t i=0;i<MAX_CONTACTS;++i){auto& c=contacts[i];if(!c.used || !c.packets || (filter==0&&!radarVisible(c,now)) || (filter==1&&c.ble)||(filter==2&&!c.ble)||(filter!=0&&now-c.seen>=60000))continue;order[deviceTotal++]=i;}
    std::sort(order,order+deviceTotal,[now](int a,int b){return preferContact(contacts[a],contacts[b],now);});
    devicePage=constrain(devicePage,0,deviceTotal?(deviceTotal-1)/10:0);deviceRows=0;
    for(int k=devicePage*10;k<deviceTotal&&deviceRows<10;++k){auto& c=contacts[order[k]];deviceKeys[deviceRows]=contactKey(c);
      char mac[18],sub[96];displayMac(c.mac,mac);auto flags=deviceRules(contactKey(c));
      snprintf(sub,sizeof(sub),"%d dBm / %lus / %s%s",c.rssi,(unsigned long)((now-c.seen)/1000),flags&RuleWatch?"WATCH / ":flags&RuleIgnore?"IGNORED / ":c.match.confidence==Confidence::Low?"CLUE / ":"",dragon::familyName(visibleFamily(c,now,detector.families)));
      row(body,*c.name?displayName(c.name).c_str():mac,sub,deviceRows,6000+deviceRows,familyColor(visibleFamily(c,now,detector.families)));++deviceRows;
    }
    if(!deviceRows)label(body,"Listening...\nNo recent targets",10,30,230);
    button(root,"<",68,274,40,31,114);char pages[30];snprintf(pages,sizeof(pages),"%d/%d",devicePage+1,std::max(1,(deviceTotal+9)/10));label(root,pages,118,280,60,F12);
    button(root,">",180,274,40,31,115);button(root,LV_SYMBOL_REFRESH,230,274,50,31,25);
  }else if(page==Details){
    title("CONTACT EVIDENCE");body=scrollBody(88,169);if(selectionValid()){auto& c=contacts[selected];char mac[18];displayMac(c.mac,mac);
      String s=(*c.name?displayName(c.name):String(mac))+"\n"+mac+"\n\n"+dragon::familyName(c.match.family)+"\n"+dragon::confidenceName(c.match.confidence)+"\n"+(detector.privacy?"Evidence hidden by privacy mode":c.match.evidence)+"\n\n"+String(c.rssi)+" dBm  |  "+(c.ble?"BLE":String("CH ")+c.channel)+"\n"+c.signal.quality()+" / "+c.signal.direction()+"\nLast seen "+String((millis()-c.seen)/1000)+"s ago\nFrames: "+String(c.packets);
      if(trafficFresh(c,millis()))s+="\n\nTraffic: "+String(dragon::familyName(c.trafficFamily))+"\n"+(detector.privacy?String("Evidence hidden"):String(c.trafficEvidence))+"\nHunt follows this observed address, not a proven attacker.";
      if(c.drone.valid && !detector.privacy){if(c.drone.hasId)s+="\nRemote ID: "+String(c.drone.id);if(c.drone.hasLocation){s+="\nAircraft: "+String(c.drone.latitude,6)+", "+String(c.drone.longitude,6)+"\nPosition age: "+String((millis()-c.drone.locationAt)/1000)+"s";if(c.drone.altitude>-1000)s+="\nAltitude WGS84: "+String(c.drone.altitude,1)+" m";if(c.drone.speed<255)s+="\nSpeed: "+String(c.drone.speed,1)+" m/s";}if(c.drone.hasOperator)s+="\nOperator location: "+String(c.drone.operatorLatitude,6)+", "+String(c.drone.operatorLongitude,6)+"\nOperator position age: "+String((millis()-c.drone.operatorAt)/1000)+"s";s+="\nBroadcast data; not authenticated.";}
      s+="\n\nIdentity and activity unverified. Signal strength is not distance.";
      label(body,s.c_str(),8,0,240);
      button(root,"Controls",52,263,124,37,56);
      button(root,"Hunt signal",184,263,124,37,52);
    }else label(body,"Contact expired. Return to the radar and select again.",8,0,240);
  }else if(page==Hunt){
    title("DRAGON HUNT",trackedContact>=0?(contacts[trackedContact].ble?"Dedicated BLE / Wi-Fi paused":"Dedicated Wi-Fi / BLE paused"):"Select a radar dot or history record");
    if(trackedContact<0){label(root,"Choose a target, then HUNT.\nWalk and compare the signal.\nStronger readings move inward.",65,125,234,F14);button(root,"Nearby contacts",75,230,210,44,21);}
    else {
      auto& c=contacts[trackedContact];char mac[18];displayMac(c.mac,mac);auto* name=label(root,*c.name?displayName(c.name).c_str():mac,65,92,230,F14,0xe8d66e);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_CENTER,0);
      huntValue=label(root,"LISTENING",70,119,220,F20,0xc9fc68);lv_obj_set_style_text_align(huntValue,LV_TEXT_ALIGN_CENTER,0);
      huntGauge=lv_bar_create(root);lv_obj_set_pos(huntGauge,S(70),S(151));lv_obj_set_size(huntGauge,S(220),S(14));lv_bar_set_range(huntGauge,-105,-25);lv_obj_set_style_bg_color(huntGauge,color(0x274c30),0);lv_obj_set_style_bg_color(huntGauge,color(0xd2e75a),LV_PART_INDICATOR);
      huntMeta=label(root,"Waiting for target",62,172,240,F12,0x9fbb91);lv_obj_set_style_text_align(huntMeta,LV_TEXT_ALIGN_CENTER,0);
      huntGraph=lv_obj_create(root);lv_obj_remove_style_all(huntGraph);lv_obj_set_pos(huntGraph,S(60),S(219));lv_obj_set_size(huntGraph,S(240),S(43));lv_obj_add_event_cb(huntGraph,huntDraw,LV_EVENT_DRAW_MAIN,nullptr);
      button(root,"Mark",62,272,76,31,58);button(root,"Target",144,272,76,31,59);button(root,"Stop",226,272,72,31,54);
    }
  }else if(page==Activity){
    title("RADIO ACTIVITY","Observed traffic / possible attacks");body=scrollBody(88,178);activityLabel=label(body,"Listening...",5,0,246,F12);
    button(root,"Events",63,272,105,32,22);button(root,"Channels",184,272,113,32,23);
  }else if(page==Detectors){
    title("DETECTION FILTERS","All enabled clues appear on the radar");body=scrollBody();
    row(body,detector.minimum==Confidence::Low?"Alerts: all hints":detector.minimum==Confidence::Medium?"Alerts: medium +":"Alerts: strong only","Sound/popups threshold; dots stay visible",0,62);
    for(unsigned i=2;i<unsigned(Family::Count);++i){auto f=Family(i);String text=String(detector.enabled(f)?"ON  ":"OFF  ")+dragon::familyName(f);row(body,text.c_str(),"Tap to enable or disable this family",i-1,4000+i);}
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
    title("CHANNEL ACTIVITY","Frames / second of observed dwell time");body=scrollBody();for(int ch=1;ch<=11;++ch){char s[96];float rate=channelDwellMs[ch]?channelFrames[ch]*1000.0/channelDwellMs[ch]:0;snprintf(s,sizeof(s),"CH %02d     %.1f f/s     %lus",ch,rate,(unsigned long)(channelDwellMs[ch]/1000));row(body,s,channelLock==ch?"Wi-Fi locked; BLE still alternates":"Tap to lock Wi-Fi channel",ch-1,200+ch);}
    button(root,"Auto",215,307,52,31,200);
  }else if(page==Files){
    title("SD CARD",directory.c_str());loadFiles();body=scrollBody();
    if(!sdReady)label(body,BOARD_HAS_SD?"SD card unavailable.\nInsert FAT32 card and restart.":"This board has no SD card slot.",5,20,244);
    else{row(body,".. / Parent",fileCount?"Tap a folder, MP3, WAV, or text file":"This folder is empty",0,39);for(int i=0;i<fileCount;++i){String name=files[i].path.substring(files[i].path.lastIndexOf('/')+1);String sub=files[i].dir?"Folder":String(files[i].size/1024)+" KB";row(body,name.c_str(),sub.c_str(),i+1,2000+i);}if(filesTruncated)label(body,"First 96 entries shown",5,(fileCount+1)*59,235);}
  }else if(page==Audio){
    title("AUDIO / HUNT",audioReady()?"Device melodies & signal feedback":"Speaker unavailable");body=scrollBody();int n=0;
    row(body,alertSound?"Alert tones: ON":"Alert tones: OFF","Unique RTTTL melody per detector family",n++,78);
    String volume=String("Volume: ")+audioStatus().volume+"%";row(body,volume.c_str(),"Tap: 0 / 10 / 25 / 50 / 75 / 100%",n++,85);
    row(body,huntSound?"Hunt feedback: ON":"Hunt feedback: OFF","Pitch & cadence follow signal strength",n++,86);
    String focus=audioFocus<0?"Audio focus: OFF":String("Focus: ")+dragon::familyName(Family(audioFocus));row(body,focus.c_str(),"Off / Axon / ALPR + Flock + Raven / Glasses",n++,87);
    row(body,"Preview Axon alert","Plays once at the selected volume",n++,88);
    row(body,"Preview glasses alert","Short, simple chime",n++,94);
    row(body,"Customize with USB web UI","RTTTL editor beside Badge Studio",n++,95);
    row(body,"Stop current sound","Quiet mode mutes automatic feedback",n++,34);
  }else if(page==Settings){
    title("SETTINGS");body=scrollBody();int n=0;
    row(body,"Display & timeouts","Brightness, dimming and screen off",n++,71);
    row(body,"Radar theme & labels",radarThemeNames[radarTheme],n++,117);
    row(body,"Audio & hunt tones","Volume, target focus and RTTTL alerts",n++,31);
    row(body,"Detection filters","Radar families / audible alert threshold",n++,61);
    row(body,activeBle?"BLE scan: ACTIVE":"BLE scan: PASSIVE",activeBle?"Requests scan-response names; transmits":"Listen only; tap for scan-response names",n++,98);
    row(body,"Radio health","Queue, memory and reception diagnostics",n++,99);
    row(body,"Watch / ignore list","Manage saved and snoozed addresses",n++,64);
    String quiet=detector.autoQuiet?String("Auto quiet: after ")+detector.autoQuiet:"Auto quiet: off";row(body,quiet.c_str(),"Re-alert at +7 dB; resets after 30 minutes",n++,72);
    row(body,detector.quiet?"Quiet mode: ON":"Quiet mode: OFF","Hide normal popups; continue history",n++,73);
    row(body,detector.history?"Flash history: ON":"Flash history: OFF","512 unique records / USB CSV export",n++,74);
    row(body,detector.privacy?"Display privacy: ON":"Display privacy: OFF","Mask addresses, names and coordinates",n++,75);
    row(body,candidatesOnly?"Radar: candidates":"Radar: all contacts","Show filtered candidates or nearby traffic",n++,53);
    row(body,radioEnabled?"Radio: listening":"Radio: paused","Tap to pause or resume reception",n++,42);
    row(body,"Signal smoothing",detector.smoothingMs==600?"Fast / 0.6 s":detector.smoothingMs==1800?"Balanced / 1.8 s":"Steady / 4 s",n++,76);
    row(body,foxTransmit()?"Fox beacon: TRANSMITTING":"Fox beacon: OFF","Opt in to a public Dragon training beacon",n++,77);
#ifdef BOARD_STOPWATCH
    row(body,alertVibration?"Vibration: ON":"Vibration: OFF","Built-in StopWatch motor",n++,79);
#endif
    row(body,loggingEnabled&&sdReady?"SD event logging: ON":"SD event logging: OFF",sdReady?sessionPath.c_str():"Requires Spotpear SD card",n++,43);
    row(body,captureEnabled?"PCAP: recording":"PCAP: stopped",sdReady?"Raw 802.11 / 16 MB per capture":"Requires Spotpear SD card",n++,24);
    row(body,"Boot to radar","Clear the saved badge startup choice",n++,55);
  }else if(page==RadarStyle){
    title("RADAR STYLE");body=scrollBody();int n=0;
    for(int i=0;i<3;++i){String name=String(radarTheme==i?LV_SYMBOL_OK "  ":"")+radarThemeNames[i];row(body,name.c_str(),i==0?"Emerald grid / yellow signals":i==1?"Green phosphor / rings and live sweep":"Dark field / gold targets / red sweep",n++,118+i);}
    row(body,radarLabels?"Device labels: ON":"Device labels: OFF",radarLabels?"Every dot labeled / six per radar page":"All dots together / tap for evidence",n++,121);
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
    label(body,"DRAGON DETECTOR " DRAGON_VERSION "\nDetection / hunt edition\n\nGreen radar / signal proximity.\nAngles are decorative.\nNo distance or direction.\n\nAll recognized clues appear.\nHollow dots: weak evidence.\nAlerts wait for repeat evidence.\nTap a dot, then Hunt signal\nfor dedicated BLE or Wi-Fi.\n\nAirTag / Find My is a family\nmatch, not model or owner ID.\nBiscuit Pro / Ultra cannot be\ndistinguished from adverts.\nNo proof of malicious intent.\n\nHold badge: tools.\nAlways on at 100% by default.\nDisplay timeouts in Settings.\nUSB Badge Studio on GitHub.\n\nBy Your Pal Kal / Valleytech\nOriginal code: MIT.\nFlock Noir / Colonel Panic\nOUI Spy / Unified Blue\nMarauder: Just Call Me Koko\nSquachWatch feature reference\nOpenDroneID: Apache-2.0\nFull notices in CREDITS.md",5,0,244,F14);
  }
  micBadge=label(root,"MIC REC",137,17,86,F12,0xff8585);lv_obj_set_style_text_align(micBadge,LV_TEXT_ALIGN_CENTER,0);legible(micBadge);lv_obj_add_flag(micBadge,LV_OBJ_FLAG_HIDDEN);
}
static void action(lv_event_t* e){
  lastInteraction=millis();
  int id=(intptr_t)lv_event_get_user_data(e);
  if(id>=2000 && id<2096){int i=id-2000;if(i>=fileCount)return;if(files[i].dir){directory=files[i].path;go(Files);page=Boot;}else{String ext=files[i].path;ext.toLowerCase();if(ext.endsWith(".mp3")||ext.endsWith(".wav")){audioPlay(files[i].path);go(Audio);}else{
      xSemaphoreTake(sdMutex,portMAX_DELAY);File f=SD_MMC.open(files[i].path);String text;while(f && f.available() && text.length()<2048){char ch=f.read();text+=(ch=='\n'||ch=='\t'||(ch>=32&&ch<127))?ch:'.';}f.close();xSemaphoreGive(sdMutex);
      lv_obj_t* box=lv_msgbox_create(root,"FILE PREVIEW",text.c_str(),nullptr,true);lv_obj_set_size(box,S(260),S(225));lv_obj_center(box);
    }}return;}
  if(id>=6000 && id<6000+deviceRows){int i=findContact(deviceKeys[id-6000]);if(i>=0){selectContact(i);go(Details);}else{uiNotify("Target expired","Refresh the list or reopen it from History.",nullptr,false);}return;}
  if(id>=1000 && id<1000+(int)MAX_CONTACTS){selectContact(id-1000);go(Details);return;}
  if(id>=3000&&id<3000+historyRows){for(size_t i=0;i<historyCount;++i)if(historyRecords[i].key==historyKeys[id-3000]&&historyRecords[i].family==historyFamilies[id-3000]){historySelection=historyRecords[i];go(HistoryDetail);return;}go(History);page=Boot;return;}
  if(id>=4002&&id<4000+int(Family::Count)){detector.families^=1u<<(id-4000);detectorSave();go(Detectors);page=Boot;return;}
  if(id>=5000&&id<5000+int(RULE_LIMIT)){controlKey=ruleKeys[id-5000];controlChannel=0;go(Controls);return;}
  if(id>=201 && id<=211){trackedContact=-1;channelLock=id-200;setRadio(true);go(Channels);page=Boot;return;}
  switch(id){
    case 1:go(Radar);break;case 2:go(Menu);break;
    case 10:case 11:case 12:menuTab=id-10;go(Menu);page=Boot;break;
    case 20:filter=1;devicePage=0;go(Devices);break;case 21:filter=2;devicePage=0;go(Devices);break;case 22:go(Alerts);break;case 23:go(Channels);break;
    case 24:setCapture(!captureEnabled);go(Settings);page=Boot;break;case 25:page=Boot;go(Devices);break;
    case 30:directory="/music";go(Files);break;case 31:go(Audio);break;case 32:directory="/";go(Files);break;
    case 33:audioRecord();break;case 34:audioStop();break;case 35:audioVolume(audioStatus().volume-10);break;case 36:audioVolume(audioStatus().volume+10);break;
    case 39:directory=directory.substring(0,directory.lastIndexOf('/'));if(directory.isEmpty())directory="/";go(Files);page=Boot;break;
    case 40:go(Settings);break;case 41:go(About);break;case 42:setRadio(!radioEnabled);pending=page;page=Boot;break;
    case 43:loggingEnabled=!loggingEnabled;pending=page;page=Boot;break;
    case 44:brightness=brightness==100?25:brightness+25;settings.putUChar("brightness",brightness);outputBrightness=-1;pending=page;page=Boot;break;
    case 45:if(selectionValid() && !contacts[selected].ble && contacts[selected].channel){trackedContact=-1;channelLock=contacts[selected].channel;setRadio(true);go(Channels);}break;
    case 46:setCapture(!captureEnabled);pending=Details;page=Boot;break;
    case 50:go(Badge);break;
    case 51:{auto* box=lv_msgbox_create(root,"BADGE STUDIO","Open the GitHub web flasher on a computer, choose Badge Studio, connect USB, then crop and upload. Hold your badge to return to tools.",nullptr,true);lv_obj_set_size(box,S(260),S(225));lv_obj_center(box);break;}
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
    case 114:if(devicePage>0)--devicePage;go(Devices);page=Boot;break;
    case 115:if((devicePage+1)*10<deviceTotal)++devicePage;go(Devices);page=Boot;break;
    case 116:go(Activity);break;
    case 117:go(RadarStyle);break;
    case 118:case 119:case 120:radarTheme=id-118;settings.putUChar("radar-theme",radarTheme);go(RadarStyle);page=Boot;break;
    case 121:radarLabels=!radarLabels;settings.putBool("radar-labels",radarLabels);radarPage=0;go(RadarStyle);page=Boot;break;
    case 122:if(radarPage)--radarPage;break;
    case 123:if(radarPage+1<dragon::radarPages(radarCount))++radarPage;break;
    case 124:radarRecent=!radarRecent;settings.putBool("radar-recent",radarRecent);go(RadarStyle);page=Boot;break;
    case 110:if(historyPage>0)--historyPage;go(History);page=Boot;break;
    case 111:if(size_t((historyPage+1)*10)<historyCount)++historyPage;go(History);page=Boot;break;
    case 112:{static const char* buttons[]={"Cancel","Clear",""};auto* box=lv_msgbox_create(root,"CLEAR HISTORY?","Erase stored detections on this device? Active scanning can create new records.",buttons,true);lv_obj_set_width(box,S(260));lv_obj_center(box);lv_obj_add_event_cb(box,[](lv_event_t* event){auto* b=lv_event_get_current_target(event);if(lv_msgbox_get_active_btn(b)==1)historyClear();lv_msgbox_close(b);go(History);page=Boot;},LV_EVENT_VALUE_CHANGED,nullptr);break;}
    case 200:trackContact(-1);pending=page;page=Boot;break;
  }
}
bool uiTouch(bool pressed,int x,int y){
  static bool consumeWake=false;
  if(pressed && outputBrightness==0)consumeWake=true;
  if(pressed)lastInteraction=millis();
  if(consumeWake){if(!pressed)consumeWake=false;return false;}
  static bool was=false;static int sx,sy,lx,ly;static uint32_t start;
  if(pressed){if(!was){sx=x;sy=y;start=millis();}lx=x;ly=y;}
  else if(was && millis()-start<1500){
    int dx=lx-sx,dy=ly-sy;
    // Raw gestures must never consume a scroll on list/detail screens. Only the
    // full-screen radar owns the bottom-edge shortcut. Menu tabs are buttons,
    // so no global horizontal/downward gesture is needed there.
    if(page==Radar && sy>S(285) && dy<-S(55) && abs(dy)>abs(dx))go(Menu);
  }
  was=pressed;
  return true;
}
void uiShow(const String& name){
  if(name=="radar-style"){go(RadarStyle);return;}
  lastInteraction=millis();
  if(name=="activity"){go(Activity);return;}
  if(name=="badge")go(Badge);else if(name=="radar")go(Radar);else if(name=="menu")go(Menu);else if(name=="wifi"){filter=1;go(Devices);}else if(name=="ble"){filter=2;go(Devices);}else if(name=="audio")go(Audio);else if(name=="files")go(Files);else if(name=="alerts")go(Alerts);else if(name=="channels")go(Channels);else if(name=="about")go(About);else if(name=="settings")go(Settings);else if(name=="hunt")go(Hunt);else if(name=="detectors")go(Detectors);else if(name=="history")go(History);else if(name=="display")go(Display);else if(name=="health")go(Diagnostics);else if(name=="targets"){filter=0;devicePage=0;go(Devices);}
}
void uiBegin(){
  settings.begin("dd-ui",false);
  if(!settings.getBool("panel-v04",false)){settings.putUChar("brightness",100);settings.putBool("panel-v04",true);}
  brightness=settings.getUChar("brightness",100);if(brightness<25||brightness>100)brightness=100;candidatesOnly=settings.getBool("candidates",true);
  if(!settings.getBool("radar-v07",false)){settings.putBool("radar-labels",true);settings.putUChar("radar-theme",1);settings.putBool("radar-v07",true);}
  radarLabels=settings.getBool("radar-labels",true);radarRecent=settings.getBool("radar-recent",true);radarTheme=settings.getUChar("radar-theme",1);if(radarTheme>2)radarTheme=1;
  scr_lvgl_init();set_brightness(brightness);lastInteraction=millis();root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,color(0x071510),0);
#ifdef BOARD_AMOLED_175C
  pinMode(BTN_PIN,INPUT_PULLUP);
#endif
  auto* img=lv_img_create(root);lv_img_set_src(img,&vts_logo);lv_obj_align(img,LV_ALIGN_CENTER,0,S(-25));
  auto* name=label(root,"DRAGON DETECTOR",49,266,262,F20,0xd6f49a);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_CENTER,0);
  label(root,"VALLEYTECH  /  INITIALIZING",70,296,225,F12,0x91b178);
  lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,img);lv_anim_set_values(&a,S(200),S(256));lv_anim_set_time(&a,900);lv_anim_set_exec_cb(&a,[](void* o,int32_t v){lv_img_set_zoom((lv_obj_t*)o,v);});lv_anim_start(&a);
  pageAt=millis();
}
void uiLoop(){
  uint32_t idle=millis()-lastInteraction;
  int level=detector.offSeconds&&idle>=uint32_t(detector.offSeconds)*1000?0:detector.dimSeconds&&idle>=uint32_t(detector.dimSeconds)*1000?detector.idleBrightness:brightness;
  if(level!=outputBrightness){outputBrightness=level;set_brightness(level);}
  if(notice&&!noticeWatch&&millis()-noticeAt>=10000)dismissNotice();
  if(page==Boot && pending==Boot && millis()-pageAt>2200)pending=Radar;
  if(page!=pending)build();
  if(millis()-lastUpdate>100){lastUpdate=millis();if(micBadge){if(audioStatus().mode==AudioMode::Recording)lv_obj_clear_flag(micBadge,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(micBadge,LV_OBJ_FLAG_HIDDEN);}
#ifdef BOARD_AMOLED_175C
    // BOOT is the hardware fallback for navigation: radar <-> tools.
    static bool buttonWas=false;bool buttonDown=digitalRead(BTN_PIN)==LOW;if(buttonDown && !buttonWas && page!=Boot){lastInteraction=millis();go(page==Radar?Menu:Radar);}buttonWas=buttonDown;
#endif
    if(page==Radar && radar){
      bool deauth=detector.enabled(Family::Deauth)&&trafficStatus.disconnectActive(millis()),spam=detector.enabled(Family::BleSpam)&&trafficStatus.bleActive(millis());
      lv_label_set_text(radarActivity,deauth&&spam?"DEAUTH + BLE BURST  >":deauth?"DEAUTH ACTIVITY  >":spam?"BLE SPAM CANDIDATE  >":"");
      if(deauth||spam)lv_obj_clear_flag(radarActivity,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(radarActivity,LV_OBJ_FLAG_HIDDEN);
      unsigned pages=radarLabels?dragon::radarPages(radarCount):1;for(auto* o:{radarPrevious,radarNext,radarPageLabel}){if(pages>1)lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
      if(pages>1){char label[24];snprintf(label,sizeof(label),"%u / %u",radarPage+1,pages);lv_label_set_text(radarPageLabel,label);if(radarPage)lv_obj_clear_state(radarPrevious,LV_STATE_DISABLED);else lv_obj_add_state(radarPrevious,LV_STATE_DISABLED);if(radarPage+1<pages)lv_obj_clear_state(radarNext,LV_STATE_DISABLED);else lv_obj_add_state(radarNext,LV_STATE_DISABLED);}
      if(trackedContact>=0 && trackingLabel){auto& t=contacts[trackedContact];uint32_t age=millis()-t.seen;String line=age>10000?"SIGNAL LOST / HUNT":String("HUNT / ")+t.signal.direction();lv_label_set_text(trackingLabel,line.c_str());}
      lv_obj_invalidate(radar);int count=0;for(auto& c:contacts)if(radarVisible(c,millis()))++count;char s[96];int n=snprintf(s,sizeof(s),"%s %s / %d %s",radioEnabled?(blePhase?"BLE":"WI-FI"):"PAUSED",captureEnabled?"PCAP":"RX",count,count==1?"target":"targets");
      int battery=batteryPercent();if(battery>=0)snprintf(s+n,sizeof(s)-n,"  /  %s%d%%",batteryCharging()?LV_SYMBOL_CHARGE " ":"",battery);lv_label_set_text(statusLabel,s);}
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
  lv_timer_handler();
}

void uiKey(bool home){lastInteraction=millis();go(home?Radar:page==Menu?Badge:Menu);}
bool uiCommand(const String& cmd){
  if(cmd=="radar status"){Serial.printf("RADAR_SETTINGS theme=%u labels=%u recent=%u page=%u pages=%u plotted=%u\n",radarTheme,radarLabels,radarRecent,radarPage+1,radarLabels?dragon::radarPages(radarCount):1,dotCount);return true;}
  if(cmd=="radar scope all"||cmd=="radar scope candidates"){candidatesOnly=cmd.endsWith("candidates");settings.putBool("candidates",candidatesOnly);radarPage=0;radarPrepared=0;Serial.printf("RADAR_SCOPE %s\n",candidatesOnly?"candidates":"all");return true;}
  if(cmd=="radar labels on"||cmd=="radar labels off"){radarLabels=cmd.endsWith("on");radarPage=0;settings.putBool("radar-labels",radarLabels);Serial.printf("RADAR_LABELS %u\n",radarLabels);return true;}
  if(cmd=="radar recent on"||cmd=="radar recent off"){radarRecent=cmd.endsWith("on");settings.putBool("radar-recent",radarRecent);Serial.printf("RADAR_RECENT %u\n",radarRecent);return true;}
  if(cmd=="radar next"||cmd=="radar previous"){unsigned pages=radarLabels?dragon::radarPages(radarCount):1;if(cmd=="radar next"&&radarPage+1<pages)++radarPage;else if(cmd=="radar previous"&&radarPage)--radarPage;Serial.printf("RADAR_PAGE %u %u\n",radarPage+1,pages);return true;}
  if(cmd=="radar theme dragon"||cmd=="radar theme green"||cmd=="radar theme gold"){radarTheme=cmd.endsWith("dragon")?0:cmd.endsWith("green")?1:2;settings.putUChar("radar-theme",radarTheme);go(Radar);page=Boot;Serial.printf("RADAR_THEME %u\n",radarTheme);return true;}
  if(cmd=="display status"){Serial.printf("DISPLAY_SETTINGS brightness=%d output=%d dim=%u off=%u dim_level=%u wake=%u\n",brightness,outputBrightness,detector.dimSeconds,detector.offSeconds,detector.idleBrightness,detector.wakeAlert);return true;}
  if(cmd.startsWith("display set ")){
    unsigned b=0,dim=0,off=0,level=0;char extra;
    if(sscanf(cmd.c_str(),"display set %u %u %u %u %c",&b,&dim,&off,&level,&extra)!=4||b<25||b>100||dim>600||off>600||level>25){Serial.println("DISPLAY_ERROR");return true;}
    brightness=b;detector.dimSeconds=dim;detector.offSeconds=off;detector.idleBrightness=level;settings.putUChar("brightness",brightness);detectorSave();lastInteraction=millis();outputBrightness=-1;
    if(page==Display){pending=Display;page=Boot;}Serial.println("DISPLAY_OK");return true;
  }
  return false;
}
