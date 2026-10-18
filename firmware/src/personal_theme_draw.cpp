#include "app.h"
#include "personal_theme.h"
#include <cmath>

namespace {
constexpr float Tau=6.28318530718f;
int scale(int v){return (v*SCREEN_RES+(v>=0?180:-180))/360;}
#if SCREEN_RES<=240
const lv_font_t* small=&lv_font_montserrat_10;
const lv_font_t* large=&lv_font_montserrat_22;
#elif SCREEN_RES>=466
const lv_font_t* small=&lv_font_montserrat_18;
const lv_font_t* large=&lv_font_montserrat_40;
#else
const lv_font_t* small=&lv_font_montserrat_14;
const lv_font_t* large=&lv_font_montserrat_32;
#endif
lv_point_t polar(float turn,float radius,float side=0){float a=turn*Tau;return {lv_coord_t(scale(lroundf(180+sinf(a)*radius+cosf(a)*side))),lv_coord_t(scale(lroundf(180-cosf(a)*radius+sinf(a)*side)))};}
void stroke(lv_draw_ctx_t* ctx,lv_point_t a,lv_point_t b,uint32_t rgb,int width=1,int opacity=255){lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=lv_color_hex(rgb);d.width=std::max(1,scale(width));d.opa=opacity;d.round_start=d.round_end=1;lv_draw_line(ctx,&d,&a,&b);}
void box(lv_draw_ctx_t* ctx,int x,int y,int w,int h,uint32_t rgb,int radius=0){lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=lv_color_hex(rgb);d.radius=scale(radius);lv_area_t a{lv_coord_t(scale(x)),lv_coord_t(scale(y)),lv_coord_t(scale(x+w)-1),lv_coord_t(scale(y+h)-1)};lv_draw_rect(ctx,&d,&a);}
void text(lv_draw_ctx_t* ctx,const char* value,int x,int y,int w,uint32_t rgb,const lv_font_t* font=small){lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=lv_color_hex(rgb);d.font=font;d.align=LV_TEXT_ALIGN_CENTER;lv_area_t a{lv_coord_t(scale(x)),lv_coord_t(scale(y)),lv_coord_t(scale(x+w)),lv_coord_t(scale(y)+font->line_height+2)};lv_draw_label(ctx,&d,&a,value,nullptr);}
void ring(lv_draw_ctx_t* ctx,int radius,uint32_t rgb,int width){lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=lv_color_hex(rgb);d.width=std::max(1,scale(width));lv_point_t center{SCREEN_RES/2,SCREEN_RES/2};lv_draw_arc(ctx,&d,&center,scale(radius),0,360);}
void hand(lv_draw_ctx_t* ctx,float turns,int length,bool minute){
  const auto& c=personalTheme();const uint32_t rgb=minute?c.secondary:c.ink;
  stroke(ctx,polar(turns,-12),polar(turns,length),c.bg,c.hands==1?10:7);
  stroke(ctx,polar(turns,-10),polar(turns,length),rgb,c.hands==1?6:3);
  if(c.hands==2){lv_point_t triangle[]={polar(turns,length),polar(turns,length-22,8),polar(turns,length-22,-8)};lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=lv_color_hex(rgb);lv_draw_triangle(ctx,&d,triangle);}
  if(c.hands==3){stroke(ctx,polar(turns,-9),polar(turns,length*.4f),c.accent,9);stroke(ctx,polar(turns,length-10),polar(turns,length,-7),rgb,4);}
}
void digits(lv_draw_ctx_t* ctx,const char* value,int y,uint32_t rgb){
  const unsigned font=personalTheme().font;
  if(!font){text(ctx,value,50,y,260,rgb,large);return;}
  static const uint8_t pixels[10][7]={{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
  static const uint8_t segments[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
  int x=180-int(strlen(value))*16;
  for(const char* p=value;*p;++p,x+=32){
    if(*p==':'){box(ctx,x+12,y+8,4,4,rgb);box(ctx,x+12,y+24,4,4,rgb);continue;}
    if(*p=='-'){box(ctx,x+4,y+16,20,3,rgb);continue;}
    if(*p<'0'||*p>'9')continue;
    const unsigned n=*p-'0';
    if(font==1){for(unsigned row=0;row<7;++row)for(unsigned col=0;col<5;++col)if(pixels[n][row]&(16>>col))box(ctx,x+col*5,y+row*5,4,4,rgb);}
    else {const unsigned mask=segments[n];const int rects[7][4]={{4,0,18,3},{22,3,3,13},{22,19,3,13},{4,32,18,3},{1,19,3,13},{1,3,3,13},{4,16,18,3}};for(unsigned i=0;i<7;++i)if(mask&(1<<i))box(ctx,x+rects[i][0],y+rects[i][1],rects[i][2],rects[i][3],rgb,1);}
  }
}
}
void personalThemeClock(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
  const auto& c=personalTheme();const auto& ui=themeUiColors(dragon::PersonalThemeId);
  box(ctx,0,0,360,360,c.bg);ring(ctx,176,c.accent,2);ring(ctx,171,ui.border,1);
  if(c.markers!=3){
    for(int n=0;n<60;++n)stroke(ctx,polar(n/60.f,n%5?164:156),polar(n/60.f,168),n%5?ui.border:c.secondary,n%5?1:2);
    if(c.markers<2){static const char* roman[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};for(int n=0;n<12;++n){if(c.layout==1&&n>=5&&n<=7)continue;auto p=polar(n/12.f,140);char number[4];snprintf(number,sizeof(number),"%d",n?n:12);text(ctx,c.markers?roman[n]:number,p.x*360/SCREEN_RES-20,p.y*360/SCREEN_RES-8,40,c.ink);}}
  }
  if(const auto* logo=personalThemeLogo(true)){
    const int side=logo->header.w,x=(SCREEN_RES-side)/2,y=scale(c.layout==2?56:62);
    lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);lv_area_t a{lv_coord_t(x),lv_coord_t(y),lv_coord_t(x+side-1),lv_coord_t(y+side-1)};lv_draw_img(ctx,&d,&a,logo);
  }
  if(c.layout!=2&&valid){hand(ctx,(date.hour%12+date.minute/60.f+date.second/3600.f)/12.f,83,false);hand(ctx,(date.minute+date.second/60.f)/60.f,116,true);box(ctx,174,174,12,12,c.accent,6);}
  if(valid&&(c.flags&1)){auto p=polar(date.second/60.f,173);box(ctx,p.x*360/SCREEN_RES-3,p.y*360/SCREEN_RES-3,6,6,c.accent,3);}
  char value[32];
  if(c.layout){if(valid)snprintf(value,sizeof(value),"%02d:%02d",c.flags&4?date.hour:dragon::hour12(date.hour),date.minute);else strcpy(value,"--:--");
    const int y=c.layout==2?197:260;box(ctx,93,y-3,174,43,c.bg,4);digits(ctx,value,y,c.ink);
    if(valid&&!(c.flags&4))text(ctx,date.hour<12?"AM":"PM",145,y-22,70,c.secondary);
  }
  if(!valid)text(ctx,"SET TIME IN SETTINGS",66,305,228,c.accent);
  else if(c.flags&2){snprintf(value,sizeof(value),"%04d-%02d-%02d",date.year,date.month,date.day);text(ctx,value,80,c.layout==2?254:c.layout==1?305:278,200,c.secondary);}
}
void personalThemeSweep(lv_draw_ctx_t* ctx,float angle){
  const auto& c=personalTheme();const float turn=angle/Tau+.25f;
  if(c.sweep==2){for(int n=12;n>=0;--n){auto p=polar(turn-n*.006f,145);box(ctx,p.x*360/SCREEN_RES-3,p.y*360/SCREEN_RES-3,6,6,n?themeUiColors(dragon::PersonalThemeId).border:c.accent,3);}}
  else {
    for(int n=10;n>=1;--n)stroke(ctx,polar(turn,0),polar(turn-n*.008f,164),c.accent,2,45-n*4);
    stroke(ctx,polar(turn,0),polar(turn,164),c.accent,2);
    if(c.sweep==1)stroke(ctx,polar(turn,0),polar(turn+.5f,164),c.secondary,2);
  }
}
