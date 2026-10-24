extern lv_img_dsc_t watch_logo_sparrows;
static void sparrowsDial(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(178),0x25272b);disc(ctx,CENTER,CENTER,S(175),0x000000);
  for(int q=0;q<4;++q){capsuleRing(ctx,168,0x891827,5,q*90+8,q*90+82);capsuleRing(ctx,159,0x42434a,1,q*90+12,q*90+78);}
  for(int i=0;i<60;++i){float a=i*.104719755f;int r=i%5?151:145;
    line(ctx,CENTER+sinf(a)*S(r),CENTER-cosf(a)*S(r),CENTER+sinf(a)*S(155),CENTER-cosf(a)*S(155),i%5?0x575b62:0xe6e7e9,std::max(1,S(i%5?1:2)));
  }
}
static void sparrowsPick(lv_draw_ctx_t* ctx,float turns,int length,bool hook){
  float a=turns*6.28318530718f;auto p=[&](int r,int side=0){return lv_point_t{lv_coord_t(CENTER+sinf(a)*S(r)+cosf(a)*S(side)),lv_coord_t(CENTER-cosf(a)*S(r)+sinf(a)*S(side))};};
  auto stroke=[&](int r,int s,int rr,int ss,uint32_t c,int w){auto x=p(r,s),y=p(rr,ss);line(ctx,x.x,x.y,y.x,y.y,c,std::max(1,S(w)));};
  stroke(-15,0,37,0,0xc3c8cc,9);stroke(-15,0,37,0,0x131519,6);
  for(int r=-6;r<31;r+=7)stroke(r,-2,r+2,2,0x7c8187,1);
  stroke(-13,0,-7,0,0xdf354a,3);stroke(37,0,length-6,0,0xcbd1d6,3);
  stroke(length-6,0,length,hook?-8:5,0xf1f3f5,3);
  if(!hook)stroke(length,5,length+4,0,0xf1f3f5,2);
}
static void sparrowsRadarField(lv_draw_ctx_t* ctx){
  sparrowsDial(ctx);themeArtwork(ctx,watch_logo_sparrows,108);
}
static void sparrowsWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
#if SCREEN_RES>=400
  const lv_font_t *small=&lv_font_montserrat_14,*medium=&lv_font_montserrat_20,*digits=&lv_font_montserrat_40;
#elif SCREEN_RES>=360
  const lv_font_t *small=&lv_font_montserrat_10,*medium=&lv_font_montserrat_16,*digits=&lv_font_montserrat_32;
#else
  const lv_font_t *small=&lv_font_montserrat_8,*medium=&lv_font_montserrat_10,*digits=&lv_font_montserrat_22;
#endif
  sparrowsDial(ctx);themeArtwork(ctx,watch_logo_sparrows,83);
  capsuleText(ctx,"12",160,42,40,medium,0xf1f1ee);capsuleText(ctx,"3",300,171,22,medium,0xf1f1ee);capsuleText(ctx,"9",38,171,22,medium,0xf1f1ee);
  if(valid){sparrowsPick(ctx,(date.hour%12+date.minute/60.f+date.second/3600.f)/12.f,70,true);sparrowsPick(ctx,(date.minute+date.second/60.f)/60.f,119,false);
    disc(ctx,CENTER,CENTER,S(7),0xa81e30);disc(ctx,CENTER,CENTER,S(3),0xe9edf1);int a=(int(date.second)*6+270)%360;capsuleRing(ctx,173,0xff586c,3,a,(a+4)%360);}
  themeStroke(ctx,122,251,238,251,0x852132);capsulePanel(ctx,105,255,150,61,0x000000,0x32141b,6);char value[30];
  if(valid)snprintf(value,sizeof(value),"%02d:%02d",date.hour,date.minute);else strcpy(value,"--:--");capsuleText(ctx,value,105,258,150,digits,0xf0f1ee);
  if(valid)snprintf(value,sizeof(value),"%04d.%02d.%02d",date.year,date.month,date.day);else strcpy(value,"SET TIME");capsuleText(ctx,value,104,299,152,small,0xff8790);
}
