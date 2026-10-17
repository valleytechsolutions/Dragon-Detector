#pragma once
#include "ui_palette.h"
#include <lvgl.h>
const dragon::UiPalette& uiColors();
void uiApplyTheme(unsigned index);
bool uiAscii();
const lv_font_t* uiFont(const lv_font_t* standard);
extern "C" {
LV_FONT_DECLARE(dragon_ascii_micro);
LV_FONT_DECLARE(dragon_ascii_small);
LV_FONT_DECLARE(dragon_ascii_body);
LV_FONT_DECLARE(dragon_ascii_heading);
LV_FONT_DECLARE(dragon_ascii_digits);
}
