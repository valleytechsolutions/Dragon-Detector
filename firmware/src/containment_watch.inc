extern lv_img_dsc_t watch_logo_ghostbusters;
static void watchContainment(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
 const bool pins=watchFace==31;const auto& p=watches[watchFace];
 watchRing(ctx,176,p.muted);watchRing(ctx,151,p.muted);
 if(pins)locksportPinRim(ctx,uint64_t(d.second)*1000000+(watchFrameAt%1000)*1000);
 else for(int i=0;i<60;++i)watchRay(ctx,i/60.f,i%5?164:158,171,i%5?0x484448:p.accent,i%5?1:3);
 for(int i:{0,3,9}){auto at=watchPolar(i/12.f,137);char n[4];snprintf(n,sizeof(n),"%d",i?i:12);watchText(ctx,n,at.x-20,at.y-9,40,F16,p.ink);}
 watchLogo(ctx,pins?watch_logo_lpu:watch_logo_ghostbusters,pins?72:83);
 watchText(ctx,pins?"LOCKPICKERS UNITED":"NO GHOSTS ALLOWED",65,226,230,F12,p.accent);
 if(valid){
  float hours=(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,minutes=(d.minute+d.second/60.f)/60.f;
  if(pins){watchPick(ctx,hours,pickShortHook,pickShortHookTriangles,false);watchPick(ctx,minutes,pickMcTickler,pickMcTicklerTriangles,true);}
  else {watchDauphine(ctx,hours,86,10,0xffffff,0x73777b);watchDauphine(ctx,minutes,118,7,0xff6155,0x8f1822);watchRay(ctx,d.second/60.f,154,172,0xffffff,2);}
  disc(ctx,CENTER,CENTER,S(7),0x000000);watchScrew(ctx,180,180,p.ink,0x15191c);
 }
 // Protected digital panel stays legible when either hand crosses six.
 lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.bg_color=color(0x080a0c);box.border_color=color(p.muted);box.border_width=1;box.radius=S(3);
 lv_area_t a{lv_coord_t(S(113)),lv_coord_t(S(247)),lv_coord_t(S(247)),lv_coord_t(S(296))};lv_draw_rect(ctx,&box,&a);
 char text[28];if(valid)snprintf(text,sizeof(text),"%02d:%02d:%02d",d.hour,d.minute,d.second);else strcpy(text,"--:--:--");watchText(ctx,text,116,251,128,F20,p.ink);
 if(valid)snprintf(text,sizeof(text),"%04d-%02d-%02d",d.year,d.month,d.day);else strcpy(text,"SYNC LOCAL TIME");watchText(ctx,text,116,278,128,F12,0xaeb5bb);
 if(!pins){
  // Containment-trap doors pulse once each second; the display is decorative.
  int gap=2+int(5*sinf((watchFrameAt%1000)*.00314159f));
  for(int side:{-1,1}){int x=180+side*(gap+27);watchLine(ctx,x,303,x-side*47,303,0x71757a,5);for(int i=0;i<5;++i)watchLine(ctx,x-side*i*9,301,x-side*(i*9+4),305,i%2?0xff4b48:0xd9dbdd,2);}
 }
 for(unsigned i=0;i<WatchCount;++i)disc(ctx,S(180+(int(i)*2-int(WatchCount-1))*3),S(338),std::max(1,S(i==watchFacePosition()?2:1)),i==watchFacePosition()?p.accent:p.muted);
}
