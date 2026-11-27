#pragma once
#include <lvgl.h>
#include "p4_menu.h"
extern const lv_img_dsc_t node_logo_biscuit_launcher;
extern const lv_img_dsc_t node_logo_jcmk_launcher;

namespace p4ui {
// Original vector artwork: shared geometry, stroke weight and theme colors.
struct IconPainter {
  lv_draw_ctx_t* ctx; int x,y,phase; lv_color_t accent,ink;
  void line(int ax,int ay,int bx,int by,bool bright=false,int width=5){
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=bright?ink:accent;d.width=width;d.round_start=d.round_end=1;
    lv_point_t a{lv_coord_t(x+ax),lv_coord_t(y+ay)},b{lv_coord_t(x+bx),lv_coord_t(y+by)};lv_draw_line(ctx,&d,&a,&b);
  }
  void arc(int cx,int cy,int radius,int start,int end,bool bright=false,int width=5){
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=bright?ink:accent;d.width=width;d.rounded=1;
    lv_point_t c{lv_coord_t(x+cx),lv_coord_t(y+cy)};lv_draw_arc(ctx,&d,&c,radius,start,end);
  }
  void rect(int ax,int ay,int bx,int by,int radius=8,bool fill=false,bool bright=false){
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=radius;d.bg_color=bright?ink:accent;
    d.bg_opa=fill?LV_OPA_COVER:LV_OPA_TRANSP;d.border_color=bright?ink:accent;d.border_width=fill?0:4;
    lv_area_t a{lv_coord_t(x+ax),lv_coord_t(y+ay),lv_coord_t(x+bx),lv_coord_t(y+by)};lv_draw_rect(ctx,&d,&a);
  }
  int wave(int offset,int amplitude=10)const{return lv_trigo_sin((phase+offset)%360)*amplitude/32767;}
  void hand(int cx,int cy,int angle,int radius,bool bright=false,int width=5){
    line(cx,cy,cx+lv_trigo_sin(angle%360)*radius/32767,cy-lv_trigo_sin((angle+90)%360)*radius/32767,bright,width);
  }
  void dot(int cx,int cy,int r=4,bool bright=true){rect(cx-r,cy-r,cx+r,cy+r,r,true,bright);}
  void clock(bool timer=false){arc(60,64,36,0,360);hand(60,64,phase,26,true,3);hand(60,64,310,19);dot(60,64,4);
    if(timer){line(49,17,71,17,true);line(60,17,60,27);line(89,29,96,22);}}
  void usb(){line(60,98,60,22,true);line(60,22,51,33,true);line(60,22,69,33,true);
    line(60,76,38,60);line(38,60,38,43);dot(38,39,5);line(60,62,82,48);line(82,48,82,35);rect(76,24,88,36,2,true);dot(60,100,6);}
  void network(bool mesh){dot(60,60,10);for(int i=0;i<3;++i){int a=i*120+phase/12;int px=60+lv_trigo_sin(a)*39/32767,py=60-lv_trigo_sin((a+90)%360)*39/32767;line(60,60,px,py,false,3);dot(px,py,7,i==phase/120);}
    if(mesh)arc(60,60,23,phase,(phase+90)%360,true,3);}
  void logo(const lv_img_dsc_t& source){
    // Supplied brand artwork (CREDITS.md), kept upright while its light and halo move.
    lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.recolor=lv_color_mix(ink,accent,lv_opa_t(190+wave(0,45)));
    d.recolor_opa=LV_OPA_COVER;d.antialias=1;
    d.pivot={lv_coord_t(source.header.w/2),lv_coord_t(source.header.h/2)};
    const int left=x+60-int(source.header.w)/2,top=y+60-int(source.header.h)/2;
    lv_area_t a{lv_coord_t(left),lv_coord_t(top),lv_coord_t(left+source.header.w-1),lv_coord_t(top+source.header.h-1)};
    lv_draw_img(ctx,&d,&a,&source);
    arc(60,60,56,phase,(phase+60)%360,false,2);arc(60,60,56,(phase+180)%360,(phase+200)%360,true,2);
  }
  void draw(int action){
    switch(action){
      case 1: // radar
        arc(60,60,43,0,360);arc(60,60,25,0,360,false,2);line(17,60,103,60,false,2);line(60,17,60,103,false,2);
        hand(60,60,phase,40,true,4);dot(79,37,4);dot(39,73,3);break;
      case int(Action::Modes):
        arc(60,60,37,205,345);line(96,45,98,61,true);line(98,61,83,55,true);
        arc(60,60,37,25,165,true);line(24,75,22,59);line(22,59,37,65);dot(60,60,5+wave(0,2));break;
      case 7103: case 61: // shield
        line(60,17,96,30);line(96,30,90,76);line(90,76,60,104);line(60,104,30,76);line(30,76,24,30);line(24,30,60,17);
        if(action==7103){line(67,38,48,64,true);line(48,64,70,64,true);line(70,64,54,84,true);}
        else {arc(60,58,19,0,360,true,3);hand(60,58,phase,16,true,3);}break;
      case 7102: case 40:
        if(action==40){arc(60,60,28,0,360);arc(60,60,11,0,360,true);for(int i=0;i<8;++i){int a=i*45+phase/18;line(60+lv_trigo_sin(a)*33/32767,60-lv_trigo_sin((a+90)%360)*33/32767,60+lv_trigo_sin(a)*43/32767,60-lv_trigo_sin((a+90)%360)*43/32767,false,7);}}
        else for(int i=0;i<3;++i){int yy=32+i*28,xx=60+wave(i*120,22);line(22,yy,98,yy,false,3);rect(xx-7,yy-8,xx+7,yy+8,5,true,true);}break;
      case -22800: logo(node_logo_biscuit_launcher);break;
      case -22801: logo(node_logo_jcmk_launcher);break;
      case int(Action::BleLab):case int(Action::BleContacts):
        line(57,17,57,103,true);line(57,17,83,39,true);line(83,39,35,82);line(35,38,83,81);line(83,81,57,103,true);
        arc(62,60,44,340,20+((phase/6)%20),false,3);break;
      case 20:
        arc(60,90,65,226,314);arc(60,90,43,226,314,true);arc(60,90,22,226,314);dot(60,91,5+wave(0,1));break;
      case 57:
        arc(60,60,32,0,360);for(int i=0;i<4;++i)hand(60,60,i*90,45,false,3);dot(60+wave(0,8),60+wave(90,8),6);break;
      case 60: clock();arc(60,64,46,180,275,false,2);line(14,63,10,47,true);line(14,63,28,55,true);break;
      case 153:
        rect(27,18,93,103);line(43,37,78,37,true,3);line(43,53,78,53,false,3);line(43,69,68+wave(0,8),69,false,3);line(21,35,33,35);line(21,57,33,57);line(21,80,33,80);break;
      case int(Action::Library):
        line(52,29,52,79,true);line(52,29,90,21,true);line(90,21,90,70,true);line(52,42,90,34,false,4);dot(42,85,11);dot(80,76,11);
        for(int i=0;i<3;++i){int h=7+wave(i*90,5);line(21+i*9,48-h,21+i*9,48+h,false,3);}break;
      case 190:
        rect(45,17,75,70,15,true);arc(60,61,27,0,180,true);line(33,50,33,61,true);line(87,50,87,61,true);line(60,89,60,103);line(44,103,76,103);
        for(int i=0;i<2;++i){int h=7+wave(i*90,4);line(20,45-h,20,45+h,false,3);line(100,45-h,100,45+h,false,3);}break;
      case 7120:usb();break;
      case 32:
        rect(19,34,101,95,9);line(20,34,20,23);line(20,23,50,23);line(50,23,60,34);line(34,55,86,55,true,3);dot(82,78,5+wave(0,2));break;
      case 133:clock();break;
      case 130:clock(true);break;
      case 50:
        rect(21,27,99,97);rect(44,19,76,33,6,true);dot(44,54,10);arc(44,82,17,195,345);line(68,50,86,50,true,3);line(68,64,86,64,false,3);line(68,78,81,78,false,3);break;
      case 22:
        arc(60,51,25,180,360);line(35,51,30,83);line(85,51,90,83);line(30,83,90,83,true);arc(60,86,11,10,170,true);dot(60,21,4);
        arc(60,51,40,205,250+wave(0,10),false,3);arc(60,51,40,290,335,false,3);break;
      case 99:
        rect(19,27,101,96);line(26,65,42,65,true);line(42,65,52,44,true);line(52,44,66,81,true);line(66,81,77,58,true);line(77,58,95,58,true);dot(30+phase*60/360,108,3);break;
      case 116:
        line(23,22,23,98,false,3);line(23,98,102,98,false,3);
        for(int i=0;i<5;++i){int h=20+wave(i*60,14);rect(34+i*13,87-h,41+i*13,88,3,true,i%2==0);}break;
      case 7111:
        line(40,19,91,19);line(91,19,91,102);line(91,102,29,102);line(29,102,29,33);line(29,33,40,19);
        for(int i=0;i<4;++i)line(43+i*11,25,43+i*11,42,true,5);rect(40,59,80,89,4);break;
      case 7200:
        rect(15,43,105,95);for(int r=0;r<2;++r)for(int c=0;c<6;++c)dot(27+c*13,56+r*13,2,(c+r)%2==0);
        line(40,83,80,83,true,4);line(60,43,60,20);line(60,20,84,20);break;
      case 181:
        arc(60,60,39,65,295);arc(77,48,34,108,247,true);dot(88,26,3+wave(0,1));dot(101,48,2);break;
      case 180:
        arc(60,65,37,305,235);line(60,15,60,61,true,7);break;
      default:rect(25,25,95,95,18);arc(60,60,23,phase,(phase+220)%360,true);break;
    }
  }
};
}
