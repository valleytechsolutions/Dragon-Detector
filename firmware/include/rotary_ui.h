#pragma once
#if defined(BOARD_CROWPANEL_21) || defined(BOARD_WATCHER)
#include <lvgl.h>
void rotaryUiBegin();
void rotaryUiLoop();
void rotaryUiInput(int steps,bool click,bool back);
lv_obj_t* uiRotaryScope();
bool uiRotaryWake();
// A running node takes the knob: a click only shows how to leave, a hold asks to leave. True while one runs.
bool uiRotaryNode(bool click,bool back);
#ifdef BOARD_WATCHER
bool uiRotaryOpenMenu();
void rotaryUiTouch(bool down);
void rotaryUiReset();
void rotaryUiDiagnostics();
#endif
#endif
