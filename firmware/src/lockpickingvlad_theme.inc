#include "pick_hands.inc"
// The portrait, or nullptr when the packed 466 px copy could not be decoded (vlad_art.cpp).
const lv_img_dsc_t* vladPortraitImage();

static void vladDial(lv_draw_ctx_t* ctx, bool radar) {
  disc(ctx, CENTER, CENTER, S(180), 0x070708);
  if (const lv_img_dsc_t* portrait = vladPortraitImage()) {
    const int side = portrait->header.w, inset = (SCREEN_RES - side) / 2;
    lv_area_t bounds{lv_coord_t(inset), lv_coord_t(inset), lv_coord_t(inset+side-1), lv_coord_t(inset+side-1)};
    lv_draw_img_dsc_t image;
    lv_draw_img_dsc_init(&image);
    image.opa = radar ? 196 : LV_OPA_COVER;
    lv_draw_img(ctx, &image, &bounds, portrait);
  }
  capsuleRing(ctx, 176, 0xb38a43);
  capsuleRing(ctx, 173, 0x584427);
  for (int i=0; i<60; ++i) {
    const float a=i*.104719755f, dx=sinf(a), dy=-cosf(a);
    const bool hour=i%5==0;
    const int inner=hour ? 158 : 166;
    themeStroke(ctx, 180+dx*inner, 180+dy*inner, 180+dx*170, 180+dy*170,
                hour ? 0xf2d79b : 0x95794c, hour ? 3 : 1);
  }
  if (radar) {
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color=color(0xdbba77);
    arc.opa=65;
    arc.width=1;
    lv_point_t center{CENTER,CENTER};
    for (int r : {54,106,152}) lv_draw_arc(ctx,&arc,&center,S(r),0,359);
  }
}

// Both instruments use the traced McNally profiles, including their original lengths.
template<size_t N, size_t T>
static void vladPick(lv_draw_ctx_t* ctx, float turns, const lv_point_t (&profile)[N],
                     const uint8_t (&triangles)[T][3], bool longPick, float scale=1.f) {
  const float a=turns*6.28318530718f, dx=sinf(a), dy=-cosf(a);
  auto point=[&](float along, float across) -> lv_point_t {
    return {lv_coord_t(CENTER+lroundf((along*dx-across*dy)*scale*SCREEN_RES/360.f)),
            lv_coord_t(CENTER+lroundf((along*dy+across*dx)*scale*SCREEN_RES/360.f))};
  };
  lv_point_t outline[N];
  for (size_t i=0;i<N;++i) outline[i]=point(profile[i].x,profile[i].y);
  auto edge=[&](size_t i, uint32_t ink, int width) {
    auto p=outline[i], q=outline[(i+1)%N];
    line(ctx,p.x,p.y,q.x,q.y,ink,std::max(1,S(width)));
  };
  for (size_t i=0;i<N;++i) edge(i,0x100e0b,4);
  lv_draw_rect_dsc_t fill;
  lv_draw_rect_dsc_init(&fill);
  fill.bg_color=color(longPick ? 0xebcd8c : 0xad8242);
  for (const auto& t : triangles) {
    lv_point_t pts[]={outline[t[0]],outline[t[1]],outline[t[2]]};
    lv_draw_polygon(ctx,&fill,pts,3);
  }
  for (size_t i=0;i<N;++i) edge(i,i<N/2 ? 0xffe6af : 0x86693c,1);
  const int end=longPick ? 73 : 40;
  auto left=point(9,-3), right=point(end,-3);
  line(ctx,left.x,left.y,right.x,right.y,0xffe5a4,std::max(1,S(1)));
  for (int r : {14,end-5}) {
    auto p=point(r,0);
    disc(ctx,p.x,p.y,std::max(1,S(2)),0x513c20);
  }
}

static void vladWatch(lv_draw_ctx_t* ctx, const dragon::Calendar& date, bool valid) {
  vladDial(ctx,false);
  if (valid) {
    const float minutes=date.minute+date.second/60.f;
    vladPick(ctx,(date.hour%12+minutes/60.f)/12.f,pickShortHook,pickShortHookTriangles,false);
    vladPick(ctx,minutes/60.f,pickMcTickler,pickMcTicklerTriangles,true);
    disc(ctx,CENTER,CENTER,S(6),0x282016);
    disc(ctx,CENTER,CENTER,S(3),0xf7d891);
    const float a=date.second*.104719755f;
    disc(ctx,CENTER+lroundf(S(173)*sinf(a)),CENTER-lroundf(S(173)*cosf(a)),std::max(1,S(2)),0xffe6af);
  }
  char time[8]="--:--";
  if (valid) snprintf(time,sizeof(time),"%02u:%02u",unsigned(date.hour),unsigned(date.minute));
  capsulePanel(ctx,141,279,78,24,0x11100e,0x937445);
#if SCREEN_RES >= 400
  const lv_font_t* font=&lv_font_montserrat_20;
#elif SCREEN_RES >= 360
  const lv_font_t* font=&lv_font_montserrat_16;
#else
  const lv_font_t* font=&lv_font_montserrat_10;
#endif
  capsuleText(ctx,time,141,282,78,font,0xffe6af);
}
