// Original emblem in a circular inset; both pans turn with the balance beam.
// The animation is decoration, never a measured bearing. Shared with Lite.
static void radarLplEmblem(lv_draw_ctx_t* ctx,int opacity=160){
  const auto& logo=radar_logo_lpl;
  int x=(SCREEN_RES-logo.header.w)/2,y=(SCREEN_RES-logo.header.h)/2;
  lv_area_t area{(lv_coord_t)x,(lv_coord_t)y,(lv_coord_t)(x+logo.header.w-1),(lv_coord_t)(y+logo.header.h-1)};
  lv_draw_mask_radius_param_t mask;lv_draw_mask_radius_init(&mask,&area,LV_RADIUS_CIRCLE,false);
  int16_t id=lv_draw_mask_add(&mask,nullptr);
  lv_draw_img_dsc_t d;lv_draw_img_dsc_init(&d);d.opa=opacity;lv_draw_img(ctx,&d,&area,&logo);
  if(id>=0)lv_draw_mask_remove_id(id);lv_draw_mask_free_param(&mask);
}
static void radarLplScales(lv_draw_ctx_t* ctx,float angle){
  float dx=cosf(angle),dy=sinf(angle);
  auto point=[&](int along,int across){return lv_point_t{(lv_coord_t)S(180+lroundf(along*dx-across*dy)),(lv_coord_t)S(180+lroundf(along*dy+across*dx))};};
  auto stroke=[&](int a,int b,int c,int d,uint32_t ink,int width){auto p=point(a,b),q=point(c,d);line(ctx,p.x,p.y,q.x,q.y,ink,std::max(1,S(width)));};
  // Dark outline separates polished gold from the emblem and signal field.
  stroke(-117,0,117,0,0x020813,7);stroke(-117,0,117,0,0x9b7529,4);stroke(-117,-1,117,-1,0xffe597,1);
  for(int side:{-1,1}){
    int end=side*115;
    for(int edge:{-19,19}){stroke(end,0,end+edge,34,0x020813,4);stroke(end,0,end+edge,34,0xc4a249,1);}
    // A shallow faceted bowl, with a bright rim and darker underside.
    stroke(end-21,34,end+21,34,0x020813,6);stroke(end-21,34,end+21,34,0xffdf7e,2);
    stroke(end-19,35,end-12,42,0xa78332,3);stroke(end-12,42,end+12,42,0xa78332,3);stroke(end+12,42,end+19,35,0xa78332,3);
    auto p=point(end,0);disc(ctx,p.x,p.y,std::max(2,S(3)),0xffdf7e);
  }
  disc(ctx,CENTER,CENTER,S(7),0x020813);disc(ctx,CENTER,CENTER,S(5),0xd7ad46);disc(ctx,CENTER,CENTER,S(2),0xfff4c5);
}
