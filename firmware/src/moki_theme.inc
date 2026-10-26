// Supplied Moki logo and original handled-pick / AKAB-inspired drawing code.
extern lv_img_dsc_t radar_logo_moki,watch_logo_moki;
static constexpr unsigned MokiTheme=44,MokiWatch=47;
#if SCREEN_RES>=400
static const lv_font_t *MokiSmall=&lv_font_montserrat_14,*MokiMedium=&lv_font_montserrat_20;
#elif SCREEN_RES>=360
static const lv_font_t *MokiSmall=&lv_font_montserrat_10,*MokiMedium=&lv_font_montserrat_16;
#else
static const lv_font_t *MokiSmall=&lv_font_montserrat_8,*MokiMedium=&lv_font_montserrat_10;
#endif
static void mokiRim(lv_draw_ctx_t* ctx){
  capsuleRing(ctx,176,0x19f000,2);capsuleRing(ctx,170,0x17401b);
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?164:158;
    themeStroke(ctx,180+sinf(a)*inner,180-cosf(a)*inner,180+sinf(a)*167,180-cosf(a)*167,i%5?0x446847:0xb8ffb1,i%5?1:2);
  }
}
static void mokiPick(lv_draw_ctx_t* ctx,float turns,int length,bool akab,bool minute=false){
  float a=turns*6.28318530718f,dx=sinf(a),dy=-cosf(a);
  auto p=[&](float r,float side){return lv_point_t{lv_coord_t(CENTER+dx*S(r)+cosf(a)*S(side)),lv_coord_t(CENTER+dy*S(r)+sinf(a)*S(side))};};
  auto stroke=[&](float r,float s,float rr,float ss,uint32_t c,int w){auto x=p(r,s),y=p(rr,ss);line(ctx,x.x,x.y,y.x,y.y,c,std::max(1,S(w)));};
  // Thick screw-on scales, a recessed spine and three visible fasteners.
  stroke(-19,0,54,0,0x000000,21);stroke(-18,0,53,0,0x83c47a,17);stroke(-17,0,52,0,minute?0x22ce0b:0x113c19,13);
  stroke(-11,-3,46,-3,minute?0x89ff65:0x3ad81c,2);stroke(-10,2,46,2,0x06250b,2);
  for(int r:{-10,20,46}){auto at=p(r,0);disc(ctx,at.x,at.y,S(4),0x000000);disc(ctx,at.x,at.y,S(3),0xd7e5df);stroke(r-2,1,r+2,-1,0x263a2c,1);}
  if(akab){
    // AKAB's broad tapered shoulder ends in a straight shaft and square flag.
    lv_point_t q[]={p(54,-6),p(83,-2),p(83,2),p(54,6)};lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color(0xd9e5e0);lv_draw_polygon(ctx,&d,q,4);
    for(int pass=0;pass<2;++pass){uint32_t c=pass?0xe3eee9:0x000000;int w=pass?4:8;
      stroke(80,0,length,0,c,w);stroke(length,0,length,-12,c,w);
    }
    stroke(81,-1,length-1,-1,0x96a99f,1);
  }else{
    for(int pass=0;pass<2;++pass){uint32_t c=pass?0xe3eee9:0x000000;int w=pass?3:7;
      stroke(54,0,length-13,0,c,w);stroke(length-13,0,length-5,-3,c,w);stroke(length-5,-3,length,-(minute?11:7),c,w);
    }
  }
}
static void mokiRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0x000000);mokiRim(ctx);
  for(int r:{42,84,126})capsuleRing(ctx,r,0x15391b);
  themeStroke(ctx,24,180,336,180,0x15391b);themeStroke(ctx,180,104,180,276,0x15391b);
  themeArtwork(ctx,watch_logo_moki,92);capsuleText(ctx,"MOKI",60,32,240,MokiMedium,0x34ff16);
}
static void mokiRadarSweep(lv_draw_ctx_t* ctx,float turns){mokiPick(ctx,turns,150,true);disc(ctx,CENTER,CENTER,S(5),0x041507);disc(ctx,CENTER,CENTER,S(2),0x34ff16);}
static void mokiWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0x000000);mokiRim(ctx);themeArtwork(ctx,watch_logo_moki,92);
  capsuleText(ctx,"MOKI",75,54,210,MokiMedium,0x34ff16);capsuleText(ctx,"12",163,25,34,MokiMedium,0xe2ffdd);
  capsuleText(ctx,"3",316,172,24,MokiMedium,0xe2ffdd);capsuleText(ctx,"9",20,172,24,MokiMedium,0xe2ffdd);
  if(valid){mokiPick(ctx,(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,91,false);mokiPick(ctx,(d.minute+d.second/60.f)/60.f,133,false,true);
    disc(ctx,CENTER,CENTER,S(7),0x041507);disc(ctx,CENTER,CENTER,S(4),0x34ff16);disc(ctx,CENTER,CENTER,S(2),0xf1ffec);
    int angle=(int(d.second)*6+270)%360;capsuleRing(ctx,173,0xa0ff76,3,angle,(angle+4)%360);
  }
  capsulePanel(ctx,109,286,142,38,0x071e0b,0x267234,4);char value[32];
  if(valid)snprintf(value,sizeof(value),"%02d:%02d:%02d",d.hour,d.minute,d.second);else strcpy(value,"SET TIME");capsuleText(ctx,value,109,289,142,MokiMedium,0xe2ffdd);
  if(valid)snprintf(value,sizeof(value),"%02d.%02d.%04d",d.month,d.day,d.year);else strcpy(value,"CLOCK NOT SYNCED");capsuleText(ctx,value,107,310,146,MokiSmall,0x9dcf99);
}
