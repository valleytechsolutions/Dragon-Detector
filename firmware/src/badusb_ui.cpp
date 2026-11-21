#include "app.h"
#include "badusb.h"
#include "badusb_ui.h"
#ifndef BOARD_RADAR_LITE
#include "ui_theme.h"
#include <lvgl.h>

namespace {
constexpr int scale(int n){return (n*SCREEN_RES+180)/360;}
lv_obj_t *panel=nullptr,*scriptLabel=nullptr,*previewLabel=nullptr,*stateLabel=nullptr,*usbLabel=nullptr;
lv_obj_t *runButton=nullptr,*stopButton=nullptr,*previousButton=nullptr,*nextButton=nullptr,*refreshButton=nullptr,*backButton=nullptr;
unsigned choice=0;
char shownName[33]{};
uint32_t updatedAt=0;
#if SCREEN_RES<=240
const lv_font_t* smallFont(){return uiFont(&lv_font_montserrat_8);}
const lv_font_t* bodyFont(){return uiFont(&lv_font_montserrat_10);}
#elif SCREEN_RES>=400
const lv_font_t* smallFont(){return uiFont(&lv_font_montserrat_16);}
const lv_font_t* bodyFont(){return uiFont(&lv_font_montserrat_18);}
#else
const lv_font_t* smallFont(){return uiFont(&lv_font_montserrat_12);}
const lv_font_t* bodyFont(){return uiFont(&lv_font_montserrat_14);}
#endif
lv_obj_t* text(lv_obj_t* parent,const char* value,int x,int y,int width,const lv_font_t* font){
  auto* label=lv_label_create(parent);lv_label_set_text(label,value);lv_obj_set_pos(label,scale(x),scale(y));lv_obj_set_width(label,scale(width));
  lv_obj_set_style_text_font(label,font,0);lv_obj_set_style_text_color(label,lv_color_hex(uiColors().ink),0);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);return label;
}
void enabled(lv_obj_t* button,bool yes){if(yes)lv_obj_clear_state(button,LV_STATE_DISABLED);else lv_obj_add_state(button,LV_STATE_DISABLED);}
void showSelection(){
  auto state=badusbStatus();char caption[64];snprintf(caption,sizeof(caption),"%u/%u  %.32s",badusbCount()?choice+1:0,badusbCount(),state.selected?state.name:"No script");
  strlcpy(shownName,state.selected?state.name:"",sizeof(shownName));
  lv_label_set_text(scriptLabel,caption);
  char preview[257];strlcpy(preview,state.selected?badusbPreview():"Select a script to preview it.",sizeof(preview));
  if(state.selected&&strlen(badusbPreview())>=sizeof(preview))strcpy(preview+sizeof(preview)-4,"...");
  lv_label_set_text(previewLabel,preview);lv_obj_scroll_to_y(lv_obj_get_parent(previewLabel),0,LV_ANIM_OFF);updatedAt=0;
}
void select(int step){
  if(badusbStatus().running||!badusbCount())return;
  choice=(choice+badusbCount()+step)%badusbCount();badusbSelect(choice);showSelection();
}
void action(lv_event_t* event){
  int id=int(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  // Keyboard mode only lives on this page: BACK (shown as EXIT) restarts as USB serial.
  if(id==0){if(badusbKeyboardMode()){badusbExitKeyboard();return;}badusbStop();uiShow("utilities");return;}
  if(id==1)select(-1);else if(id==2)select(1);else if(id==3)badusbRun();else if(id==4)badusbStop();
  else if(id==5&&!badusbStatus().running){badusbRefresh();choice=0;if(badusbCount())badusbSelect(0);showSelection();}
  updatedAt=0;badusbViewLoop();
}
lv_obj_t* button(const char* caption,int x,int y,int width,int height,int id){
  auto* b=lv_btn_create(panel);lv_obj_set_pos(b,scale(x),scale(y));lv_obj_set_size(b,scale(width),scale(height));
  lv_obj_set_style_radius(b,scale(8),0);lv_obj_set_style_pad_all(b,0,0);lv_obj_set_style_bg_color(b,lv_color_hex(uiColors().surface),0);
  lv_obj_set_style_border_color(b,lv_color_hex(uiColors().border),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_shadow_width(b,0,0);
  auto* label=text(b,caption,0,0,width-4,bodyFont());lv_obj_center(label);
  lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(id)));return b;
}
}

void badusbView(void* parent){
  badusbRefresh();choice=0;auto current=badusbStatus();
  if(current.selected){for(unsigned i=0;i<badusbCount();++i)if(!strcmp(current.name,badusbName(i))){choice=i;break;}}
  else if(badusbCount())badusbSelect(0);
  panel=lv_obj_create(static_cast<lv_obj_t*>(parent));lv_obj_remove_style_all(panel);lv_obj_set_size(panel,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
  text(panel,"BADUSB",80,27,200,bodyFont());
  usbLabel=text(panel,"",55,57,250,smallFont());
  previousButton=button("<",47,87,38,34,1);nextButton=button(">",275,87,38,34,2);
  scriptLabel=text(panel,"",89,94,182,smallFont());lv_label_set_long_mode(scriptLabel,LV_LABEL_LONG_DOT);
  auto* preview=lv_obj_create(panel);lv_obj_set_pos(preview,scale(52),scale(130));lv_obj_set_size(preview,scale(256),scale(85));
  lv_obj_set_style_pad_all(preview,scale(6),0);lv_obj_set_style_bg_color(preview,lv_color_hex(uiColors().surface),0);lv_obj_set_style_border_color(preview,lv_color_hex(uiColors().border),0);
  lv_obj_set_scroll_dir(preview,LV_DIR_VER);previewLabel=text(preview,"",0,0,240,smallFont());lv_obj_set_style_text_align(previewLabel,LV_TEXT_ALIGN_LEFT,0);
  stateLabel=text(panel,"",52,222,256,smallFont());lv_obj_set_height(stateLabel,scale(43));lv_label_set_long_mode(stateLabel,LV_LABEL_LONG_DOT);
  runButton=button("RUN",68,270,108,36,3);stopButton=button("STOP",184,270,108,36,4);
  backButton=button("BACK",80,313,94,28,0);refreshButton=button("REFRESH",182,313,98,28,5);
  showSelection();badusbViewLoop();
}
void badusbViewLoop(){
  if(!panel||(updatedAt&&uint32_t(millis()-updatedAt)<150))return;updatedAt=millis();auto state=badusbStatus();char line[160];
  if(strcmp(shownName,state.selected?state.name:"")){for(unsigned i=0;i<badusbCount();++i)if(!strcmp(state.name,badusbName(i))){choice=i;break;}showSelection();}
  if(state.keyboardMode)snprintf(line,sizeof(line),"Keyboard mode / USB %s",state.connected?"connected":"waiting for host");
  else snprintf(line,sizeof(line),"USB serial / KEYBOARD restarts as keyboard");
  lv_label_set_text(usbLabel,line);
  lv_label_set_text(lv_obj_get_child(runButton,0),state.keyboardMode?"RUN":"KEYBOARD");lv_label_set_text(lv_obj_get_child(backButton,0),state.keyboardMode?"EXIT":"BACK");
  if(state.running)snprintf(line,sizeof(line),"RUNNING / line %u\n%s",state.line,state.message);
  else snprintf(line,sizeof(line),"%s",state.message);
  lv_label_set_text(stateLabel,line);enabled(runButton,state.selected&&!state.running&&(state.connected||!state.keyboardMode));enabled(stopButton,state.running);
  enabled(previousButton,!state.running&&badusbCount()>1);enabled(nextButton,!state.running&&badusbCount()>1);enabled(refreshButton,!state.running);
}
void badusbLeave(){badusbStop();panel=scriptLabel=previewLabel=stateLabel=usbLabel=nullptr;runButton=stopButton=previousButton=nextButton=refreshButton=backButton=nullptr;}
#else
void badusbView(void*){}
void badusbViewLoop(){}
void badusbLeave(){badusbStop();}
#endif
