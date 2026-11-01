// Enamel-black field and gold chapter ring, matching the Valleytech watch.
// Sweep geometry is decorative, never a bearing measurement.
static void valleyRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(178),0x6d5b38);
  disc(ctx,CENTER,CENTER,S(177),0x080a0b);
  lv_point_t center{lv_coord_t(CENTER),lv_coord_t(CENTER)};
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.width=std::max(1,S(1));
  for(int radius:{171,160,108,54}){
    arc.color=color(radius==171?0xb89a5c:radius==160?0x3d3930:0x24231e);
    lv_draw_arc(ctx,&arc,&center,S(radius),0,359);
  }
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?165:162;
    line(ctx,CENTER+int(cosf(a)*S(inner)),CENTER+int(sinf(a)*S(inner)),CENTER+int(cosf(a)*S(168)),CENTER+int(sinf(a)*S(168)),i%5?0x584b33:0xc7ab73,std::max(1,S(1)));
  }
  // Restrained crosshair segments leave the center emblem and labels clear.
  for(int i=0;i<4;++i){float a=i*1.570796327f;line(ctx,CENTER+int(cosf(a)*S(120)),CENTER+int(sinf(a)*S(120)),CENTER+int(cosf(a)*S(153)),CENTER+int(sinf(a)*S(153)),0x302b21,std::max(1,S(1)));}
}
static void valleyRadarSweep(lv_draw_ctx_t* ctx,float angle){
  lv_point_t center{lv_coord_t(CENTER),lv_coord_t(CENTER)};
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.width=std::max(1,S(2));arc.color=color(0xd6b46e);
  lv_draw_rect_dsc_t glow;lv_draw_rect_dsc_init(&glow);glow.bg_color=color(0xd6b46e);
  for(int i=11;i>=0;--i){float a=angle-i*.035f,b=a-.035f;glow.bg_opa=31-i*2;
    lv_point_t wedge[]={{lv_coord_t(CENTER),lv_coord_t(CENTER)},{lv_coord_t(CENTER+cosf(a)*S(157)),lv_coord_t(CENTER+sinf(a)*S(157))},{lv_coord_t(CENTER+cosf(b)*S(157)),lv_coord_t(CENTER+sinf(b)*S(157))}};
    lv_draw_triangle(ctx,&glow,wedge);
    int degree=int(a*57.2957795f+720)%360;arc.opa=220-i*16;lv_draw_arc(ctx,&arc,&center,S(171),(degree+357)%360,degree);
  }
  int x=CENTER+int(cosf(angle)*S(157)),y=CENTER+int(sinf(angle)*S(157));
  line(ctx,CENTER,CENTER,x,y,0xd6b46e,std::max(1,S(2)),185);
  line(ctx,CENTER,CENTER,x,y,0xffedbc,std::max(1,S(1)),210);
  disc(ctx,CENTER+int(cosf(angle)*S(171)),CENTER+int(sinf(angle)*S(171)),std::max(1,S(2)),0xffe4a2);
  disc(ctx,CENTER,CENTER,S(4),0x8a7143);disc(ctx,CENTER,CENTER,S(2),0xf2dfad);
}
