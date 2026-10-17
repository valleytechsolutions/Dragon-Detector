extern lv_img_dsc_t watch_logo_lpu;
// Each pin represents one second. Its driver lifts radially and settles;
// completed pins brighten until the next minute. Timer input is monotonic.
static void locksportPinRim(lv_draw_ctx_t* ctx,uint64_t us){
  unsigned second=(us/1000000)%60;float phase=(us%1000000)/1000000.f;
  for(unsigned i=0;i<60;++i){
    float angle=i*.104719755f,dx=sinf(angle),dy=-cosf(angle);
    auto point=[&](float r){return lv_point_t{lv_coord_t(CENTER+S(lroundf(dx*r))),lv_coord_t(CENTER+S(lroundf(dy*r)))};};
    auto a=point(157),b=point(173);line(ctx,a.x,a.y,b.x,b.y,0x343a40,std::max(1,S(4)));
    float lift=i==second?7*sinf(phase*3.14159265f):0;
    auto c=point(164+lift),d=point(170+lift*.3f);
    line(ctx,c.x,c.y,d.x,d.y,i==second?0xffffff:i<second?0xaeb9c2:0x626b74,std::max(1,S(2)));
    auto tip=point(159+lift);disc(ctx,tip.x,tip.y,std::max(1,S(1)),i==second?0xffffff:0x77828c);
  }
}
static void lpuEmblem(lv_draw_ctx_t* ctx,int opacity=255){
#ifndef BOARD_RADAR_LITE
  const auto& img=watch_logo_lpu;
#else
  const auto& img=radar_logo_lpu;
#endif
  int x=(SCREEN_RES-img.header.w)/2,y=(SCREEN_RES-img.header.h)/2;
  lv_area_t a{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+img.header.w-1),lv_coord_t(y+img.header.h-1)};
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.opa=opacity;lv_draw_img(ctx,&d,&a,&img);
}
static void ghostProtonSweep(lv_draw_ctx_t* ctx,float angle,uint32_t now){
  float dx=cosf(angle),dy=sinf(angle);int lastX=CENTER,lastY=CENTER;
  for(int i=1;i<=18;++i){
    float r=i*9,shake=sinf(i*2.3f+now*.018f)*(2+i*.14f);
    int x=CENTER+S(lroundf(dx*r-dy*shake)),y=CENTER+S(lroundf(dy*r+dx*shake));
    line(ctx,lastX,lastY,x,y,0x902329,std::max(2,S(7)),100);
    line(ctx,lastX,lastY,x,y,0xff4b35,std::max(1,S(3)),230);
    line(ctx,lastX,lastY,x,y,0xffe5d8,std::max(1,S(1)),255);lastX=x;lastY=y;
  }
  disc(ctx,CENTER,CENTER,S(5),0x080808);disc(ctx,CENTER,CENTER,S(2),0xff6b50);
  // Hazard brackets stay outside the signal field; no decorative fake targets.
  for(int side:{-1,1})for(int i=0;i<5;++i){int x=180+side*(153-i*3),y=147+i*13;line(ctx,S(x),S(y),S(x-side*6),S(y+7),i%2?0xdfe1df:0xc12a32,S(2),170);}
}
