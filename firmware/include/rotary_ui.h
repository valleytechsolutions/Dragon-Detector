#pragma once
#if defined(BOARD_CROWPANEL_21) || defined(BOARD_WATCHER) || defined(BOARD_WAVESHARE_P4_KNOB)
#if defined(BOARD_WATCHER) || defined(BOARD_WAVESHARE_P4_KNOB)
// A knob beside a touch screen: keypad focus moves, and touch takes over input.
#define ROTARY_TOUCH_UI 1
#endif
#include <lvgl.h>
#ifdef BOARD_WAVESHARE_P4_KNOB
bool uiP4ClockOwnsDial();
bool uiP4TurnClock(int steps);
bool uiP4DialInput(int steps,bool click);
#endif
void rotaryUiBegin();
void rotaryUiLoop();
void rotaryUiInput(int steps,bool click,bool back);
lv_obj_t* uiRotaryScope();
bool uiRotaryWake();
// A running node takes the knob: a click only shows how to leave, a hold asks to leave. True while one runs.
bool uiRotaryNode(bool click,bool back);
#ifdef ROTARY_TOUCH_UI
bool uiRotaryOpenMenu();
void rotaryUiTouch(bool down);
void rotaryUiReset();
void rotaryUiDiagnostics();
#endif
#endif
