#pragma once
// The node screen shared by every board: logo in the middle, the number of
// networks found below it, link state above it. Black with white artwork on
// every theme, so it reads the same on LCD and AMOLED panels.
#include <lvgl.h>

struct NodeScreenOptions {
  bool exitButton=true;          // touch or rotary boards: an on-screen EXIT
  const char* exitHint=nullptr;  // boards without one: how to leave, e.g. "HOLD BUTTON TO EXIT"
  const char* confirmHint=nullptr; // after the first exit request, e.g. "HOLD AGAIN TO LEAVE"
  const char* keyHint=nullptr;   // shown for a moment after a short key press; default TAP EXIT TO LEAVE
  const char* keyHintShort=nullptr; // the same for a narrow status line
  void (*exited)()=nullptr;      // called after the node stopped from this screen
};

// Builds the screen inside parent, which must cover the whole panel.
void nodeScreenOpen(lv_obj_t* parent,const NodeScreenOptions& options);
// Forget the widgets; call before the parent is cleaned or deleted.
void nodeScreenClose();
bool nodeScreenShown();
// Refreshes the numbers and state. Cheap to call on every UI pass.
void nodeScreenLoop();
// How an exit request was made: EXIT on the screen (touch, or a knob that
// presses the focused EXIT), or a key held down.
enum class NodeExitInput : uint8_t { Screen, Hold };
// An exit request. The first arms; a second within three seconds (five after a
// hold) stops the node, if it comes at least half a second after the request
// before it, counted or not, so a bouncing contact, a double tap or a burst of
// quick taps is never both. True when the node stopped.
bool nodeScreenExitPress(NodeExitInput how=NodeExitInput::Screen);
// A short key press while the node screen is up. It is never an exit request:
// the status line says how to leave for a few seconds instead.
void nodeScreenKeyHint();
