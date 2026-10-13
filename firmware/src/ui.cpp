#include "app.h"
#include "board_pins.h"
#include <lvgl.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <Preferences.h>

extern void scr_lvgl_init();
extern void set_brightness(uint8_t);
extern const lv_img_dsc_t vts_logo;
// The layout is authored for the 360 px round panel. S() maps it onto the
// board's panel; label/button/row helpers apply it, so callers pass 360 px units.
#define S(v) ((v)*SCREEN_RES/360)
#if SCREEN_RES>=466
#define F12 (&lv_font_montserrat_16)
#define F14 (&lv_font_montserrat_18)
#define F16 (&lv_font_montserrat_20)
#define F20 (&lv_font_montserrat_26)
#define FCALLOUT (&lv_font_montserrat_14)
#else
#define F12 (&lv_font_montserrat_12)
#define F14 (&lv_font_montserrat_14)
#define F16 (&lv_font_montserrat_16)
#define F20 (&lv_font_montserrat_20)
#define FCALLOUT F12
#endif
static const int CENTER=SCREEN_RES/2;
enum Page {Boot,Radar,Menu,Devices,Details,Alerts,Channels,Files,Audio,Settings,About,Badge};
static Page page=Boot,pending=Boot;
static lv_obj_t *root,*body,*statusLabel,*audioLabel,*audioMeter,*radar,*micBadge;
static int menuTab=0,filter=0,selected=-1,filePage=0,brightness=50;
static uint8_t selectedMac[6]{};static bool selectedBle=false;static uint8_t selectedType=0;
static void selectContact(int i){selected=i;memcpy(selectedMac,contacts[i].mac,6);selectedBle=contacts[i].ble;selectedType=contacts[i].addressType;}
static bool selectionValid(){return selected>=0 && contacts[selected].used && !memcmp(selectedMac,contacts[selected].mac,6) && selectedBle==contacts[selected].ble && selectedType==contacts[selected].addressType;}
static lv_obj_t* trackingLabel=nullptr;
static uint32_t pageAt=0,lastUpdate=0,lastInteraction=0;
static bool dimmed=false;
static Preferences settings;
static String directory="/";
struct FileRow {String path;bool dir;uint32_t size;};static FileRow files[96];static int fileCount=0;static bool filesTruncated=false;
struct Dot {int x,y,index;};static Dot dots[60];static int dotCount=0;
static lv_color_t color(uint32_t c){return lv_color_hex(c);}
static uint32_t familyColor(Family f){switch(f){case Family::Flock:return 0xff6878;case Family::Axon:return 0xf5b565;case Family::Meta:return 0xb8a0ff;case Family::Pineapple:return 0xffff70;case Family::Pwnagotchi:return 0xff86db;case Family::Nyanbox:return 0x80eaff;case Family::Halehound:return 0xff582f;case Family::Tracker:return 0x99abff;case Family::Skimmer:return 0xffbb88;case Family::Ble:return 0x83def0;default:return 0xfcdc58;}}
static lv_obj_t* label(lv_obj_t* parent,const char* text,int x,int y,int w=0,const lv_font_t* font=F14,uint32_t c=0xd9efe0){
  auto* o=lv_label_create(parent);lv_label_set_text(o,text);lv_obj_set_pos(o,S(x),S(y));if(w)lv_obj_set_width(o,S(w));lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,color(c),0);return o;
}
static void go(Page p){pending=p;}
static void action(lv_event_t* e);
static lv_obj_t* button(lv_obj_t* parent,const char* text,int x,int y,int w,int h,int id){
  auto* b=lv_btn_create(parent);lv_obj_set_pos(b,S(x),S(y));lv_obj_set_size(b,S(w),S(h));lv_obj_set_style_pad_all(b,S(8),0);lv_obj_set_style_bg_color(b,color(0x132b22),0);lv_obj_set_style_radius(b,S(13),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,color(0x31543a),0);lv_obj_set_style_shadow_width(b,0,0);
  lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* t=label(b,text,0,0,w-16);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(t);return b;
}
static void title(const char* text,const char* subtitle=nullptr){
  auto* t=label(root,text,55,39,250,F20);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
  if(subtitle){auto* s=label(root,subtitle,55,67,250,F12,0x8caf99);lv_obj_set_style_text_align(s,LV_TEXT_ALIGN_CENTER,0);}
  button(root,LV_SYMBOL_HOME,149,307,62,33,1);
}
static void legible(lv_obj_t* o){lv_obj_set_style_bg_color(o,color(0x092317),0);lv_obj_set_style_bg_opa(o,LV_OPA_80,0);lv_obj_set_style_radius(o,7,0);}
static lv_obj_t* scrollBody(int y=91,int h=211){
  auto* o=lv_obj_create(root);lv_obj_set_pos(o,S(44),S(y));lv_obj_set_size(o,S(272),S(h));lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_scroll_dir(o,LV_DIR_VER);return o;
}
static void row(lv_obj_t* parent,const char* top,const char* bottom,int index,int id,uint32_t c=0xd9efe0){
  auto* b=button(parent,"",1,index*59,256,54,id);label(b,top,0,0,231,F14,c);label(b,bottom,0,22,231,F12,0x88a995);
}
static void line(lv_draw_ctx_t* ctx,int x1,int y1,int x2,int y2,uint32_t c,int width=1,int opa=255){
  lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.color=color(c);d.width=width;d.opa=opa;lv_point_t a{(lv_coord_t)x1,(lv_coord_t)y1},b{(lv_coord_t)x2,(lv_coord_t)y2};lv_draw_line(ctx,&d,&a,&b);
}
static void disc(lv_draw_ctx_t* ctx,int x,int y,int r,uint32_t c,int opa=255){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=LV_RADIUS_CIRCLE;d.bg_color=color(c);d.bg_opa=opa;lv_area_t a{(lv_coord_t)(x-r),(lv_coord_t)(y-r),(lv_coord_t)(x+r),(lv_coord_t)(y+r)};lv_draw_rect(ctx,&d,&a);
}
static void callout(lv_draw_ctx_t* ctx,int dotX,int dotY,int boxX,int boxY,const char* text,uint32_t c){
  int width=S(118),height=S(22);
  lv_draw_rect_dsc_t box;lv_draw_rect_dsc_init(&box);box.radius=6;box.bg_color=color(0x071510);box.bg_opa=LV_OPA_90;
  box.border_color=color(c);box.border_opa=LV_OPA_COVER;box.border_width=1;
  lv_area_t area{(lv_coord_t)boxX,(lv_coord_t)boxY,(lv_coord_t)(boxX+width),(lv_coord_t)(boxY+height)};lv_draw_rect(ctx,&box,&area);
  int edgeX=boxX>dotX?boxX:boxX+width;int edgeY=boxY+height/2;line(ctx,dotX,dotY,edgeX,edgeY,c,1,220);
  lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.color=color(c);d.font=FCALLOUT;
  lv_area_t textArea{(lv_coord_t)(boxX+5),(lv_coord_t)(boxY+3),(lv_coord_t)(boxX+width-5),(lv_coord_t)(boxY+height-3)};
  lv_draw_label(ctx,&d,&textArea,text,nullptr);
}
static void radarDraw(lv_event_t* e){
  auto* ctx=lv_event_get_draw_ctx(e);disc(ctx,CENTER,CENTER,S(176),0x092317);
  for(int off=-120;off<=120;off+=40){int length=sqrtf(167*167-off*off);line(ctx,CENTER+S(off),CENTER-S(length),CENTER+S(off),CENTER+S(length),0x19412b);line(ctx,CENTER-S(length),CENTER+S(off),CENTER+S(length),CENTER+S(off),0x19412b);}
  lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.color=color(0x387b43);arc.width=1;lv_point_t center{(lv_coord_t)CENTER,(lv_coord_t)CENTER};
  for(int r=42;r<=168;r+=42)lv_draw_arc(ctx,&arc,&center,S(r),0,359);
  arc.width=3;arc.color=color(0x78985c);lv_draw_arc(ctx,&arc,&center,S(177),0,359);
  float angle=millis()*0.00075f;
  if(radioEnabled)for(int i=12;i>=0;--i){float a=angle-i*.022f;line(ctx,CENTER,CENTER,CENTER+cosf(a)*S(164),CENTER+sinf(a)*S(164),0x70ff5a,2,120-i*8);}
  disc(ctx,CENTER,CENTER,S(3),0xcafc9b);dotCount=0;
  // Display freshest contacts, oldest age limited to 30 s. Angle is a stable address hash, not a bearing.
  int order[MAX_CONTACTS],n=0;for(size_t i=0;i<MAX_CONTACTS;++i)if(radarVisible(contacts[i],millis()) || (int(i)==trackedContact && millis()-contacts[i].seen<30000))order[n++]=i;
  for(int i=0;i<n;++i)for(int j=i+1;j<n;++j){auto& a=contacts[order[i]];auto& b=contacts[order[j]];if(b.match.confidence>a.match.confidence || (b.match.confidence==a.match.confidence&&b.rssi>a.rssi))std::swap(order[i],order[j]);}
  int callouts=0;const int u5=S(5),u6=S(6),u7=S(7),u8=S(8);
  for(int k=0;k<n && dotCount<60;++k){int i=order[k];auto& c=contacts[i];uint32_t hash=2166136261u;for(uint8_t b:c.mac)hash=(hash^b)*16777619u;
    float a=(hash%360)*.01745329f;int r=S(constrain((-c.rssi-25)*2,20,152));int x=CENTER+cosf(a)*r,y=CENTER+sinf(a)*r;uint32_t cc=familyColor(c.match.family);
    uint8_t opacity=millis()-c.seen>10000?100:255;disc(ctx,x,y,S(9),cc,22);disc(ctx,x,y,S(4),cc,opacity);
    if(c.match.family==Family::Flock){line(ctx,x-u5,y-u6,x+u5,y-u6,cc,2);line(ctx,x-u5,y+u6,x+u5,y+u6,cc,2);}
    else if(c.match.family==Family::Meta){disc(ctx,x-u5,y,S(2),cc);disc(ctx,x+u5,y,S(2),cc);}
    else if(c.match.family==Family::Pwnagotchi || c.match.family==Family::Nyanbox){line(ctx,x-u5,y-u5,x-S(2),y-u8,cc,2);line(ctx,x+u5,y-u5,x+S(2),y-u8,cc,2);}
    else if(c.match.family!=Family::Wifi && c.match.family!=Family::Ble){line(ctx,x,y-u7,x+u7,y,cc,1);line(ctx,x+u7,y,x,y+u7,cc,1);line(ctx,x,y+u7,x-u7,y,cc,1);line(ctx,x-u7,y,x,y-u7,cc,1);}
    if(c.wifiAlerts && millis()-c.lastWifiAlert<30000){lv_draw_arc_dsc_t d=arc;d.color=color(0xff5040);d.width=2;lv_point_t p{(lv_coord_t)x,(lv_coord_t)y};lv_draw_arc(ctx,&d,&p,S(12),0,359);}
    if(callouts<3 && ((c.wifiAlerts && millis()-c.lastWifiAlert<30000) || c.match.confidence>=Confidence::Low)){
      const char* text=(c.wifiAlerts && millis()-c.lastWifiAlert<30000)?"DISCONNECT BURST":dragon::familyName(c.match.family);
      int boxX=x<CENTER?S(226):S(16);int boxY=S(92+callouts*31);callout(ctx,x,y,boxX,boxY,text,(c.wifiAlerts && millis()-c.lastWifiAlert<30000)?0xff6655:cc);++callouts;
    }
    dots[dotCount++]={x,y,i};
  }
}
static void radarTap(lv_event_t*){lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);int nearest=S(18)*S(18),idx=-1;for(int i=0;i<dotCount;++i){int d=(dots[i].x-p.x)*(dots[i].x-p.x)+(dots[i].y-p.y)*(dots[i].y-p.y);if(d<nearest){nearest=d;idx=dots[i].index;}}if(idx>=0){selectContact(idx);go(Details);}}
static void loadFiles(){
  fileCount=0;filesTruncated=false;if(!sdReady)return;
  xSemaphoreTake(sdMutex,portMAX_DELAY);File dir=SD_MMC.open(directory);
  if(dir&&dir.isDirectory()){File f=dir.openNextFile();while(f){if(fileCount>=96){filesTruncated=true;f.close();break;}String name=f.name();String path=directory=="/"?"/"+name:directory+"/"+name;files[fileCount++]={path,f.isDirectory(),uint32_t(f.size())};f.close();f=dir.openNextFile();}}dir.close();xSemaphoreGive(sdMutex);
}
static void build(){
  badgeUnload();page=pending;pageAt=millis();statusLabel=audioLabel=audioMeter=radar=trackingLabel=nullptr;
  lv_obj_clean(root);lv_obj_set_style_bg_color(root,color(0x071510),0);
  if(page==Badge){lv_obj_set_style_bg_color(root,color(0x000000),0);badgeView(root);
  }else if(page==Radar){
    radar=lv_obj_create(root);lv_obj_remove_style_all(radar);lv_obj_set_pos(radar,0,0);lv_obj_set_size(radar,SCREEN_RES,SCREEN_RES);lv_obj_clear_flag(radar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(radar,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(radar,radarDraw,LV_EVENT_DRAW_MAIN,nullptr);lv_obj_add_event_cb(radar,radarTap,LV_EVENT_CLICKED,nullptr);
    auto* t=label(root,"DRAGON DETECTOR",62,37,236,F16,0xd6f49a);lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_clear_flag(t,LV_OBJ_FLAG_CLICKABLE);legible(t);
    statusLabel=label(root,"LISTENING",56,60,248,F12,0xa7c57f);lv_obj_set_style_text_align(statusLabel,LV_TEXT_ALIGN_CENTER,0);legible(statusLabel);
    trackingLabel=label(root,"RSSI proximity / no bearing",50,280,260,F12,0xb3ca98);lv_obj_set_style_text_align(trackingLabel,LV_TEXT_ALIGN_CENTER,0);legible(trackingLabel);
    button(root,LV_SYMBOL_UP "  TOOLS",123,306,114,31,2);
  }else if(page==Menu){
    title("DRAGON TOOLS");const char* tabs[]={"RECON","BADGE","SYSTEM"};
    for(int i=0;i<3;++i){auto* b=button(root,tabs[i],47+i*89,76,86,32,10+i);if(menuTab==i)lv_obj_set_style_bg_color(b,color(0x36582b),0);}
    if(menuTab==0){button(root,"Wi-Fi",57,123,119,49,20);button(root,"Bluetooth",184,123,119,49,21);button(root,"Alerts",57,180,119,49,22);button(root,"Channels",184,180,119,49,23);button(root,"Packet capture",77,240,206,45,24);}
    if(menuTab==1){button(root,"Show my badge",59,120,242,48,50);button(root,"USB badge editor",59,178,242,48,51);button(root,LV_SYMBOL_DIRECTORY "  SD files",79,242,202,43,32);}

    if(menuTab==2){button(root,"Settings",60,126,240,48,40);button(root,"Signatures & limits",60,184,240,48,41);button(root,radioEnabled?"Pause radio":"Resume radio",79,243,202,44,42);}
  }else if(page==Devices){
    title(filter==1?"WI-FI CONTACTS":"BLE CONTACTS","Observed in the last 60 seconds");body=scrollBody();int rowNo=0;
    for(size_t i=0;i<MAX_CONTACTS;++i){auto& c=contacts[i];if(!c.used || (filter==1&&c.ble)||(filter==2&&!c.ble)||millis()-c.seen>60000)continue;
      char mac[18],sub[96];macText(c.mac,mac);snprintf(sub,sizeof(sub),"%d dBm  %s",c.rssi,dragon::familyName(c.match.family));row(body,*c.name?c.name:mac,sub,rowNo++,1000+i,familyColor(c.match.family));}
    if(!rowNo)label(body,"Listening...\nNo recent contacts",10,30,230);
    button(root,LV_SYMBOL_REFRESH,217,307,42,33,25);
  }else if(page==Details){
    title("CONTACT EVIDENCE");body=scrollBody(88,169);if(selectionValid()){auto& c=contacts[selected];char mac[18];macText(c.mac,mac);
      String s=String(*c.name?c.name:mac)+"\n"+mac+"\n\n"+dragon::familyName(c.match.family)+"\n"+dragon::confidenceName(c.match.confidence)+"\n"+c.match.evidence+"\n\n"+String(c.rssi)+" dBm  |  "+(c.ble?"BLE":String("CH ")+c.channel)+"\n"+c.signal.quality()+" / "+c.signal.direction()+"\nLast seen "+String((millis()-c.seen)/1000)+"s ago\nFrames: "+String(c.packets)+"\n\nIdentity is not verified. This does not establish recording or malicious intent.";
      label(body,s.c_str(),8,0,240);
      if(!c.ble && c.channel){char monitor[20];snprintf(monitor,sizeof(monitor),"Monitor CH %u",c.channel);button(root,monitor,52,263,124,37,45);}
      button(root,trackedContact==selected?"Stop tracking":"Track signal",184,263,124,37,52);
    }else label(body,"Contact expired. Return to the radar and select again.",8,0,240);
  }else if(page==Alerts){
    title("OBSERVATIONS","Pattern evidence, not proof of intent");body=scrollBody();uint32_t count=std::min<uint32_t>(alertTotal,MAX_ALERTS);
    if(!count)label(body,"No alerts observed.\nScan gaps and silent devices\ncan hide activity.",6,25,246);
    for(uint32_t i=0;i<count;++i){auto& a=alerts[(alertTotal-1-i)%MAX_ALERTS];String s=String(a.at/1000)+"s  "+a.title+"\n"+a.detail;auto* l=label(body,s.c_str(),4,i*110,244,F12);lv_obj_set_height(l,S(104));}
  }else if(page==Channels){
    title("CHANNEL ACTIVITY","Frames / second of observed dwell time");body=scrollBody();for(int ch=1;ch<=11;++ch){char s[96];float rate=channelDwellMs[ch]?channelFrames[ch]*1000.0/channelDwellMs[ch]:0;snprintf(s,sizeof(s),"CH %02d     %.1f f/s     %lus",ch,rate,(unsigned long)(channelDwellMs[ch]/1000));row(body,s,channelLock==ch?"Wi-Fi locked; BLE still alternates":"Tap to lock Wi-Fi channel",ch-1,200+ch);}
    button(root,"Auto",215,307,52,31,200);
  }else if(page==Files){
    title("SD CARD",directory.c_str());loadFiles();body=scrollBody();
    if(!sdReady)label(body,BOARD_HAS_SD?"SD card unavailable.\nInsert FAT32 card and restart.":"This board has no SD card slot.",5,20,244);
    else{row(body,".. / Parent",fileCount?"Tap a folder, MP3, WAV, or text file":"This folder is empty",0,39);for(int i=0;i<fileCount;++i){String name=files[i].path.substring(files[i].path.lastIndexOf('/')+1);String sub=files[i].dir?"Folder":String(files[i].size/1024)+" KB";row(body,name.c_str(),sub.c_str(),i+1,2000+i);}if(filesTruncated)label(body,"First 96 entries shown",5,(fileCount+1)*59,235);}
  }else if(page==Audio){
    title("AUDIO",sdReady?"Local SD playback & microphone":"SD card unavailable");
    audioLabel=label(root,"Ready",58,94,244,F14);lv_obj_set_height(audioLabel,S(65));
    audioMeter=lv_bar_create(root);lv_obj_set_pos(audioMeter,S(67),S(168));lv_obj_set_size(audioMeter,S(226),S(10));lv_bar_set_range(audioMeter,0,32767);
    button(root,"Record",54,198,120,43,33);button(root,"Stop / Save",182,198,124,43,34);button(root,"Browse",102,251,156,42,32);
    button(root,"-",51,254,42,37,35);button(root,"+",267,254,42,37,36);
  }else if(page==Settings){
    title("SETTINGS");body=scrollBody();row(body,loggingEnabled&&sdReady?"Event logging: on":"Event logging: off",sdReady?sessionPath.c_str():BOARD_HAS_SD?"SD card unavailable":"No SD slot on this board",0,43);
    row(body,captureEnabled?"PCAP: recording":"PCAP: stopped",sdReady?"Raw 802.11 / 16 MB per capture":BOARD_HAS_SD?"Needs an SD card":"No SD slot on this board",1,24);
    row(body,"Brightness","Tap to cycle 25 / 50 / 75 / 100%",2,44);
    row(body,radioEnabled?"Radio: listening":"Radio: paused","Tap to toggle",3,42);
    row(body,candidatesOnly?"Radar: candidates":"Radar: all contacts","Tap to change radar filter",5,53);
    row(body,trackedContact>=0?"Tracking: active":"Tracking: off","Tap to return to balanced scanning",6,54);
    row(body,"Boot to radar","Clear the saved badge startup choice",7,55);
    row(body,channelLock?"Wi-Fi channel locked":"Automatic Wi-Fi / BLE","Tap to restore alternating scans",4,200);
  }else if(page==About){
    title("SIGNATURES & LIMITS");body=scrollBody();
    label(body,"DRAGON DETECTOR 0.3\nDaily badge edition\n\nGreen radar / signal proximity.\nAngles are decorative.\nNo distance or direction.\n\nCandidates use repeated fresh\nevidence. Vendor-only hints\nare in the contact lists.\nTap a dot, then Track signal\nfor dedicated BLE or Wi-Fi.\n\nAirTag / Find My is a family\nmatch, not model or owner ID.\nBiscuit Pro / Ultra cannot be\ndistinguished from adverts.\nNo proof of malicious intent.\n\nHold badge: tools.\nIdle dimming: 45 seconds.\nUSB Badge Studio on GitHub.\n\nBy Your Pal Kal / Valleytech\nOriginal code: MIT.\nFlock Noir / Colonel Panic\nOUI Spy / Unified Blue\nMarauder: Just Call Me Koko\nFull notices in CREDITS.md",5,0,244,F14);
  }
  micBadge=label(root,"MIC REC",137,17,86,F12,0xff8585);lv_obj_set_style_text_align(micBadge,LV_TEXT_ALIGN_CENTER,0);legible(micBadge);lv_obj_add_flag(micBadge,LV_OBJ_FLAG_HIDDEN);
}
static void action(lv_event_t* e){
  lastInteraction=millis();
  int id=(intptr_t)lv_event_get_user_data(e);
  if(id>=2000 && id<2096){int i=id-2000;if(i>=fileCount)return;if(files[i].dir){directory=files[i].path;go(Files);page=Boot;}else{String ext=files[i].path;ext.toLowerCase();if(ext.endsWith(".mp3")||ext.endsWith(".wav")){audioPlay(files[i].path);go(Audio);}else{
      xSemaphoreTake(sdMutex,portMAX_DELAY);File f=SD_MMC.open(files[i].path);String text;while(f && f.available() && text.length()<2048){char ch=f.read();text+=(ch=='\n'||ch=='\t'||(ch>=32&&ch<127))?ch:'.';}f.close();xSemaphoreGive(sdMutex);
      lv_obj_t* box=lv_msgbox_create(root,"FILE PREVIEW",text.c_str(),nullptr,true);lv_obj_set_size(box,S(260),S(225));lv_obj_center(box);
    }}return;}
  if(id>=1000 && id<1000+(int)MAX_CONTACTS){selectContact(id-1000);go(Details);return;}
  if(id>=201 && id<=211){trackedContact=-1;channelLock=id-200;setRadio(true);go(Channels);page=Boot;return;}
  switch(id){
    case 1:go(Radar);break;case 2:go(Menu);break;
    case 10:case 11:case 12:menuTab=id-10;go(Menu);page=Boot;break;
    case 20:filter=1;go(Devices);break;case 21:filter=2;go(Devices);break;case 22:go(Alerts);break;case 23:go(Channels);break;
    case 24:setCapture(!captureEnabled);go(Settings);page=Boot;break;case 25:page=Boot;go(Devices);break;
    case 30:directory="/music";go(Files);break;case 31:go(Audio);break;case 32:directory="/";go(Files);break;
    case 33:audioRecord();break;case 34:audioStop();break;case 35:audioVolume(audioStatus().volume-10);break;case 36:audioVolume(audioStatus().volume+10);break;
    case 39:directory=directory.substring(0,directory.lastIndexOf('/'));if(directory.isEmpty())directory="/";go(Files);page=Boot;break;
    case 40:go(Settings);break;case 41:go(About);break;case 42:setRadio(!radioEnabled);pending=page;page=Boot;break;
    case 43:loggingEnabled=!loggingEnabled;pending=page;page=Boot;break;
    case 44:brightness=brightness==100?25:brightness+25;settings.putUChar("brightness",brightness);set_brightness(brightness);break;
    case 45:if(selectionValid() && !contacts[selected].ble && contacts[selected].channel){trackedContact=-1;channelLock=contacts[selected].channel;setRadio(true);go(Channels);}break;
    case 46:setCapture(!captureEnabled);pending=Details;page=Boot;break;
    case 50:go(Badge);break;
    case 51:{auto* box=lv_msgbox_create(root,"BADGE STUDIO","Open the GitHub web flasher on a computer, choose Badge Studio, connect USB, then crop and upload. Hold your badge to return to tools.",nullptr,true);lv_obj_set_size(box,S(260),S(225));lv_obj_center(box);break;}
    case 52:if(selectionValid())trackContact(trackedContact==selected?-1:selected);go(Radar);break;
    case 53:candidatesOnly=!candidatesOnly;settings.putBool("candidates",candidatesOnly);pending=page;page=Boot;break;
    case 54:trackContact(-1);pending=page;page=Boot;break;
    case 55:badgeCommand("badge boot off");go(Radar);break;
    case 200:trackContact(-1);pending=page;page=Boot;break;
  }
}
void uiTouch(bool pressed,int x,int y){
  if(pressed)lastInteraction=millis();
  static bool was=false;static int sx,sy,lx,ly;static uint32_t start;
  if(pressed){if(!was){sx=x;sy=y;start=millis();}lx=x;ly=y;}
  else if(was && millis()-start<1500){
    int dx=lx-sx,dy=ly-sy;
    // Raw gestures must never consume a scroll on list/detail screens. Only the
    // full-screen radar owns the bottom-edge shortcut. Menu tabs are buttons,
    // so no global horizontal/downward gesture is needed there.
    if(page==Radar && sy>S(285) && dy<-S(55) && abs(dy)>abs(dx))go(Menu);
  }
  was=pressed;
}
void uiShow(const String& name){
  lastInteraction=millis();
  if(name=="badge")go(Badge);else if(name=="radar")go(Radar);else if(name=="menu")go(Menu);else if(name=="wifi"){filter=1;go(Devices);}else if(name=="ble"){filter=2;go(Devices);}else if(name=="audio")go(Audio);else if(name=="files")go(Files);else if(name=="alerts")go(Alerts);else if(name=="channels")go(Channels);else if(name=="about")go(About);else if(name=="settings")go(Settings);
}
void uiBegin(){
  settings.begin("dd-ui",false);brightness=settings.getUChar("brightness",50);if(brightness<25||brightness>100)brightness=50;candidatesOnly=settings.getBool("candidates",true);
  scr_lvgl_init();set_brightness(brightness);lastInteraction=millis();root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root,color(0x071510),0);
#ifdef BOARD_AMOLED_175C
  pinMode(BTN_PIN,INPUT_PULLUP);
#endif
  auto* img=lv_img_create(root);lv_img_set_src(img,&vts_logo);lv_obj_align(img,LV_ALIGN_CENTER,0,S(-25));
  auto* name=label(root,"DRAGON DETECTOR",49,266,262,F20,0xd6f49a);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_CENTER,0);
  label(root,"VALLEYTECH  /  INITIALIZING",70,296,225,F12,0x91b178);
  lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,img);lv_anim_set_values(&a,S(200),S(256));lv_anim_set_time(&a,900);lv_anim_set_exec_cb(&a,[](void* o,int32_t v){lv_img_set_zoom((lv_obj_t*)o,v);});lv_anim_start(&a);
  pageAt=millis();
}
void uiLoop(){
  bool idle=millis()-lastInteraction>45000;if(idle!=dimmed){dimmed=idle;set_brightness(dimmed?8:brightness);}
  if(page==Boot && pending==Boot && millis()-pageAt>2200)pending=Radar;
  if(page!=pending)build();
  if(millis()-lastUpdate>100){lastUpdate=millis();if(micBadge){if(audioStatus().mode==AudioMode::Recording)lv_obj_clear_flag(micBadge,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(micBadge,LV_OBJ_FLAG_HIDDEN);}
#ifdef BOARD_AMOLED_175C
    // BOOT is the hardware fallback for navigation: radar <-> tools.
    static bool buttonWas=false;bool buttonDown=digitalRead(BTN_PIN)==LOW;if(buttonDown && !buttonWas && page!=Boot){lastInteraction=millis();go(page==Radar?Menu:Radar);}buttonWas=buttonDown;
#endif
    if(page==Radar && radar){
      if(trackedContact>=0 && trackingLabel){auto& t=contacts[trackedContact];uint32_t age=millis()-t.seen;String line=age>10000?String("Target stale / ")+age/1000+"s ago":String(t.rssi)+" dBm / "+t.signal.direction();lv_label_set_text(trackingLabel,line.c_str());}
      lv_obj_invalidate(radar);int count=0;for(auto& c:contacts)if(radarVisible(c,millis()))++count;char s[96];int n=snprintf(s,sizeof(s),"%s %s / %d matches",radioEnabled?(trackedContact>=0?"TRACK":blePhase?"BLE":"WI-FI"):"PAUSED",captureEnabled?"PCAP":"RX",count);
      int battery=batteryPercent();if(battery>=0)snprintf(s+n,sizeof(s)-n,"  /  %s%d%%",batteryCharging()?LV_SYMBOL_CHARGE " ":"",battery);lv_label_set_text(statusLabel,s);}
    if(page==Audio && audioLabel){auto s=audioStatus();String text=String(s.mode==AudioMode::Recording?"RECORDING":s.mode==AudioMode::Playing?"PLAYING":s.mode==AudioMode::Error?"ERROR":"READY")+"  "+s.seconds+"s  Vol "+s.volume+"%\n"+s.message;lv_label_set_text(audioLabel,text.c_str());lv_bar_set_value(audioMeter,s.peak,LV_ANIM_OFF);}
  }
  lv_timer_handler();
}

void uiKey(bool home){lastInteraction=millis();go(home?Radar:page==Menu?Badge:Menu);}
