// Shared, allocation-free ornaments for every display, including Lite.
#include "dino_runner.h"
extern lv_img_dsc_t watch_logo_hackazillarex,watch_logo_m33k;
static void themeStroke(lv_draw_ctx_t* ctx,int x,int y,int xx,int yy,uint32_t c,int w=1){line(ctx,S(x),S(y),S(xx),S(yy),c,std::max(1,S(w)));}
static void themeBlock(lv_draw_ctx_t* ctx,int x,int y,int w,int h,uint32_t c){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color(c);lv_area_t a{lv_coord_t(S(x)),lv_coord_t(S(y)),lv_coord_t(S(x+w)-1),lv_coord_t(S(y+h)-1)};lv_draw_rect(ctx,&d,&a);
}
static void themeArtwork(lv_draw_ctx_t* ctx,const lv_img_dsc_t& logo,int y){
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);int x=(SCREEN_RES-logo.header.w)/2;
  lv_area_t a{lv_coord_t(x),lv_coord_t(S(y)),lv_coord_t(x+logo.header.w-1),lv_coord_t(S(y)+logo.header.h-1)};lv_draw_img(ctx,&d,&a,&logo);
}
static void boneSweep(lv_draw_ctx_t* ctx,float a){
  float dx=cosf(a),dy=sinf(a);auto point=[&](int r,int s){return lv_point_t{lv_coord_t(CENTER+S(r)*dx-S(s)*dy),lv_coord_t(CENTER+S(r)*dy+S(s)*dx)};};
  auto p=point(11,0),q=point(146,0);line(ctx,p.x,p.y,q.x,q.y,0x6c1823,std::max(1,S(15)));line(ctx,p.x,p.y,q.x,q.y,0xffe9ce,std::max(1,S(8)));
  for(int r:{11,146})for(int s:{-5,5}){auto k=point(r,s);disc(ctx,k.x,k.y,S(8),0x6c1823);disc(ctx,k.x,k.y,S(6),0xffe9ce);}
  p=point(70,-4);q=point(75,1);line(ctx,p.x,p.y,q.x,q.y,0x9b5e50,std::max(1,S(1)));
  p=q;q=point(79,-3);line(ctx,p.x,p.y,q.x,q.y,0x9b5e50,std::max(1,S(1)));
}
static void cyberSweep(lv_draw_ctx_t* ctx,float a){
  for(int side:{-1,1}){float b=a+side*.025f;uint32_t c=side<0?0xff54d8:0x4ae9ff;
    line(ctx,CENTER,CENTER,CENTER+cosf(b)*S(164),CENTER+sinf(b)*S(164),c,std::max(1,S(2)),220);
    for(int r:{61,111,159}){int x=CENTER+cosf(b)*S(r),y=CENTER+sinf(b)*S(r);disc(ctx,x,y,S(3),c);}
  }
}
static void dinoRun(lv_draw_ctx_t* ctx,uint32_t now){
  auto f=dragon::dinoRunner(now);constexpr int ground=302;int y=ground-f.lift;
  themeStroke(ctx,61,ground+1,299,ground+1,0x782c33);
  // Moving ground and an original pixel dinosaur; no external game sprites.
  for(int i=0;i<11;++i){int x=65+(i*23+230-int(now/30%230))%230;themeStroke(ctx,x,ground+4,x+4,ground+4,0x4f232b);}
  int c=f.cactusX;
  if(c>=62&&c<=281){themeBlock(ctx,c+7,ground-25,4,25,0xff7073);themeBlock(ctx,c,ground-19,4,10,0xb13c49);themeBlock(ctx,c+3,ground-12,6,4,0xff7073);themeBlock(ctx,c+14,ground-22,4,12,0xb13c49);themeBlock(ctx,c+10,ground-13,6,4,0xff7073);}
  const uint32_t bone=0xffedcf,shade=0xc18672;
  themeBlock(ctx,124,y-40,23,14,bone);themeBlock(ctx,121,y-36,8,25,bone);themeBlock(ctx,110,y-25,20,17,bone);
  themeBlock(ctx,103,y-22,9,11,bone);themeBlock(ctx,99,y-28,5,12,bone);themeBlock(ctx,128,y-20,9,3,bone);themeBlock(ctx,134,y-18,3,4,bone);
  themeBlock(ctx,140,y-37,3,3,0xc72739);themeBlock(ctx,133,y-28,14,2,0x1e080b);
  int l=f.lift?1:f.leg;themeBlock(ctx,112,y-9,5,l?5:9,shade);themeBlock(ctx,112,y-(l?6:2),10,3,bone);themeBlock(ctx,124,y-9,5,l?9:5,bone);themeBlock(ctx,124,y-(l?2:6),10,3,bone);
}
static void cyberRim(lv_draw_ctx_t* ctx,uint32_t now,int second){
  for(int i=0;i<60;++i){float a=i*.104719755f;uint32_t c=i<=second?(i%2?0xff54d8:0x4ae9ff):0x283251;
    themeStroke(ctx,180+sinf(a)*165,180-cosf(a)*165,180+sinf(a)*(i%5?169:173),180-cosf(a)*(i%5?169:173),c,i%5?1:2);
  }
  for(int side:{-1,1}){int x=180+side*111;uint32_t c=side<0?0xff54d8:0x4ae9ff;
    themeStroke(ctx,x,105,x+side*18,123,c,2);themeStroke(ctx,x+side*18,123,x+side*18,219,0x384b6c,2);themeStroke(ctx,x+side*18,219,x,237,c,2);
    int y=130+(now/30)%80;themeBlock(ctx,x+side*18-2,y,5,7,c);
  }
}
