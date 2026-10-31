// 44Delta: supplied wordmark and padlock, original vector lockpick hands.
// All geometry uses the shared 360px design space; no per-frame allocations.
extern lv_img_dsc_t radar_logo_delta44,watch_logo_delta44;
static constexpr unsigned Delta44Theme=45,Delta44Watch=48;
#if SCREEN_RES>=400
static const lv_font_t *Delta44Small=&lv_font_montserrat_14,*Delta44Medium=&lv_font_montserrat_20;
#elif SCREEN_RES>=360
static const lv_font_t *Delta44Small=&lv_font_montserrat_10,*Delta44Medium=&lv_font_montserrat_16;
#else
static const lv_font_t *Delta44Small=&lv_font_montserrat_8,*Delta44Medium=&lv_font_montserrat_10;
#endif
static void delta44Rim(lv_draw_ctx_t* ctx){
  capsuleRing(ctx,176,0x084147);capsuleRing(ctx,171,0x00dce4);
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?162:153;
    themeStroke(ctx,180+sinf(a)*inner,180-cosf(a)*inner,180+sinf(a)*166,180-cosf(a)*166,i%5?0x346066:0x8cf8fa,i%5?1:2);
  }
}
static void delta44Pick(lv_draw_ctx_t* ctx,float turns,bool minute){
  float a=turns*6.28318530718f,dx=sinf(a),dy=-cosf(a);int tip=minute?143:103;
  auto p=[&](float r,float side){return lv_point_t{lv_coord_t(CENTER+dx*S(r)+cosf(a)*S(side)),lv_coord_t(CENTER+dy*S(r)+sinf(a)*S(side))};};
  auto stroke=[&](float r,float s,float rr,float ss,uint32_t c,int w){auto x=p(r,s),y=p(rr,ss);line(ctx,x.x,x.y,y.x,y.y,c,std::max(1,S(w)));};
  // Distinct long slender hook and shorter deep hook, with machined handle scales.
  stroke(-20,0,46,0,0x000000,16);stroke(-19,0,45,0,0x9dc9ce,12);
  stroke(-18,0,44,0,minute?0x00c9d3:0x095c68,9);
  stroke(-13,-2,39,-2,minute?0xaffcff:0x32c4d1,1);
  for(int r:{-12,34}){auto q=p(r,0);disc(ctx,q.x,q.y,std::max(1,S(3)),0x061216);disc(ctx,q.x,q.y,std::max(1,S(1)),0xebffff);}
  for(int pass=0;pass<2;++pass){uint32_t c=pass?0xe8f4f7:0x000000;int w=pass?3:7;
    stroke(45,0,tip-16,0,c,w);stroke(tip-16,0,tip-6,-3,c,w);stroke(tip-6,-3,tip,-(minute?9:13),c,w);
  }
  stroke(48,-1,tip-18,-1,0x93abb5,1);
}
static void delta44RadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0x000000);delta44Rim(ctx);
  for(int r:{44,88,132})capsuleRing(ctx,r,0x0a292d);
  themeStroke(ctx,28,180,332,180,0x0a292d);themeStroke(ctx,180,91,180,271,0x0a292d);
  themeArtwork(ctx,watch_logo_delta44,24);themeArtwork(ctx,radar_logo_delta44,105);
}
static void delta44RadarSweep(lv_draw_ctx_t* ctx,float turns){
  // Keep the padlock unobstructed: the sweep travels around its perimeter.
  for(int n=10;n>=0;--n){float a=(turns-n*.006f)*6.28318530718f;
    disc(ctx,CENTER+S(147)*sinf(a),CENTER-S(147)*cosf(a),std::max(1,S(n?1:3)),n?0x12666c:0x00f2f5);
  }
}
static void delta44Watch(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0x000000);delta44Rim(ctx);
  capsuleRing(ctx,148,0x0c2429);
  capsuleText(ctx,"XII",155,29,50,Delta44Medium,0xb8ebee);
  capsuleText(ctx,"III",309,170,30,Delta44Medium,0xb8ebee);
  capsuleText(ctx,"VI",161,310,38,Delta44Medium,0xb8ebee);
  capsuleText(ctx,"IX",22,170,30,Delta44Medium,0xb8ebee);
  themeArtwork(ctx,watch_logo_delta44,76);
  capsuleText(ctx,"L O C K S P O R T",80,265,200,Delta44Small,0x739ea5);
  if(valid){
    delta44Pick(ctx,(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,false);
    delta44Pick(ctx,(d.minute+d.second/60.f)/60.f,true);
    float a=d.second*.104719755f;disc(ctx,CENTER+S(171)*sinf(a),CENTER-S(171)*cosf(a),std::max(1,S(3)),0xf0ffff);
  }else capsuleText(ctx,"SET TIME",104,213,152,Delta44Medium,0x9ccbd0);
  disc(ctx,CENTER,CENTER,S(8),0x000000);disc(ctx,CENTER,CENTER,S(6),0x00dce4);disc(ctx,CENTER,CENTER,S(3),0xdffaff);
}
