#pragma once
#include <lvgl.h>
#include "ui_palette.h"
#include "p4_launcher_art.h"
#include <cstdio>
#include <cstdint>

namespace p4ui {
struct LauncherItem { const char* name; const char* detail; const char* symbol; int action; bool available; };

// Parent-owned LVGL objects also own animation lifetimes; no independent timer.
class Launcher {
  lv_obj_t *tile=nullptr,*art=nullptr,*name=nullptr,*detail=nullptr,*position=nullptr,*orbit=nullptr;
  const LauncherItem* items=nullptr;
  dragon::UiPalette colors{};
  unsigned count=0,index=0;
  int phase=0,ringAngle=0;
  bool glowing=false;
  void (*selected)(int)=nullptr;
  static Launcher* owner(void* obj){return static_cast<Launcher*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(obj)));}
  static void enter(void* obj,int32_t value){lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj),value,0);}
  static void animate(void* obj,int32_t value){auto* self=owner(obj);self->phase=value;lv_obj_set_style_translate_y(self->art,lv_trigo_sin(value)*2/32767,0);lv_obj_invalidate(self->art);lv_obj_invalidate(self->orbit);}
  static void ring(void* obj,int32_t value){auto* self=owner(obj);self->ringAngle=value;lv_obj_invalidate(self->orbit);}
  static void drawArt(lv_event_t* e){
    auto* obj=lv_event_get_target(e);auto* self=owner(obj);lv_area_t box;lv_obj_get_coords(obj,&box);
    IconPainter p{lv_event_get_draw_ctx(e),box.x1,box.y1,self->phase,lv_color_hex(self->colors.accent),lv_color_hex(self->colors.ink)};
    p.draw(self->items[self->index].action);
  }
  static void drawOrbit(lv_event_t* e){
    auto* obj=lv_event_get_target(e);auto* self=owner(obj);auto* ctx=lv_event_get_draw_ctx(e);lv_area_t box;lv_obj_get_coords(obj,&box);
    lv_point_t center{lv_coord_t((box.x1+box.x2)/2),lv_coord_t((box.y1+box.y2)/2)};
    const auto accent=lv_color_hex(self->colors.accent),bg=lv_color_hex(self->colors.bg),ink=lv_color_hex(self->colors.ink);
    auto arc=[&](int r,int start,int end,int w,lv_color_t c){lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=c;d.width=w;d.rounded=1;
      lv_draw_arc(ctx,&d,&center,r,end-start>=360?0:(start%360+360)%360,end-start>=360?360:(end%360+360)%360);};
    arc(221,0,360,2,lv_color_hex(self->colors.border));
    for(unsigned i=0;i<self->count;++i){int a=int(i)*360/int(self->count)-90;
      arc(206,a-1,a+1,i==self->index?5:2,i==self->index?ink:lv_color_hex(self->colors.border));}
    const int head=self->ringAngle/10-90;
    // A layered comet follows the dial; its highlight breathes within the theme.
    for(int i=0;i<12;++i)arc(221,head-6*(12-i),head-6*(11-i)+1,8,lv_color_mix(accent,bg,lv_opa_t(25+i*18)));
    arc(221,head-5,head+4,9,lv_color_mix(ink,accent,lv_opa_t(155+lv_trigo_sin(self->phase)*70/32767)));
    arc(213,self->phase-90,self->phase-55,2,lv_color_mix(accent,bg,100));
  }
  static lv_obj_t* caption(lv_obj_t* parent,const char* text,int y,int width,const lv_font_t* font,uint32_t ink){
    auto* obj=lv_label_create(parent);lv_label_set_text(obj,text);lv_obj_set_width(obj,width);
    lv_obj_set_style_text_font(obj,font,0);lv_obj_set_style_text_color(obj,lv_color_hex(ink),0);
    lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(obj,LV_ALIGN_TOP_MID,0,y);return obj;
  }
  void refresh(int direction){
    const auto& item=items[index];lv_obj_invalidate(art);
    lv_label_set_text(name,item.name);lv_label_set_text(detail,item.available?item.detail:"Port in progress");
    char text[32];std::snprintf(text,sizeof(text),"%02u  /  %02u",index+1,count);lv_label_set_text(position,text);
    lv_anim_del(orbit,ring);
    const int target=int(index)*3600/int(count);
    if(direction){
      int from=(ringAngle%3600+3600)%3600,delta=(target-from+5400)%3600-1800;
      lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,orbit);lv_anim_set_exec_cb(&a,ring);
      lv_anim_set_values(&a,from,from+delta);lv_anim_set_time(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);
    }else {ringAngle=target;lv_obj_invalidate(orbit);}
    lv_anim_del(tile,enter);lv_obj_set_style_translate_x(tile,0,0);
    if(direction){lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,tile);lv_anim_set_exec_cb(&a,enter);
      lv_anim_set_values(&a,direction>0?32:-32,0);lv_anim_set_time(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);}
  }
public:
  unsigned current() const{return index;}
  const LauncherItem& currentItem() const{return items[index];}
  void forget(){tile=art=name=detail=position=orbit=nullptr;glowing=false;}
  void awake(bool value){
    if(!tile||glowing==value)return;glowing=value;lv_anim_del(tile,animate);
    if(!value){lv_anim_del(tile,enter);lv_anim_del(orbit,ring);lv_obj_set_style_translate_x(tile,0,0);ringAngle=int(index)*3600/int(count);return;}
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,tile);lv_anim_set_exec_cb(&a,animate);
    lv_anim_set_values(&a,0,359);lv_anim_set_time(&a,6000);lv_anim_set_repeat_count(&a,LV_ANIM_REPEAT_INFINITE);lv_anim_start(&a);
  }
  void create(lv_obj_t* parent,const dragon::UiPalette& palette,const LauncherItem* entries,unsigned size,void (*open)(int)){
    items=entries;count=size;selected=open;colors=palette;if(index>=count)index=0;
    orbit=lv_obj_create(parent);lv_obj_remove_style_all(orbit);lv_obj_set_size(orbit,466,466);lv_obj_center(orbit);
    lv_obj_set_user_data(orbit,this);lv_obj_clear_flag(orbit,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(orbit,drawOrbit,LV_EVENT_DRAW_MAIN,nullptr);
    caption(parent,"DRAGON DETECTOR",54,300,&lv_font_montserrat_18,palette.muted);
    position=caption(parent,"",83,250,&lv_font_montserrat_14,palette.accent);
    tile=lv_btn_create(parent);lv_obj_set_user_data(tile,this);lv_obj_set_size(tile,176,176);lv_obj_align(tile,LV_ALIGN_TOP_MID,0,124);
    lv_obj_set_style_radius(tile,46,0);lv_obj_set_style_bg_color(tile,lv_color_hex(palette.surface),0);

    lv_obj_set_style_border_width(tile,2,0);lv_obj_set_style_border_color(tile,lv_color_hex(palette.accent),0);
    lv_obj_set_style_shadow_color(tile,lv_color_hex(palette.accent),0);lv_obj_set_style_shadow_width(tile,32,0);lv_obj_set_style_shadow_opa(tile,90,0);
    art=lv_obj_create(tile);lv_obj_remove_style_all(art);lv_obj_set_size(art,120,120);lv_obj_center(art);lv_obj_set_user_data(art,this);
    lv_obj_clear_flag(art,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(art,drawArt,LV_EVENT_DRAW_MAIN,nullptr);
    lv_obj_add_event_cb(tile,[](lv_event_t* e){static_cast<Launcher*>(lv_event_get_user_data(e))->activate();},LV_EVENT_CLICKED,this);
    name=caption(parent,"",320,340,&lv_font_montserrat_26,palette.ink);
    detail=caption(parent,"",360,310,&lv_font_montserrat_14,palette.muted);
    caption(parent,"TURN / TAP TO OPEN",390,260,&lv_font_montserrat_14,palette.accent);
    glowing=false;awake(true);refresh(0);
  }
  bool turn(int steps){if(!tile||!count||!steps)return false;
    // Preserve every detent but replace the animation: fast turns never queue.
    index=unsigned((int64_t(index)+steps%int(count)+count)%count);refresh(steps);return true;}
  void activate(){if(tile&&selected)selected(items[index].action);}
};
}
