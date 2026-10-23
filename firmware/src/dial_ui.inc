// Dial's compact controls share the same contact identities and tap gestures as the full UI.
static dragon::RadarPress dialPress;
static bool dialBlankPress=false;
static dragon::DeviceKey dialSignalKey;
static bool dialHasSignal=false;
static lv_obj_t *dialTargetPanel,*dialTargetName,*dialTargetHuntLabel;
static int dialTargetChoice=0;
#include "dial_menu.inc"
static void dialPick(int index){if(index<0)return;dialSignalKey=contactKey(contacts[index]);dialHasSignal=true;}
static void dialNextSignal(){
  int previous=dialHasSignal?findContact(dialSignalKey):-1;
  for(size_t n=1;n<=MAX_CONTACTS;++n){int i=(previous+int(n))%MAX_CONTACTS;if(radarVisible(contacts[i],millis())){dialPick(i);return;}}
}
static void dialClearSelection(){radarHasSelection=false;dialPress.clear();dialBlankPress=false;lv_obj_add_flag(dialTargetPanel,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(radar);}
static void dialPageChanged(){if(dialTargetPanel&&litePage!=LitePage::Radar)dialClearSelection();}
static void dialOpen(const dragon::DeviceKey& key){dialSignalKey=key;dialHasSignal=true;dialClearSelection();setLitePage(LitePage::Signals);}
static void dialHunt(){
  int i=dialHasSignal?findContact(dialSignalKey):-1;
  if(i>=0){if(trackedContact==i)trackContact(-1);else if(uint32_t(millis()-contacts[i].seen)<30000)trackContact(i);}
}
static void dialRadarDown(int x,int y){
  dialPress.clear();int nearest=100000,index=-1;
  for(int i=0;i<dotCount;++i)if(dragon::radarHit(x,y,dots[i].x,dots[i].y,17,nearest))index=i;
  dialBlankPress=index<0;if(index>=0)dialPress.begin(dots[index].key);
}
static void dialRadarRelease(){
  if(dialBlankPress){dialClearSelection();return;}
  dragon::DeviceKey key;if(!dialPress.take(key)||findContact(key)<0)return;
  if(radarHasSelection&&radarSelectedKey==key){dialClearSelection();return;}
  radarSelectedKey=key;radarHasSelection=true;radarSelectedAt=millis();dialTargetChoice=0;lv_obj_invalidate(radar);
}
static void dialRadarEvent(lv_event_t* e){
  auto event=lv_event_get_code(e);
  if(event==LV_EVENT_PRESSED){lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);dialRadarDown(p.x,p.y);}
  else if(event==LV_EVENT_SHORT_CLICKED)dialRadarRelease();
  else if(event==LV_EVENT_PRESS_LOST||event==LV_EVENT_LONG_PRESSED){dialPress.clear();dialBlankPress=false;}
}
static void dialTargetCreate(){
  dialTargetPanel=lv_obj_create(root);lv_obj_remove_style_all(dialTargetPanel);lv_obj_set_pos(dialTargetPanel,32,141);lv_obj_set_size(dialTargetPanel,176,60);lv_obj_clear_flag(dialTargetPanel,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(dialTargetPanel,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(dialTargetPanel,LV_OPA_COVER,0);lv_obj_set_style_border_width(dialTargetPanel,1,0);lv_obj_set_style_radius(dialTargetPanel,7,0);
  dialTargetName=lv_label_create(dialTargetPanel);lv_obj_set_pos(dialTargetName,6,7);lv_obj_set_width(dialTargetName,164);lv_label_set_long_mode(dialTargetName,LV_LABEL_LONG_DOT);lv_obj_set_style_text_align(dialTargetName,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_font(dialTargetName,&lv_font_montserrat_10,0);
  const char* names[]={"Details","Hunt","X"};const int positions[]={6,75,140},widths[]={65,61,30};
  for(int i=0;i<3;++i){auto* b=lv_btn_create(dialTargetPanel);lv_obj_set_pos(b,positions[i],28);lv_obj_set_size(b,widths[i],26);lv_obj_set_style_radius(b,4,0);auto* label=lv_label_create(b);lv_label_set_text(label,names[i]);lv_obj_set_style_text_font(label,&lv_font_montserrat_12,0);lv_obj_center(label);if(i==1)dialTargetHuntLabel=label;
    lv_obj_add_event_cb(b,[](lv_event_t* e){int action=int(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));if(action==2){dialClearSelection();return;}int index=radarHasSelection?findContact(radarSelectedKey):-1;if(index<0){dialClearSelection();return;}if(action==0)dialOpen(radarSelectedKey);else{dialPick(index);dialHunt();}},LV_EVENT_SHORT_CLICKED,reinterpret_cast<void*>(intptr_t(i)));
  }
  lv_obj_add_flag(dialTargetPanel,LV_OBJ_FLAG_HIDDEN);
}
static void dialTargetUpdate(){
  int index=radarHasSelection?findContact(radarSelectedKey):-1;
  if(litePage!=LitePage::Radar||index<0){if(index<0)radarHasSelection=false;lv_obj_add_flag(dialTargetPanel,LV_OBJ_FLAG_HIDDEN);return;}
  const auto& p=themeUiColors(radarTheme);const auto& c=contacts[index];char value[80];snprintf(value,sizeof(value),"%s / %d dBm",radarFamilyName(c.match.family),c.rssi);lv_label_set_text(dialTargetName,value);
  lv_obj_set_style_bg_color(dialTargetPanel,color(p.bg),0);lv_obj_set_style_border_color(dialTargetPanel,color(p.accent),0);lv_obj_set_style_text_color(dialTargetPanel,color(p.ink),0);
  for(uint32_t i=1;i<lv_obj_get_child_cnt(dialTargetPanel);++i){auto* b=lv_obj_get_child(dialTargetPanel,i);lv_obj_set_style_bg_color(b,color(p.accent),0);lv_obj_set_style_bg_opa(b,int(i)-1==dialTargetChoice?LV_OPA_40:LV_OPA_20,0);lv_obj_set_style_border_color(b,color(p.accent),0);lv_obj_set_style_border_width(b,int(i)-1==dialTargetChoice?1:0,0);lv_obj_set_style_text_color(b,color(p.ink),0);lv_obj_set_style_shadow_width(b,0,0);}
  const bool canHunt=trackedContact==index||uint32_t(millis()-c.seen)<30000;
  auto* huntButton=lv_obj_get_parent(dialTargetHuntLabel);if(canHunt)lv_obj_clear_state(huntButton,LV_STATE_DISABLED);else lv_obj_add_state(huntButton,LV_STATE_DISABLED);
  lv_label_set_text(dialTargetHuntLabel,trackedContact==index?"Stop":canHunt?"Hunt":"Stale");lv_obj_center(dialTargetHuntLabel);
  lv_obj_clear_flag(dialTargetPanel,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(dialTargetPanel);lv_obj_add_flag(pageLabel,LV_OBJ_FLAG_HIDDEN);
}
static void dialUiBegin(){
  dialMenuCreate();
  dialTargetCreate();
  lv_obj_set_y(pageLabel,187);
  lv_obj_set_pos(notice,37,55);lv_obj_set_size(notice,166,32);lv_label_set_long_mode(notice,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(notice,&lv_font_montserrat_10,0);lv_obj_set_style_pad_all(notice,4,0);lv_obj_set_style_bg_opa(notice,LV_OPA_COVER,0);lv_obj_set_style_border_width(notice,1,0);lv_obj_set_style_radius(notice,5,0);lv_obj_add_flag(notice,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(notice,[](lv_event_t*){noticeAt=0;lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);},LV_EVENT_SHORT_CLICKED,nullptr);
  lv_obj_add_flag(pageValue,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pageValue,[](lv_event_t*){if(litePage==LitePage::Timer)stopwatch.toggle(stopwatchNow());else if(litePage==LitePage::Signals)dialHunt();},LV_EVENT_SHORT_CLICKED,nullptr);
  lv_obj_add_flag(radar,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(radar,dialRadarEvent,LV_EVENT_ALL,nullptr);
  lv_obj_add_flag(pageLabel,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pageLabel,[](lv_event_t*){if(radarHasSelection)dialOpen(radarSelectedKey);},LV_EVENT_SHORT_CLICKED,nullptr);
  lv_obj_add_flag(pageCanvas,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pageCanvas,[](lv_event_t*){if(litePage==LitePage::Signals)dialHunt();},LV_EVENT_SHORT_CLICKED,nullptr);
}
void dialRotate(int step){if(!root)return;if(dialMenuVisible()){dialMenuRotate(step);return;}if(litePage==LitePage::Radar&&radarHasSelection){dialTargetChoice=(dialTargetChoice+3+(step>0?1:-1))%3;dialTargetUpdate();return;}dialClearSelection();setLitePage(LitePage((int(litePage)+5+(step>0?1:-1))%5));}
static void dialButton(dragon::LitePress event){
  if(dialMenuVisible()){if(event==dragon::LitePress::Hold)dialMenuBack();else dialMenuSelect();return;}
  if(event==dragon::LitePress::Hold){dialClearSelection();setLitePage(LitePage::Radar);trackContact(-1);audioStop();return;}
  if(litePage==LitePage::Radar&&radarHasSelection){auto* action=lv_obj_get_child(dialTargetPanel,dialTargetChoice+1);if(!lv_obj_has_state(action,LV_STATE_DISABLED))lv_event_send(action,LV_EVENT_SHORT_CLICKED,nullptr);return;}
  if(litePage==LitePage::Timer){
    if(event==dragon::LitePress::Tap)stopwatch.toggle(stopwatchNow());
    else if(event==dragon::LitePress::Double){if(stopwatch.running){if(liteLocksport())stopwatch.finish(stopwatchNow());else stopwatch.lap(stopwatchNow());}else stopwatch.reset();}
  }else if(litePage==LitePage::Signals){if(event==dragon::LitePress::Double)dialHunt();else dialNextSignal();}
  else dialMenuOpen(DialMenu::Main);
}
static void dialSignalUpdate(){
  if(!dialHasSignal)dialNextSignal();int i=dialHasSignal?findContact(dialSignalKey):-1;
  char value[180];lv_label_set_text(pageTitle,"SIGNAL DETAILS");
  if(i<0){lv_label_set_text(pageValue,dialHasSignal?"SIGNAL EXPIRED":"LISTENING");lv_label_set_text(pageSub,"Press for next signal");}
  else{
    const auto& c=contacts[i];char mac[18];macText(c.mac,mac);
    snprintf(value,sizeof(value),"%.24s\n%d dBm / %s\n%s",*c.name?c.name:radarFamilyName(c.match.family),c.rssi,c.ble?"BLE":"WI-FI",mac);lv_label_set_text(pageValue,value);
    snprintf(value,sizeof(value),"%s / %lus ago\n%s",radarFamilyName(c.match.family),uint32_t(millis()-c.seen)/1000,trackedContact==i?"HUNT ON / tap to stop":"Tap to hunt / RSSI only");lv_label_set_text(pageSub,value);
  }
  lv_label_set_text(pageHint,"PRESS NEXT / 2x HUNT");
}
static void dialUiUpdate(){
  if(liteStartup)return;
  const auto& colors=themeUiColors(radarTheme);
  lv_obj_set_style_bg_color(notice,color(colors.bg),0);lv_obj_set_style_border_color(notice,color(colors.accent),0);lv_obj_set_style_text_color(notice,color(colors.ink),0);
  lv_obj_set_style_bg_color(dialMenuButton,color(colors.bg),0);lv_obj_set_style_bg_opa(dialMenuButton,LV_OPA_COVER,0);
  lv_obj_set_style_text_color(dialMenuButton,color(colors.ink),0);lv_obj_set_style_border_color(dialMenuButton,color(colors.accent),0);lv_obj_set_style_border_width(dialMenuButton,1,0);
  lv_obj_move_foreground(dialMenuButton);lv_obj_add_flag(pageHint,LV_OBJ_FLAG_HIDDEN);
  dialTargetUpdate();if(!lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN))lv_obj_move_foreground(notice);
  if(litePage==LitePage::Timer){lv_obj_set_y(pageHint,174);lv_obj_set_width(pageHint,180);lv_obj_set_x(pageHint,30);lv_obj_set_style_text_font(pageHint,&lv_font_montserrat_10,0);lv_obj_clear_flag(pageHint,LV_OBJ_FLAG_HIDDEN);}
  if(litePage==LitePage::Radar){
    char value[96];snprintf(value,sizeof(value),"%s RX   %02d SIGNALS",blePhase?"BLE":"WI-FI",radarCount);lv_label_set_text(status,value);
    int i=radarHasSelection?findContact(radarSelectedKey):-1;if(i<0)lv_obj_clear_flag(pageLabel,LV_OBJ_FLAG_HIDDEN);
    if(i>=0){snprintf(value,sizeof(value),"%s / %d dBm",radarFamilyName(contacts[i].match.family),contacts[i].rssi);lv_label_set_text(pageLabel,value);}
    lv_label_set_text(pageHint,i>=0?"TAP SUMMARY FOR DETAILS":"TAP DOT / TURN FOR PAGES");
  }else if(litePage==LitePage::Clock)lv_label_set_text(pageHint,"PRESS MENU / TURN PAGE");
  else if(litePage==LitePage::Status){
    char value[160];snprintf(value,sizeof(value),"WI-FI %lu / BLE %lu\nDROPS %lu\nHEAP %u KB\nSOUND %s",wifiFrames,bleFrames,queueDrops,ESP.getFreeHeap()/1024,alertSound?"ON":"MUTED");lv_label_set_text(pageValue,value);
    lv_label_set_text(pageHint,"PRESS MENU / HOLD RADAR");
  }
}
extern void dialInputStatus();
static bool dialUiCommand(const String& c){
  if(c=="screen menu"||c=="screen tools"||c=="screen settings"){dialMenuOpen(c=="screen menu"?DialMenu::Main:c=="screen tools"?DialMenu::Tools:DialMenu::Settings);Serial.println("DIAL_OK");return true;}
  if(c=="dial menu status"){Serial.printf("DIAL_MENU page=%u selection=%d editing=%u brightness=%d\n",unsigned(dialMenu),dialChoice,dialEditing,dialBrightness);return true;}
  if(c=="display status"){Serial.printf("DISPLAY_SETTINGS brightness=%d output=%d dim=0 off=0 dim_level=0 wake=0\n",dialBrightness,dialBrightness);return true;}
  if(c=="touch status"||c=="rotary status"){dialInputStatus();return true;}
  if(c=="dial hunt stop"){if(dialMenuVisible())dialMenuClose();setLitePage(LitePage::Radar);trackContact(-1);audioStop();Serial.println("DIAL_OK");return true;}
  if(c.startsWith("radar theme ")){
    String name=c.substring(12);for(unsigned i=0;i<RadarThemeCount;++i)if(i!=dragon::PersonalThemeId&&name==radarThemeCommands[i]){radarTheme=i;refreshLiteTheme();dialMenuRefresh();Serial.println("RADAR_OK");return true;}
    Serial.println("RADAR_ERROR unknown or unavailable theme");return true;
  }
  if(c=="dial status"){Serial.printf("DIAL_STATUS beta=0 contacts=%u rtc=%u touch=FT3267 encoder=1 psram=0 minimum_heap=%u largest_block=%u\n",unsigned(MAX_CONTACTS),rtcAvailable,ESP.getMinFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));return true;}
  return false;
}
