#include "board_pins.h"
#ifndef BOARD_RADAR_LITE
#include "ui_theme.h"
#include "personal_theme.h"
#include <initializer_list>
namespace {
unsigned selected=1;
lv_theme_t theme{};
lv_style_t panel,ink,button,selection,disabled,scrollbar;
bool initialized=false;
void apply(lv_theme_t*,lv_obj_t* obj){
  lv_obj_add_style(obj,&scrollbar,LV_PART_SCROLLBAR);
  if(lv_obj_check_type(obj,&lv_label_class)){lv_obj_add_style(obj,&ink,0);return;}
  if(lv_obj_check_type(obj,&lv_btn_class)){
    lv_obj_add_style(obj,&button,0);lv_obj_add_style(obj,&selection,LV_STATE_PRESSED|LV_PART_MAIN);lv_obj_add_style(obj,&disabled,LV_STATE_DISABLED);return;
  }
  if(lv_obj_check_type(obj,&lv_btnmatrix_class)){
    lv_obj_add_style(obj,&panel,0);lv_obj_add_style(obj,&button,LV_PART_ITEMS);lv_obj_add_style(obj,&selection,LV_PART_ITEMS|LV_STATE_PRESSED);lv_obj_add_style(obj,&selection,LV_PART_ITEMS|LV_STATE_CHECKED);return;
  }
  if(lv_obj_check_type(obj,&lv_roller_class)){
    lv_obj_add_style(obj,&panel,0);lv_obj_add_style(obj,&selection,LV_PART_SELECTED);return;
  }
  if(lv_obj_check_type(obj,&lv_obj_class)||lv_obj_check_type(obj,&lv_msgbox_class)||lv_obj_check_type(obj,&lv_textarea_class))lv_obj_add_style(obj,&panel,0);
}
}
const dragon::UiPalette& uiColors(){return themeUiColors(selected);}
lv_obj_t* uiMessageBox(lv_obj_t* parent,const char* title,const char* text,const char* buttons[],bool close){
  const auto& p=uiColors();auto* box=lv_msgbox_create(parent,title,text,buttons,close);
  lv_obj_set_style_bg_color(box,lv_color_hex(p.surface),0);
  lv_obj_set_style_bg_opa(box,LV_OPA_COVER,0);
  lv_obj_set_style_border_color(box,lv_color_hex(p.accent),0);
  lv_obj_set_style_border_width(box,2,0);
  lv_obj_set_style_radius(box,uiAscii()?0:12,0);
  lv_obj_set_style_shadow_color(box,lv_color_hex(p.bg),0);
  lv_obj_set_style_shadow_width(box,20,0);
  lv_obj_set_style_shadow_opa(box,LV_OPA_50,0);
  if(auto* heading=lv_msgbox_get_title(box))lv_obj_set_style_text_color(heading,lv_color_hex(p.accent),0);
  if(auto* message=lv_msgbox_get_text(box))lv_obj_set_style_text_color(message,lv_color_hex(p.ink),0);
  auto* content=lv_msgbox_get_content(box);lv_obj_set_style_bg_opa(content,LV_OPA_TRANSP,0);
  if(auto* choices=lv_msgbox_get_btns(box)){
    lv_obj_set_style_bg_opa(choices,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(choices,0,0);
    lv_obj_set_width(choices,LV_PCT(100));
    lv_obj_set_height(choices,SCREEN_RES*40/360);
    lv_obj_set_style_border_color(choices,lv_color_hex(p.accent),LV_PART_ITEMS);
  }
  return box;
}
bool uiAscii(){return selected==24;}
const lv_font_t* uiFont(const lv_font_t* standard){
  if(!uiAscii())return standard;
  // Montserrat includes ascender/descender padding in line_height. Use the
  // board's actual regular-font metrics rather than rounding pixel sizes up.
#if defined(BOARD_LILYGO_175) || defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175) || defined(BOARD_STOPWATCH) || defined(BOARD_CROWPANEL_21)
  const int small=21,body=22;
#elif defined(BOARD_XIAO_ROUND)
  const int small=11,body=15;
#else
  const int small=16,body=18;
#endif
  if(standard->line_height<=small)return &dragon_ascii_small;
  if(standard->line_height<=body)return &dragon_ascii_body;
  return &dragon_ascii_heading;
}
void uiApplyTheme(unsigned index){
  selected=index<dragon::UiPaletteCount?index:1;const auto& p=uiColors();
  if(!initialized){for(auto* s:{&panel,&ink,&button,&selection,&disabled,&scrollbar})lv_style_init(s);initialized=true;}
  auto style=[](lv_style_t* s,uint32_t bg,uint32_t fg,uint32_t border){lv_style_set_bg_color(s,lv_color_hex(bg));lv_style_set_text_color(s,lv_color_hex(fg));lv_style_set_border_color(s,lv_color_hex(border));};
  style(&panel,p.bg,p.ink,p.border);style(&button,p.surface,p.ink,p.border);style(&selection,p.raised,p.ink,p.accent);
  lv_style_set_text_color(&ink,lv_color_hex(p.ink));lv_style_set_bg_color(&scrollbar,lv_color_hex(p.muted));
  for(auto* s:{&panel,&ink,&button,&selection})lv_style_set_text_font(s,uiFont(LV_FONT_DEFAULT));
  lv_style_set_color_filter_opa(&disabled,LV_OPA_TRANSP);lv_style_set_opa(&disabled,LV_OPA_50);
  auto* display=lv_disp_get_default();auto* base=lv_theme_default_init(display,lv_color_hex(p.accent),lv_color_hex(p.border),!p.light,uiFont(LV_FONT_DEFAULT));
  theme=*base;lv_theme_set_parent(&theme,base);lv_theme_set_apply_cb(&theme,apply);lv_disp_set_theme(display,&theme);lv_obj_report_style_change(nullptr);
}
#endif
