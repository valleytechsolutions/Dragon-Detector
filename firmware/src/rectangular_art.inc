// Native panel versions of the themed instruments, scaled along the true hand angle.
struct PanelInstrument {
  dragon::PanelPoint from, to;
  lv_point_t at(float along,float across)const{
    const int dx=to.x-from.x,dy=to.y-from.y;
    return {lv_coord_t(from.x+lroundf(dx*along-dy*across)),
            lv_coord_t(from.y+lroundf(dy*along+dx*across))};
  }
};

static void panelStroke(lv_draw_ctx_t* ctx,lv_point_t a,lv_point_t b,uint32_t ink,int width=1,int opacity=255){
  line(ctx,a.x,a.y,b.x,b.y,ink,width,opacity);
}

template<size_t N,size_t T>
static void panelPick(lv_draw_ctx_t* ctx,const PanelInstrument& ray,
                      const lv_point_t (&profile)[N],const uint8_t (&triangles)[T][3],
                      float length,uint32_t ink,uint32_t highlight){
  lv_point_t points[N];
  for(size_t i=0;i<N;++i)points[i]=ray.at(profile[i].x/length,profile[i].y/length);
  lv_draw_rect_dsc_t fill;
  lv_draw_rect_dsc_init(&fill);
  fill.bg_color=color(ink);
  for(const auto& tri:triangles){
    const lv_point_t vertices[]{points[tri[0]],points[tri[1]],points[tri[2]]};
    lv_draw_triangle(ctx,&fill,vertices);
  }
  for(size_t i=0;i<N;++i)panelStroke(ctx,points[i],points[(i+1)%N],highlight);
  for(float step:{.12f,.29f}){
    const auto pin=ray.at(step,0);
    disc(ctx,pin.x,pin.y,2,highlight);
  }
}

static void panelPick(lv_draw_ctx_t* ctx,const PanelInstrument& ray,bool longPick,uint32_t ink,uint32_t highlight){
  if(longPick)panelPick(ctx,ray,pickMcTickler,pickMcTicklerTriangles,140.f,ink,highlight);
  else panelPick(ctx,ray,pickShortHook,pickShortHookTriangles,107.f,ink,highlight);
}

static void panelFeather(lv_draw_ctx_t* ctx,const PanelInstrument& ray,uint32_t ink,uint32_t highlight){
  panelStroke(ctx,ray.at(-.12f,0),ray.at(1,0),highlight,2);
  for(int i=0;i<12;++i){
    const float at=.10f+i*.065f;
    const float spread=.10f*(1-at);
    panelStroke(ctx,ray.at(at,0),ray.at(at+.16f,spread),ink,2);
    panelStroke(ctx,ray.at(at,0),ray.at(at+.11f,-spread),ink,2);
  }
}

static void panelSpear(lv_draw_ctx_t* ctx,const PanelInstrument& ray,uint32_t ink,uint32_t shade,uint32_t edge){
  const auto back=ray.at(-.12f,0),tip=ray.at(1,0);
  lv_draw_rect_dsc_t fill;
  lv_draw_rect_dsc_init(&fill);
  const lv_point_t left[]{back,ray.at(.25f,-.075f),tip};
  const lv_point_t right[]{back,tip,ray.at(.25f,.075f)};
  fill.bg_color=color(shade);
  lv_draw_triangle(ctx,&fill,left);
  fill.bg_color=color(ink);
  lv_draw_triangle(ctx,&fill,right);
  panelStroke(ctx,ray.at(.15f,0),ray.at(.86f,0),edge);
}

static bool panelLocksport(unsigned theme){
  switch(theme){
    case 9: case 21: case 26: case 28: case 32: case 41: case 42: case 43: case 44: case 45:
      return true;
    default: return theme==dragon::VladTheme;
  }
}

static void panelThemeSweep(lv_draw_ctx_t* ctx,const PanelInstrument& ray,unsigned theme,const RadarPalette& p,uint32_t tick){
  if(panelLocksport(theme)){
    panelPick(ctx,ray,true,p.ring,p.sweep);
  }else if(theme==33){
    panelFeather(ctx,ray,p.ring,p.text);
  }else if(theme==29){
    panelStroke(ctx,ray.at(.07f,0),ray.at(.92f,0),p.text,6);
    for(float end:{.07f,.94f})for(float side:{-.03f,.03f}){
      const auto joint=ray.at(end,side);
      disc(ctx,joint.x,joint.y,4,p.text);
    }
  }else if(theme==27||theme==30||theme==35){
    auto prior=ray.at(0,0);
    for(int i=1;i<=12;++i){
      const float along=i/12.f;
      const float signal=i<12?((i+(tick/150)%2)%2?.035f:-.035f):0.f;
      const auto point=ray.at(along,signal);
      panelStroke(ctx,prior,point,p.ring,5,120);
      panelStroke(ctx,prior,point,p.sweep,2);
      prior=point;
    }
  }else{
    panelStroke(ctx,ray.at(0,0),ray.at(1,0),p.sweep,4,85);
    panelStroke(ctx,ray.at(0,0),ray.at(1,0),p.sweep,2,230);
  }
}
