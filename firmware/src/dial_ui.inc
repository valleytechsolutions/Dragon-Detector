// Dial's compact controls share the same contact identities and tap gestures as the full UI.
static dragon::RadarPress dialPress;
static dragon::RadarTapGesture dialTaps;
static dragon::DeviceKey dialSignalKey;
static bool dialHasSignal=false;
static void dialPick(int index){if(index<0)return;dialSignalKey=contactKey(contacts[index]);dialHasSignal=true;}
static void dialNextSignal(){
  int previous=dialHasSignal?findContact(dialSignalKey):-1;
  for(size_t n=1;n<=MAX_CONTACTS;++n){int i=(previous+int(n))%MAX_CONTACTS;if(radarVisible(contacts[i],millis())){dialPick(i);return;}}
}
static void dialOpen(const dragon::DeviceKey& key){dialSignalKey=key;dialHasSignal=true;setLitePage(LitePage::Signals);}
static void dialHunt(){
  int i=dialHasSignal?findContact(dialSignalKey):-1;
  if(i>=0){if(trackedContact==i)trackContact(-1);else if(uint32_t(millis()-contacts[i].seen)<30000)trackContact(i);}
}
static void dialRadarEvent(lv_event_t* e){
  auto event=lv_event_get_code(e);
  if(event==LV_EVENT_PRESSED){
    dialPress.clear();lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);int nearest=100000,index=-1;
    for(int i=0;i<dotCount;++i)if(dragon::radarHit(p.x,p.y,dots[i].x,dots[i].y,17,nearest))index=i;
    if(index>=0)dialPress.begin(dots[index].key);
  }else if(event==LV_EVENT_SHORT_CLICKED){
    dragon::DeviceKey key;if(!dialPress.take(key)||findContact(key)<0)return;
    radarSelectedKey=key;radarHasSelection=true;radarSelectedAt=millis();
    if(dialTaps.tap(key,millis()))dialOpen(key);else lv_obj_invalidate(radar);
  }else if(event==LV_EVENT_PRESS_LOST||event==LV_EVENT_LONG_PRESSED)dialPress.clear();
}
static void dialUiBegin(){
  lv_obj_add_flag(radar,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(radar,dialRadarEvent,LV_EVENT_ALL,nullptr);
  lv_obj_add_flag(pageLabel,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pageLabel,[](lv_event_t*){if(radarHasSelection)dialOpen(radarSelectedKey);},LV_EVENT_SHORT_CLICKED,nullptr);
  lv_obj_add_flag(pageCanvas,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pageCanvas,[](lv_event_t*){if(litePage==LitePage::Signals)dialHunt();},LV_EVENT_SHORT_CLICKED,nullptr);
}
void dialRotate(int step){if(!root)return;dialPress.clear();dialTaps.clear();setLitePage(LitePage((int(litePage)+5+(step>0?1:-1))%5));}
static void dialButton(dragon::LitePress event){
  if(event==dragon::LitePress::Hold){setLitePage(LitePage::Radar);trackContact(-1);audioStop();return;}
  if(litePage==LitePage::Timer){
    if(event==dragon::LitePress::Tap)stopwatch.toggle(stopwatchNow());
    else if(event==dragon::LitePress::Double){if(stopwatch.running){if(liteLocksport())stopwatch.finish(stopwatchNow());else stopwatch.lap(stopwatchNow());}else stopwatch.reset();}
  }else if(litePage==LitePage::Signals){if(event==dragon::LitePress::Double)dialHunt();else dialNextSignal();}
  else if(litePage==LitePage::Status){alertSound=!alertSound;audioSave();if(!alertSound)audioStop();}
  else uiKey(false);
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
  if(litePage==LitePage::Radar){
    char value[96];snprintf(value,sizeof(value),"%s RX   %02d SIGNALS",blePhase?"BLE":"WI-FI",radarCount);lv_label_set_text(status,value);
    int i=radarHasSelection?findContact(radarSelectedKey):-1;
    if(i>=0){snprintf(value,sizeof(value),"%s / %d dBm",radarFamilyName(contacts[i].match.family),contacts[i].rssi);lv_label_set_text(pageLabel,value);}
    lv_label_set_text(pageHint,i>=0?"TAP SUMMARY FOR DETAILS":"TAP DOT / TURN FOR PAGES");
  }else if(litePage==LitePage::Clock)lv_label_set_text(pageHint,"PRESS THEME / TURN PAGE");
  else if(litePage==LitePage::Status){
    char value[160];snprintf(value,sizeof(value),"WI-FI %lu / BLE %lu\nDROPS %lu\nHEAP %u KB\nSOUND %s",wifiFrames,bleFrames,queueDrops,ESP.getFreeHeap()/1024,alertSound?"ON":"MUTED");lv_label_set_text(pageValue,value);
    lv_label_set_text(pageHint,"PRESS SOUND / HOLD RADAR");
  }
}
extern void dialInputStatus();
static bool dialUiCommand(const String& c){
  if(c=="touch status"||c=="rotary status"){dialInputStatus();return true;}
  if(c=="dial hunt stop"){setLitePage(LitePage::Radar);trackContact(-1);audioStop();Serial.println("DIAL_OK");return true;}
  if(c.startsWith("radar theme ")){
    String name=c.substring(12);for(unsigned i=0;i<RadarThemeCount;++i)if(i!=dragon::PersonalThemeId&&name==radarThemeCommands[i]){radarTheme=i;refreshLiteTheme();Serial.println("RADAR_OK");return true;}
    Serial.println("RADAR_ERROR unknown or unavailable theme");return true;
  }
  if(c=="dial status"){Serial.printf("DIAL_STATUS beta=1 contacts=%u rtc=%u touch=FT3267 encoder=1 psram=0 minimum_heap=%u largest_block=%u\n",unsigned(MAX_CONTACTS),rtcAvailable,ESP.getMinFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));return true;}
  return false;
}
