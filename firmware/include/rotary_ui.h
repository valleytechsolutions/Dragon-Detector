#pragma once
#if defined(BOARD_CROWPANEL_21) || defined(BOARD_WATCHER)
#include <lvgl.h>
void rotaryUiBegin();
void rotaryUiLoop();
void rotaryUiInput(int steps,bool click,bool back);
lv_obj_t* uiRotaryScope();
bool uiRotaryWake();
#ifdef BOARD_WATCHER
bool uiRotaryOpenMenu();
void rotaryUiTouch(bool down);
void rotaryUiReset();
#endif
#endif
