// 0ct0's Pig: a character-only dial and radar. Animation state advances in the
// UI tick, never in draw callbacks (LVGL may draw one frame in several tiles).
#include "octopig_art.inc"
static float pigX=180;
static int pigTarget=180,pigLook=0;
static uint32_t pigFrameAt=0,pigNextAt=0,pigReactUntil=0,pigStep=0;
static bool pigReacting=false;
static void asciiMark(lv_draw_ctx_t* ctx,const char* text,int x,int y,uint32_t ink){
  watchText(ctx,text,x-12,y-6,24,&dragon_ascii_small,ink);
}
static void octoMark(lv_draw_ctx_t* ctx,int y,uint32_t ink){
  const auto* f=&dragon_ascii_micro;int step=f->line_height;
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(ink);d.font=f;d.align=LV_TEXT_ALIGN_CENTER;
  for(unsigned row=0;row<23;++row){int top=S(y)+row*step;
    lv_area_t area{0,(lv_coord_t)top,SCREEN_RES-1,(lv_coord_t)(top+step-1)};lv_draw_label(ctx,&d,&area,OctoAscii[row],nullptr);
  }
}
static void radarPigArt(lv_draw_ctx_t* ctx){
  octoMark(ctx,74,0x7e3856);
  watchText(ctx,"[ 0ct0's Pig ]",65,34,230,&dragon_ascii_small,0xff89b6);
  for(int radius:{76,122,164})for(int n=0;n<36;++n){auto at=watchPolar(n/36.f,radius);asciiMark(ctx,n%3?".":"+",at.x,at.y,0x52243a);}
}
static void radarPigSweep(lv_draw_ctx_t* ctx,float angle){
  for(int r=26;r<=160;r+=14)asciiMark(ctx,r>145?"#":"+",180+std::lround(cosf(angle)*r),180+std::lround(sinf(angle)*r),0xff89b6);
}
static void pigTick(uint32_t now){
  if(now-pigFrameAt<120)return;
  pigFrameAt=now;++pigStep;
  if(pigReacting&&int32_t(now-pigReactUntil)>=0)pigReacting=false;
  if(int32_t(now-pigNextAt)>=0){
    pigTarget=144+int(esp_random()%73);pigNextAt=now+1800+esp_random()%2400;
    pigLook=pigTarget>pigX?1:-1;
  }
  const float delta=pigTarget-pigX;
  if(fabsf(delta)>1)pigX+=std::max(-2.f,std::min(2.f,delta));
  else if(!pigReacting&&pigStep%13==0)pigLook=int(esp_random()%3)-1;
  lv_area_t moving{(lv_coord_t)S(30),(lv_coord_t)S(162),(lv_coord_t)S(330),(lv_coord_t)S(308)};
  lv_obj_invalidate_area(watchDial,&moving);
}
static void pigReact(int x){
  pigTarget=constrain(x,144,216);pigLook=x<180?-1:1;
  pigReacting=true;pigReactUntil=millis()+1400;pigNextAt=pigReactUntil+900;
}
static void pigTap(lv_event_t* e){
  if(lv_event_get_target(e)!=watchDial)return;
  auto* input=lv_indev_get_act();if(!input)return;lv_point_t p;lv_indev_get_point(input,&p);
  pigReact(p.x*360/SCREEN_RES);
}
static void watchPig(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  octoMark(ctx,29,0xee77a5);
  watchText(ctx,"[ 0ct0's Pig ]",45,142,270,&dragon_ascii_small,0xffa3c6);
  int x=std::lround(pigX);char time[16];
  if(valid)snprintf(time,sizeof(time),"%02d:%02d:%02d",d.hour,d.minute,d.second);else strcpy(time,"--:--:--");
  watchText(ctx,"+--------------------------+",x-130,167,260,&dragon_ascii_small,0xc16c91);
  watchText(ctx,time,x-125,182,250,&dragon_ascii_digits,0xffdbe9);
  watchText(ctx,"+--------------------------+",x-130,220,260,&dragon_ascii_small,0xc16c91);
  watchText(ctx,pigReacting?"< OINK! >":valid?" /   24H":" / SET TIME",x-80,232,160,&dragon_ascii_small,0xff89b6);
  const char* eyes=pigStep%37<2?"  ( -   - )  ":pigLook<0?"  ( <   < )  ":pigLook>0?"  ( >   > )  ":"  ( o   o )  ";
  const char* pig[]={"   ^-----^   ",eyes,"  /  (oo) \\__",pigStep%6<3?" (  _____  )~":" (  _____  )-",pigStep%6<3?"  |_|   |_|  ":"  _||   ||_  "};
  for(int row=0;row<5;++row)watchText(ctx,pig[row],x-100,246+row*12,200,&dragon_ascii_small,0xffa1b6);
  // The time lives in the pig's bubble; no AM/PM is attached to the date.
}
