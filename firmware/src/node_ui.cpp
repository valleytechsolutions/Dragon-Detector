// Node screen for BISCUIT NODE and JCMK Node, shared by the full and Lite UIs.
// Original Dragon Detector layout; the logos are supplied artwork (CREDITS.md).
#include "node_ui.h"
#include "node_mode.h"
#include <Arduino.h>
#include <math.h>

extern const lv_img_dsc_t node_logo_biscuit;
extern const lv_img_dsc_t node_logo_jcmk;

namespace {
using dragon::node::Core;
using dragon::node::Phase;

// Panel classes, matching the font sets in lv_conf.h.
#if defined(BOARD_XIAO_ROUND) || defined(BOARD_RADAR_LITE)
const lv_font_t* const NameFont=&lv_font_montserrat_14;
const lv_font_t* const StatusFont=&lv_font_montserrat_10;
const lv_font_t* const CountFont=&lv_font_montserrat_22;
const lv_font_t* const SmallFont=&lv_font_montserrat_10;
#elif defined(BOARD_WATCHER) || defined(BOARD_LILYGO_175) || defined(BOARD_WAVESHARE_143C) || defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175) || defined(BOARD_STOPWATCH) || defined(BOARD_CROWPANEL_21)
const lv_font_t* const NameFont=&lv_font_montserrat_26;
const lv_font_t* const StatusFont=&lv_font_montserrat_16;
const lv_font_t* const CountFont=&lv_font_montserrat_48;
const lv_font_t* const SmallFont=&lv_font_montserrat_14;
#else
const lv_font_t* const NameFont=&lv_font_montserrat_20;
const lv_font_t* const StatusFont=&lv_font_montserrat_14;
const lv_font_t* const CountFont=&lv_font_montserrat_40;
const lv_font_t* const SmallFont=&lv_font_montserrat_12;
#endif

// Row tops in 466 units (the logo row is its centre). The 240 px panels use
// fonts that are large for their size, so their rows sit closer together and
// clear of the lower curve of the glass.
struct Rows{int16_t name,status,logo,count,label,stats,button,hint;};
#if defined(BOARD_XIAO_ROUND) || defined(BOARD_RADAR_LITE)
constexpr Rows Row={58,93,208,295,339,363,394,392};
#elif defined(BOARD_WATCHER)
constexpr Rows Row={56,96,203,285,350,374,404,412};  // 466 fonts on a 412 px panel
#else
constexpr Rows Row={56,96,208,296,350,374,404,412};
#endif

constexpr uint32_t Black=0x000000,White=0xffffff,Muted=0x8b9099,Track=0x1c1f24;
constexpr uint32_t Linked=0x34d07a,Searching=0xffb020,Problem=0xff5a52,ButtonFill=0x17191d,ButtonEdge=0x3a3f47;
constexpr uint32_t ConfirmMs=3000,DriftMs=60000,RefreshMs=200;
// Small orbit that moves the static artwork a few pixels every minute (AMOLED wear).
constexpr int8_t Drift[][2]={{0,0},{2,-2},{-2,2},{2,2},{-2,-2},{0,3},{0,-3},{3,0},{-3,0}};

struct Widgets {
  lv_obj_t *screen,*content,*ring,*name,*status,*logo,*count,*countLabel,*stats,*button,*buttonText,*hint;
} w{};
NodeScreenOptions options;
NodeKind shownKind=NodeKind::None;
float scale=1;
lv_coord_t panel=466;
uint32_t refreshedAt=0,driftAt=0,armedAt=0;
unsigned driftStep=0;
bool armed=false,leaving=false;
char lastStatus[64]="",lastCount[16]="",lastStats[64]="";
uint32_t lastColor=0;

int px(int units){return int(units*scale+(units>=0?.5f:-.5f));}
// Width the round glass leaves for a row spanning top..top+height, inside the
// ring with a small gap and room for the burn-in drift.
lv_coord_t room(int top,int height){
  const float r=panel/2.f,inner=r-px(16);
  const float above=r-top,below=top+height-r;
  const float d=above>below?above:below;
  if(d<=0)return lv_coord_t(2*inner);
  return d>=inner?0:lv_coord_t(2*sqrtf(inner*inner-d*d));
}
// One line of centred text, as wide as the glass allows at that height.
lv_obj_t* text(lv_obj_t* parent,const lv_font_t* font,uint32_t color,int y,int width){
  auto* l=lv_label_create(parent);
  lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);
  lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
  const lv_coord_t line=lv_font_get_line_height(font),fit=room(px(y),line);
  lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);lv_obj_set_size(l,px(width)<fit?px(width):fit,line);
  lv_obj_align(l,LV_ALIGN_TOP_MID,0,px(y));lv_label_set_text(l,"");
  return l;
}
// True when value fits the label on one line.
bool fits(lv_obj_t* label,const char* value){
  lv_point_t size;
  lv_txt_get_size(&size,value,lv_obj_get_style_text_font(label,0),lv_obj_get_style_text_letter_space(label,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  return size.x<=lv_obj_get_style_width(label,LV_PART_MAIN);
}
void thousands(char* out,size_t size,uint32_t value){
  char digits[12];const int n=snprintf(digits,sizeof(digits),"%lu",(unsigned long)value);
  size_t at=0;
  for(int i=0;i<n&&at+1<size;++i){
    if(i&&(n-i)%3==0&&at+2<size)out[at++]=',';
    out[at++]=digits[i];
  }
  out[at]=0;
}
void setText(lv_obj_t* label,char* last,size_t size,const char* value){
  if(!label||!strcmp(last,value))return;
  snprintf(last,size,"%s",value);lv_label_set_text(label,value);
}
void arm(bool on){
  armed=on;armedAt=millis();
  if(!w.button)return;
  lv_obj_set_style_bg_color(w.button,lv_color_hex(on?Problem:ButtonFill),0);
  lv_obj_set_style_border_color(w.button,lv_color_hex(on?Problem:ButtonEdge),0);
  lv_label_set_text(w.buttonText,on?"CONFIRM":"EXIT");
  lastStatus[0]=0;           // the status line explains the second press
}
void pressed(lv_event_t*){nodeScreenExitPress();}

// The link state in a few words, longest wording first, and its colour.
uint8_t describe(const char* (&out)[3],uint32_t& color){
  const auto& s=nodeStatus();
  const bool biscuitNode=shownKind==NodeKind::Biscuit;
  if(armed){
    out[0]=options.confirmHint?options.confirmHint:w.button?"TAP CONFIRM TO LEAVE":"PRESS AGAIN TO LEAVE";color=Problem;return 1;
  }
  if(s.linked){
    color=Linked;
    if(s.broadcast){out[0]="LINKED (BROADCAST)";return 1;}
    out[0]=s.core==Core::Piglet?"LINKED TO PIGLET CORE":biscuitNode?"LINKED TO BISCUIT":"LINKED TO CORE";
    out[1]="LINKED";return 2;
  }
  color=Problem;
  switch(s.other){
    case Core::BiscuitSecure:out[0]="SET BISCUIT TO EXTENDED MODE";out[1]="BISCUIT: SET EXTENDED MODE";out[2]="USE EXTENDED MODE";return 3;
    case Core::Biscuit:out[0]="BISCUIT FOUND: USE BISCUIT NODE";out[1]="BISCUIT: USE BISCUIT NODE";out[2]="USE BISCUIT NODE";return 3;
    case Core::Jcmk:case Core::Piglet:out[0]="JCMK CORE FOUND: USE JCMK Node";out[1]="JCMK CORE: USE JCMK Node";out[2]="USE JCMK Node";return 3;
    default:break;
  }
  color=Searching;
  out[0]=biscuitNode?"SEARCHING FOR BISCUIT":"SEARCHING FOR CORE";out[1]="SEARCHING";return 2;
}
void refresh(uint32_t now){
  const auto& s=nodeStatus();
  const char* wording[3]={};uint32_t color=Searching;
  const uint8_t choices=describe(wording,color);
  uint8_t pick=0;while(pick+1<choices&&!fits(w.status,wording[pick]))++pick;
  setText(w.status,lastStatus,sizeof(lastStatus),wording[pick]);
  if(color!=lastColor){
    lastColor=color;
    lv_obj_set_style_text_color(w.status,lv_color_hex(color),0);
    lv_obj_set_style_arc_color(w.ring,lv_color_hex(color),LV_PART_INDICATOR);
  }
  char number[16];thousands(number,sizeof(number),s.wifi);
  if(!fits(w.count,number))snprintf(number,sizeof(number),"%lu.%luM",(unsigned long)(s.wifi/1000000u),(unsigned long)(s.wifi/100000u%10u));
  setText(w.count,lastCount,sizeof(lastCount),number);
  // Fullest line that fits: wide gaps, then narrow gaps, then without the channel.
  char sent[16],ble[16],line[64];thousands(sent,sizeof(sent),s.sent);thousands(ble,sizeof(ble),s.ble);
  const bool withBle=shownKind==NodeKind::Jcmk&&nodeBleReports();
  for(uint8_t form=0;form<4;++form){
    const char* gap=form==0?"   ":"  ";
    if(withBle&&form<3){
      if(form<2)snprintf(line,sizeof(line),"BLE %s%sSENT %s%sCH %u",ble,gap,sent,gap,unsigned(s.channel));
      else snprintf(line,sizeof(line),"BLE %s  SENT %s",ble,sent);
    }else if(form<2)snprintf(line,sizeof(line),"SENT %s%sCH %u",sent,gap,unsigned(s.channel));
    else snprintf(line,sizeof(line),"SENT %s",sent);
    if(fits(w.stats,line))break;
  }
  setText(w.stats,lastStats,sizeof(lastStats),line);
  // Ring: a sweep marks the channel being scanned; while searching it turns slowly.
  int16_t middle=0;
  if(s.linked&&s.planCount){
    unsigned at=0;for(uint8_t i=0;i<s.planCount;++i)if(s.plan[i]==s.channel){at=i;break;}
    middle=int16_t((at*360u+180u)/s.planCount);
  }else middle=int16_t((now/12)%360);
  lv_arc_set_angles(w.ring,uint16_t((middle+360-14)%360),uint16_t((middle+14)%360));
}
}

void nodeScreenOpen(lv_obj_t* parent,const NodeScreenOptions& opened){
  nodeScreenClose();
  options=opened;shownKind=nodeKind();leaving=false;armed=false;
  const lv_coord_t size=lv_obj_get_width(parent)?lv_obj_get_width(parent):lv_disp_get_hor_res(nullptr);
  panel=size;scale=float(size)/466.f;
  w.screen=lv_obj_create(parent);
  lv_obj_remove_style_all(w.screen);lv_obj_set_size(w.screen,size,size);lv_obj_set_pos(w.screen,0,0);
  lv_obj_set_style_bg_color(w.screen,lv_color_hex(Black),0);lv_obj_set_style_bg_opa(w.screen,LV_OPA_COVER,0);
  lv_obj_clear_flag(w.screen,LV_OBJ_FLAG_SCROLLABLE);

  w.ring=lv_arc_create(w.screen);
  lv_obj_set_size(w.ring,size-px(10),size-px(10));lv_obj_center(w.ring);
  lv_arc_set_rotation(w.ring,270);lv_arc_set_bg_angles(w.ring,0,360);
  lv_obj_remove_style(w.ring,nullptr,LV_PART_KNOB);lv_obj_clear_flag(w.ring,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(w.ring,px(3)>1?px(3):2,LV_PART_MAIN);lv_obj_set_style_arc_color(w.ring,lv_color_hex(Track),LV_PART_MAIN);
  lv_obj_set_style_arc_width(w.ring,px(5)>2?px(5):3,LV_PART_INDICATOR);lv_obj_set_style_arc_rounded(w.ring,true,LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(w.ring,lv_color_hex(Searching),LV_PART_INDICATOR);

  w.content=lv_obj_create(w.screen);
  lv_obj_remove_style_all(w.content);lv_obj_set_size(w.content,size,size);lv_obj_set_pos(w.content,0,0);
  lv_obj_clear_flag(w.content,LV_OBJ_FLAG_SCROLLABLE);lv_obj_clear_flag(w.content,LV_OBJ_FLAG_CLICKABLE);

  w.name=text(w.content,NameFont,White,Row.name,330);
  lv_obj_set_style_text_letter_space(w.name,px(2),0);lv_label_set_text(w.name,nodeName(shownKind));
  w.status=text(w.content,StatusFont,Searching,Row.status,340);

  w.logo=lv_img_create(w.content);
  lv_img_set_src(w.logo,shownKind==NodeKind::Biscuit?&node_logo_biscuit:&node_logo_jcmk);
  lv_obj_set_style_img_recolor(w.logo,lv_color_hex(White),0);lv_obj_set_style_img_recolor_opa(w.logo,LV_OPA_COVER,0);
  lv_obj_align(w.logo,LV_ALIGN_CENTER,0,px(Row.logo-233));

  w.count=text(w.content,CountFont,White,Row.count,300);
  w.countLabel=text(w.content,SmallFont,Muted,Row.label,300);
  lv_obj_set_style_text_letter_space(w.countLabel,px(2),0);lv_label_set_text(w.countLabel,"WI-FI NETWORKS");
  w.stats=text(w.content,StatusFont,Muted,Row.stats,330);

  if(options.exitButton){
    w.button=lv_btn_create(w.screen);
    lv_obj_set_size(w.button,px(132),px(38));lv_obj_align(w.button,LV_ALIGN_TOP_MID,0,px(Row.button));
    lv_obj_set_style_radius(w.button,LV_RADIUS_CIRCLE,0);lv_obj_set_style_shadow_width(w.button,0,0);
    lv_obj_set_style_bg_color(w.button,lv_color_hex(ButtonFill),0);lv_obj_set_style_bg_opa(w.button,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(w.button,1,0);lv_obj_set_style_border_color(w.button,lv_color_hex(ButtonEdge),0);
    lv_obj_set_style_outline_color(w.button,lv_color_hex(White),LV_STATE_FOCUS_KEY);lv_obj_set_style_outline_width(w.button,2,LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(w.button,pressed,LV_EVENT_CLICKED,nullptr);
    w.buttonText=lv_label_create(w.button);lv_obj_set_style_text_font(w.buttonText,StatusFont,0);
    lv_obj_set_style_text_color(w.buttonText,lv_color_hex(White),0);lv_obj_set_style_text_letter_space(w.buttonText,px(2),0);
    lv_label_set_text(w.buttonText,"EXIT");lv_obj_center(w.buttonText);
  }else if(options.exitHint){
    w.hint=text(w.screen,SmallFont,Muted,Row.hint,300);lv_label_set_text(w.hint,options.exitHint);
  }
  lastStatus[0]=lastCount[0]=lastStats[0]=0;lastColor=0;
  refreshedAt=driftAt=millis();driftStep=0;
  refresh(refreshedAt);
}

void nodeScreenClose(){w=Widgets{};armed=false;}
bool nodeScreenShown(){return w.screen!=nullptr;}

void nodeScreenLoop(){
  if(!w.screen||leaving)return;
  const uint32_t now=millis();
  if(!nodeBusy()||nodeKind()!=shownKind){
    // Stopped or switched elsewhere (USB): hand the screen back.
    leaving=true;if(options.exited)options.exited();return;
  }
  if(armed&&now-armedAt>=ConfirmMs)arm(false);
  if(now-driftAt>=DriftMs){
    driftAt=now;driftStep=(driftStep+1)%(sizeof(Drift)/sizeof(Drift[0]));
    lv_obj_set_pos(w.content,px(Drift[driftStep][0]),px(Drift[driftStep][1]));
  }
  if(now-refreshedAt<RefreshMs)return;
  refreshedAt=now;refresh(now);
}

bool nodeScreenExitPress(){
  if(!nodeBusy())return false;
  if(!armed||millis()-armedAt>=ConfirmMs){arm(true);if(w.screen)refresh(millis());return false;}
  armed=false;leaving=true;
  nodeStop();
  if(options.exited)options.exited();
  return true;
}
