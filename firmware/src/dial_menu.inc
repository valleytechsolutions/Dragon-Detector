enum class DialMenu {Closed,Main,Tools,Settings,Themes,Time,Date,Brightness};
extern void set_brightness(uint8_t);
static DialMenu dialMenu=DialMenu::Closed;
static int dialChoice=0,dialFirstRow=0,dialBrightness=100,dialOldBrightness=100;
static bool dialEditing=false;
static dragon::Calendar dialEditDate;
static lv_obj_t *dialPanel=nullptr,*dialMenuTitle,*dialMenuNote,*dialMenuHint,*dialMenuRows[5],*dialMenuLabels[5],*dialMenuButton;
static void dialMenuRefresh();
static void dialMenuSelect();
static unsigned dialThemeAt(int choice){for(unsigned i=0;i<RadarThemeCount;++i)if(i!=dragon::PersonalThemeId&&choice--==0)return i;return 0;}
static const char* dialThemeLabel(unsigned theme){return theme==9?"CI / McNally":theme==26?"CI / LPL":radarThemeNames[theme];}
static int dialMenuCount(){switch(dialMenu){case DialMenu::Main:return 6;case DialMenu::Tools:return 4;case DialMenu::Settings:return 5;case DialMenu::Themes:return RadarThemeCount-1;case DialMenu::Time:case DialMenu::Date:return 5;default:return 1;}}
static bool dialMenuVisible(){return dialMenu!=DialMenu::Closed;}
static void dialMenuClose(){
  if(dialMenu==DialMenu::Brightness){dialBrightness=dialOldBrightness;set_brightness(dialBrightness);}
  dialMenu=DialMenu::Closed;dialEditing=false;lv_obj_add_flag(dialPanel,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(root);
}
static void dialMenuOpen(DialMenu menu){
  dialMenu=menu;dialChoice=0;dialEditing=false;
  if(menu==DialMenu::Time||menu==DialMenu::Date){clockLocal(dialEditDate);if(!wallTime())dialEditDate={2026,1,1,12,0,0};}
  if(menu==DialMenu::Brightness)dialOldBrightness=dialBrightness;
  if(menu==DialMenu::Themes)for(int i=0;i<dialMenuCount();++i)if(dialThemeAt(i)==radarTheme)dialChoice=i;
  lv_obj_clear_flag(dialPanel,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(dialPanel);dialMenuRefresh();
}
static void dialMenuCreate(){
  dialBrightness=prefsReady?constrain(litePrefs.getUChar("brightness",100),10,100):100;set_brightness(dialBrightness);
  dialMenuButton=lv_btn_create(root);lv_obj_set_size(dialMenuButton,96,25);lv_obj_set_pos(dialMenuButton,72,205);
  auto* caption=lv_label_create(dialMenuButton);lv_label_set_text(caption,"MENU");lv_obj_center(caption);lv_obj_set_style_text_font(caption,&lv_font_montserrat_12,0);
  lv_obj_add_event_cb(dialMenuButton,[](lv_event_t*){dialMenuOpen(DialMenu::Main);},LV_EVENT_SHORT_CLICKED,nullptr);
  dialPanel=lv_obj_create(root);lv_obj_remove_style_all(dialPanel);lv_obj_set_size(dialPanel,240,240);lv_obj_clear_flag(dialPanel,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(dialPanel,LV_OBJ_FLAG_CLICKABLE);
  auto label=[](lv_obj_t* parent,int y,int width,const lv_font_t* font){auto* o=lv_label_create(parent);lv_obj_set_size(o,width,LV_SIZE_CONTENT);lv_obj_set_pos(o,(240-width)/2,y);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_font(o,font,0);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);return o;};
  dialMenuTitle=label(dialPanel,20,142,&lv_font_montserrat_14);dialMenuNote=label(dialPanel,40,160,&lv_font_montserrat_10);dialMenuHint=label(dialPanel,214,124,&lv_font_montserrat_10);
  for(int i=0;i<5;++i){
    auto* row=dialMenuRows[i]=lv_btn_create(dialPanel);lv_obj_remove_style_all(row);lv_obj_set_pos(row,37,56+i*30);lv_obj_set_size(row,166,28);lv_obj_add_flag(row,LV_OBJ_FLAG_CLICKABLE);
    dialMenuLabels[i]=lv_label_create(row);lv_obj_set_size(dialMenuLabels[i],156,LV_SIZE_CONTENT);lv_label_set_long_mode(dialMenuLabels[i],LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(dialMenuLabels[i],LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_font(dialMenuLabels[i],&lv_font_montserrat_14,0);
    lv_obj_add_event_cb(row,[](lv_event_t* e){int row=int(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));if(dialEditing&&dialChoice!=dialFirstRow+row)dialEditing=false;dialChoice=dialFirstRow+row;dialMenuSelect();},LV_EVENT_SHORT_CLICKED,reinterpret_cast<void*>(intptr_t(i)));
  }
  lv_obj_add_flag(dialPanel,LV_OBJ_FLAG_HIDDEN);
}
static void dialMenuRefresh(){
  if(!dialMenuVisible())return;const auto& p=themeUiColors(radarTheme);char value[64];
  lv_obj_set_style_bg_color(dialPanel,color(p.bg),0);lv_obj_set_style_bg_opa(dialPanel,LV_OPA_COVER,0);
  const char* title=dialMenu==DialMenu::Main?"MENU":dialMenu==DialMenu::Tools?"TOOLS":dialMenu==DialMenu::Settings?"SETTINGS":dialMenu==DialMenu::Themes?"THEMES":dialMenu==DialMenu::Time?"SET TIME":dialMenu==DialMenu::Date?"SET DATE":"BRIGHTNESS";
  lv_label_set_text(dialMenuTitle,title);lv_obj_set_style_text_color(dialMenuTitle,color(p.ink),0);
  snprintf(value,sizeof(value),dialEditing?"Turn to adjust / press done":"%d / %d",dialChoice+1,dialMenuCount());lv_label_set_text(dialMenuNote,value);lv_obj_set_style_text_color(dialMenuNote,color(p.muted),0);
  lv_label_set_text(dialMenuHint,"HOLD TO GO BACK");lv_obj_set_style_text_color(dialMenuHint,color(p.muted),0);
  dialFirstRow=std::max(0,std::min(dialChoice-2,dialMenuCount()-5));
  static const char* mainItems[]={"Radar","Clock","Tools","Themes","Settings","Close"};
  static const char* toolItems[]={"Stopwatch","Signals / Hunt","Device status","Back"};
  static const char* settingItems[]={"Time","Date","Brightness","Sound","Back"};
  for(int row=0;row<5;++row){int item=dialFirstRow+row;auto* button=dialMenuRows[row];if(item>=dialMenuCount()){lv_obj_add_flag(button,LV_OBJ_FLAG_HIDDEN);continue;}lv_obj_clear_flag(button,LV_OBJ_FLAG_HIDDEN);
    const char* name="";
    if(dialMenu==DialMenu::Main)name=mainItems[item];else if(dialMenu==DialMenu::Tools)name=item==0&&liteLocksport()?"Locksport timer":toolItems[item];
    else if(dialMenu==DialMenu::Settings){name=settingItems[item];if(item==3){snprintf(value,sizeof(value),"Sound: %s",alertSound?"on":"muted");name=value;}}
    else if(dialMenu==DialMenu::Themes)name=dialThemeLabel(dialThemeAt(item));
    else if(dialMenu==DialMenu::Brightness){snprintf(value,sizeof(value),"%d%%  /  press to save",dialBrightness);name=value;}
    else if(item==3)name=dialMenu==DialMenu::Time?"Save time":"Save date";
    else if(item==4)name="Cancel";
    else{bool date=dialMenu==DialMenu::Date;const char* field=date?(item==0?"Year":item==1?"Month":"Day"):(item==0?"Hour":item==1?"Minute":"Second");int n=date?(item==0?dialEditDate.year:item==1?dialEditDate.month:dialEditDate.day):(item==0?dialEditDate.hour:item==1?dialEditDate.minute:dialEditDate.second);snprintf(value,sizeof(value),"%s: %02d%s",field,n,dialEditing&&item==dialChoice?"  <>":"");name=value;}
    lv_label_set_text(dialMenuLabels[row],name);lv_obj_center(dialMenuLabels[row]);lv_obj_set_style_text_color(dialMenuLabels[row],color(p.ink),0);
    lv_obj_set_style_bg_color(button,color(p.accent),0);lv_obj_set_style_bg_opa(button,item==dialChoice?LV_OPA_30:LV_OPA_TRANSP,0);lv_obj_set_style_border_color(button,color(p.accent),0);lv_obj_set_style_border_width(button,item==dialChoice?1:0,0);lv_obj_set_style_radius(button,6,0);
  }
}
static void dialMenuBack(){if(dialEditing){dialEditing=false;dialMenuRefresh();return;}if(dialMenu==DialMenu::Main){dialMenuClose();return;}if(dialMenu==DialMenu::Brightness){dialBrightness=dialOldBrightness;set_brightness(dialBrightness);}dialMenuOpen(dialMenu==DialMenu::Time||dialMenu==DialMenu::Date||dialMenu==DialMenu::Brightness?DialMenu::Settings:DialMenu::Main);}
static void dialMenuRotate(int step){
  step=step>0?1:-1;
  if(dialMenu==DialMenu::Brightness){dialBrightness=constrain(dialBrightness+step*5,10,100);set_brightness(dialBrightness);}
  else if(dialEditing){
    auto wrap=[&](int n,int low,int high){return n<low?high:n>high?low:n;};
    if(dialMenu==DialMenu::Time){if(dialChoice==0)dialEditDate.hour=wrap(dialEditDate.hour+step,0,23);if(dialChoice==1)dialEditDate.minute=wrap(dialEditDate.minute+step,0,59);if(dialChoice==2)dialEditDate.second=wrap(dialEditDate.second+step,0,59);}
    else {if(dialChoice==0)dialEditDate.year=wrap(dialEditDate.year+step,2000,2099);if(dialChoice==1)dialEditDate.month=wrap(dialEditDate.month+step,1,12);int days=dragon::monthDays(dialEditDate.year,dialEditDate.month);if(dialChoice==2)dialEditDate.day=wrap(dialEditDate.day+step,1,days);else dialEditDate.day=std::min(dialEditDate.day,days);}
  }else dialChoice=(dialChoice+dialMenuCount()+step)%dialMenuCount();dialMenuRefresh();
}
static void dialMenuSelect(){
  int item=dialChoice;
  if(dialMenu==DialMenu::Main){if(item==0||item==1){dialMenuClose();setLitePage(item==0?LitePage::Radar:LitePage::Clock);}else if(item==2)dialMenuOpen(DialMenu::Tools);else if(item==3)dialMenuOpen(DialMenu::Themes);else if(item==4)dialMenuOpen(DialMenu::Settings);else dialMenuClose();}
  else if(dialMenu==DialMenu::Tools){if(item==3)dialMenuBack();else{dialMenuClose();setLitePage(item==0?LitePage::Timer:item==1?LitePage::Signals:LitePage::Status);}}
  else if(dialMenu==DialMenu::Settings){if(item==0)dialMenuOpen(DialMenu::Time);else if(item==1)dialMenuOpen(DialMenu::Date);else if(item==2)dialMenuOpen(DialMenu::Brightness);else if(item==3){alertSound=!alertSound;audioSave();if(!alertSound)audioStop();dialMenuRefresh();}else dialMenuBack();}
  else if(dialMenu==DialMenu::Themes){unsigned theme=dialThemeAt(item);dialMenuClose();radarTheme=theme;refreshLiteTheme();}
  else if(dialMenu==DialMenu::Brightness){if(prefsReady)litePrefs.putUChar("brightness",dialBrightness);dialOldBrightness=dialBrightness;dialMenuOpen(DialMenu::Settings);}
  else if(item<3){dialEditing=!dialEditing;dialMenuRefresh();}
  else if(item==4)dialMenuBack();
  else {
    dragon::Calendar current;clockLocal(current);if(!wallTime())current=dialEditDate;
    if(dialMenu==DialMenu::Time){current.hour=dialEditDate.hour;current.minute=dialEditDate.minute;current.second=dialEditDate.second;}else{current.year=dialEditDate.year;current.month=dialEditDate.month;current.day=dialEditDate.day;}
    uint32_t local=dragon::dateEpoch(current);int64_t utc=int64_t(local)-clockOffsetMinutes*60;
    if(!local||utc<=0||!configureClock(uint32_t(utc))){lv_label_set_text(dialMenuNote,local?"RTC save failed":"Check the calendar date");return;}
    dialMenuOpen(DialMenu::Settings);lv_label_set_text(dialMenuNote,"Saved to hardware clock");
  }
}
