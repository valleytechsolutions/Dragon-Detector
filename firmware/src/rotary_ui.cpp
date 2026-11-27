#if defined(BOARD_CROWPANEL_21) || defined(BOARD_WATCHER) || defined(BOARD_WAVESHARE_P4_KNOB)
#include "app.h"
#include "rotary_ui.h"
#include "rotary_input.h"
#include "ui_theme.h"

namespace {
lv_group_t* group=nullptr;
lv_obj_t* members[160]{};
unsigned memberCount=0;
#ifdef ROTARY_TOUCH_UI
// One current navigation intent, never a queue of turns to replay after a
// slow frame. Ordinary individual detents still each move focus once.
int pendingMove=0;
uint32_t moveAt=0;
uint32_t appliedMoves=0,appliedClicks=0,coalescedMoves=0,expiredMoves=0,touchCancelled=0;
#else
dragon::RotaryNavigation navigation;
#endif
unsigned pendingClicks=0;
bool releasing=false;
#ifdef ROTARY_TOUCH_UI
lv_indev_t* encoderInput=nullptr;
bool touchActive=false,touchSeen=false;
uint32_t touchAt=0;
uint32_t activeKey=LV_KEY_ENTER;
bool touchOwnsInput(){return touchActive||(touchSeen&&uint32_t(millis()-touchAt)<150);}
void cancelEncoder(){
  pendingMove=0;pendingClicks=0;releasing=false;
  if(encoderInput)lv_indev_reset(encoderInput,nullptr);
}
#endif

lv_obj_t* dialog(lv_obj_t* obj){
  if(!obj||lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))return nullptr;
  for(int i=int(lv_obj_get_child_cnt(obj))-1;i>=0;--i)
    if(auto* box=dialog(lv_obj_get_child(obj,i)))return box;
  return lv_obj_check_type(obj,&lv_msgbox_class)?obj:nullptr;
}
bool selectable(lv_obj_t* obj){
  return lv_obj_has_flag(obj,LV_OBJ_FLAG_CLICKABLE)&&
    (lv_obj_check_type(obj,&lv_btn_class)||lv_obj_check_type(obj,&lv_btnmatrix_class)||
     lv_obj_check_type(obj,&lv_roller_class)||lv_obj_check_type(obj,&lv_dropdown_class)||
     lv_obj_check_type(obj,&lv_slider_class)||lv_obj_check_type(obj,&lv_switch_class)||
     lv_obj_check_type(obj,&lv_checkbox_class)||lv_obj_check_type(obj,&lv_textarea_class));
}
void collect(lv_obj_t* obj,lv_obj_t** found,unsigned& count){
  if(!obj||lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN)||lv_obj_has_state(obj,LV_STATE_DISABLED))return;
  if(selectable(obj)&&count<160)found[count++]=obj;
  for(unsigned i=0;i<lv_obj_get_child_cnt(obj);++i)collect(lv_obj_get_child(obj,i),found,count);
}
void readEncoder(lv_indev_drv_t*,lv_indev_data_t* data){
  data->key=LV_KEY_ENTER;data->enc_diff=0;
#ifdef ROTARY_TOUCH_UI
  if(touchOwnsInput()){cancelEncoder();data->state=LV_INDEV_STATE_REL;return;}
#endif
#ifdef ROTARY_TOUCH_UI
  // Finish each key press/release in one LVGL read cycle, before rendering.
  if(releasing){
    releasing=false;data->key=activeKey;data->state=LV_INDEV_STATE_REL;
  }else{
    if(pendingClicks){pendingClicks=0;pendingMove=0;activeKey=LV_KEY_ENTER;++appliedClicks;}
    else if(pendingMove){
      int move=pendingMove;pendingMove=0;
      if(uint32_t(millis()-moveAt)>350){++expiredMoves;data->state=LV_INDEV_STATE_REL;return;}
      activeKey=move>0?LV_KEY_NEXT:LV_KEY_PREV;++appliedMoves;
    }else {data->key=activeKey;data->state=LV_INDEV_STATE_REL;return;}
    data->key=activeKey;data->state=LV_INDEV_STATE_PR;releasing=true;
  }
  data->continue_reading=releasing; // a selection cannot remain held across slow redraws
#else
  if(releasing){releasing=false;data->state=LV_INDEV_STATE_REL;}
  else if(pendingClicks){--pendingClicks;releasing=true;data->state=LV_INDEV_STATE_PR;}
  else {data->enc_diff=navigation.take(millis());data->state=LV_INDEV_STATE_REL;}
#endif
}
}
void rotaryUiBegin(){
  group=lv_group_create();lv_group_set_wrap(group,true);
  static lv_indev_drv_t driver;lv_indev_drv_init(&driver);
#ifdef ROTARY_TOUCH_UI
  driver.type=LV_INDEV_TYPE_KEYPAD;
#else
  driver.type=LV_INDEV_TYPE_ENCODER;
#endif
  driver.read_cb=readEncoder;
  auto* input=lv_indev_drv_register(&driver);lv_indev_set_group(input,group);
#ifdef ROTARY_TOUCH_UI
  encoderInput=input;
#endif
}
void rotaryUiInput(int steps,bool click,bool back){
  if(!group||(!steps&&!click&&!back))return;
#ifdef ROTARY_TOUCH_UI
  if(touchOwnsInput()){++touchCancelled;cancelEncoder();return;}
#endif
  if(uiRotaryWake()){
#ifdef ROTARY_TOUCH_UI
    cancelEncoder();
#else
    navigation.clear();
#endif
    return;
  }
  // On the node screen a knob click would land on EXIT, and two clicks would
  // leave: the knob is no screen tap there, so nothing reaches LVGL.
  if(uiRotaryNode(click,back)){
#ifdef ROTARY_TOUCH_UI
    cancelEncoder();  // the Watcher keeps one pending move instead of a navigation queue
#else
    navigation.clear();
#endif
    pendingClicks=0;releasing=false;lv_group_set_editing(group,false);
    return;
  }
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(!back && uiP4DialInput(steps,click)){
    cancelEncoder();
    return;
  }
  if(!click && !back && uiP4TurnClock(steps)){
    cancelEncoder();
    return;
  }
#endif
  if(back){
#ifdef ROTARY_TOUCH_UI
    cancelEncoder();
#endif
#ifndef ROTARY_TOUCH_UI
    navigation.clear();
#endif
    pendingClicks=0;releasing=false;lv_group_set_editing(group,false);
#ifdef ROTARY_TOUCH_UI
    if(uiRotaryOpenMenu())return;
#endif
    uiKey(true);return;
  }
#ifdef ROTARY_TOUCH_UI
  if(click){pendingMove=0;pendingClicks=1;}
  else if(!pendingClicks&&!releasing){
    if(steps){
      unsigned magnitude=steps>0?unsigned(steps):unsigned(-steps);
      coalescedMoves+=magnitude-1+(pendingMove!=0);
      pendingMove=steps>0?1:-1;moveAt=millis();
    }
  }
#else
  if(click){navigation.clear();pendingClicks=1;}
  else if(!pendingClicks&&!releasing)navigation.push(steps,millis());
#endif
}
#ifdef ROTARY_TOUCH_UI
void rotaryUiTouch(bool down){
  if(down||touchActive){touchAt=millis();touchSeen=true;}
  touchActive=down;
  if(down){
    if(pendingMove||pendingClicks||releasing)++touchCancelled;
    cancelEncoder();
    // Touch keeps its normal focus anchor, without the wheel's focus outline.
    if(group)if(auto* focused=lv_group_get_focused(group))lv_obj_clear_state(focused,LV_STATE_FOCUS_KEY);
  }
}
void rotaryUiReset(){
  cancelEncoder();memberCount=0;
  if(group){lv_group_remove_all_objs(group);lv_group_set_editing(group,false);}
}
void rotaryUiDiagnostics(){
  Serial.printf("ROTARY_UI moves=%lu selects=%lu coalesced=%lu expired=%lu touch_cancelled=%lu pending=%d touching=%u\n",
    appliedMoves,appliedClicks,coalescedMoves,expiredMoves,touchCancelled,pendingMove,touchOwnsInput());
}
#endif
void rotaryUiLoop(){
  if(!group)return;
  auto* scope=uiRotaryScope();
  if(auto* box=dialog(lv_layer_top()))scope=box;
  else if(auto* box=dialog(scope))scope=box;
  lv_obj_t* found[160]{};unsigned count=0;collect(scope,found,count);
  bool changed=count!=memberCount||lv_group_get_obj_count(group)!=count;
  for(unsigned i=0;i<count&&!changed;++i)changed=found[i]!=members[i];
  if(!changed)return;
#ifdef ROTARY_TOUCH_UI
  cancelEncoder();
#else
  navigation.clear();
#endif
  auto* focused=lv_group_get_focused(group);bool keep=false;
  for(unsigned i=0;i<count;++i)if(found[i]==focused)keep=true;
  bool editing=keep&&lv_group_get_editing(group);
  lv_group_remove_all_objs(group);memberCount=count;
  for(unsigned i=0;i<count;++i){
    auto* obj=members[i]=found[i];
    lv_obj_add_flag(obj,LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    const auto focusStyle=
#ifdef ROTARY_TOUCH_UI
      LV_STATE_FOCUS_KEY;
#else
      LV_STATE_FOCUSED;
#endif
    lv_obj_set_style_outline_color(obj,lv_color_hex(uiColors().accent),focusStyle);
    lv_obj_set_style_outline_width(obj,3,focusStyle);
    lv_obj_set_style_outline_pad(obj,2,focusStyle);
    lv_group_add_obj(group,obj);
  }
  if(keep)lv_group_focus_obj(focused);
  lv_group_set_editing(group,editing);
}
#endif
