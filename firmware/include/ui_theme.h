#pragma once
#include "ui_palette.h"
#include <lvgl.h>
const dragon::UiPalette& uiColors();
void uiApplyTheme(unsigned index);
lv_obj_t* uiMessageBox(lv_obj_t* parent,const char* title,const char* text,const char* buttons[],bool close);
#ifdef BOARD_WAVESHARE_143C
// The touch driver drops taps made on a picture that has since changed.
void waveshare143cPictureChanged(bool removed);
#endif
bool uiAscii();
const lv_font_t* uiFont(const lv_font_t* standard);
extern "C" {
LV_FONT_DECLARE(dragon_ascii_micro);
LV_FONT_DECLARE(dragon_ascii_small);
LV_FONT_DECLARE(dragon_ascii_body);
LV_FONT_DECLARE(dragon_ascii_heading);
LV_FONT_DECLARE(dragon_ascii_digits);
}
