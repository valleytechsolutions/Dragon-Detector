// Three simple pages share one draft; changing tabs never writes the clock.
static lv_obj_t* dateWheels[6]{};
static dragon::ClockEdit clockEditor;
static bool clockDateTab=false,clockPromptShown=false;
static bool onClockSettings(){return page==ClockWheels||page==ClockSettings||page==ClockEdit;}
static void clockTab(unsigned tab,bool fresh=false){
  if(fresh||!onClockSettings()){
    dragon::Calendar now;clockLocal(now);clockEditor.begin(now,wallTime()!=0);
  }
  clockDateTab=tab==1;go(tab==2?ClockSettings:ClockWheels);page=Boot;
}
static void clockTabs(unsigned active){
  const char* names[]={"Time","Date","Sync"};const int ids[]={160,162,163};
  for(int i=0;i<3;++i){
    auto* tab=button(root,names[i],58+i*82,92,80,32,ids[i]);
    if(unsigned(i)==active){lv_obj_set_style_bg_color(tab,color(uiColors().accent),0);lv_obj_set_style_text_color(lv_obj_get_child(tab,0),color(uiColors().onAccent),0);}
  }
}
static void wheelOptions(lv_obj_t* wheel,int first,int last,int selected){
  String options;for(int i=first;i<=last;++i){if(i!=first)options+='\n';if(i<10)options+='0';options+=i;}
  lv_roller_set_options(wheel,options.c_str(),LV_ROLLER_MODE_NORMAL);lv_roller_set_selected(wheel,selected-first,LV_ANIM_OFF);
}
static void wheelChanged(lv_event_t* e){
  auto& d=clockEditor.draft;int field=(intptr_t)lv_event_get_user_data(e),value=lv_roller_get_selected(lv_event_get_target(e));
  if(field==0)d.year=2000+value;else if(field==1)d.month=1+value;else if(field==2)d.day=1+value;
  else if(field==3)d.hour=dragon::hour24(value+1,d.hour>=12);else if(field==4)d.minute=value;
  else d.hour=dragon::hour24(dragon::hour12(d.hour),value==1);
  if(field<2){clockEditor.clampDay();wheelOptions(dateWheels[2],1,dragon::monthDays(d.year,d.month),d.day);}
}
static void buildClockWheels(){
  auto& d=clockEditor.draft;clockEditor.clampDay();dismissNotice();
  title(clockDateTab?"SET DATE":"SET TIME",wallTime()?"Scroll to choose, then Save":"Confirm both time and date to start");
  clockTabs(clockDateTab?1:0);
  const int fields[]={clockDateTab?1:3,clockDateTab?2:4,clockDateTab?0:5};
  const int first[]={2000,1,1,1,0,0},last[]={2099,12,dragon::monthDays(d.year,d.month),12,59,1};
  const int current[]={d.year,d.month,d.day,dragon::hour12(d.hour),d.minute,d.hour>=12};
  const char* names[]={"YEAR","MONTH","DAY","HOUR","MINUTE","AM / PM"};
  for(int col=0;col<3;++col){int field=fields[col],x=64+col*80;
    centered(label(root,names[field],x,140,72,F12,uiColors().accent));
    auto* r=dateWheels[field]=lv_roller_create(root);lv_obj_set_pos(r,S(x),S(160));lv_obj_set_width(r,S(72));
    lv_obj_set_style_text_font(r,uiFont(F20),0);lv_obj_set_style_text_line_space(r,S(5),0);
    lv_obj_set_style_bg_color(r,color(uiColors().bg),0);lv_obj_set_style_text_color(r,color(uiColors().muted),0);
    lv_obj_set_style_bg_color(r,color(uiColors().raised),LV_PART_SELECTED);lv_obj_set_style_text_color(r,color(uiColors().ink),LV_PART_SELECTED);
    lv_obj_set_style_border_width(r,0,0);lv_roller_set_visible_row_count(r,3);
    if(field==5){lv_roller_set_options(r,"AM\nPM",LV_ROLLER_MODE_NORMAL);lv_roller_set_selected(r,current[field],LV_ANIM_OFF);}
    else if(field==1){lv_roller_set_options(r,"Jan\nFeb\nMar\nApr\nMay\nJun\nJul\nAug\nSep\nOct\nNov\nDec",LV_ROLLER_MODE_NORMAL);lv_roller_set_selected(r,d.month-1,LV_ANIM_OFF);}
    else wheelOptions(r,first[field],last[field],current[field]);
    lv_obj_add_event_cb(r,wheelChanged,LV_EVENT_VALUE_CHANGED,(void*)(intptr_t)field);
  }
  button(root,"Cancel",65,274,100,32,164);
  button(root,wallTime()||(clockDateTab?clockEditor.timeConfirmed:clockEditor.dateConfirmed)?"Save":clockDateTab?"Next: time":"Next: date",185,274,110,32,141);
}
static void saveClockTab(){
  bool valid=wallTime()!=0;
  if(!valid&&!clockEditor.confirm(clockDateTab)){clockTab(clockDateTab?0:1);return;}
  dragon::Calendar now;clockLocal(now);auto d=clockEditor.merge(now,valid,clockDateTab);
  int64_t epoch=int64_t(dragon::dateEpoch(d))-clockOffsetMinutes*60;
  if(dragon::validDate(d)&&dragon::validEpoch(epoch)&&configureClock(epoch)){go(Clock);return;}
  auto* box=uiMessageBox(root,"COULD NOT SAVE","Check the date and the clock connection, then try again.",nullptr,true);lv_obj_set_width(box,S(260));lv_obj_center(box);
}
static void updateClockStatus(){
  if(!clockValue||!clockDate)return;dragon::Calendar d;clockLocal(d);char text[48];
  if(wallTime())snprintf(text,sizeof(text),"%02d:%02d:%02d %s",dragon::hour12(d.hour),d.minute,d.second,d.hour>=12?"PM":"AM");else strcpy(text,"Time not set");lv_label_set_text(clockValue,text);
  if(wallTime())snprintf(text,sizeof(text),"%04d-%02d-%02d",d.year,d.month,d.day);else strcpy(text,"Set time and date, or sync by USB");lv_label_set_text(clockDate,text);
}
static void buildClockSync(){
  dismissNotice();title("CLOCK SYNC","Use your computer's current local time");clockTabs(2);
  clockValue=label(root,"",55,138,250,F20);centered(clockValue);
  clockDate=label(root,"",55,168,250,F12,uiColors().accent);centered(clockDate);updateClockStatus();
  centered(label(root,"Open the web flasher by USB.\nClick Connect detector to sync.\nKeep it connected to stay in sync.",60,200,240,F12));
  button(root,"Time zone",65,272,110,32,161);button(root,"Back",195,272,100,32,164);
}
static void updateClockZone(){
  char text[32];snprintf(text,sizeof(text),"UTC %c%02d:%02d",editOffset<0?'-':'+',abs(editOffset)/60,abs(editOffset)%60);lv_label_set_text(editValue,text);
}
static void buildClockZone(){
  dismissNotice();title("TIME ZONE","15-minute steps / hold to repeat");
  editValue=label(root,"",58,135,244,F20);centered(editValue);updateClockZone();
  auto* minus=button(root,"-",80,180,80,45,137);auto* plus=button(root,"+",200,180,80,45,138);
  lv_obj_add_event_cb(minus,action,LV_EVENT_LONG_PRESSED_REPEAT,(void*)137);lv_obj_add_event_cb(plus,action,LV_EVENT_LONG_PRESSED_REPEAT,(void*)138);
  centered(label(root,"Changes the displayed local time.\nUSB sync uses your computer's zone.",60,236,240,F12,uiColors().muted));
  button(root,"Cancel",65,274,100,32,163);button(root,"Save",195,274,100,32,171);
}
static void clockSetupPrompt(){
  if(clockPrompt&&wallTime()){lv_msgbox_close(clockPrompt);clockPrompt=nullptr;}
  if(clockPromptShown||wallTime()||millis()<10000||page!=Radar)return;
  clockPromptShown=true;dismissNotice();static const char* choices[]={"Later","Set now",""};
  clockPrompt=uiMessageBox(root,"SET YOUR CLOCK","Time needs setup after power loss. Set time and date here, or sync with the web flasher by USB.",choices,true);
  lv_obj_set_width(clockPrompt,S(260));lv_obj_center(clockPrompt);
  lv_obj_add_event_cb(clockPrompt,[](lv_event_t* e){auto* box=lv_event_get_current_target(e);bool setup=lv_msgbox_get_active_btn(box)==1;lv_msgbox_close(box);clockPrompt=nullptr;if(setup)clockTab(0,true);},LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_add_event_cb(clockPrompt,[](lv_event_t*){clockPrompt=nullptr;},LV_EVENT_DELETE,nullptr);
}
