static constexpr int p4SessionsOpen=-22720, p4StartWifi=-22721, p4StartBle=-22722, p4StopSession=-22723;
static lv_obj_t* p4SessionButtons[3];
static int p4ChannelBank=0;
static lv_obj_t *p4RadioStateLabel=nullptr, *p4RadioDetailLabel=nullptr;
static lv_obj_t *p4ChannelBars[7], *p4ChannelValues[7], *p4ChannelNumbers[7];
static constexpr int p4BankFirst=-22600, p4BankLast=-22601, p4ModeBase=-22610;
static constexpr int p4OpenProfiles=-22619, p4ProfileBase=-22620;
static constexpr int p4InspectContact=-22624;
static constexpr int p4SaveSurveyAction=-22625;
static constexpr const char* p4ProfileNames[]={"Dragon Radar","Axon Watch","ALPR Watch","Sky Watch"};
#include "p4_ble_workbench.inc"

static lv_obj_t* p4Centered(const char* text,int y,int width,const lv_font_t* font){
  auto *obj=label(root,text,(360-width)/2,y,width,font);
  lv_obj_set_style_text_align(obj,LV_TEXT_ALIGN_CENTER,0);
  return obj;
}

// Legacy advertising service identifiers are transmitted least-significant byte first.
static void p4ListAdvertisedServices(String& text,const dragon::notebook::Advert& advert){
  for(size_t offset=0;offset<advert.boundary;){
    const unsigned length=advert.data[offset++];
    if(!length || length>advert.boundary-offset)break;
    const uint8_t type=advert.data[offset];
    const size_t width=(type==2 || type==3 || type==0x16)?2:
                       (type==4 || type==5 || type==0x20)?4:
                       (type==6 || type==7 || type==0x21)?16:0;
    const bool serviceData=type==0x16 || type==0x20 || type==0x21;
    const size_t bytes=length-1;
    if(width && (serviceData || bytes%width==0)){
      for(size_t pos=0;pos+width<=bytes;pos+=width){
        const uint8_t* uuid=advert.data+offset+1+pos;
        text+="\nService UUID: ";
        for(size_t i=width;i>0;--i){
          char pair[3];snprintf(pair,sizeof(pair),"%02X",unsigned(uuid[i-1]));text+=pair;
          if(width==16 && (i==13 || i==11 || i==9 || i==7))text+='-';
        }
        if(serviceData)break;
      }
    }
    offset+=length;
  }
}

static void p4RefreshRadioPage(bool force){
  if(page!=P4Channels && page!=P4Beacons && page!=P4Modes && page!=P4Profiles &&
     page!=P4BleLab && page!=P4Gatt && page!=P4AdvertChanges && page!=P4Sessions)return;
  static uint32_t lastDraw=0;
  const uint32_t now=millis();
  if(!force && now-lastDraw<350)return;
  lastDraw=now;
  if(page==P4AdvertChanges){p4RefreshChanges(force);return;}
  if(page==P4Gatt){p4RefreshGatt(force);return;}
  char text[176];
  if(page==P4Sessions){
    missionStatus(text,sizeof(text));setText(p4RadioStateLabel,text);
    for(unsigned i=0;i<3;++i){
      const bool disabled=(i==2)?!missionActive():missionActive();
      if(disabled)lv_obj_add_state(p4SessionButtons[i],LV_STATE_DISABLED);
      else lv_obj_clear_state(p4SessionButtons[i],LV_STATE_DISABLED);
    }
    return;
  }
  if(page==P4Channels){
    uint16_t counts[14]={};int8_t strongest[14]={};uint32_t completed=0;
    const bool valid=p4RadarChannelSummary(counts,strongest,&completed);
    unsigned maximum=1;
    if(valid)for(uint16_t count:counts)if(count>maximum)maximum=count;
    for(int i=0;i<7;++i){
      const int channel=i+7*p4ChannelBank;
      snprintf(text,sizeof(text),"%02d",channel+1);setText(p4ChannelNumbers[i],text);
      lv_bar_set_value(p4ChannelBars[i],valid?int((uint32_t(counts[channel])*100)/maximum):0,LV_ANIM_OFF);
      if(!valid)strcpy(text,"--     --");
      else if(!counts[channel] || strongest[channel]==INT8_MIN)snprintf(text,sizeof(text),"%u     --",unsigned(counts[channel]));
      else snprintf(text,sizeof(text),"%u     %d",unsigned(counts[channel]),int(strongest[channel]));
      setText(p4ChannelValues[i],text);
    }
    if(valid)snprintf(text,sizeof(text),"%s / %lus ago",p4RadarStatus(),(unsigned long)((now-completed)/1000));
    else snprintf(text,sizeof(text),"%s / awaiting survey",p4RadarStatus());
    setText(p4RadioStateLabel,text);
  }else if(page==P4Modes){
    static const char* names[]={"Wi-Fi + Bluetooth","Wi-Fi","Bluetooth"};
    const unsigned mode=p4RadarScanMode();
    snprintf(text,sizeof(text),"Selected: %s",mode<3?names[mode]:"Unknown");
    setText(p4RadioStateLabel,text);
  }else if(page==P4Profiles){
    const unsigned profile=unsigned(reconMode());
    snprintf(text,sizeof(text),"Selected: %s",profile<4?p4ProfileNames[profile]:"Dragon Radar");
    setText(p4RadioStateLabel,text);
  }else{
    snprintf(text,sizeof(text),"Bluetooth: %s",p4RadarBluetoothStatus());
    setText(p4RadioStateLabel,text);
    if(page==P4Beacons){
      snprintf(text,sizeof(text),"iBeacon     %lu\nEddystone     %lu\n\nListening windows     %lu",
          (unsigned long)p4RadarIbeaconReports(),(unsigned long)p4RadarEddystoneReports(),
          (unsigned long)p4RadarBleWindows());
    }else{
      snprintf(text,sizeof(text),"%lu advertisements / %lu windows",
          (unsigned long)p4RadarBleReports(),(unsigned long)p4RadarBleWindows());
    }
    setText(p4RadioDetailLabel,text);
  }
}

static bool p4BuildRadioPage(){
  if(p4BuildResearchPage())return true;
  if(page==P4Sessions){
    title("RECON SESSIONS","Five-minute passive recording");
    p4RadioStateLabel=p4Centered("",84,245,F12);
    p4SessionButtons[0]=button(root,"Record Wi-Fi",70,163,220,34,p4StartWifi);
    p4SessionButtons[1]=button(root,"Record BLE",70,206,220,34,p4StartBle);
    p4SessionButtons[2]=button(root,"Stop and save",79,249,202,34,p4StopSession);
  }else if(page==P4Channels){
    title("CHANNEL VISUALIZER","Wi-Fi survey");
    label(root,"CH",68,88,29,F12);label(root,"ACCESS POINTS",101,88,101,F12);
    label(root,"APs   PEAK dBm",202,88,92,F12);
    for(int i=0;i<7;++i){
      const int y=108+18*i;
      p4ChannelNumbers[i]=label(root,"",69,y,27,F12);
      auto* bar=lv_bar_create(root);lv_obj_set_pos(bar,S(101),S(y+5));
      lv_obj_set_size(bar,S(91),S(7));
      lv_obj_set_style_bg_color(bar,color(uiColors().accent),LV_PART_INDICATOR);
      p4ChannelBars[i]=bar;
      p4ChannelValues[i]=label(root,"",203,y,85,F12);
    }
    p4RadioStateLabel=p4Centered("",231,264,F12);
    button(root,"1-7",57,260,57,33,p4BankFirst);
    button(root,"Save survey",121,260,118,33,p4SaveSurveyAction);
    button(root,"8-14",246,260,57,33,p4BankLast);
  }else if(page==P4Beacons){
    title("BLE BEACON SCAN","Received advertisements");
    p4RadioStateLabel=p4Centered("",100,244,F14);
    p4RadioDetailLabel=p4Centered("",141,238,F14);
    p4Centered("Received advertisements since startup",237,244,F12);
  }else if(page==P4Modes){
    title("SCAN MODES","Passive discovery");
    p4RadioStateLabel=p4Centered("",90,248,F14);
    const char* choices[]={"Wi-Fi + Bluetooth","Wi-Fi only","Bluetooth only"};
    for(int i=0;i<3;++i)button(root,choices[i],75,117+42*i,210,35,p4ModeBase-i);
    button(root,"Detector profile",75,250,210,36,p4OpenProfiles);
  }else if(page==P4Profiles){
    title("DETECTOR PROFILE","Passive observations");
    p4RadioStateLabel=p4Centered("",89,250,F12);
    for(unsigned i=0;i<4;++i){
      auto *b=button(root,p4ProfileNames[i],74,111+36*i,212,32,p4ProfileBase-int(i));
      if(!p4RadarProfileSupported(i))lv_obj_add_state(b,LV_STATE_DISABLED);
    }
    p4Centered("Sky Watch uses broadcast BLE. Raw Wi-Fi frames are unavailable.",261,238,F12);
  }else if(page==P4BleLab){
    title("BLE LAB");
    p4RadioStateLabel=p4Centered("",78,248,F12);
    p4RadioDetailLabel=p4Centered("",98,248,F12);
    button(root,"Advertisement inspector",65,123,230,34,155);
    button(root,"GATT services",75,164,210,34,p4GattChoose);
    button(root,"Observed devices",75,205,210,34,int(p4ui::Action::BleContacts));
    button(root,"BLE Beacon Scan",75,246,210,34,int(p4ui::Action::Beacons));
  }else return false;
  p4RefreshRadioPage(true);
  return true;
}

static bool p4RadioAction(int id){
  if(p4ResearchAction(id))return true;
  if(id==p4SessionsOpen){go(P4Sessions);return true;}
  if(page==P4Sessions && (id==p4StartWifi || id==p4StartBle)){
    const bool ble=id==p4StartBle;
    if(const char* reason=p4SessionUnavailable(ble))p4Message("RECON SESSION",reason);
    else{
      char name[24];snprintf(name,sizeof(name),"session-%08lx",(unsigned long)esp_random());
      if(!missionStart(ble,name))p4Message("RECON SESSION","Cannot create the session. Check card space and retry.");
    }
    p4RefreshRadioPage(true);return true;
  }
  if(page==P4Sessions && id==p4StopSession){missionStop();p4RefreshRadioPage(true);return true;}
  if(page==P4Channels && id==p4SaveSurveyAction){
    char message[120];p4SaveWifiSurvey(nullptr,message,sizeof(message));
    p4Message("WI-FI SURVEY",message);return true;
  }
  if(page==Details && id==p4InspectContact){
    if(selectionValid() && selectedBle && notebookAdverts){
      for(unsigned i=0;i<dragon::notebook::Capacity;++i){
        const auto& advert=notebookAdverts[i];
        if(advert.used && advert.type==selectedType && !memcmp(advert.mac,selectedMac,6)){
          notebookSelection=advert;
          go(NotebookDetail);
          return true;
        }
      }
    }
    p4Message("ADVERTISEMENT","No complete advertisement is available for this contact yet.");
    return true;
  }
  if(page==P4Modes && id==p4OpenProfiles){go(P4Profiles);return true;}
  if(page==P4Profiles && id<=p4ProfileBase && id>=p4ProfileBase-3){
    const unsigned next=unsigned(p4ProfileBase-id);
    if(!p4RadarProfileSupported(next) || !reconSetMode(dragon::ReconMode(next)))
      p4Message("DETECTOR PROFILE","This receiver cannot use that profile right now.");
    p4RefreshRadioPage(true);return true;
  }
  if(page==P4Channels && (id==p4BankFirst || id==p4BankLast)){
    p4ChannelBank=id==p4BankLast; p4RefreshRadioPage(true);return true;
  }
  if(page==P4Modes && id<=p4ModeBase && id>=p4ModeBase-2){
    if(!p4RadarSetScanMode(unsigned(p4ModeBase-id)))
      p4Message("SCAN MODE","This receiver cannot use that mode right now.");
    p4RefreshRadioPage(true); return true;
  }
  return false;
}
