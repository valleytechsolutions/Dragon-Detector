#include "lite_button.h"
enum class LitePage {Radar,Clock,Timer,Signals,Status};
static LitePage litePage=LitePage::Radar;
static const char* litePageNames[]={"radar","clock","stopwatch","signals","diagnostics"};
static lv_obj_t *pageCanvas,*pageTitle,*pageValue,*pageSub,*pageHint;
static unsigned signalIndex=0;
static uint64_t timerFrame=0;
static dragon::Calendar clockFrame;
static bool clockValid=false;
static bool liteLocksport(){return radarTheme==6||radarTheme==8||radarTheme==9||radarTheme==21||radarTheme==26||radarTheme==28||radarTheme==32||radarTheme==VectorPotentialTheme||radarTheme==ArgyleTheme||radarTheme==LawLockToolsTheme||radarTheme==MokiTheme||(radarTheme==dragon::PersonalThemeId&&(personalTheme().flags&8));}
static void pageText(lv_draw_ctx_t* ctx,const char* value,int x,int y,int width,uint32_t rgb){
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(rgb);d.font=&lv_font_montserrat_10;d.align=LV_TEXT_ALIGN_CENTER;
  lv_area_t a{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+width-1),lv_coord_t(y+15)};lv_draw_label(ctx,&d,&a,value,nullptr);
}
static void liteHand(lv_draw_ctx_t* ctx,float angle,float length,uint32_t rgb,bool pick){
  float dx=sinf(angle),dy=-cosf(angle),sx=cosf(angle),sy=sinf(angle);
  auto point=[&](float along,float side){return lv_point_t{lv_coord_t(120+dx*along+sx*side),lv_coord_t(120+dy*along+sy*side)};};
  auto stroke=[&](float a,float b,float c,float d,int width,uint32_t ink){auto p=point(a,b),q=point(c,d);line(ctx,p.x,p.y,q.x,q.y,ink,width);};
  stroke(-10,0,length,0,pick?3:4,rgb);
  if(pick){stroke(-9,0,length*.4f,0,7,0x677684);stroke(-8,-1,length*.4f,-1,2,0xd6dce1);stroke(length-8,0,length,-4,3,rgb);}
}
static void litePageDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);const auto& p=themeUiColors(radarTheme);
  if(litePage==LitePage::Clock&&radarTheme==LawLockToolsTheme){lltWatch(ctx,clockFrame,clockValid);return;}
  if(litePage==LitePage::Clock&&radarTheme==MokiTheme){mokiWatch(ctx,clockFrame,clockValid);return;}
  if(litePage==LitePage::Clock&&radarTheme==ArgyleTheme){argyleWatch(ctx,clockFrame,clockValid);return;}
  if(litePage==LitePage::Clock&&radarTheme==VectorPotentialTheme){vpWatch(ctx,clockFrame,clockValid);return;}
  if(litePage==LitePage::Clock&&radarTheme==HypnoticTheme){hypnoticWatch(ctx,clockFrame,clockValid,radarFrameAt);return;}
  if(litePage==LitePage::Clock&&radarTheme==MitmTheme){mitmWatch(ctx,clockFrame,clockValid,radarFrameAt);return;}
  if(litePage==LitePage::Clock&&radarTheme==MuldersRevengeTheme){muldersWatch(ctx,clockFrame,clockValid);return;}
  if(litePage==LitePage::Clock&&greenArtTheme(radarTheme)){greenWatch(ctx,clockFrame,clockValid,radarTheme==XombiTheme);return;}
  if(litePage==LitePage::Clock&&radarTheme==CanCommanderTheme){canCommanderWatch(ctx,clockFrame,clockValid,radarFrameAt);return;}
  if(litePage==LitePage::Clock&&radarTheme==33){dzazWatch(ctx,clockFrame,clockValid);return;}
  if(litePage==LitePage::Clock&&radarTheme==dragon::PersonalThemeId){personalThemeClock(ctx,clockFrame,clockValid);return;}
  const bool dragonClock=litePage==LitePage::Clock&&radarTheme==0;
  disc(ctx,120,120,118,dragonClock?0x76532b:p.border);disc(ctx,120,120,116,dragonClock?0x000000:p.bg);
  if(litePage==LitePage::Clock){
    if(radarTheme==0){
      themeArtwork(ctx,boot_dragon_logo,16);
      for(int i=0;i<60;++i){float a=i*.104719755f;line(ctx,120+sinf(a)*113,120-cosf(a)*113,120+sinf(a)*116,120-cosf(a)*116,0x76532b);}
      if(clockValid){float a=clockFrame.second*.104719755f;disc(ctx,120+sinf(a)*114,120-cosf(a)*114,2,0xffb000);}return;
    }
    if(radarTheme==32){sparrowsWatch(ctx,clockFrame,clockValid);return;}
    if(radarTheme==31){capsuleWatch(ctx,clockFrame,clockValid);return;}
    if(radarTheme==29){themeArtwork(ctx,watch_logo_hackazillarex,90);dinoRun(ctx,radarFrameAt);return;}
    if(radarTheme==30){themeArtwork(ctx,watch_logo_m33k,68);cyberRim(ctx,radarFrameAt,clockValid?clockFrame.second:-1);return;}
    for(int i=0;i<60;++i){float a=i*.104719755f;int r=i%5?91:86;line(ctx,120+sinf(a)*r,120-cosf(a)*r,120+sinf(a)*96,120-cosf(a)*96,i%5?p.border:p.accent,i%5?1:2);}
    for(int i:{3,6,9}){
#ifdef BOARD_M5DIAL
      if(i==6)continue; // The date occupies this space on the 240px Dial.
#endif
      float a=i*.523598776f;char n[4];snprintf(n,sizeof(n),"%d",i);pageText(ctx,n,108+sinf(a)*76,115-cosf(a)*76,24,p.muted);
    }
    if(radarTheme==13){for(int side:{-1,1}){line(ctx,120+side*7,82,120+side*27,72,0x6f289b,8);line(ctx,120+side*7,82,120+side*27,72,0xefceff,3);}}
    if(radarTheme==24)pageText(ctx,"(o..o) /||||\\",78,76,84,p.accent);
    if(radarTheme==28)locksportPinRim(ctx,uint64_t(clockFrame.second)*1000000+(millis()%1000)*1000);
    if(clockValid){
      const float seconds=clockFrame.second,minutes=clockFrame.minute+seconds/60.f,hours=clockFrame.hour%12+minutes/60.f;
      bool picks=radarTheme==9||radarTheme==21||radarTheme==26||radarTheme==28||radarTheme==32;
      liteHand(ctx,hours*.523598776f,49,picks?0xbcc6cf:p.ink,picks);
      liteHand(ctx,minutes*.104719755f,70,picks?0xf1f4f6:p.accent,picks);
      liteHand(ctx,seconds*.104719755f,79,p.muted,false);disc(ctx,120,120,5,p.accent);disc(ctx,120,120,2,p.bg);
    }
  }else if(litePage==LitePage::Timer){
    if(radarTheme==LawLockToolsTheme){lltPinRim(ctx,timerFrame/1000000);return;}
    if(radarTheme==CanCommanderTheme){canBusRim(ctx,uint32_t(timerFrame/10000),true);return;}
    if(radarTheme==28){locksportPinRim(ctx,timerFrame);return;}
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=color(p.border);d.width=3;lv_point_t center{120,120};lv_draw_arc(ctx,&d,&center,105,0,359);
    float a=float(timerFrame%60000000ull)*.000000104719755f;
    disc(ctx,120+int(sinf(a)*105),120-int(cosf(a)*105),4,p.accent);
    d.color=color(p.accent);d.opa=150;lv_draw_arc(ctx,&d,&center,105,uint16_t(timerFrame/1000000%60*6+270)%360,uint16_t(timerFrame/1000000%60*6+310)%360);
  }
}
static void setLitePage(LitePage next){
  setRadioStandby(next==LitePage::Clock||next==LitePage::Timer);
  litePage=next;noticeAt=0;
#ifdef BOARD_M5DIAL
  dialPageChanged();
#endif
  lv_obj_clear_flag(pageHint,LV_OBJ_FLAG_HIDDEN);
  for(auto* o:{radar,status,activity,pageLabel,notice})lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
  for(auto* o:{pageCanvas,pageTitle,pageValue,pageSub})lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
  if(next==LitePage::Radar){for(auto* o:{radar,status,pageLabel})lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);}
  else for(auto* o:{pageCanvas,pageTitle,pageValue,pageSub})lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(pageValue,next==LitePage::Signals||next==LitePage::Status?&lv_font_montserrat_14:&lv_font_montserrat_22,0);
  lv_obj_set_y(pageValue,next==LitePage::Clock?158:next==LitePage::Timer?95:76);
  lv_obj_set_y(pageSub,next==LitePage::Clock?187:next==LitePage::Timer?138:171);
  bool special=next==LitePage::Clock&&(radarTheme==0||radarTheme>=29);
  if(radarLogo(radarTheme)&&!special&&!(next==LitePage::Radar&&(radarTheme==LawLockToolsTheme||radarTheme==MokiTheme||radarTheme==ArgyleTheme||radarTheme==VectorPotentialTheme||radarTheme==32||radarTheme==CanCommanderTheme||radarTheme==MuldersRevengeTheme||greenArtTheme(radarTheme))))lv_obj_clear_flag(logo,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(logo,LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(logo,LV_ALIGN_TOP_MID,0,9);
  lv_obj_set_style_img_recolor(logo,color(LltBlue),0);
  lv_obj_set_style_img_recolor_opa(logo,radarTheme==LawLockToolsTheme?LV_OPA_COVER:LV_OPA_TRANSP,0);
  if(special){lv_obj_add_flag(pageTitle,LV_OBJ_FLAG_HIDDEN);lv_obj_set_y(pageValue,radarTheme==29?24:157);lv_obj_set_y(pageSub,radarTheme==29?48:188);}
  if(next==LitePage::Clock&&(radarTheme==HypnoticTheme||radarTheme==MitmTheme||radarTheme==31||radarTheme==32||radarTheme==VectorPotentialTheme||radarTheme==ArgyleTheme||radarTheme==LawLockToolsTheme||radarTheme==MokiTheme||radarTheme==33||radarTheme==CanCommanderTheme||radarTheme==MuldersRevengeTheme||greenArtTheme(radarTheme)||radarTheme==dragon::PersonalThemeId))for(auto* o:{pageTitle,pageValue,pageSub,pageHint})lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(root);
}
static void createLitePages(){
  pageCanvas=lv_obj_create(root);lv_obj_remove_style_all(pageCanvas);lv_obj_set_size(pageCanvas,240,240);lv_obj_clear_flag(pageCanvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(pageCanvas,litePageDraw,LV_EVENT_DRAW_MAIN,nullptr);
  lv_obj_move_foreground(logo);
  pageTitle=text("",45,178,&lv_font_montserrat_10,0xffffff);
  pageValue=text("",95,190,&lv_font_montserrat_22,0xffffff);
  lv_label_set_long_mode(pageValue,LV_LABEL_LONG_WRAP);
  pageSub=text("",171,174,&lv_font_montserrat_10,0xffffff);lv_label_set_long_mode(pageSub,LV_LABEL_LONG_WRAP);
  pageHint=text("TAP THEME / HOLD PAGE",211,144,&lv_font_montserrat_8,0xffffff);
  setLitePage(LitePage::Radar);
}
static void updateLitePage(){
  const auto& p=themeUiColors(radarTheme);char value[180];
  for(auto* o:{pageTitle,pageSub,pageHint})lv_obj_set_style_text_color(o,color(p.muted),0);
  lv_obj_set_style_text_color(pageValue,color(p.ink),0);
  lv_obj_set_style_bg_color(pageValue,color(p.bg),0);lv_obj_set_style_bg_opa(pageValue,LV_OPA_COVER,0);
  if(litePage==LitePage::Clock&&radarTheme==0){lv_obj_set_style_text_color(pageValue,color(0xfff0cf),0);lv_obj_set_style_text_color(pageSub,color(0xffb000),0);lv_obj_set_style_bg_color(pageValue,color(0x000000),0);}
  if(litePage==LitePage::Radar){lv_label_set_text(pageHint,"TAP THEME / HOLD PAGE");if(!noticeAt||uint32_t(millis()-noticeAt)>5000)lv_obj_clear_flag(pageHint,LV_OBJ_FLAG_HIDDEN);return;}
  timerFrame=stopwatch.elapsed(stopwatchNow());clockLocal(clockFrame);clockValid=wallTime()!=0;
  lv_obj_invalidate(pageCanvas);
  if(litePage==LitePage::Clock){
    lv_label_set_text(pageTitle,radarThemeNames[radarTheme]);
    if(clockValid){snprintf(value,sizeof(value),"%02d:%02d:%02d",clockFrame.hour,clockFrame.minute,clockFrame.second);lv_label_set_text(pageValue,value);snprintf(value,sizeof(value),"%04d-%02d-%02d",clockFrame.year,clockFrame.month,clockFrame.day);}
    else{lv_label_set_text(pageValue,"--:--:--");snprintf(value,sizeof(value),"Sync time in USB Studio");}
    lv_label_set_text(pageSub,value);lv_label_set_text(pageHint,"TAP THEME / HOLD PAGE");
  }else if(litePage==LitePage::Timer){
    lv_label_set_text(pageTitle,liteLocksport()?"LOCKSPORT TIMER":"STOPWATCH");uint64_t ms=timerFrame/1000;
    snprintf(value,sizeof(value),"%02llu:%02llu.%02llu",ms/60000,(ms/1000)%60,(ms/10)%100);lv_label_set_text(pageValue,value);
    snprintf(value,sizeof(value),"%s / %lu %s",stopwatch.running?"RUNNING":stopwatch.finished?"OPEN":timerFrame?"PAUSED":"READY",stopwatch.lapCount,liteLocksport()?"SPLITS":"LAPS");lv_label_set_text(pageSub,value);
    lv_label_set_text(pageHint,stopwatch.running?(liteLocksport()?"TAP PAUSE / 2x OPEN":"TAP PAUSE / 2x LAP"):"TAP START / 2x RESET");
  }else if(litePage==LitePage::Signals){
#ifdef BOARD_M5DIAL
    dialSignalUpdate();return;
#endif
    unsigned count=0;for(auto& c:contacts)if(radarVisible(c,millis()))++count;
    Contact* chosen=nullptr;unsigned target=count?signalIndex%count:0,index=0;
    for(auto& c:contacts)if(radarVisible(c,millis())&&index++==target){chosen=&c;break;}
    snprintf(value,sizeof(value),"SIGNAL %u / %u",count?target+1:0,count);lv_label_set_text(pageTitle,value);
    if(chosen){char mac[18];macText(chosen->mac,mac);snprintf(value,sizeof(value),"%.26s\n%d dBm / %s\n%s",*chosen->name?chosen->name:radarFamilyName(chosen->match.family),chosen->rssi,chosen->ble?"BLE":"WI-FI",mac);lv_label_set_text(pageValue,value);snprintf(value,sizeof(value),"Last seen %lus ago\nRSSI is not distance",(millis()-chosen->seen)/1000);}
    else{lv_label_set_text(pageValue,"LISTENING");snprintf(value,sizeof(value),"No recent visible signals");}
    lv_label_set_text(pageSub,value);lv_label_set_text(pageHint,"TAP NEXT / HOLD PAGE");
  }else{
    lv_label_set_text(pageTitle,"DETECTOR STATUS");
    snprintf(value,sizeof(value),"WI-FI  %lu\nBLE  %lu\nDROPS  %lu\nHEAP  %u KB\nPSRAM  %u KB",wifiFrames,bleFrames,queueDrops,ESP.getFreeHeap()/1024,ESP.getFreePsram()/1024);lv_label_set_text(pageValue,value);
    snprintf(value,sizeof(value),"UP %lu MIN / CH %u\nCLOCK %s",millis()/60000,wifiChannel,wallTime()?"SET":"NEEDS SYNC");lv_label_set_text(pageSub,value);lv_label_set_text(pageHint,"HOLD TO RETURN TO RADAR");
  }
}
void liteButtonEvent(dragon::LitePress event){
  if(!root)return;
#ifdef BOARD_M5DIAL
  dialButton(event);return;
#endif
  if(event==dragon::LitePress::Hold){setLitePage(LitePage((unsigned(litePage)+1)%5));return;}
  if(litePage==LitePage::Timer){if(event==dragon::LitePress::Tap)stopwatch.toggle(stopwatchNow());else if(event==dragon::LitePress::Double){if(stopwatch.running){if(liteLocksport())stopwatch.finish(stopwatchNow());else stopwatch.lap(stopwatchNow());}else stopwatch.reset();}}
  else if(litePage==LitePage::Signals)++signalIndex;
  else if(litePage!=LitePage::Status)uiKey(false);
}
