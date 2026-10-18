#pragma once
#include "personal_theme_format.h"
#include "ui_palette.h"
#include <Arduino.h>
#include <lvgl.h>
namespace dragon {struct Calendar;}
void personalThemeBegin();
bool personalThemeCommand(const String&);
bool personalThemeReady();
const dragon::PersonalTheme& personalTheme();
const dragon::UiPalette& themeUiColors(unsigned);
const lv_img_dsc_t* personalThemeLogo(bool clock=false);
void personalThemeClock(lv_draw_ctx_t*,const dragon::Calendar&,bool);
void personalThemeSweep(lv_draw_ctx_t*,float);
void uiPersonalThemeChanged();
void uiPersonalThemeRemoved();
