// VectorPotential: supplied artwork and native Law Lock Tools-inspired picks.
// Reference silhouettes and original artwork attribution are in CREDITS.md.
extern lv_img_dsc_t radar_logo_vp,watch_logo_vp,watch_lock_vp;
static constexpr unsigned VectorPotentialTheme=41,VectorPotentialWatch=44;
#if SCREEN_RES>=400
static const lv_font_t *VpSmall=&lv_font_montserrat_14,*VpMedium=&lv_font_montserrat_20;
#elif SCREEN_RES>=360
static const lv_font_t *VpSmall=&lv_font_montserrat_10,*VpMedium=&lv_font_montserrat_16;
#else
static const lv_font_t *VpSmall=&lv_font_montserrat_8,*VpMedium=&lv_font_montserrat_10;
#endif
static void vpRim(lv_draw_ctx_t* ctx){
  capsuleRing(ctx,175,0x284550,2);capsuleRing(ctx,170,0x7fcbdc);
  for(int i=0;i<60;++i){float a=i*.104719755f;int inner=i%5?164:158;
    themeStroke(ctx,180+sinf(a)*inner,180-cosf(a)*inner,180+sinf(a)*167,180-cosf(a)*167,i%5?0x45636e:0xdaf8ff,i%5?1:2);
  }
}
static void vpRadarField(lv_draw_ctx_t* ctx){
  disc(ctx,CENTER,CENTER,S(179),0x000000);vpRim(ctx);
  for(int r:{42,84,126})capsuleRing(ctx,r,0x15323c);
  themeStroke(ctx,25,180,335,180,0x15323c);themeStroke(ctx,180,102,180,272,0x15323c);
  themeArtwork(ctx,radar_logo_vp,123);
  capsuleText(ctx,"VECTOR POTENTIAL",60,36,240,VpSmall,0xdaf8ff);
}
static void vpPick(lv_draw_ctx_t* ctx,float turns,int length,bool nessie){
  // Two full tools, with the laminated PRO handle, three slotted grips, a
  // narrow shank and distinct tips. Nessie's three waves and hooked end are
  // deliberately larger than true scale so its profile reads on a watch.
  float a=turns*6.28318530718f,dx=sinf(a),dy=-cosf(a);
  auto p=[&](float r,float side){return lv_point_t{lv_coord_t(S(180)+dx*S(r)+cosf(a)*S(side)),lv_coord_t(S(190)+dy*S(r)+sinf(a)*S(side))};};
  auto stroke=[&](float r,float s,float rr,float ss,uint32_t ink,int w){auto x=p(r,s),y=p(rr,ss);line(ctx,x.x,x.y,y.x,y.y,ink,std::max(1,S(w)));};
  uint32_t steel=nessie?0xf4f7fa:0x91e3f0;
  stroke(-17,0,42,0,0x020509,17);stroke(-16,0,41,0,0x526a78,13);
  stroke(-15,-2,40,-2,steel,8);stroke(-11,0,-6,0,0x152631,4);
  stroke(-2,0,10,0,0x152631,3);
  for(int r:{18,26,34})stroke(r,-3,r+3,2,0x152631,2);
  stroke(42,0,length-(nessie?28:11),0,0x020509,7);
  stroke(42,0,length-(nessie?28:11),0,steel,3);
  // Outline each tip before its bright steel edge for contrast on the lock.
  for(int pass=0;pass<2;++pass){uint32_t ink=pass?steel:0x020509;int w=pass?3:7;
    if(nessie){
      const lv_point_t tip[]={{-28,0},{-24,-4},{-20,0},{-16,-4},{-12,0},{-8,-4},{-4,-3},{0,4}};
      for(unsigned i=1;i<sizeof(tip)/sizeof(tip[0]);++i)stroke(length+tip[i-1].x,tip[i-1].y,length+tip[i].x,tip[i].y,ink,w);
    }else{stroke(length-11,0,length-5,-4,ink,w);stroke(length-5,-4,length,-11,ink,w);}
  }
  stroke(-15,-2,-10,-2,nessie?0xf1384e:0x42cfe8,3);
}
static void vpWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid){
  disc(ctx,CENTER,CENTER,S(179),0x000000);
  // Large lock fills the dial. The VP mark sits in the opening below its arch.
  lv_area_t bounds{lv_coord_t(CENTER-S(170)),lv_coord_t(CENTER-S(170)),lv_coord_t(CENTER+S(170)),lv_coord_t(CENTER+S(170))};
  lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&bounds,LV_RADIUS_CIRCLE,false);
  int16_t maskId=lv_draw_mask_add(&mask,nullptr);themeArtwork(ctx,watch_lock_vp,14);
  if(maskId>=0)lv_draw_mask_remove_id(maskId);lv_draw_mask_free_param(&mask);
  vpRim(ctx);themeArtwork(ctx,watch_logo_vp,99);
  capsuleText(ctx,"12",165,25,30,VpMedium,0xe4faff);
  capsuleText(ctx,"3",318,172,20,VpMedium,0xe4faff);
  capsuleText(ctx,"9",22,172,20,VpMedium,0xe4faff);
  if(valid){
    vpPick(ctx,(date.hour%12+date.minute/60.f+date.second/3600.f)/12.f,88,false);
    vpPick(ctx,(date.minute+date.second/60.f)/60.f,133,true);
    disc(ctx,S(180),S(190),S(7),0x07141b);disc(ctx,S(180),S(190),S(4),0xe5f9ff);disc(ctx,S(180),S(190),S(2),0xe82e47);
    int angle=(int(date.second)*6+270)%360;capsuleRing(ctx,173,0xf04459,3,angle,(angle+3)%360);
  }
  // Keep the artwork readable; a small, crisp lower readout provides precision.
  capsulePanel(ctx,111,290,138,34,0x030a12,0x315b68,5);char value[32];
  if(valid)snprintf(value,sizeof(value),"%02d:%02d:%02d",date.hour,date.minute,date.second);else strcpy(value,"SET TIME");
  capsuleText(ctx,value,112,292,136,VpMedium,0xe4faff);
  if(valid)snprintf(value,sizeof(value),"%02d.%02d.%04d",date.month,date.day,date.year);else strcpy(value,"CLOCK NOT SYNCED");
  capsuleText(ctx,value,108,313,144,VpSmall,0xa2d2df);
}
