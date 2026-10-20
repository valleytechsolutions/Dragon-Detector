// Shared by full UI and the radar-only Lite UI. Coordinates use the 360px design.
#ifdef BOARD_RADAR_LITE
static constexpr unsigned RadarRenderLabelLimit=dragon::LiteRadarLabelLimit;
static constexpr bool RadarRenderLite=true;
#else
static constexpr unsigned RadarRenderLabelLimit=dragon::RadarLabelLimit;
static constexpr bool RadarRenderLite=false;
#endif
static dragon::DeviceKey radarSelectedKey;
static bool radarHasSelection=false;
static uint32_t radarSelectedAt=0;
static int radarPrimary=-1;
static uint32_t contactColor(Contact& c){
  if(!c.radarColor||c.radarColorFamily!=c.match.family){
    c.radarColor=dragon::uniqueRadarColor(c.match.family,contactHash(contactKey(c)),[&](uint16_t color){
      for(auto& other:contacts)if(&other!=&c&&other.used&&other.radarColor==color)return true;
      return false;
    });
    c.radarColorFamily=c.match.family;
  }
  return dragon::radarRgb888(c.radarColor);
}
static void line(lv_draw_ctx_t* ctx,int x1,int y1,int x2,int y2,uint32_t c,int width=1,int opa=255){
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=color(c);d.width=width;d.opa=opa;lv_point_t a{(lv_coord_t)x1,(lv_coord_t)y1},b{(lv_coord_t)x2,(lv_coord_t)y2};lv_draw_line(ctx,&d,&a,&b);
}
static void disc(lv_draw_ctx_t* ctx,int x,int y,int r,uint32_t c,int opa=255){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=LV_RADIUS_CIRCLE;d.bg_color=color(c);d.bg_opa=opa;lv_area_t a{(lv_coord_t)(x-r),(lv_coord_t)(y-r),(lv_coord_t)(x+r),(lv_coord_t)(y+r)};lv_draw_rect(ctx,&d,&a);
}
#include "lpl_radar.inc"
#include "containment_radar.inc"
#include "cyber_runner.inc"
#include "capsule_theme.inc"
#include "sparrows_theme.inc"
#include "dzaz_theme.inc"
#include "can_commander_theme.inc"
static const char* radarFamilyName(Family f){
#if SCREEN_RES<=240
  // Keep the family on one line, leaving the second line for signal/age.
  switch(f){case Family::Axon:return "Axon";case Family::Meta:return "Glasses";case Family::Tracker:return "Find My";case Family::Skimmer:return "Serial";case Family::Camera:return "Camera";case Family::Alpr:return "ALPR";case Family::Deauth:return "Deauth";case Family::EvilTwin:return "SSID clash";default:break;}
#endif
  switch(f){case Family::Axon:return "Axon / TASER";case Family::Meta:return "Camera glasses";case Family::Tracker:return "Find My / DULT";case Family::GoogleTag:return "Find Hub";case Family::Skimmer:return "Serial module";case Family::Camera:return "Camera vendor";case Family::Alpr:return "ALPR vendor";case Family::Drone:return "Drone RID";case Family::Deauth:return "Deauth burst";case Family::BleSpam:return "BLE burst";case Family::EvilTwin:return "SSID conflict";default:return dragon::familyName(f);}
}
static void callout(lv_draw_ctx_t* ctx,const Dot& dot,unsigned slot,const char* text,uint32_t c){
  auto b=dragon::radarLabelSlot(slot,RadarRenderLite);int x=S(b.x),y=S(b.y),w=S(b.w),h=S(b.h);
  int edgeX=constrain(dot.x,x,x+w),edgeY=constrain(dot.y,y,y+h);
  line(ctx,dot.x,dot.y,edgeX,edgeY,dragon::RadarLabelBackground,S(4),255);
  line(ctx,dot.x,dot.y,edgeX,edgeY,c,std::max(2,S(2)),255);
#ifdef BOARD_RADAR_LITE
  // Slim, open callouts: the family stays bright while signal state is secondary.
  lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.radius=2;box.bg_color=color(0x020c0c);box.bg_opa=245;
  lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w),(lv_coord_t)(y+h)};lv_draw_rect(ctx,&box,&area);
  line(ctx,x+2,y+3,x+2,y+h-3,c,2);line(ctx,x+6,y+h,x+w,y+h,c,1,80);
  char title[40];const char* state=strchr(text,'\n');size_t n=state?size_t(state-text):strlen(text);n=std::min(n,sizeof(title)-1);memcpy(title,text,n);title[n]=0;
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(c);d.font=FCALLOUT;
  lv_area_t textArea{(lv_coord_t)(x+8),(lv_coord_t)(y+1),(lv_coord_t)(x+w-2),(lv_coord_t)(y+h-1)};
  lv_area_t clip;if(_lv_area_intersect(&clip,ctx->clip_area,&textArea)){const lv_area_t* saved=ctx->clip_area;ctx->clip_area=&clip;
    lv_draw_label(ctx,&d,&textArea,title,nullptr);
    if(state){d.font=&lv_font_montserrat_10;d.color=color(0xb5cfc3);textArea.y1=y+17;lv_draw_label(ctx,&d,&textArea,state+1,nullptr);}
    ctx->clip_area=saved;
  }
#else
  lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.radius=uiAscii()?0:S(4);box.bg_color=color(dragon::RadarLabelBackground);box.bg_opa=LV_OPA_COVER;
  box.border_color=color(c);box.border_opa=LV_OPA_80;box.border_width=dot.index==radarPrimary?2:1;
  lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w),(lv_coord_t)(y+h)};lv_draw_rect(ctx,&box,&area);
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(c);d.font=uiFont(FCALLOUT);
  lv_area_t textArea{(lv_coord_t)(x+S(4)),(lv_coord_t)(y+S(3)),(lv_coord_t)(x+w-S(4)),(lv_coord_t)(y+h-S(3))};
  // Clip locally so an unusually long translated/family label cannot escape.
  lv_area_t clip;if(_lv_area_intersect(&clip,ctx->clip_area,&textArea)){const lv_area_t* saved=ctx->clip_area;ctx->clip_area=&clip;lv_draw_label(ctx,&d,&textArea,text,nullptr);ctx->clip_area=saved;}
#endif
}
static void radarDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);const auto& theme=radarPalette(radarTheme);
  // LVGL may render several internal-RAM tiles for one frame. Advance signal
  // positions just once so leader lines and dots cannot disagree across tiles.
  const uint32_t now=radarFrameAt;
  static uint32_t plotFrame=UINT32_MAX;const bool advancePlot=plotFrame!=now;plotFrame=now;
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(theme.ring);arc.width=std::max(1,S(1));arc.opa=160;
  if(radarTheme==CanCommanderTheme)canRadarField(ctx,now,radioEnabled);else if(radarTheme==31)capsuleRadarField(ctx,now);else if(radarTheme==32)sparrowsRadarField(ctx);else{
#ifdef BOARD_RADAR_LITE
  if(liteBackdrop.data){lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_area_t a{0,0,SCREEN_RES-1,SCREEN_RES-1};lv_draw_img(ctx,&d,&a,&liteBackdrop);}
  else disc(ctx,CENTER,CENTER,S(178),theme.bottom);

#else
  disc(ctx,CENTER,CENTER,S(178),theme.grid);disc(ctx,CENTER,CENTER,S(176),theme.ring);
  lv_draw_rect_dsc_t glass;lv_draw_rect_dsc_init(&glass);glass.radius=LV_RADIUS_CIRCLE;
  glass.bg_color=color(theme.top);glass.bg_grad.dir=LV_GRAD_DIR_VER;glass.bg_grad.stops[0].color=glass.bg_color;glass.bg_grad.stops[1].color=color(theme.bottom);
  lv_area_t field{(lv_coord_t)S(9),(lv_coord_t)S(9),(lv_coord_t)(SCREEN_RES-S(9)-1),(lv_coord_t)(SCREEN_RES-S(9)-1)};lv_draw_rect(ctx,&glass,&field);
  static const int reaches[]={77,119,143,157,166,169,166,157,143,119,77};
  if(radarTheme<6)for(int n=0;n<11;++n){int axis=CENTER+S(n*30-150),reach=S(reaches[n]);line(ctx,axis,CENTER-reach,axis,CENTER+reach,theme.grid,S(1),180);line(ctx,CENTER-reach,axis,CENTER+reach,axis,theme.grid,S(1),180);}
  else if(radarTheme==9){
    // Covert: keep the field clear beneath the CI mark and silver pick sweep.
  }else if(radarTheme==6||radarTheme==8||radarTheme==21){
    // Pin chambers evoke a lock cylinder while leaving signal labels legible.
    for(int i=0;i<5;++i){int x=S(140+i*20);line(ctx,x-S(6),S(112),x-S(6),S(160),theme.grid,S(1));line(ctx,x+S(6),S(112),x+S(6),S(160),theme.grid,S(1));
      for(int n=0;n<5;++n)line(ctx,x-S(4),S(114+n*4),x+S(4),S(117+n*4),theme.grid,S(1));
      line(ctx,x,S(140+(i%3)*3),x,S(157),theme.ring,S(3),100);
    }
    line(ctx,S(124),S(163),S(236),S(163),theme.ring,S(1),100);
  }else if(radarTheme==10)radarAwokArt(ctx);
  else if(radarTheme==11)radarBaphArt(ctx);
  else if(radarTheme==12)radarWdgArt(ctx);
  else if(radarTheme==13)radarGhostArt(ctx);
  else if(radarTheme==14)radarN3rdArt(ctx);
  else if(radarTheme==15)radarHaleArt(ctx);
  else if(radarTheme==16)radarWaveArt(ctx);
  else if(radarTheme==17)radarM5Art(ctx);
  else if(radarTheme==18)radarLabArt(ctx);
  else if(radarTheme==19)radarSasquachArt(ctx);
  else if(radarTheme==20)radarNyanArt(ctx);
  else if(radarTheme==22)radarSeeedArt(ctx);
  else if(radarTheme==23)radarDagArt(ctx);
  else if(radarTheme==24)radarPigArt(ctx);
  else if(radarTheme==25)radarDeflockArt(ctx);
  else if(radarTheme>=26){} // Emblem is drawn once below the rings.
  else{
    for(int side:{-1,1})for(int n=0;n<4;++n){int y=115+n*30,x=180+side*(102+n%2*12),elbow=x-side*32;
      disc(ctx,S(x),S(y),S(4),0x686868);disc(ctx,S(x),S(y),S(2),0x080808);
      line(ctx,S(x),S(y),S(elbow),S(y),0x505050,S(2));line(ctx,S(elbow),S(y),S(elbow-side*18),S(y+18),0x505050,S(2));line(ctx,S(elbow-side*18),S(y+18),S(180+side*32),S(y+18),0x505050,S(2));
    }
  }
  lv_point_t center{(lv_coord_t)CENTER,(lv_coord_t)CENTER};
  if(radarTheme&&radarTheme!=24)for(int radius=42;radius<169;radius+=42)lv_draw_arc(ctx,&arc,&center,S(radius),0,359);
  if(radarTheme!=24){arc.width=S(2);lv_draw_arc(ctx,&arc,&center,S(170),0,359);}
#endif
  }
  if(radarTheme==9){
    const auto& logo=radar_logo_mcnally;int x=(SCREEN_RES-logo.header.w)/2,y=(SCREEN_RES-logo.header.h)/2;
    lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+logo.header.w-1),(lv_coord_t)(y+logo.header.h-1)};
    lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.opa=180;lv_draw_img(ctx,&d,&area,&logo);
  }
  if(radarTheme==26)radarLplEmblem(ctx);
  // Sweep and pulse are animation, not measurements of bearing or RF activity.
  static uint32_t sweepAt=0;if(radioEnabled)sweepAt=now;
  float angle=(sweepAt%6000)*.00104719755f,dx=cosf(angle),dy=sinf(angle);
  if(radarTheme==28){locksportPinRim(ctx,uint64_t(sweepAt%6000)*10000);lpuEmblem(ctx,150);}
  if(radarTheme==dragon::PersonalThemeId)personalThemeSweep(ctx,angle);
  else if(radarTheme==CanCommanderTheme)canRadarSweep(ctx,angle/6.28318530718f+.25f);
  else if(radarTheme==31){}
  else if(radarTheme==33)crowFeather(ctx,angle/6.28318530718f+.25f,164,17,0xc7d5e0);
  else if(radarTheme==32)sparrowsPick(ctx,angle/6.28318530718f+.25f,146,true);
  else if(radarTheme==29)boneSweep(ctx,angle);
  else if(radarTheme==30)cyberSweep(ctx,angle);
  else if(radarTheme==27)ghostProtonSweep(ctx,angle,sweepAt);
  else if(radarTheme==26)radarLplScales(ctx,(sweepAt%12000)*.000523598776f);
#ifndef BOARD_RADAR_LITE
  else if(radarTheme==9)radarPick(ctx,angle);
  else if(radarTheme==24)radarPigSweep(ctx,angle);
#endif
  else {
  lv_draw_rect_dsc_t trail;lv_draw_rect_dsc_init(&trail);trail.bg_color=color(theme.sweep);
  for(int n=0;n<18;++n){float nextX=dx*.998629535f+dy*.052335956f,nextY=dy*.998629535f-dx*.052335956f;
    lv_point_t wedge[]={{(lv_coord_t)CENTER,(lv_coord_t)CENTER},{(lv_coord_t)(CENTER+int(dx*S(166))),(lv_coord_t)(CENTER+int(dy*S(166)))},{(lv_coord_t)(CENTER+int(nextX*S(166))),(lv_coord_t)(CENTER+int(nextY*S(166)))}};
    trail.bg_opa=58-n*3;lv_draw_triangle(ctx,&trail,wedge);dx=nextX;dy=nextY;
  }
  line(ctx,CENTER,CENTER,CENTER+int(cosf(angle)*S(166)),CENTER+int(sinf(angle)*S(166)),theme.sweep,S(2),200);
  }
  if(!radarPrepared || uint32_t(now-radarPrepared)>=250){
    radarPrepared=now;radarCount=0;
    for(size_t i=0;i<MAX_CONTACTS;++i)if(radarVisible(contacts[i],now))radarOrder[radarCount++]=i;
    std::sort(radarOrder,radarOrder+radarCount,[now](int a,int b){if(a==trackedContact||b==trackedContact)return a==trackedContact&&b!=trackedContact;
      return preferContact(contacts[a],contacts[b],now);});
  }
  if(radarHasSelection&&dragon::radarSelectionExpired(now,radarSelectedAt))radarHasSelection=false;
  dotCount=0;
  for(unsigned k=0;k<unsigned(radarCount);++k){int i=radarOrder[k];auto& c=contacts[i];if(!radarVisible(c,now))continue;
    float target=c.signal.count?dragon::radarRadius(c.signal.smooth):158.f;if(!c.plotRadius)c.plotRadius=target;else if(advancePlot&&!(radarHasSelection&&contactKey(c)==radarSelectedKey))c.plotRadius+=(target-c.plotRadius)*.35f;
    auto plot=dragon::radarPlotPoint(c.plotRadius,c.radarX,c.radarY);dots[dotCount++]={S(plot.x),S(plot.y),i,contactKey(c)};
  }
  // Paint low-priority dots first so a coincident tracker cannot cover Axon.
  for(int i=dotCount-1;i>=0;--i){auto& dot=dots[i];auto& c=contacts[dot.index];int x=dot.x,y=dot.y;uint32_t age=now-c.seen;uint8_t opacity=std::max<uint8_t>(205,dragon::radarOpacity(age));Family family=visibleFamily(c,now,detector.families);uint32_t deviceColor=contactColor(c);
    #ifndef BOARD_RADAR_LITE
    if(radarTheme==24){asciiMark(ctx,age>=30000?"?":dot.index==trackedContact?"@":"*",x*360/SCREEN_RES,y*360/SCREEN_RES,deviceColor);continue;}
#endif
    bool tracker=dragon::trackerFamily(family);disc(ctx,x,y,S(tracker?5:8),dragon::RadarLabelBackground);
    if(radarHasSelection&&dot.key==radarSelectedKey){auto selected=arc;selected.color=color(0xffffff);selected.width=std::max(1,S(2));selected.opa=255;lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};lv_draw_arc(ctx,&selected,&point,S(12),0,359);}
    if(age>=30000){auto stale=arc;stale.color=color(deviceColor);stale.opa=opacity;stale.width=S(2);lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};
      for(int a=0;a<360;a+=90)lv_draw_arc(ctx,&stale,&point,S(6),a,a+48);
    }else if(tracker){disc(ctx,x,y,S(3),deviceColor,opacity);}
    else{unsigned pulse=radioEnabled?unsigned((now+contactHash(contactKey(c)))%1800):900;pulse=pulse>900?1800-pulse:pulse;disc(ctx,x,y,S(9),deviceColor,unsigned(opacity)*(25+pulse/20)/255);disc(ctx,x,y,S(5),deviceColor,opacity);disc(ctx,x-S(1),y-S(1),S(2),0xffffff,opacity);}
    if(dot.index==trackedContact || (detector.enabled(c.trafficFamily)&&trafficFresh(c,now))){auto lock=arc;lock.color=color(dot.index==trackedContact?theme.text:0xff5040);lock.opa=220;lock.width=2;lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};lv_draw_arc(ctx,&lock,&point,S(12),0,359);}
  }
  // Dots are never paginated. Named leaders belong to priority devices and
  // the tapped contact; trackers otherwise consume no text or leader space.
  static dragon::RadarLabelCandidate candidates[MAX_CONTACTS];
  unsigned labels[RadarRenderLabelLimit];
  for(int i=0;i<dotCount;++i){auto& c=contacts[dots[i].index];candidates[i]={c.match.family,uint32_t(now-c.seen)>=30000,radarHasSelection&&dots[i].key==radarSelectedKey};}
  unsigned labelCount=dragon::selectRadarLabels(candidates,dotCount,labels,RadarRenderLabelLimit,radarLabels);
  radarPrimary=labelCount?dots[labels[0]].index:-1;
  dragon::RadarPoint points[RadarRenderLabelLimit];uint8_t preferred[RadarRenderLabelLimit];
  static dragon::DeviceKey labelKeys[RadarRenderLabelLimit];static uint8_t slots[RadarRenderLabelLimit];static unsigned priorCount=0;
  for(unsigned j=0;j<labelCount;++j){auto& dot=dots[labels[j]];points[j]={dot.x*360/SCREEN_RES,dot.y*360/SCREEN_RES};preferred[j]=255;for(unsigned k=0;k<priorCount;++k)if(labelKeys[k]==dot.key)preferred[j]=slots[k];}
  dragon::placeRadarLabels(points,labelCount,slots,preferred,RadarRenderLite);priorCount=labelCount;
  for(unsigned j=0;j<labelCount;++j){auto& dot=dots[labels[j]];labelKeys[j]=dot.key;dot.labelSlot=slots[j];auto& c=contacts[dot.index];uint32_t age=now-c.seen;
    char state[24],text[64];if(age>=10000)snprintf(state,sizeof(state),"%lus ago",(unsigned long)(age/1000));else snprintf(state,sizeof(state),"%d dBm",c.rssi);
    Family family=c.match.family;snprintf(text,sizeof(text),"%s\n%s",radarFamilyName(family),state);callout(ctx,dot,slots[j],text,contactColor(c));
    #ifndef BOARD_RADAR_LITE
    if(radarTheme==24)asciiMark(ctx,"*",dot.x*360/SCREEN_RES,dot.y*360/SCREEN_RES,contactColor(c));else
#endif
    disc(ctx,dot.x,dot.y,S(5),contactColor(c));
  }
  // Decorative center cursor, not a compass. Keep it above the sweep/labels.
  if(radarTheme==CanCommanderTheme)return;
#ifdef BOARD_RADAR_LITE
  for(int sign:{-1,1}){line(ctx,CENTER+sign*3,CENTER,CENTER+sign*8,CENTER,theme.ring,1);line(ctx,CENTER,CENTER+sign*3,CENTER,CENTER+sign*8,theme.ring,1);}
  disc(ctx,CENTER,CENTER,1,theme.text);
#else
  if(radarTheme==29){disc(ctx,CENTER,CENTER,S(5),0xffedcf);disc(ctx,CENTER,CENTER,S(2),0xa52c3d);}
  else if(radarTheme==30){line(ctx,CENTER-S(5),CENTER-S(5),CENTER+S(5),CENTER+S(5),0xff54d8,S(2));line(ctx,CENTER-S(5),CENTER+S(5),CENTER+S(5),CENTER-S(5),0x4ae9ff,S(2));}
  else if(radarTheme==26){} // The balance already has its own jeweled pivot.
  else if(radarTheme==24)asciiMark(ctx,"+",180,180,theme.sweep);
  else if(radarTheme==9){disc(ctx,CENTER,CENTER,S(7),0x000000);disc(ctx,CENTER,CENTER,S(5),0xc5cdd3);disc(ctx,CENTER,CENTER,S(3),0x30383e);line(ctx,CENTER-S(2),CENTER+S(1),CENTER+S(2),CENTER-S(1),0xe7ecef,std::max(1,S(1)));}
  else for(int y=-7;y<=6;++y){int half=(y+7)/2;line(ctx,CENTER-S(half),CENTER+S(y),CENTER+S(half),CENTER+S(y),theme.sweep,S(1)+1);}
#endif
}
