#pragma once
#ifdef BOARD_CROWPANEL_21
#include <lvgl.h>
void rotaryUiBegin();
void rotaryUiLoop();
void rotaryUiInput(int steps,bool click,bool back);
lv_obj_t* uiRotaryScope();
bool uiRotaryWake();
#endif
