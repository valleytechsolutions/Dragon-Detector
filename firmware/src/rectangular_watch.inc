static void panelDigits(lv_draw_ctx_t* ctx,const char* value,int y,int width,uint32_t ink,unsigned font){
  if(!font){panelText(ctx,value,20,y,width-40,&lv_font_montserrat_40,ink);return;}
  static const uint8_t pixels[10][7]={{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
  static const uint8_t segments[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
  int x=width/2-int(strlen(value))*32;
  for(const char* p=value;*p;++p,x+=64){
    auto box=[&](int dx,int dy,int w,int h){panelBox(ctx,x+dx*2,y+dy*2,w*2,h*2,ink,ink,0);};
    if(*p==':'){box(12,8,4,4);box(12,24,4,4);continue;}
    if(*p=='-'){box(4,16,20,3);continue;}
    if(*p<'0'||*p>'9')continue;const unsigned n=*p-'0';
    if(font==1){for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col)if(pixels[n][row]&(16>>col))box(col*5,row*5,4,4);}
    else{const int rects[7][4]={{4,0,18,3},{22,3,3,13},{22,19,3,13},{4,32,18,3},{1,19,3,13},{1,3,3,13},{4,16,18,3}};for(unsigned i=0;i<7;++i)if(segments[n]&(1<<i))box(rects[i][0],rects[i][1],rects[i][2],rects[i][3]);}
  }
}
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
  if(personal)panelLogo(ctx,personalThemeLogo(true),w/2,p.analog?106:110,p.analog?256:std::min(256,(cy-146)*256/123));
  else if(watchFace==12){
    panelLogo(ctx,&radar_logo_ci,cx,60);
    const int logoY=std::min(cy+hh/2,h-140-int(watch_logo_mcnally.header.h));
    panelLogo(ctx,&watch_logo_mcnally,cx,logoY);
  }else if(watchFace!=10&&watchFace!=27)panelLogo(ctx,radarLogo(logoTheme),w/2,76);
  if(p.analog||personal){
    if(!personal||custom.markers!=3)for(int i=0;i<60;++i){auto outer=dragon::rectangularRay(i/60.f,cx,cy,hw,hh);auto inner=dragon::rectangularRay(i/60.f,cx,cy,hw-(i%5?5:12),hh-(i%5?5:12));line(ctx,inner.x,inner.y,outer.x,outer.y,i%5?p.muted:p.accent,i%5?1:2);}
    if(personal&&custom.markers<2){static const char* roman[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};for(int i=0;i<12;++i){if(custom.layout==1&&i>=5&&i<=7)continue;auto at=dragon::rectangularRay(i/12.f,cx,cy,hw-30,hh-24);snprintf(text,sizeof(text),"%d",i?i:12);panelText(ctx,custom.markers?roman[i]:text,at.x-22,at.y-8,44,F12,p.ink);}}
  }
  if(p.analog){
    if(valid){
      auto hand=[&](float turn,float reach,uint32_t ink,int thickness,bool styled){
        auto tip=dragon::rectangularRay(turn,cx,cy,int(hw*reach),int(hh*reach));
        if(personal&&styled){const float angle=turn*6.28318530718f,dx=std::sin(angle),dy=-std::cos(angle);const int weight=custom.hands==1?6:3;
          line(ctx,cx,cy,tip.x,tip.y,p.bg,weight+4);line(ctx,cx,cy,tip.x,tip.y,ink,weight);
          if(custom.hands==2){lv_point_t points[]={{lv_coord_t(tip.x),lv_coord_t(tip.y)},{lv_coord_t(tip.x-dx*22-dy*8),lv_coord_t(tip.y-dy*22+dx*8)},{lv_coord_t(tip.x-dx*22+dy*8),lv_coord_t(tip.y-dy*22-dx*8)}};lv_draw_rect_dsc_t fill;lv_draw_rect_dsc_init(&fill);fill.bg_color=color(ink);lv_draw_triangle(ctx,&fill,points);}
          if(custom.hands==3){line(ctx,cx,cy,cx+(tip.x-cx)*.4f,cy+(tip.y-cy)*.4f,p.accent,9);line(ctx,tip.x-dx*10,tip.y-dy*10,tip.x-dy*7,tip.y+dx*7,ink,4);}
        }else if(styled){
          const PanelInstrument ray{{cx,cy},tip};
          if(watchFace==36)panelFeather(ctx,ray,p.muted,ink);
          else if(panelLocksport(logoTheme))panelPick(ctx,ray,reach>.6f,p.muted,ink);
          else panelSpear(ctx,ray,ink,p.muted,p.accent);
        }else line(ctx,cx,cy,tip.x,tip.y,ink,thickness);
      };
      hand((d.hour%12+d.minute/60.f)/12.f,.53f,p.ink,6,true);hand((d.minute+d.second/60.f)/60.f,.76f,personal?custom.secondary:p.ink,4,true);if(seconds)hand(d.second/60.f,.88f,p.accent,2,false);
      disc(ctx,cx,cy,5,p.accent);
    }
    if(personal&&custom.layout){if(valid)snprintf(text,sizeof(text),"%02d:%02d",hour,d.minute);else strcpy(text,"--:--");panelBox(ctx,cx-166,cy+hh/2-3,332,74,p.bg,p.bg,0);panelDigits(ctx,text,cy+hh/2,w,p.ink,custom.font);}
    else if(!personal){
      if(valid)snprintf(text,sizeof(text),"%02d:%02d:%02d",hour,d.minute,d.second);else strcpy(text,"SYNC IN SETTINGS");
      panelText(ctx,text,42,watchFace==12?h-130:cy+hh/2,w-84,F20,p.ink);
    }
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
    if(personal)panelDigits(ctx,text,cy-26,w,p.ink,custom.font);else panelText(ctx,text,20,cy-62,w-40,FWATCH,p.ink);
    if(valid)snprintf(text,sizeof(text),"%02d SECONDS",d.second);else strcpy(text,"SYNC IN SETTINGS");
    if(seconds)panelText(ctx,text,20,cy+(personal?48:30),w-40,F16,p.accent);
    line(ctx,28,cy+70,w-28,cy+70,p.muted,3);
    if(valid&&seconds)line(ctx,28,cy+70,28+(w-56)*d.second/59,cy+70,p.accent,3);
    if(watchFace==10){ // Church of Malware retains its binary time readout.
      const unsigned values[]={unsigned(d.hour),unsigned(d.minute),unsigned(d.second)};
      for(int row=0;row<3;++row)for(int bit=0;bit<6;++bit){int x=cx-78+bit*28,y=66+row*18;panelBox(ctx,x,y,17,12,valid&&(values[row]&(1u<<(5-bit)))?p.ink:p.bg,p.muted,1);}
    }
  }
  if(personal&&valid&&!(custom.flags&4)&&custom.layout)panelText(ctx,d.hour<12?"AM":"PM",w-80,56,60,F12,p.accent);
  if(valid)snprintf(text,sizeof(text),"%04d-%02d-%02d",d.year,d.month,d.day);else strcpy(text,"TIME NOT SET");
  if(dateVisible||!valid)panelText(ctx,text,20,h-105,w-40,F16,p.accent);
  snprintf(text,sizeof(text),"%d%% BATTERY%s",batteryPercent(),batteryCharging()?" / CHARGING":"");
  panelText(ctx,text,20,h-80,w-40,F12,p.muted);
}
