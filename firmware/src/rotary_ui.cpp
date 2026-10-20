#ifdef BOARD_CROWPANEL_21
#include "app.h"
#include "rotary_ui.h"
#include "rotary_input.h"
#include "ui_theme.h"

namespace {
lv_group_t* group=nullptr;
lv_obj_t* members[160]{};
unsigned memberCount=0;
dragon::RotaryNavigation navigation;
unsigned pendingClicks=0;
bool releasing=false;

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
  if(releasing){releasing=false;data->state=LV_INDEV_STATE_REL;}
  else if(pendingClicks){--pendingClicks;releasing=true;data->state=LV_INDEV_STATE_PR;}
  else {data->enc_diff=navigation.take(millis());data->state=LV_INDEV_STATE_REL;}
}
}
void rotaryUiBegin(){
  group=lv_group_create();lv_group_set_wrap(group,true);
  static lv_indev_drv_t driver;lv_indev_drv_init(&driver);
  driver.type=LV_INDEV_TYPE_ENCODER;driver.read_cb=readEncoder;
  lv_indev_set_group(lv_indev_drv_register(&driver),group);
}
void rotaryUiInput(int steps,bool click,bool back){
  if(!group||(!steps&&!click&&!back))return;
  if(uiRotaryWake()){navigation.clear();return;}
  if(back){navigation.clear();pendingClicks=0;releasing=false;lv_group_set_editing(group,false);uiKey(true);return;}
  if(click){navigation.clear();pendingClicks=1;}
  else if(!pendingClicks&&!releasing)navigation.push(steps,millis());
}
void rotaryUiLoop(){
  if(!group)return;
  auto* scope=uiRotaryScope();
  if(auto* box=dialog(lv_layer_top()))scope=box;
  else if(auto* box=dialog(scope))scope=box;
  lv_obj_t* found[160]{};unsigned count=0;collect(scope,found,count);
  bool changed=count!=memberCount||lv_group_get_obj_count(group)!=count;
  for(unsigned i=0;i<count&&!changed;++i)changed=found[i]!=members[i];
  if(!changed)return;
  navigation.clear();
  auto* focused=lv_group_get_focused(group);bool keep=false;
  for(unsigned i=0;i<count;++i)if(found[i]==focused)keep=true;
  bool editing=keep&&lv_group_get_editing(group);
  lv_group_remove_all_objs(group);memberCount=count;
  for(unsigned i=0;i<count;++i){
    auto* obj=members[i]=found[i];
    lv_obj_add_flag(obj,LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_outline_color(obj,lv_color_hex(uiColors().accent),LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(obj,3,LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(obj,2,LV_STATE_FOCUSED);
    lv_group_add_obj(group,obj);
  }
  if(keep)lv_group_focus_obj(focused);
  lv_group_set_editing(group,editing);
}
#endif
