#pragma once
#include "board_pins.h"
#include <lvgl.h>
namespace dragon::labui {
inline int panelWidth(){
#ifdef BOARD_WAVESHARE_206
  return lv_disp_get_hor_res(nullptr);
#else
  return SCREEN_RES;
#endif
}
inline int px(int n){return n*panelWidth()/466;}
inline int logical(int n){return (n*466+panelWidth()-1)/panelWidth();}
inline int rootY(lv_obj_t* o,int y){
#ifdef BOARD_WAVESHARE_206
  if(lv_obj_get_parent(o)==lv_scr_act())return y*lv_disp_get_ver_res(nullptr)/466;
#endif
  return px(y);
}
inline void pos(lv_obj_t* o,int x,int y){lv_obj_set_pos(o,px(x),rootY(o,y));}
inline void size(lv_obj_t* o,int w,int h){lv_obj_set_size(o,px(w),rootY(o,h));}
inline void width(lv_obj_t* o,int w){lv_obj_set_width(o,px(w));}
inline void height(lv_obj_t* o,int h){lv_obj_set_height(o,rootY(o,h));}
#if defined(BOARD_XIAO_ROUND)
inline const lv_font_t* font14(){return &lv_font_montserrat_8;}
inline const lv_font_t* font16(){return &lv_font_montserrat_10;}
inline const lv_font_t* font18(){return &lv_font_montserrat_10;}
inline const lv_font_t* font26(){return &lv_font_montserrat_14;}
#elif defined(BOARD_WATCHER) || defined(BOARD_LILYGO_175) || defined(BOARD_WAVESHARE_143C) || defined(BOARD_AMOLED_175C) || (defined(BOARD_AMOLED_175) || defined(BOARD_WAVESHARE_206)) || defined(BOARD_STOPWATCH) || defined(BOARD_CROWPANEL_21) || defined(BOARD_WAVESHARE_P4_KNOB)
inline const lv_font_t* font14(){return &lv_font_montserrat_14;}
inline const lv_font_t* font16(){return &lv_font_montserrat_16;}
inline const lv_font_t* font18(){return &lv_font_montserrat_18;}
inline const lv_font_t* font26(){return &lv_font_montserrat_26;}
#else
inline const lv_font_t* font14(){return &lv_font_montserrat_10;}
inline const lv_font_t* font16(){return &lv_font_montserrat_12;}
inline const lv_font_t* font18(){return &lv_font_montserrat_14;}
inline const lv_font_t* font26(){return &lv_font_montserrat_20;}
#endif
}
