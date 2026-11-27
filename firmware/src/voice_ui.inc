// Original MIT Voice Recorder and WAV player pages (BOARD_HAS_MIC), included by
// ui.cpp. LVGL work stays here in the UI task; the card and the microphones
// belong to voice_recorder.cpp, which this only asks and reads.
#if BOARD_HAS_MIC
#if !BOARD_HAS_SD
#error "The Voice Recorder saves to the SD card: BOARD_HAS_MIC needs BOARD_HAS_SD"
#endif
static lv_obj_t *voiceButton=nullptr,*voiceShape=nullptr,*voiceTime=nullptr,*voiceMeter=nullptr,*voiceInfo=nullptr,*voiceCard=nullptr,*voiceList=nullptr,*voiceBox=nullptr;
static lv_obj_t *playButton=nullptr,*playSymbol=nullptr,*playPosition=nullptr,*playBar=nullptr,*playVolume=nullptr,*playInfo=nullptr,*playName=nullptr;
static VoiceListing voiceRows;            // what the list on screen shows
static int voiceShownState=-1;static bool voiceShownCard=false;
static String playerPath;static Page playerReturn=Recorder;
static char voiceMessage[96]="";static uint32_t voiceMessageAt=0;
static char voiceDeletePath[64]="";
static constexpr uint32_t VoiceRecordRed=0xff4b4b;
#ifdef BOARD_WAVESHARE_P4_KNOB
static lv_obj_t *playOrbit=nullptr,*voiceTrace=nullptr;
static uint8_t voiceLevels[40]{};
static void voiceTraceDraw(lv_event_t* e){
  auto* obj=lv_event_get_target(e);auto* ctx=lv_event_get_draw_ctx(e);lv_area_t area;lv_obj_get_coords(obj,&area);
  lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.color=color(uiColors().accent);line.width=3;line.round_start=line.round_end=1;
  const int height=lv_area_get_height(&area),width=lv_area_get_width(&area),middle=(area.y1+area.y2)/2;
  for(unsigned i=0;i<40;++i){int h=std::max(1,int(voiceLevels[i])*height/200);int x=area.x1+int(i)*width/40;
    lv_point_t a{lv_coord_t(x),lv_coord_t(middle-h)},b{lv_coord_t(x),lv_coord_t(middle+h)};lv_draw_line(ctx,&line,&a,&b);}
}
#endif
static void voiceUiForget(){
#ifdef BOARD_WAVESHARE_P4_KNOB
  playOrbit=voiceTrace=nullptr;
#endif
  voiceButton=voiceShape=voiceTime=voiceMeter=voiceInfo=voiceCard=voiceList=voiceBox=nullptr;
  playButton=playSymbol=playPosition=playBar=playVolume=playInfo=playName=nullptr;
}
static void voiceSay(const char* text){strlcpy(voiceMessage,text,sizeof(voiceMessage));voiceMessageAt=millis();}
static void voiceClock(char* out,size_t size,uint32_t seconds){
  if(seconds>=3600)snprintf(out,size,"%lu:%02u:%02u",(unsigned long)(seconds/3600),unsigned(seconds/60%60),unsigned(seconds%60));
  else snprintf(out,size,"%u:%02u",unsigned(seconds/60),unsigned(seconds%60));
}
static void voiceBytes(char* out,size_t size,uint64_t bytes){
  if(bytes>=1000000000ull)snprintf(out,size,"%.1f GB",double(bytes)/1e9);
  else if(bytes>=1000000ull)snprintf(out,size,"%.1f MB",double(bytes)/1e6);
  else snprintf(out,size,"%u KB",unsigned(bytes/1000));
}
// REC_2026-11-20_15-33-07_2.wav -> "11-20 15:33:07 #2"; other names lose ".wav".
static void voiceRowName(char* out,size_t size,const char* name){
  int y,mo,d,h,mi,s;unsigned long up;
  if(sscanf(name,"REC_%4d-%2d-%2d_%2d-%2d-%2d",&y,&mo,&d,&h,&mi,&s)==6){
    const char* again=strlen(name)>23&&name[23]=='_'?name+24:nullptr;
    snprintf(out,size,"%02d-%02d %02d:%02d:%02d%s%.*s",mo,d,h,mi,s,again?" #":"",again?int(strcspn(again,".")):0,again?again:"");
  }else if(sscanf(name,"REC_UPTIME_%lus",&up)==1)snprintf(out,size,"Uptime %lu s",up);
  else{const size_t n=strlen(name);snprintf(out,size,"%.*s",int(n>4?n-4:n),name);}
}
static void voiceFillList(){
  if(!voiceList)return;
  lv_obj_clean(voiceList);voiceRows=voiceListing();
  if(!sdReady){label(voiceList,"Insert a FAT32 card to record.",5,8,250,F12,uiColors().muted);return;}
  if(!voiceRows.ready){label(voiceList,"Reading /recordings ...",5,8,250,F12,uiColors().muted);return;}
  unsigned n=0;
  for(;n<voiceRows.count;++n){
    char name[40],length[16],text[64];voiceRowName(name,sizeof(name),voiceRows.name[n]);
    const uint32_t bytes=voiceRows.bytes[n];voiceClock(length,sizeof(length),bytes>44?(bytes-44)/32000:0); // 16 kHz mono 16-bit
    snprintf(text,sizeof(text),"%s   %s",name,length);
    auto* item=button(voiceList,"",1,n*40,212,36,2200+n);auto* t=lv_obj_get_child(item,0);
    lv_label_set_text(t,text);lv_obj_set_style_text_font(t,uiFont(F12),0);lv_label_set_long_mode(t,LV_LABEL_LONG_DOT);lv_obj_set_height(t,S(17));lv_obj_center(t);
    button(voiceList,LV_SYMBOL_TRASH,218,n*40,40,36,2210+n);
  }
  if(!voiceRows.total)label(voiceList,"No recordings yet.\nThey save to /recordings.",5,4,250,F12,uiColors().muted);
  else{char text[48];snprintf(text,sizeof(text),"All %lu in SD files  >",(unsigned long)voiceRows.total);button(voiceList,text,1,n*40,257,36,192);}
}
static void voiceCardText(){
  if(!voiceCard)return;
  if(!sdReady){setText(voiceCard,"No card");return;}
  if(!voiceRows.ready){setText(voiceCard,"Reading the card...");return;}
  char size[16],text[64];voiceBytes(size,sizeof(size),voiceRows.freeBytes);
  const uint64_t minutes=voiceRows.freeBytes/(32000ull*60); // 16 kHz mono 16-bit
  if(minutes>=120)snprintf(text,sizeof(text),"Card: %s free / %lu h",size,(unsigned long)(minutes/60));
  else snprintf(text,sizeof(text),"Card: %s free / %lu min",size,(unsigned long)minutes);
  setText(voiceCard,text);
}
static void voiceRecorderTick(){
  if(!voiceButton)return;
  if(sdReady!=voiceShownCard){voiceShownCard=sdReady;if(sdReady)voiceRefresh();}
  const auto list=voiceListing();
  if((list.revision!=voiceRows.revision||list.ready!=voiceRows.ready)&&!voiceBox)voiceFillList();
  const auto s=voiceStatus();const bool rec=s.rec==VoiceRec::Recording;
  if(int(rec)!=voiceShownState){
    voiceShownState=rec;const int size=rec?24:30;
    lv_obj_set_size(voiceShape,S(size),S(size));lv_obj_set_style_radius(voiceShape,rec?S(4):LV_RADIUS_CIRCLE,0);lv_obj_center(voiceShape);
    lv_obj_set_style_border_color(voiceButton,color(rec?VoiceRecordRed:uiColors().accent),0);
  }
  char time[16];voiceClock(time,sizeof(time),rec||*s.recFile?s.recSeconds:0);setText(voiceTime,time);
  const int32_t level=rec?s.peak:0;if(lv_bar_get_value(voiceMeter)!=level)lv_bar_set_value(voiceMeter,level,LV_ANIM_OFF);
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(voiceTrace){memmove(voiceLevels,voiceLevels+1,sizeof(voiceLevels)-1);voiceLevels[39]=level;lv_obj_invalidate(voiceTrace);}
#endif
  char info[112];
  if(*voiceMessage&&millis()-voiceMessageAt<5000)snprintf(info,sizeof(info),"%s",voiceMessage);
  else if(rec){
    char size[16];voiceBytes(size,sizeof(size),s.recBytes);
    if(s.dropped)snprintf(info,sizeof(info),"REC %s / %lu s lost: card slow",size,(unsigned long)(s.dropped/16000+1));
    else snprintf(info,sizeof(info),"REC %s / tap to stop",size);
  }else if(*s.recWhy)snprintf(info,sizeof(info),"%s",s.recWhy);
  else snprintf(info,sizeof(info),"%s",sdReady?"Tap the button to record":"Insert a card to record");
  setText(voiceInfo,info);setTextColor(voiceInfo,rec?VoiceRecordRed:s.rec==VoiceRec::Error?uiColors().accent:uiColors().muted);
  voiceCardText();
}
static void voiceAskDelete(unsigned row){
  if(voiceBox||row>=voiceRows.count)return;
  snprintf(voiceDeletePath,sizeof(voiceDeletePath),"/recordings/%s",voiceRows.name[row]);
  static const char* choices[]={"Cancel","Delete",""};
  char text[96];snprintf(text,sizeof(text),"%s\nThis cannot be undone.",voiceRows.name[row]);
  voiceBox=uiMessageBox(root,"DELETE RECORDING?",text,choices,true);lv_obj_set_width(voiceBox,S(270));lv_obj_center(voiceBox);
  lv_obj_add_event_cb(voiceBox,[](lv_event_t* e){
    auto* box=lv_event_get_current_target(e);
    if(lv_msgbox_get_active_btn(box)==1){char why[96];voiceSay(voiceDelete(voiceDeletePath,why,sizeof(why))?"Deleted":why);}
    lv_msgbox_close(box);voiceBox=nullptr;
  },LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_add_event_cb(voiceBox,[](lv_event_t*){voiceBox=nullptr;},LV_EVENT_DELETE,nullptr);
}
static void voiceRoundButton(lv_obj_t* b){
  lv_obj_set_style_radius(b,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(b,2,0);lv_obj_set_style_border_color(b,color(uiColors().accent),0);lv_obj_set_style_pad_all(b,0,0);
}
static lv_obj_t* voiceBar(int x,int y,int w,int max){
  auto* bar=lv_bar_create(root);lv_obj_set_pos(bar,S(x),S(y));lv_obj_set_size(bar,S(w),S(8));lv_bar_set_range(bar,0,max);lv_bar_set_value(bar,0,LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar,color(uiColors().raised),0);lv_obj_set_style_bg_color(bar,color(uiColors().accent),LV_PART_INDICATOR);return bar;
}
static void buildRecorder(){
  // The record button comes first, so the knob starts on it; the title (and
  // its home button) comes last.
  voiceButton=button(root,"",145,82,70,70,191);voiceRoundButton(voiceButton);
  voiceShape=lv_obj_create(voiceButton);lv_obj_remove_style_all(voiceShape);lv_obj_clear_flag(voiceShape,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(voiceShape,LV_OPA_COVER,0);lv_obj_set_style_bg_color(voiceShape,color(VoiceRecordRed),0);
  voiceCard=label(root,"",55,62,250,F12,uiColors().muted);centered(voiceCard);
  voiceTime=label(root,"0:00",80,156,200,F20);centered(voiceTime);
  voiceMeter=voiceBar(100,186,160,100);
#ifdef BOARD_WAVESHARE_P4_KNOB
  lv_obj_add_flag(voiceMeter,LV_OBJ_FLAG_HIDDEN);memset(voiceLevels,0,sizeof(voiceLevels));
  voiceTrace=lv_obj_create(root);lv_obj_remove_style_all(voiceTrace);lv_obj_set_pos(voiceTrace,S(75),S(179));lv_obj_set_size(voiceTrace,S(210),S(18));
  lv_obj_clear_flag(voiceTrace,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(voiceTrace,voiceTraceDraw,LV_EVENT_DRAW_MAIN,nullptr);
#endif
  voiceInfo=label(root,"",50,198,260,F12,uiColors().muted);centered(voiceInfo);lv_label_set_long_mode(voiceInfo,LV_LABEL_LONG_DOT);lv_obj_set_height(voiceInfo,S(17));
  voiceList=scrollBody(220,82);
  voiceRows=VoiceListing{};voiceShownState=-1;voiceShownCard=sdReady;
  voiceFillList();
  title("VOICE RECORDER");
  if(sdReady)voiceRefresh();
  voiceRecorderTick();
}
static void voicePlayerTick(){
  if(!playButton)return;
  const auto s=voiceStatus();
  setText(playSymbol,s.play==VoicePlay::Playing?LV_SYMBOL_PAUSE:LV_SYMBOL_PLAY);
  setText(playName,*s.playFile?s.playFile:playerPath.c_str());
  char now[16],total[16],text[40];voiceClock(now,sizeof(now),s.playMs/1000);voiceClock(total,sizeof(total),s.playTotalMs/1000);
  snprintf(text,sizeof(text),"%s / %s",now,total);setText(playPosition,text);
  const int32_t at=s.playTotalMs?int32_t(uint64_t(s.playMs)*1000/s.playTotalMs):0;if(lv_bar_get_value(playBar)!=at)lv_bar_set_value(playBar,at,LV_ANIM_OFF);
  snprintf(text,sizeof(text),"Vol %u%%",unsigned(audioStatus().volume));setText(playVolume,text);
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(playOrbit)lv_arc_set_value(playOrbit,audioStatus().volume);
#endif
  char info[112];
  if(s.play==VoicePlay::Error)snprintf(info,sizeof(info),"%s",s.playWhy);
  else if(s.play==VoicePlay::Playing||s.play==VoicePlay::Paused){
    if(!s.playRate)snprintf(info,sizeof(info),"Opening...");
    else snprintf(info,sizeof(info),"%lu Hz %s%s",(unsigned long)s.playRate,s.playChannels==2?"stereo":"mono",s.play==VoicePlay::Paused?" / paused":"");
  }else snprintf(info,sizeof(info),"%s",*s.playWhy?s.playWhy:"Stopped");
  setText(playInfo,info);setTextColor(playInfo,s.play==VoicePlay::Error?uiColors().accent:uiColors().muted);
}
static void buildPlayer(){
#ifdef BOARD_WAVESHARE_P4_KNOB
  playOrbit=lv_arc_create(root);lv_obj_set_size(playOrbit,SCREEN_RES-20,SCREEN_RES-20);lv_obj_center(playOrbit);
  lv_arc_set_rotation(playOrbit,135);lv_arc_set_bg_angles(playOrbit,0,270);lv_arc_set_range(playOrbit,0,100);
  lv_obj_remove_style(playOrbit,nullptr,LV_PART_KNOB);lv_obj_clear_flag(playOrbit,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(playOrbit,3,LV_PART_MAIN);lv_obj_set_style_arc_color(playOrbit,color(uiColors().border),LV_PART_MAIN);
  lv_obj_set_style_arc_width(playOrbit,7,LV_PART_INDICATOR);lv_obj_set_style_arc_color(playOrbit,color(uiColors().accent),LV_PART_INDICATOR);
#endif
  playButton=button(root,LV_SYMBOL_PLAY,145,88,70,70,193);voiceRoundButton(playButton);
  playSymbol=lv_obj_get_child(playButton,0);lv_obj_set_style_text_font(playSymbol,uiFont(F20),0);lv_obj_set_style_text_color(playSymbol,color(uiColors().accent),0);lv_obj_center(playSymbol);
  button(root,LV_SYMBOL_STOP,76,102,56,42,194);
  button(root,LV_SYMBOL_LEFT,228,102,56,42,197);
  button(root,LV_SYMBOL_MINUS,84,212,52,36,195);
  button(root,LV_SYMBOL_PLUS,224,212,52,36,196);
  playName=label(root,"",55,64,250,F12,uiColors().muted);centered(playName);lv_label_set_long_mode(playName,LV_LABEL_LONG_DOT);lv_obj_set_height(playName,S(17));
  playPosition=label(root,"0:00 / 0:00",70,164,220,F20);centered(playPosition);
  playBar=voiceBar(80,196,200,1000);
  playVolume=label(root,"",136,220,88,F14);centered(playVolume);
  playInfo=label(root,"",60,256,240,F12,uiColors().muted);centered(playInfo);lv_label_set_long_mode(playInfo,LV_LABEL_LONG_DOT);lv_obj_set_height(playInfo,S(36));
  title("PLAYER");
#ifdef BOARD_WAVESHARE_P4_KNOB
  auto* hint=label(root,"TURN KNOB FOR VOLUME",65,288,230,F12,uiColors().accent);centered(hint);
#endif
  voicePlayerTick();
}
// A WAV or MP3 file opens in the player.
static void voiceOpen(const String& path,Page back){
  String lower=path;lower.toLowerCase();
  if(!lower.endsWith(".wav")
#if BOARD_HAS_MP3
     &&!lower.endsWith(".mp3")
#endif
    ){
    auto* box=uiMessageBox(root,"CANNOT PLAY",
#if BOARD_HAS_MP3
      "This player opens WAV and MP3 files.",
#else
      "This player opens 16-bit PCM WAV files (8-48 kHz, mono or stereo).",
#endif
      nullptr,true);
    lv_obj_set_width(box,S(270));lv_obj_center(box);return;
  }
  playerPath=path;playerReturn=back;char why[96];voicePlay(path.c_str(),why,sizeof(why)); // a failure shows in the player
  go(Player);if(page==Player)page=Boot;
}
static bool voiceAction(int id){
  switch(id){
    case 190:go(Recorder);return true;
    case 191:
      if(voiceRecording())voiceRecordStop();
      else{char why[96];if(!voiceRecordStart(why,sizeof(why)))voiceSay(why);else voiceMessage[0]=0;}
      voiceRecorderTick();return true;
    case 192:directory="/recordings";go(Files);return true;
    case 193:{
      const auto s=voiceStatus();
      if(s.play==VoicePlay::Playing)voicePause(true);
      else if(s.play==VoicePlay::Paused)voicePause(false);
      else if(playerPath.length()){char why[96];voicePlay(playerPath.c_str(),why,sizeof(why));}
      voicePlayerTick();return true;}
    case 194:voicePlayStop();return true;
    case 195:audioVolume(int(audioStatus().volume)-10);voicePlayerTick();return true;
    case 196:audioVolume(int(audioStatus().volume)+10);voicePlayerTick();return true;
    case 197:go(playerReturn);return true;
  }
  if(id>=2200&&id<2200+int(VoiceListMax)){const unsigned row=id-2200;if(row<voiceRows.count)voiceOpen(String("/recordings/")+voiceRows.name[row],Recorder);return true;}
  if(id>=2210&&id<2210+int(VoiceListMax)){voiceAskDelete(id-2210);return true;}
  return false;
}
#endif
