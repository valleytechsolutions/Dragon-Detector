extern lv_img_dsc_t watch_logo_dzaz;
static void crowFeather(lv_draw_ctx_t* ctx,float turns,int length,int width,uint32_t ink){
  const float angle=turns*6.28318530718f;
  auto point=[&](float along,float side=0){side+=sinf(along/length*3.14159265f)*4;return lv_point_t{lv_coord_t(CENTER+sinf(angle)*S(lroundf(along))+cosf(angle)*S(lroundf(side))),lv_coord_t(CENTER-cosf(angle)*S(lroundf(along))+sinf(angle)*S(lroundf(side)))};};
  auto stroke=[&](lv_point_t a,lv_point_t b,uint32_t c,int w){line(ctx,a.x,a.y,b.x,b.y,c,std::max(1,S(w)));};
  lv_draw_rect_dsc_t vane;lv_draw_rect_dsc_init(&vane);vane.bg_color=color(0x1a232e);
  for(int side:{-1,1})for(int r=18;r<length-5;r+=5){
    float spread=sinf((r-12.f)/(length-12)*3.14159265f)*width*(side<0?.8f:1.f);
    float next=sinf((r-7.f)/(length-12)*3.14159265f)*width*(side<0?.8f:1.f);
    lv_point_t triangle[]={point(12),point(r,spread*side),point(std::min(length,r+5),next*side)};lv_draw_triangle(ctx,&vane,triangle);
    stroke(point(r,spread*side),point(std::min(length,r+11),0),side<0?0x566777:ink,1);
  }
  for(int r=-12;r<length;r+=5)stroke(point(r),point(std::min(length,r+5)),0x04070b,4);
  for(int r=-12;r<length;r+=5)stroke(point(r),point(std::min(length,r+5)),ink,1);
}
static void dzazWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
#if SCREEN_RES>=466
  const lv_font_t *small=&lv_font_montserrat_16,*medium=&lv_font_montserrat_20,*digits=&lv_font_montserrat_40;
#elif SCREEN_RES>=360
  const lv_font_t *small=&lv_font_montserrat_12,*medium=&lv_font_montserrat_16,*digits=&lv_font_montserrat_32;
#else
  const lv_font_t *small=&lv_font_montserrat_8,*medium=&lv_font_montserrat_12,*digits=&lv_font_montserrat_22;
#endif
  disc(ctx,CENTER,CENTER,S(179),0x708393);disc(ctx,CENTER,CENTER,S(176),0x070a10);capsuleRing(ctx,171,0x263441);capsuleRing(ctx,166,0x17212b);
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?158:151;line(ctx,CENTER+sinf(a)*S(inner),CENTER-cosf(a)*S(inner),CENTER+sinf(a)*S(163),CENTER-cosf(a)*S(163),i%5?0x455361:0xc3ced8,std::max(1,S(i%5?1:2)));}
  themeArtwork(ctx,watch_logo_dzaz,54);
  capsuleText(ctx,"12",160,22,40,medium,0xdce5ed);capsuleText(ctx,"3",309,171,22,medium,0xdce5ed);capsuleText(ctx,"9",29,171,22,medium,0xdce5ed);
  if(valid){crowFeather(ctx,(d.hour%12+d.minute/60.f+d.second/3600.f)/12.f,87,17,0xa2b1c0);crowFeather(ctx,(d.minute+d.second/60.f)/60.f,126,13,0xf0f4f8);disc(ctx,CENTER,CENTER,S(5),0x71899e);disc(ctx,CENTER,CENTER,S(2),0xe0e8ee);int a=(d.second*6+270)%360;capsuleRing(ctx,173,0xa8c8df,2,a,(a+3)%360);}
  capsulePanel(ctx,103,262,154,41,0x070a10,0x273b4c,4);char value[32];
  if(valid)snprintf(value,sizeof(value),"%02d:%02d",d.hour,d.minute);else strcpy(value,"--:--");capsuleText(ctx,value,103,263,154,digits,0xe5edf4);
  if(valid)snprintf(value,sizeof(value),"%04d.%02d.%02d",d.year,d.month,d.day);else strcpy(value,"SET TIME");capsuleText(ctx,value,99,309,162,small,0x9eb6ca);
}
