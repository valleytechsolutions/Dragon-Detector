static void watchLpl(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  const auto& p=watches[29];
  watchRing(ctx,176,0x493b20);watchRing(ctx,173,p.accent,2);watchRing(ctx,169,0x745c2c);
  // Rope-like bezel and a navy chapter ring echo the supplied seal.
  for(int i=0;i<120;++i){auto a=watchPolar(i/120.f,170),b=watchPolar((i+.5f)/120.f,173);watchLine(ctx,a.x,a.y,b.x,b.y,i%2?0xb18c3d:0xf5d87b);}
  for(int i=0;i<60;++i)watchRay(ctx,i/60.f,i%5?158:154,162,i%5?0x6e613e:p.accent,i%5?1:2);
  static const char* roman[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};
  for(int i=0;i<12;++i){auto a=watchPolar(i/12.f,142);watchText(ctx,roman[i],a.x-23,a.y-8,46,F16,i%3?p.accent:p.ink);}
  watchLogo(ctx,radar_logo_ci,64);
  radarLplEmblem(ctx,235);watchRing(ctx,73,0x745c2c);
  if(valid){
    watchPick(ctx,(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,pickShortHook,pickShortHookTriangles,false);
    watchPick(ctx,(d.minute+d.second/60.f)/60.f,pickMcTickler,pickMcTicklerTriangles,true);
    watchRay(ctx,d.second/60.f,158,163,p.ink,2);
    disc(ctx,CENTER,CENTER,S(7),p.bg);watchCircle(ctx,180,180,6,p.accent,2);watchScrew(ctx,180,180,0xf5d87b,p.bg);
  }
  // Date and large 24-hour readout remain clear of both moving picks.
  lv_draw_rect_dsc_t panel;lv_draw_rect_dsc_init(&panel);panel.bg_color=color(0x09162a);panel.border_color=color(0x8b7137);panel.border_width=1;panel.radius=S(3);
  lv_area_t area{(lv_coord_t)S(119),(lv_coord_t)S(252),(lv_coord_t)S(241),(lv_coord_t)S(302)};lv_draw_rect(ctx,&panel,&area);
  char text[24];if(valid)snprintf(text,sizeof(text),"%04d-%02d-%02d",d.year,d.month,d.day);else strcpy(text,"SET LOCAL TIME");
  watchText(ctx,text,120,255,120,F12,0xc5bda4);
  watchLine(ctx,131,273,229,273,0x493b20);
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",d.hour,d.minute);else strcpy(text,"--:--");watchText(ctx,text,125,277,110,F20,p.ink);
  for(unsigned i=0;i<WatchCount;++i)disc(ctx,S(180+(int(i)*2-int(WatchCount-1))*3),S(338),std::max(1,S(i==watchFacePosition()?2:1)),i==watchFacePosition()?p.accent:0x4a432e);
}
