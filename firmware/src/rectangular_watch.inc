static void rectangularWatch(lv_draw_ctx_t* ctx,WatchPalette p,const dragon::Calendar& d,bool valid){
  const int w=lv_disp_get_hor_res(nullptr),h=lv_disp_get_ver_res(nullptr);
  if(watchFace==dragon::PersonalWatchId){const auto& custom=personalTheme();p.bg=custom.bg;p.ink=custom.ink;p.accent=custom.accent;p.muted=custom.secondary;p.name=custom.name;p.analog=custom.layout!=2;}
  const bool personal=watchFace==dragon::PersonalWatchId;
  const auto& custom=personalTheme();const bool seconds=!personal||(custom.flags&1),dateVisible=!personal||(custom.flags&2);
  const int hour=personal&&!(custom.flags&4)?dragon::hour12(d.hour):d.hour;
  panelBox(ctx,0,0,w,h,p.bg,p.bg,0);
  panelText(ctx,p.name,20,20,w-40,F12,p.accent);
  line(ctx,20,52,w-20,52,p.muted);
  const int cx=w/2,cy=(h-56)/2,hw=w/2-24,hh=(h-192)/2;
  char text[80];
  const unsigned logoTheme=watchFace>=13?watchFace-3:watchFace==12?9:watchFace==11?8:watchFace==10?7:watchFace==9?6:watchFace==8?4:watchFace==2?5:watchFace==1?3:0;
  if(watchFace!=10&&watchFace!=27)panelLogo(ctx,radarLogo(logoTheme),w/2,76);
  if(p.analog){
    if(!personal||custom.markers!=3)for(int i=0;i<60;++i){auto outer=dragon::rectangularRay(i/60.f,cx,cy,hw,hh);auto inner=dragon::rectangularRay(i/60.f,cx,cy,hw-(i%5?5:12),hh-(i%5?5:12));line(ctx,inner.x,inner.y,outer.x,outer.y,i%5?p.muted:p.accent,i%5?1:2);}
    if(valid){
      auto hand=[&](float turn,float reach,uint32_t ink,int thickness){auto tip=dragon::rectangularRay(turn,cx,cy,int(hw*reach),int(hh*reach));line(ctx,cx,cy,tip.x,tip.y,ink,thickness);};
      hand((d.hour%12+d.minute/60.f)/12.f,.53f,p.ink,6);hand((d.minute+d.second/60.f)/60.f,.76f,p.ink,4);if(seconds)hand(d.second/60.f,.88f,p.accent,2);
      disc(ctx,cx,cy,5,p.accent);
    }
    if(valid)snprintf(text,sizeof(text),"%02d:%02d:%02d",hour,d.minute,d.second);else strcpy(text,"SYNC IN SETTINGS");
    if(!personal||custom.layout)panelText(ctx,text,42,cy+hh/2,w-84,F20,p.ink);
  }else if(watchFace==27){
    if(valid)snprintf(text,sizeof(text),"%02d:%02d",hour,d.minute);else strcpy(text,"--:--");
    panelText(ctx,text,20,cy-112,w-40,FWATCH,p.ink);
    const int x=int(pigX*w/360.f)-100;
    panelText(ctx,pigReacting?"< OINK! >":"TAP TO SAY HELLO",20,cy-10,w-40,F12,p.accent);
    const char* eyes=pigStep%37<2?"  ( -   - )  ":pigLook<0?"  ( <   < )  ":pigLook>0?"  ( >   > )  ":"  ( o   o )  ";
    const char* pig[]={"   ^-----^   ",eyes,"  /  (oo) \\__",pigStep%6<3?" (  _____  )~":" (  _____  )-",pigStep%6<3?"  |_|   |_|  ":"  _||   ||_  "};
    for(int row=0;row<5;++row)panelText(ctx,pig[row],x,cy+24+row*18,200,&dragon_ascii_small,p.accent);
  }else{
    // Digital faces use wide, unmasked typography and a full-width seconds bar.
    if(valid)snprintf(text,sizeof(text),"%02d:%02d",hour,d.minute);else strcpy(text,"--:--");
    panelText(ctx,text,20,cy-62,w-40,FWATCH,p.ink);
    if(valid)snprintf(text,sizeof(text),"%02d SECONDS",d.second);else strcpy(text,"SYNC IN SETTINGS");
    if(seconds)panelText(ctx,text,20,cy+30,w-40,F16,p.accent);
    line(ctx,28,cy+70,w-28,cy+70,p.muted,3);
    if(valid&&seconds)line(ctx,28,cy+70,28+(w-56)*d.second/59,cy+70,p.accent,3);
    if(watchFace==10){ // Church of Malware retains its binary time readout.
      const unsigned values[]={unsigned(d.hour),unsigned(d.minute),unsigned(d.second)};
      for(int row=0;row<3;++row)for(int bit=0;bit<6;++bit){int x=cx-78+bit*28,y=cy-132+row*20;panelBox(ctx,x,y,17,12,valid&&(values[row]&(1u<<(5-bit)))?p.ink:p.bg,p.muted,1);}
    }
  }
  if(valid)snprintf(text,sizeof(text),"%04d-%02d-%02d",d.year,d.month,d.day);else strcpy(text,"TIME NOT SET");
  if(dateVisible||!valid)panelText(ctx,text,20,h-105,w-40,F16,p.accent);
  snprintf(text,sizeof(text),"%d%% BATTERY%s",batteryPercent(),batteryCharging()?" / CHARGING":"");
  panelText(ctx,text,20,h-80,w-40,F12,p.muted);
}
