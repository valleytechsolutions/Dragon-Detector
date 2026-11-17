// Shared by full UI and the radar-only Lite UI. Coordinates use the 360px design.
#ifdef BOARD_M5DIAL
static constexpr unsigned RadarRenderLabelLimit=2;
static constexpr bool RadarRenderLite=true;
#elif defined(BOARD_RADAR_LITE)
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
static uint32_t radarPass=0; // one per lv_timer_handler() call, so one per rendered frame
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
#include "radar_fx.inc"
#ifndef BOARD_RADAR_LITE
#define RADAR_BACKDROP 1
// Static radar field painted into PSRAM the first time the radar opens with a
// theme. It stays cached across pages, so returning to the radar is instant.
static lv_img_dsc_t radarBackdrop{};
static uint32_t backdropKey=0;
static bool backdropReady=false;
#endif
#include "lpl_radar.inc"
#include "valleytech_radar.inc"
#include "containment_radar.inc"
#include "cyber_runner.inc"
#include "capsule_theme.inc"
#include "sparrows_theme.inc"
#include "dzaz_theme.inc"
#include "can_commander_theme.inc"
#include "green_art_themes.inc"
#include "muldersrevenge_theme.inc"
#include "mitm_theme.inc"
#include "vectorpotential_theme.inc"
#include "argyle_theme.inc"
#include "lawlocktools_theme.inc"
#include "moki_theme.inc"
#include "delta44_theme.inc"
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
#ifdef BOARD_M5DIAL
  line(ctx,dot.x,dot.y,edgeX,edgeY,c,1,190);
#elif defined(BOARD_RADAR_LITE)
  line(ctx,dot.x,dot.y,edgeX,edgeY,dragon::RadarLabelBackground,S(4),255);
  line(ctx,dot.x,dot.y,edgeX,edgeY,c,std::max(2,S(2)),255);
#endif
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
  // Dark glass plate, hairline border and accent bar in the device colour.
  fxLeader(ctx,dot.x,dot.y,edgeX,edgeY,c,dragon::RadarLabelBackground);
  fxCallout(ctx,x,y,w,h,text,c,dot.index==radarPrimary,uiAscii(),uiFont(FCALLOUT),dragon::RadarLabelBackground,0xb9c2c9);
#endif
}
// Box a callout repaints: its plate, its leader and the dot redrawn over it.
static dragon::RadarRect calloutBounds(const Dot& dot,unsigned slot){
  auto b=dragon::radarLabelSlot(slot,RadarRenderLite);const int x=S(b.x),y=S(b.y),w=S(b.w),h=S(b.h),pad=S(4)+2;
  auto box=dragon::radarRect(x-2,y-2,x+w+2,y+h+2);
  const int ex=constrain(dot.x,x,x+w),ey=constrain(dot.y,y,y+h);
  box=dragon::rectUnion(box,dragon::radarRect(std::min(dot.x,ex)-pad,std::min(dot.y,ey)-pad,std::max(dot.x,ex)+pad,std::max(dot.y,ey)+pad));
  return dragon::rectUnion(box,dragon::rectAround(dot.x,dot.y,S(8)+2));
}

// ---- field -------------------------------------------------------------------
// These fields move with time, so they are drawn on every frame, never cached.
static bool radarFieldAnimated(unsigned theme){return theme==CanCommanderTheme||theme==31||theme==15||theme==20;}
// Everything under the sweep: rings, grid and per-theme artwork.
static void radarField(lv_draw_ctx_t* ctx,uint32_t now){
  const auto& theme=radarPalette(radarTheme);
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(theme.ring);arc.width=std::max(1,S(1));arc.opa=160;
  if(radarTheme==3)valleyRadarField(ctx);else if(radarTheme==Delta44Theme)delta44RadarField(ctx);else if(radarTheme==LawLockToolsTheme)lltRadarField(ctx);else if(radarTheme==MokiTheme)mokiRadarField(ctx);else if(radarTheme==ArgyleTheme)argyleRadarField(ctx);else if(radarTheme==VectorPotentialTheme)vpRadarField(ctx);else if(radarTheme==MuldersRevengeTheme)muldersRadarField(ctx);else if(greenArtTheme(radarTheme))greenRadarField(ctx,radarTheme==XombiTheme);else if(radarTheme==CanCommanderTheme)canRadarField(ctx,now,radioEnabled);else if(radarTheme==31)capsuleRadarField(ctx,now);else if(radarTheme==32)sparrowsRadarField(ctx);else{
#ifdef BOARD_RADAR_LITE
  if(liteBackdrop.data){lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_area_t a{0,0,SCREEN_RES-1,SCREEN_RES-1};lv_draw_img(ctx,&d,&a,&liteBackdrop);}
  else {
    disc(ctx,CENTER,CENTER,S(178),theme.bottom);
#ifdef BOARD_M5DIAL
    lv_point_t center{CENTER,CENTER};auto grid=arc;grid.color=color(theme.grid);grid.width=1;grid.opa=160;
    for(int radius:{30,60,90})lv_draw_arc(ctx,&grid,&center,radius,0,359);
    line(ctx,20,120,220,120,theme.grid,1,130);line(ctx,120,65,120,181,theme.grid,1,130);
    grid.color=color(theme.ring);lv_draw_arc(ctx,&grid,&center,116,0,359);
#endif
  }
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
}
#ifdef RADAR_BACKDROP
static void releaseRadarBackdrop(){
  lv_img_cache_invalidate_src(&radarBackdrop);free((void*)radarBackdrop.data);radarBackdrop={};backdropReady=false;
}
// Run LVGL's software renderer over a full-screen RGB565 buffer, the way
// lv_canvas does, so field code written for the live radar caches unchanged.
static void radarPaintInto(lv_color_t* pixels,void (*paint)(lv_draw_ctx_t*)){
  lv_area_t area{0,0,SCREEN_RES-1,SCREEN_RES-1};
  lv_disp_t disp;lv_disp_drv_t drv;lv_memset_00(&disp,sizeof(disp));disp.driver=&drv;lv_disp_drv_init(&drv);drv.hor_res=SCREEN_RES;drv.ver_res=SCREEN_RES;
  auto* ctx=(lv_draw_ctx_t*)lv_mem_alloc(drv.draw_ctx_size);if(!ctx)return;
  lv_memset_00(ctx,drv.draw_ctx_size);drv.draw_ctx_init(&drv,ctx);drv.draw_ctx=ctx;
  ctx->clip_area=&area;ctx->buf_area=&area;ctx->buf=pixels;
  lv_disp_t* refreshing=_lv_refr_get_disp_refreshing();_lv_refr_set_disp_refreshing(&disp);
  paint(ctx);
  _lv_refr_set_disp_refreshing(refreshing);drv.draw_ctx_deinit(&drv,ctx);lv_mem_free(ctx);
}
// Theme plus every colour the field reads, so a new personal theme or UI
// palette repaints the cache.
static uint32_t radarBackdropKey(){
  const auto p=radarPalette(radarTheme);uint32_t h=2166136261u;
  for(uint32_t v:{uint32_t(radarTheme),p.top,p.bottom,p.grid,p.ring,p.sweep,uiColors().bg})h=(h^v)*16777619u;
  return h;
}
static bool radarBackdropValid(){return backdropReady&&radarBackdrop.data&&backdropKey==radarBackdropKey();}
static void prepareRadarBackdrop(){
  fxPolarTable(CENTER,CENTER); // once per boot: the phosphor sweep reads it every frame
  if(radarFieldAnimated(radarTheme))return; // drawn live; keep any cached field for later
  const uint32_t key=radarBackdropKey();
  if(backdropReady&&radarBackdrop.data&&backdropKey==key)return;
  auto* pixels=(lv_color_t*)radarBackdrop.data;
  if(!pixels)pixels=(lv_color_t*)ps_malloc(SCREEN_RES*SCREEN_RES*sizeof(lv_color_t));
  if(!pixels){radarBackdrop={};backdropReady=false;return;} // live drawing remains the fallback
  lv_img_cache_invalidate_src(&radarBackdrop);
  if(radarTheme==3)valleyBackdrop(pixels,SCREEN_RES);
  else{
    const lv_color_t bg=color(uiColors().bg);for(unsigned i=0;i<unsigned(SCREEN_RES)*SCREEN_RES;++i)pixels[i]=bg;
    radarPaintInto(pixels,[](lv_draw_ctx_t* ctx){radarField(ctx,radarFrameAt);});
  }
  radarBackdrop={};radarBackdrop.header.cf=LV_IMG_CF_TRUE_COLOR;radarBackdrop.header.w=SCREEN_RES;radarBackdrop.header.h=SCREEN_RES;
  radarBackdrop.data_size=SCREEN_RES*SCREEN_RES*sizeof(lv_color_t);radarBackdrop.data=(const uint8_t*)pixels;backdropKey=key;backdropReady=true;
}
#endif

// ---- sweep ---------------------------------------------------------------------
// Valleytech and the shared field use the phosphor sweep, whose repaint area
// is known. Every other sweep shape repaints the whole radar each frame.
enum class RadarSweep:uint8_t {Valley,Shared,Custom};
static RadarSweep radarSweepKind(unsigned theme){
  if(theme==3)return RadarSweep::Valley;
  if(theme==dragon::PersonalThemeId||theme==LawLockToolsTheme||theme==Delta44Theme||theme==MokiTheme||theme==ArgyleTheme||theme==CanCommanderTheme||theme==31||theme==33||theme==32||theme==29||theme==30||theme==27||theme==26||theme==28||theme==9||theme==24)return RadarSweep::Custom;
  return RadarSweep::Shared;
}
static FxSweepStyle radarSharedSweep(){const auto& theme=radarPalette(radarTheme);return fxThemeSweepStyle(theme.sweep,theme.bottom,theme.ring);}
static float radarSweepAngle=0;static uint32_t radarSweepAt=0;
static dragon::RadarOutline radarSweepOutline(float angle){
  return radarSweepKind(radarTheme)==RadarSweep::Valley?fxSweepOutline(CENTER,CENTER,angle,valleySweepStyle()):fxSweepOutline(CENTER,CENTER,angle,radarSharedSweep());
}
static void radarSweep(lv_draw_ctx_t* ctx){
  const auto& theme=radarPalette(radarTheme);const uint32_t sweepAt=radarSweepAt;const float angle=radarSweepAngle;
  if(radarTheme==28){locksportPinRim(ctx,uint64_t(sweepAt%6000)*10000);lpuEmblem(ctx,150);}
  if(radarTheme==3)valleyRadarSweep(ctx,angle);
  else if(radarTheme==dragon::PersonalThemeId)personalThemeSweep(ctx,angle);
  else if(radarTheme==LawLockToolsTheme)lltRadarSweep(ctx,angle/6.28318530718f+.25f);
  else if(radarTheme==Delta44Theme)delta44RadarSweep(ctx,angle/6.28318530718f+.25f);
  else if(radarTheme==MokiTheme)mokiRadarSweep(ctx,angle/6.28318530718f+.25f);
  else if(radarTheme==ArgyleTheme)argyleRadarSweep(ctx,angle/6.28318530718f+.25f);
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
  else fxSweep(ctx,CENTER,CENTER,angle,radarSharedSweep());
#else
  else {
  float dx=cosf(angle),dy=sinf(angle);
  lv_draw_rect_dsc_t trail;lv_draw_rect_dsc_init(&trail);trail.bg_color=color(theme.sweep);
#ifdef BOARD_M5DIAL
  constexpr int sweepSlices=9,fadeStep=6;constexpr float sweepCos=.994521895f,sweepSin=.104528463f;
#else
  constexpr int sweepSlices=18,fadeStep=3;constexpr float sweepCos=.998629535f,sweepSin=.052335956f;
#endif
  for(int n=0;n<sweepSlices;++n){float nextX=dx*sweepCos+dy*sweepSin,nextY=dy*sweepCos-dx*sweepSin;
    lv_point_t wedge[]={{(lv_coord_t)CENTER,(lv_coord_t)CENTER},{(lv_coord_t)(CENTER+int(dx*S(166))),(lv_coord_t)(CENTER+int(dy*S(166)))},{(lv_coord_t)(CENTER+int(nextX*S(166))),(lv_coord_t)(CENTER+int(nextY*S(166)))}};
    trail.bg_opa=58-n*fadeStep;lv_draw_triangle(ctx,&trail,wedge);dx=nextX;dy=nextY;
  }
  line(ctx,CENTER,CENTER,CENTER+int(cosf(angle)*S(166)),CENTER+int(sinf(angle)*S(166)),theme.sweep,S(2),200);
  }
#endif
  (void)theme;
}

// ---- frame model ---------------------------------------------------------------
// Time since a contact event. A repaint between frames paints with the last
// frame's time while the radio keeps updating contacts, so an event can be
// newer than `now`; that counts as just now, not as 49 days ago.
static inline uint32_t radarAge(uint32_t now,uint32_t at){return int32_t(now-at)<0?0:now-at;}
// radarFrame() runs once per frame: it advances dot positions and chooses the
// labels. radarDraw() only paints that model, for as many tiles as LVGL asks.
struct RadarLabelPlan {int dot=-1;uint8_t slot=0;uint32_t color=0;char text[48]{};};
#ifdef BOARD_RADAR_LITE
static RadarLabelPlan radarLabelPlanStore[RadarRenderLabelLimit];
static RadarLabelPlan* const radarLabelPlan=radarLabelPlanStore;
#else
// PSRAM, like the other radar bookkeeping: internal RAM is kept for the radios.
static RadarLabelPlan* radarLabelPlan=nullptr;
#endif
static unsigned radarLabelCount=0;
static uint32_t radarFrameBuilt=UINT32_MAX;
static void radarFrame(uint32_t now){
  radarFrameBuilt=now;
  static uint32_t sweepAt=0;if(radioEnabled)sweepAt=now;
  radarSweepAt=sweepAt;radarSweepAngle=(sweepAt%6000)*.00104719755f;
  if(!radarPrepared || uint32_t(now-radarPrepared)>=250){
    radarPrepared=now;radarCount=0;
    for(size_t i=0;i<MAX_CONTACTS;++i)if(radarVisible(contacts[i],now))radarOrder[radarCount++]=i;
    std::sort(radarOrder,radarOrder+radarCount,[now](int a,int b){if(a==trackedContact||b==trackedContact)return a==trackedContact&&b!=trackedContact;
      return preferContact(contacts[a],contacts[b],now);});
  }
#ifndef BOARD_M5DIAL
  if(radarHasSelection&&dragon::radarSelectionExpired(now,radarSelectedAt))radarHasSelection=false;
#endif
  dotCount=0;
  for(unsigned k=0;k<unsigned(radarCount);++k){int i=radarOrder[k];auto& c=contacts[i];if(!radarVisible(c,now))continue;
    float target=c.signal.count?dragon::radarRadius(c.signal.smooth):158.f;if(!c.plotRadius)c.plotRadius=target;else if(!(radarHasSelection&&contactKey(c)==radarSelectedKey))c.plotRadius+=(target-c.plotRadius)*.35f;
    auto plot=dragon::radarPlotPoint(c.plotRadius,c.radarX,c.radarY);dots[dotCount++]={S(plot.x),S(plot.y),i,contactKey(c)};
  }
  // Dots are never paginated. Descriptions prioritize ALPR/cameras, then other
  // device families. Find My and iBeacons consume leader space only when tapped.
  static dragon::RadarLabelCandidate candidates[MAX_CONTACTS];
  unsigned labels[RadarRenderLabelLimit];
  for(int i=0;i<dotCount;++i){auto& c=contacts[dots[i].index];candidates[i]={c.match.family,uint32_t(now-c.seen)>=30000,radarHasSelection&&dots[i].key==radarSelectedKey};}
  unsigned labelCount=dragon::selectRadarLabels(candidates,dotCount,labels,RadarRenderLabelLimit,radarLabels);
  radarPrimary=labelCount?dots[labels[0]].index:-1;
  dragon::RadarPoint points[RadarRenderLabelLimit];uint8_t preferred[RadarRenderLabelLimit];
  static dragon::DeviceKey labelKeys[RadarRenderLabelLimit];static uint8_t slots[RadarRenderLabelLimit];static unsigned priorCount=0;
  for(unsigned j=0;j<labelCount;++j){auto& dot=dots[labels[j]];points[j]={dot.x*360/SCREEN_RES,dot.y*360/SCREEN_RES};preferred[j]=255;for(unsigned k=0;k<priorCount;++k)if(labelKeys[k]==dot.key)preferred[j]=slots[k];}
  dragon::placeRadarLabels(points,labelCount,slots,preferred,RadarRenderLite);priorCount=labelCount;
#ifndef BOARD_RADAR_LITE
  if(!radarLabelPlan)radarLabelPlan=(RadarLabelPlan*)heap_caps_calloc(RadarRenderLabelLimit,sizeof(RadarLabelPlan),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!radarLabelPlan)labelCount=0; // dots still draw; only the callouts are skipped
#endif
  radarLabelCount=labelCount;
  for(unsigned j=0;j<labelCount;++j){auto& dot=dots[labels[j]];labelKeys[j]=dot.key;dot.labelSlot=slots[j];auto& c=contacts[dot.index];uint32_t age=radarAge(now,c.seen);
    char state[24];if(age>=10000)snprintf(state,sizeof(state),"%lus ago",(unsigned long)(age/1000));else snprintf(state,sizeof(state),"%d dBm",c.rssi);
    auto& plan=radarLabelPlan[j];plan.dot=labels[j];plan.slot=slots[j];plan.color=contactColor(c);snprintf(plan.text,sizeof(plan.text),"%s\n%s",radarFamilyName(c.match.family),state);
  }
}
static bool radarDotCurrent(const Dot& dot){auto& c=contacts[dot.index];return c.used&&contactKey(c)==dot.key;}

// What one dot shows this frame. The same values drive painting and the
// repaint bookkeeping, so a dot repaints only when its look changes.
enum class DotKind:uint8_t {Live,Fading,Stale,Tracker};
struct RadarDotLook {int x=0,y=0;uint32_t color=0;DotKind kind=DotKind::Live;uint8_t opa=255,halo=0,ripple=0,rippleR=0;bool selected=false,tracked=false,traffic=false;uint16_t spin=0;};
static RadarDotLook radarDotLook(const Dot& dot,uint32_t now){
  auto& c=contacts[dot.index];RadarDotLook l;l.x=dot.x;l.y=dot.y;
  const uint32_t age=radarAge(now,c.seen);const auto fresh=dragon::radarFreshness(age);l.opa=dragon::radarOpacity(age);l.color=contactColor(c);
  const Family family=visibleFamily(c,now,detector.families);
  l.kind=age>=30000?DotKind::Stale:dragon::trackerFamily(family)?DotKind::Tracker:fresh==dragon::RadarFreshness::Fading?DotKind::Fading:DotKind::Live;
  if(l.kind==DotKind::Live&&radioEnabled&&c.signal.count){
    const uint32_t since=now-c.signal.last,s=int32_t(since)<0?0:since;
#ifdef BOARD_RADAR_LITE
    l.halo=dragon::radarHalo(s);
#else
    const auto r=fxRipple(s,l.opa);l.ripple=r.opa;l.rippleR=r.radius;
#endif
  }
  l.selected=radarHasSelection&&dot.key==radarSelectedKey;
  l.tracked=dot.index==trackedContact;l.traffic=!l.tracked&&detector.enabled(c.trafficFamily)&&trafficFresh(c,now);
  if(l.tracked)l.spin=uint16_t((now/8)%360);
  return l;
}
static dragon::RadarRect radarLookBounds(const RadarDotLook& l){
  int reach=S(8);
  if(l.kind==DotKind::Live)reach=std::max(reach,S(11));
  if(l.ripple)reach=std::max(reach,int(l.rippleR)+S(2));
  if(l.selected||l.tracked||l.traffic)reach=std::max(reach,S(13)+S(2));
  return dragon::rectAround(l.x,l.y,reach+2);
}
static uint32_t radarHashMix(uint32_t h,uint32_t v){h^=v+0x9e3779b9u+(h<<6)+(h>>2);return h;}
static uint32_t radarLookHash(const RadarDotLook& l){
  uint32_t h=radarHashMix(uint32_t(l.x)<<16|uint16_t(l.y),l.color);
  h=radarHashMix(h,uint32_t(l.kind)|l.opa<<8|l.ripple<<16|uint32_t(l.rippleR)<<24);
  return radarHashMix(h,uint32_t(l.halo)|l.selected<<8|l.tracked<<9|l.traffic<<10|uint32_t(l.spin)<<12);
}
static void radarPaintDot(lv_draw_ctx_t* ctx,const Dot& dot,const RadarDotLook& l,const RadarPalette& theme){
  auto& c=contacts[dot.index];const int x=l.x,y=l.y;
#ifndef BOARD_RADAR_LITE
  if(radarTheme==24){const uint32_t age=radarAge(radarFrameAt,c.seen);asciiMark(ctx,age>=30000?"?":l.tracked?"@":l.kind==DotKind::Fading?"o":"*",x*360/SCREEN_RES,y*360/SCREEN_RES,l.color);return;}
#endif
  (void)c;
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};
#ifdef BOARD_RADAR_LITE
  disc(ctx,x,y,S(l.kind==DotKind::Tracker?5:8),dragon::RadarLabelBackground);
#else
  disc(ctx,x,y,S(l.kind==DotKind::Tracker?5:7),dragon::RadarLabelBackground,170);
#endif
  if(l.selected){arc.color=color(0xffffff);arc.width=std::max(1,S(2));arc.opa=255;lv_draw_arc(ctx,&arc,&point,S(12),0,359);}
  if(l.kind==DotKind::Stale){arc.color=color(l.color);arc.opa=l.opa;arc.width=S(2);for(int a=0;a<360;a+=90)lv_draw_arc(ctx,&arc,&point,S(6),a,a+48);}
  else if(l.kind==DotKind::Tracker){
#ifdef BOARD_M5DIAL
    disc(ctx,x,y,3,l.color,l.opa);
#elif defined(BOARD_RADAR_LITE)
    disc(ctx,x,y,S(3),l.color,l.opa);
#else
    disc(ctx,x,y,S(6),l.color,l.opa/6);disc(ctx,x,y,S(3),l.color,l.opa);
#endif
  }
  else if(l.kind==DotKind::Fading)disc(ctx,x,y,S(5),l.color,l.opa);
  else{
#ifdef BOARD_RADAR_LITE
    if(l.halo)disc(ctx,x,y,S(9),l.color,l.halo);disc(ctx,x,y,S(5),l.color,l.opa);disc(ctx,x-S(1),y-S(1),S(2),0xffffff,l.opa);
#else
    FxRipple ripple;ripple.opa=l.ripple;ripple.radius=l.rippleR;fxLiveDot(ctx,x,y,l.color,l.opa,ripple);
#endif
  }
#ifdef BOARD_RADAR_LITE
  if(l.tracked||l.traffic){arc.color=color(l.tracked?theme.text:0xff5040);arc.opa=220;arc.width=2;lv_draw_arc(ctx,&arc,&point,S(12),0,359);}
#else
  if(l.tracked)fxLockBrackets(ctx,x,y,radarTheme==3?valley::Ice:theme.text,uint32_t(l.spin)*8);
  else if(l.traffic){arc.color=color(0xff5040);arc.opa=220;arc.width=2;lv_draw_arc(ctx,&arc,&point,S(12),0,359);}
#endif
}
// Decorative centre cursor, not a compass. Drawn above the sweep and labels.
static bool radarCenterBreathes(){return !RadarRenderLite&&(radarTheme==3||radarSweepKind(radarTheme)==RadarSweep::Shared);}
static dragon::RadarRect radarCenterBounds(){return radarTheme==3?valleyCenterBounds():fxCenterBounds();}
static void radarCenter(lv_draw_ctx_t* ctx,const RadarPalette& theme,uint32_t now){
  if(radarTheme==CanCommanderTheme||radarTheme==ArgyleTheme||radarTheme==LawLockToolsTheme||radarTheme==MokiTheme||radarTheme==Delta44Theme)return;
#ifdef BOARD_RADAR_LITE
  for(int sign:{-1,1}){line(ctx,CENTER+sign*3,CENTER,CENTER+sign*8,CENTER,theme.ring,1);line(ctx,CENTER,CENTER+sign*3,CENTER,CENTER+sign*8,theme.ring,1);}
  disc(ctx,CENTER,CENTER,1,theme.text);(void)now;
#else
  if(radarTheme==3)valleyCenter(ctx,now);
  else if(radarTheme==29){disc(ctx,CENTER,CENTER,S(5),0xffedcf);disc(ctx,CENTER,CENTER,S(2),0xa52c3d);}
  else if(radarTheme==30){line(ctx,CENTER-S(5),CENTER-S(5),CENTER+S(5),CENTER+S(5),0xff54d8,S(2));line(ctx,CENTER-S(5),CENTER+S(5),CENTER+S(5),CENTER-S(5),0x4ae9ff,S(2));}
  else if(radarTheme==26){} // The balance already has its own jeweled pivot.
  else if(radarTheme==24)asciiMark(ctx,"+",180,180,theme.sweep);
  else if(radarTheme==9){disc(ctx,CENTER,CENTER,S(7),0x000000);disc(ctx,CENTER,CENTER,S(5),0xc5cdd3);disc(ctx,CENTER,CENTER,S(3),0x30383e);line(ctx,CENTER-S(2),CENTER+S(1),CENTER+S(2),CENTER-S(1),0xe7ecef,std::max(1,S(1)));}
  else if(radarSweepKind(radarTheme)==RadarSweep::Shared)fxCenter(ctx,theme.sweep,dragon::RadarLabelBackground,now);
  else for(int y=-7;y<=6;++y){int half=(y+7)/2;line(ctx,CENTER-S(half),CENTER+S(y),CENTER+S(half),CENTER+S(y),theme.sweep,S(1)+1);}
#endif
}
#ifndef BOARD_RADAR_LITE
// Radar paints outside a frame's own refresh (a popup closing, a pressed
// button, a screenshot) use the last frame's model while contacts keep
// changing. radarDraw records those areas and the next frame repaints them.
static bool radarFrameRefresh=false;
static dragon::RadarDirty<6> radarOffFrame;
static bool radarTouchesClip(const lv_draw_ctx_t* ctx,const dragon::RadarRect& r){const auto* c=ctx->clip_area;return r.x1<=c->x2&&r.x2>=c->x1&&r.y1<=c->y2&&r.y2>=c->y1;}
#endif
static void radarDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);const auto& theme=radarPalette(radarTheme);
  const uint32_t now=radarFrameAt;
#ifndef BOARD_RADAR_LITE
  if(!radarFrameRefresh){const auto* c=ctx->clip_area;radarOffFrame.add(dragon::radarRect(c->x1,c->y1,c->x2,c->y2),SCREEN_RES);}
#endif
  if(radarFrameBuilt!=now)radarFrame(now); // first paint after a page build
#ifdef RADAR_BACKDROP
  if(!radarFieldAnimated(radarTheme)&&radarBackdropValid()){lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_area_t a{0,0,SCREEN_RES-1,SCREEN_RES-1};lv_draw_img(ctx,&d,&a,&radarBackdrop);}
  else
#endif
  radarField(ctx,now);
  // Sweep and pulse are animation, not measurements of bearing or RF activity.
  radarSweep(ctx);
  // Paint low-priority dots first so a coincident tracker cannot cover Axon.
  for(int i=dotCount-1;i>=0;--i){if(!radarDotCurrent(dots[i]))continue;const auto look=radarDotLook(dots[i],now);
#ifndef BOARD_RADAR_LITE
    // Arcs build their masks before clipping, so skip dots outside this tile.
    if(radarTheme!=24&&!radarTouchesClip(ctx,radarLookBounds(look)))continue;
#endif
    radarPaintDot(ctx,dots[i],look,theme);}
  for(unsigned j=0;j<radarLabelCount;++j){auto& plan=radarLabelPlan[j];if(plan.dot<0||plan.dot>=dotCount)continue;auto& dot=dots[plan.dot];if(!radarDotCurrent(dot))continue;
    auto& c=contacts[dot.index];const uint32_t age=radarAge(now,c.seen);
    callout(ctx,dot,plan.slot,plan.text,plan.color);
#ifndef BOARD_RADAR_LITE
    if(radarTheme==24)asciiMark(ctx,"*",dot.x*360/SCREEN_RES,dot.y*360/SCREEN_RES,plan.color);else
#endif
    {disc(ctx,dot.x,dot.y,S(5),dragon::RadarLabelBackground);const uint8_t opa=age>=30000?255:dragon::radarOpacity(age);disc(ctx,dot.x,dot.y,S(5),plan.color,opa); // backing first: one layer, not two
#ifndef BOARD_RADAR_LITE
     if(age<dragon::RadarLiveMs)disc(ctx,dot.x-S(1),dot.y-S(1),std::max(1,S(2)),0xffffff,uint8_t(opa*3/4));
#endif
    }
  }
#ifndef BOARD_RADAR_LITE
  if(radarCenterBreathes()&&!radarTouchesClip(ctx,radarCenterBounds()))return;
#endif
  radarCenter(ctx,theme,now);
}

#ifndef BOARD_RADAR_LITE
// ---- repaint bookkeeping (full boards) -------------------------------------------
// Each frame repaints only the sweep sector, the centre jewel, and dots and
// labels whose look changed; anything else repaints the whole radar.
struct RadarMark {dragon::DeviceKey key;dragon::RadarRect box;uint32_t sig=0,stamp=0;bool on=false;};
static RadarMark* radarMarks=nullptr; // one per contact slot, then one per label; PSRAM
static RadarMark* radarLabelMarks=nullptr;
static float radarSweepShown=0; // sweep angle of the last painted frame
static bool radarMarksValid=false;
struct RadarPaintStats {uint32_t frames=0,full=0,lastUs=0,maxUs=0,avgUs=0,lastPx=0,maxPx=0,sweepPx=0,markPx=0;uint64_t totalUs=0,totalPx=0;uint32_t interval=100;};
static RadarPaintStats radarStats;
static uint32_t radarPaintPixels=0; // pixels LVGL rendered in the last refresh
static void radarForgetMarks(){radarMarksValid=false;}
// Frame pacing: about a quarter of the loop for drawing, 10 to 15 frames/s.
static constexpr uint32_t RadarFastMs=66,RadarSlowMs=100;
static void radarRecordPaint(bool full,uint32_t us,uint32_t px){
  auto& s=radarStats;++s.frames;s.full+=full;s.lastUs=us;s.maxUs=std::max(s.maxUs,us);s.totalUs+=us;s.lastPx=px;s.maxPx=std::max(s.maxPx,px);s.totalPx+=px;
  s.avgUs=s.avgUs?(s.avgUs*7+us)/8:us;
#ifndef BOARD_WAVESHARE_185B // the 1.85B paces its UI itself and reports that interval
  s.interval=constrain(s.avgUs*4/1000,RadarFastMs,RadarSlowMs);
#endif
}
static void radarPrintStats(){
  const auto& s=radarStats;const uint32_t n=std::max<uint32_t>(1,s.frames);
  Serial.printf("RADAR_PAINT frames=%lu full=%lu partial=%lu last_us=%lu avg_us=%lu max_us=%lu mean_us=%lu last_px=%lu mean_px=%lu max_px=%lu sweep_px=%lu mark_px=%lu interval_ms=%lu backdrop=%u\n",
    (unsigned long)s.frames,(unsigned long)s.full,(unsigned long)(s.frames-s.full),(unsigned long)s.lastUs,(unsigned long)s.avgUs,(unsigned long)s.maxUs,(unsigned long)(s.totalUs/n),
    (unsigned long)s.lastPx,(unsigned long)(s.totalPx/n),(unsigned long)s.maxPx,(unsigned long)s.sweepPx,(unsigned long)s.markPx,(unsigned long)s.interval,unsigned(radarBackdropValid()));
}
static bool radarPartialTheme(){return radarSweepKind(radarTheme)!=RadarSweep::Custom&&!radarFieldAnimated(radarTheme)&&radarTheme!=24;}
// Returns true when the whole radar was invalidated.
static bool radarInvalidate(lv_obj_t* radar,uint32_t now){
  radarFrame(now);
  if(auto* disp=lv_disp_get_default())if(!disp->driver->monitor_cb)disp->driver->monitor_cb=[](lv_disp_drv_t*,uint32_t,uint32_t px){radarPaintPixels+=px;};
  if(!radarMarks){radarMarks=(RadarMark*)heap_caps_calloc(MAX_CONTACTS+RadarRenderLabelLimit,sizeof(RadarMark),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);radarLabelMarks=radarMarks?radarMarks+MAX_CONTACTS:nullptr;}
  dragon::RadarDirty<14> dirty;bool full=!radarMarks||!radarMarksValid||!radarPartialTheme()||radarOffFrame.full;
  if(!full)for(unsigned i=0;i<radarOffFrame.count;++i)dirty.add(radarOffFrame.rects[i],SCREEN_RES);
  radarOffFrame.clear();
  // Dots, by contact slot.
  if(radarMarks){
    static uint32_t stamp=0;++stamp;
    for(int i=0;i<dotCount;++i){const auto& dot=dots[i];if(!radarDotCurrent(dot))continue;
      const auto look=radarDotLook(dot,now);const auto box=radarLookBounds(look);const uint32_t sig=radarLookHash(look);auto& m=radarMarks[dot.index];
      if(!full&&(!m.on||!(m.key==dot.key)||m.sig!=sig||m.box.x1!=box.x1||m.box.y1!=box.y1||m.box.x2!=box.x2||m.box.y2!=box.y2)){if(m.on)dirty.add(m.box,SCREEN_RES);dirty.add(box,SCREEN_RES);}
      m.key=dot.key;m.box=box;m.sig=sig;m.stamp=stamp;m.on=true;
    }
    for(size_t i=0;i<MAX_CONTACTS;++i)if(radarMarks[i].on&&radarMarks[i].stamp!=stamp){if(!full)dirty.add(radarMarks[i].box,SCREEN_RES);radarMarks[i].on=false;}
  }
  // Labels, by position in the plan.
  if(radarLabelMarks)for(unsigned j=0;j<RadarRenderLabelLimit;++j){auto& m=radarLabelMarks[j];RadarMark next;
    if(j<radarLabelCount){auto& plan=radarLabelPlan[j];auto& dot=dots[plan.dot];auto& c=contacts[dot.index];
      uint32_t h=2166136261u;for(const char* p=plan.text;*p;++p)h=(h^uint8_t(*p))*16777619u;
      h=radarHashMix(h,plan.color);h=radarHashMix(h,uint32_t(dot.x)<<16|uint16_t(dot.y));h=radarHashMix(h,plan.slot|(dot.index==radarPrimary)<<8|dragon::radarOpacity(radarAge(now,c.seen))<<9);
      next.box=calloutBounds(dot,plan.slot);next.sig=h;next.on=true;next.key=dot.key;}
    if(!full&&(m.on!=next.on||(next.on&&(m.sig!=next.sig||!(m.key==next.key))))){if(m.on)dirty.add(m.box,SCREEN_RES);if(next.on)dirty.add(next.box,SCREEN_RES);}
    m=next;
  }
  // Sweep: bands that hug this frame's and the last frame's sweep, kept apart
  // from the merged dot and label areas. Then the breathing centre.
  dragon::RadarRect bands[12];unsigned bandCount=0;
  if(!full&&radarSweepAngle!=radarSweepShown){
    const dragon::RadarOutline outlines[2]={radarSweepOutline(radarSweepShown),radarSweepOutline(radarSweepAngle)};
    bandCount=dragon::outlineBands(outlines,2,40,2,SCREEN_RES,bands,12);
  }
  radarSweepShown=radarSweepAngle;
  if(!full&&radarCenterBreathes())dirty.add(radarCenterBounds(),SCREEN_RES);
  radarMarksValid=radarMarks!=nullptr;
  long sweepArea=0;for(unsigned i=0;i<bandCount;++i)sweepArea+=bands[i].area();
  const long area=dirty.area()+sweepArea;radarStats.sweepPx=sweepArea;radarStats.markPx=dirty.area();
  if(full||dirty.full||area*10>long(SCREEN_RES)*SCREEN_RES*6){lv_obj_invalidate(radar);return true;}
  auto invalidate=[&](const dragon::RadarRect& r){lv_area_t a{(lv_coord_t)r.x1,(lv_coord_t)r.y1,(lv_coord_t)r.x2,(lv_coord_t)r.y2};lv_obj_invalidate_area(radar,&a);};
  for(unsigned i=0;i<bandCount;++i)invalidate(bands[i]);
  for(unsigned i=0;i<dirty.count;++i)invalidate(dirty.rects[i]);
  return false;
}
// Draws what radarInvalidate marked, now, as part of the frame.
static void radarRefreshNow(){radarFrameRefresh=true;lv_refr_now(nullptr);radarFrameRefresh=false;}
#endif
