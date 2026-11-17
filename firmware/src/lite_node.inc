// BISCUIT NODE and JCMK Node on the Lite UIs (waveshare_lite and M5Dial). While a
// node runs its screen covers the whole panel; pages, menus, popups and the radio
// wait until it stops. The widgets exist only while the screen is shown.
#include "node_mode.h"
#include "node_ui.h"
#include "lite_button.h"
static lv_obj_t* liteNodeHost=nullptr;      // full-panel parent of the node screen
static NodeKind liteNodeKind=NodeKind::None; // the node on screen, then the last one shown
static bool liteNodeLeave=false;            // stopped from its screen; leave on the next UI pass
static void liteModesShow();
#ifdef BOARD_M5DIAL
static bool dialMenuVisible();
static void dialMenuClose();
#endif
// Called from the EXIT button's event or nodeScreenLoop(): never clean the screen here.
static void liteNodeExited(){liteNodeLeave=true;}
static void liteNodeClose(){
  liteNodeLeave=false;if(!liteNodeHost)return;
  nodeScreenClose();lv_obj_del(liteNodeHost);liteNodeHost=nullptr;lv_obj_invalidate(root);
}
static void liteNodeOpen(){
  if(!nodeBusy()||(liteNodeHost&&!liteNodeLeave&&liteNodeKind==nodeKind()))return;
  liteNodeClose();liteNodeKind=nodeKind();
#ifdef BOARD_M5DIAL
  if(dialMenuVisible())dialMenuClose();     // also restores an unsaved brightness preview
#endif
  liteNodeHost=lv_obj_create(root);lv_obj_remove_style_all(liteNodeHost);lv_obj_set_size(liteNodeHost,SCREEN_RES,SCREEN_RES);
  lv_obj_clear_flag(liteNodeHost,LV_OBJ_FLAG_SCROLLABLE);lv_obj_update_layout(liteNodeHost); // nodeScreenOpen() sizes itself from it
  NodeScreenOptions options;options.exited=liteNodeExited;
#ifdef BOARD_M5DIAL
  options.exitButton=true;                  // tap EXIT, or press the front button
#else
  options.exitButton=false;options.exitHint="HOLD BUTTON TO EXIT";options.confirmHint="HOLD AGAIN TO LEAVE";
#endif
  nodeScreenOpen(liteNodeHost,options);
}
// Starts a node and shows its screen. False leaves the reason in nodeError().
static bool liteNodeBegin(NodeKind kind){if(!nodeStart(kind))return false;liteNodeOpen();return true;}
static const char* liteNodeError(){return *nodeError()?nodeError():"Could not start the node";}
// One UI pass. True while the node screen owns the panel.
static bool liteNodeLoop(){
  if(liteNodeLeave){liteNodeClose();if(!nodeBusy())liteModesShow();}
  if(nodeBusy())liteNodeOpen();
  if(!liteNodeHost)return false;
  nodeScreenLoop();return true;
}
// Hardware button while a node runs; true when the press belongs to the node.
// The Dial's front button is its exit press; on the Lite only a BOOT hold is.
// LiteButton reports two quick Dial presses as one Double: that is both presses.
static bool liteNodeButton(dragon::LitePress press){
  if(!liteNodeHost&&!nodeBusy())return false;
#ifdef BOARD_M5DIAL
  const bool leave=press!=dragon::LitePress::None,twice=press==dragon::LitePress::Double;
#else
  const bool leave=press==dragon::LitePress::Hold,twice=false;
#endif
  if(leave&&liteNodeHost&&!liteNodeLeave)Serial.printf("NODE_EXIT_PRESS source=%s\n",twice?"button_double":"button");
  if(leave&&liteNodeHost&&!liteNodeLeave&&!nodeScreenExitPress()&&twice&&nodeBusy()&&!liteNodeLeave)nodeScreenExitPress();
  return true;
}
