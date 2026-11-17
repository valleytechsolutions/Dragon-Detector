#pragma once
// The node screen shared by every board: logo in the middle, the number of
// networks found below it, link state above it. Black with white artwork on
// every theme, so it reads the same on LCD and AMOLED panels.
#include <lvgl.h>

struct NodeScreenOptions {
  bool exitButton=true;          // touch or rotary boards: an on-screen EXIT
  const char* exitHint=nullptr;  // boards without one: how to leave, e.g. "HOLD BUTTON TO EXIT"
  const char* confirmHint=nullptr; // after the first exit request, e.g. "HOLD AGAIN TO LEAVE"
  void (*exited)()=nullptr;      // called after the node stopped from this screen
};

// Builds the screen inside parent, which must cover the whole panel.
void nodeScreenOpen(lv_obj_t* parent,const NodeScreenOptions& options);
// Forget the widgets; call before the parent is cleaned or deleted.
void nodeScreenClose();
bool nodeScreenShown();
// Refreshes the numbers and state. Cheap to call on every UI pass.
void nodeScreenLoop();
// An exit request from the EXIT button or a hardware key. The first one arms,
// a second within three seconds stops the node. True when the node stopped.
bool nodeScreenExitPress();
