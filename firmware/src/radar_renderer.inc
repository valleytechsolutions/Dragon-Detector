// Shared by full UI and the radar-only Lite UI. Coordinates use the 360px design.
#ifdef BOARD_RADAR_LITE
static constexpr unsigned RadarRenderPageSize=dragon::LiteRadarPageSize;
static constexpr bool RadarRenderLite=true;
#else
static constexpr unsigned RadarRenderPageSize=dragon::RadarPageSize;
static constexpr bool RadarRenderLite=false;
#endif
static void line(lv_draw_ctx_t* ctx,int x1,int y1,int x2,int y2,uint32_t c,int width=1,int opa=255){
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=color(c);d.width=width;d.opa=opa;lv_point_t a{(lv_coord_t)x1,(lv_coord_t)y1},b{(lv_coord_t)x2,(lv_coord_t)y2};lv_draw_line(ctx,&d,&a,&b);
}
static void disc(lv_draw_ctx_t* ctx,int x,int y,int r,uint32_t c,int opa=255){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=LV_RADIUS_CIRCLE;d.bg_color=color(c);d.bg_opa=opa;lv_area_t a{(lv_coord_t)(x-r),(lv_coord_t)(y-r),(lv_coord_t)(x+r),(lv_coord_t)(y+r)};lv_draw_rect(ctx,&d,&a);
}
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
  lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.radius=S(4);box.bg_color=color(dragon::RadarLabelBackground);box.bg_opa=LV_OPA_COVER;
  box.border_color=color(c);box.border_opa=LV_OPA_80;box.border_width=1;
  lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+w),(lv_coord_t)(y+h)};lv_draw_rect(ctx,&box,&area);
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(c);d.font=FCALLOUT;
  lv_area_t textArea{(lv_coord_t)(x+S(4)),(lv_coord_t)(y+S(3)),(lv_coord_t)(x+w-S(4)),(lv_coord_t)(y+h-S(3))};
  // Clip locally so an unusually long translated/family label cannot escape.
  lv_area_t clip;if(_lv_area_intersect(&clip,ctx->clip_area,&textArea)){const lv_area_t* saved=ctx->clip_area;ctx->clip_area=&clip;lv_draw_label(ctx,&d,&textArea,text,nullptr);ctx->clip_area=saved;}
#endif
}
static void radarDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);const auto& theme=radarPalettes[radarTheme];
#ifdef BOARD_RADAR_LITE
  // LVGL may render several internal-RAM tiles for one frame. Advance signal
  // positions just once so leader lines and dots cannot disagree across tiles.
  const uint32_t now=radarFrameAt;
  static uint32_t plotFrame=UINT32_MAX;const bool advancePlot=plotFrame!=now;plotFrame=now;
#else
  const uint32_t now=millis();constexpr bool advancePlot=true;
#endif
#ifdef BOARD_RADAR_LITE
  if(liteBackdrop.data){lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_area_t a{0,0,SCREEN_RES-1,SCREEN_RES-1};lv_draw_img(ctx,&d,&a,&liteBackdrop);}
  else disc(ctx,CENTER,CENTER,S(178),theme.bottom);
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(theme.ring);arc.width=1;arc.opa=160;
#else
  disc(ctx,CENTER,CENTER,S(178),theme.grid);disc(ctx,CENTER,CENTER,S(176),theme.ring);
  lv_draw_rect_dsc_t glass;lv_draw_rect_dsc_init(&glass);glass.radius=LV_RADIUS_CIRCLE;
  glass.bg_color=color(theme.top);glass.bg_grad.dir=LV_GRAD_DIR_VER;glass.bg_grad.stops[0].color=glass.bg_color;glass.bg_grad.stops[1].color=color(theme.bottom);
  lv_area_t field{(lv_coord_t)S(9),(lv_coord_t)S(9),(lv_coord_t)(SCREEN_RES-S(9)-1),(lv_coord_t)(SCREEN_RES-S(9)-1)};lv_draw_rect(ctx,&glass,&field);
  static const int reaches[]={77,119,143,157,166,169,166,157,143,119,77};
  for(int n=0;n<11;++n){int axis=CENTER+S(n*30-150),reach=S(reaches[n]);line(ctx,axis,CENTER-reach,axis,CENTER+reach,theme.grid,S(1),180);line(ctx,CENTER-reach,axis,CENTER+reach,axis,theme.grid,S(1),180);}
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(theme.ring);arc.width=S(1);arc.opa=160;lv_point_t center{(lv_coord_t)CENTER,(lv_coord_t)CENTER};
  if(radarTheme)for(int radius=42;radius<169;radius+=42)lv_draw_arc(ctx,&arc,&center,S(radius),0,359);
  arc.width=S(2);lv_draw_arc(ctx,&arc,&center,S(170),0,359);
#endif
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
  const unsigned pages=radarLabels?dragon::radarPages(radarCount,RadarRenderPageSize):1;if(radarPage>=pages)radarPage=pages-1;
  #ifdef BOARD_RADAR_LITE
  // No touch/buttons: every page gets a complete seven-second viewing period.
  static dragon::RadarAutoPager pager;radarPage=pager.update(pages,now);
#endif
  dotCount=0;unsigned first=radarLabels?radarPage*RadarRenderPageSize:0,last=radarLabels?std::min<unsigned>(first+RadarRenderPageSize,radarCount):radarCount;
  for(unsigned k=first;k<last;++k){int i=radarOrder[k];auto& c=contacts[i];if(!radarVisible(c,now))continue;
    float target=c.signal.count?dragon::radarRadius(c.signal.smooth):158.f;if(!c.plotRadius)c.plotRadius=target;else if(advancePlot)c.plotRadius+=(target-c.plotRadius)*.35f;
    auto plot=dragon::radarPlotPoint(c.plotRadius,c.radarX,c.radarY);dots[dotCount++]={S(plot.x),S(plot.y),i,contactKey(c)};
  }
  if(radarLabels){dragon::RadarPoint points[RadarRenderPageSize];uint8_t preferred[RadarRenderPageSize];
    static dragon::DeviceKey labelKeys[RadarRenderPageSize];static uint8_t slots[RadarRenderPageSize];static int priorCount=0;static uint32_t labelsAt=0;bool changed=priorCount!=dotCount;
    for(int i=0;i<dotCount;++i){points[i]={dots[i].x*360/SCREEN_RES,dots[i].y*360/SCREEN_RES};preferred[i]=255;if(i>=priorCount||!(labelKeys[i]==dots[i].key))changed=true;for(int j=0;j<priorCount;++j)if(labelKeys[j]==dots[i].key)preferred[i]=slots[j];}
    if(changed||uint32_t(now-labelsAt)>=500){dragon::placeRadarLabels(points,dotCount,slots,preferred,RadarRenderLite);labelsAt=now;priorCount=dotCount;for(int i=0;i<dotCount;++i)labelKeys[i]=dots[i].key;}
    for(int i=0;i<dotCount;++i){auto& c=contacts[dots[i].index];Family family=visibleFamily(c,now,detector.families);uint32_t age=now-c.seen;
      char state[24],text[64];if(age>=10000)snprintf(state,sizeof(state),"%s %lus AGO",c.match.confidence==Confidence::Low&&!trafficFresh(c,now)?"CLUE":"SEEN",(unsigned long)(age/1000));else snprintf(state,sizeof(state),"%s",c.match.confidence==Confidence::Low&&!trafficFresh(c,now)?"WEAK CLUE":age>=5000?"FADING":c.rssi>=-55?"STRONG":c.rssi>=-75?"MODERATE":"FAINT");
#if SCREEN_RES<=240
      if(age>=10000)snprintf(state,sizeof(state),"%lus ago",(unsigned long)(age/1000));
#endif
      dots[i].labelSlot=slots[i];snprintf(text,sizeof(text),"%s\n%s",radarFamilyName(family),state);callout(ctx,dots[i],slots[i],text,dragon::familyColor(family));
    }
  }
  for(int i=0;i<dotCount;++i){auto& dot=dots[i];auto& c=contacts[dot.index];int x=dot.x,y=dot.y;uint32_t age=now-c.seen;uint8_t opacity=std::max<uint8_t>(205,dragon::radarOpacity(age));uint32_t deviceColor=dragon::familyColor(visibleFamily(c,now,detector.families));
    disc(ctx,x,y,S(8),dragon::RadarLabelBackground);
    if(age>=30000 || (c.match.confidence==Confidence::Low&&!trafficFresh(c,now))){auto clue=arc;clue.color=color(deviceColor);clue.opa=opacity;clue.width=S(2);lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};
      if(age>=30000)for(int a=0;a<360;a+=90)lv_draw_arc(ctx,&clue,&point,S(6),a,a+48);else lv_draw_arc(ctx,&clue,&point,S(5),0,359);
    }else{unsigned pulse=radioEnabled?unsigned((now+contactHash(contactKey(c)))%1800):900;pulse=pulse>900?1800-pulse:pulse;disc(ctx,x,y,S(9),deviceColor,unsigned(opacity)*(25+pulse/20)/255);disc(ctx,x,y,S(5),deviceColor,opacity);disc(ctx,x-S(1),y-S(1),S(2),0xffffff,opacity);}
    if(dot.index==trackedContact || (detector.enabled(c.trafficFamily)&&trafficFresh(c,now))){auto lock=arc;lock.color=color(dot.index==trackedContact?theme.text:0xff5040);lock.opa=220;lock.width=2;lv_point_t point{(lv_coord_t)x,(lv_coord_t)y};lv_draw_arc(ctx,&lock,&point,S(12),0,359);}
  }
  // Decorative center cursor, not a compass. Keep it above the sweep/labels.
#ifdef BOARD_RADAR_LITE
  for(int sign:{-1,1}){line(ctx,CENTER+sign*3,CENTER,CENTER+sign*8,CENTER,0x86bda5,1);line(ctx,CENTER,CENTER+sign*3,CENTER,CENTER+sign*8,0x86bda5,1);}
  disc(ctx,CENTER,CENTER,1,0xd4f9e7);
#else
  for(int y=-7;y<=6;++y){int half=(y+7)/2;line(ctx,CENTER-S(half),CENTER+S(y),CENTER+S(half),CENTER+S(y),0xff6651,S(1)+1);}
#endif
}
