#include "recon_internal.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "ble_lab.h"
#include "portal.h"
#include "csi_monitor.h"
#include "network_tools.h"
#include "missions.h"
#include "recipes.h"
#include <lvgl.h>
#include "lab_ui_scale.h"
using namespace dragon;
using namespace dragon::tx;
using namespace dragon::toolkit;
namespace {
using namespace dragon::labui;
enum View {Modes,Tools,WifiTools,BleTools,Networks,Stations,Confirm,Running,Capture,Library,FileDetail,Statistics,Probes,Filters,Portal,Csi,Network,NetworkProfiles,Missions,Recipes,Attacks};
View requested=Modes;bool pageRequested=false;
View view=Modes;lv_obj_t* root=nullptr;lv_obj_t* body=nullptr;lv_obj_t* info=nullptr;
bool redraw=true;uint32_t updated=0;AttackMode chosen=AttackMode::Beacon;
char fileNames[32][48]{};size_t fileSizes[32]{};unsigned fileCount=0,fileIndex=0;
// Copy keys into the row model. A live scan can reuse the cache slot before tap.
uint8_t apKeys[ApCapacity][6]{},clientKeys[ClientCapacity][6]{};unsigned apRows=0,clientRows=0;
void action(lv_event_t*);
lv_obj_t* text(lv_obj_t* parent,const String& s,int x,int y,int w=326,uint32_t color=0xd9efe0,const lv_font_t* font=font18()){auto* l=lv_label_create(parent);dragon::labui::pos(l,x,y);dragon::labui::width(l,w);lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);lv_label_set_text(l,s.c_str());return l;}
lv_obj_t* btn(lv_obj_t* parent,const char* s,int x,int y,int w,int h,int id){auto* b=lv_btn_create(parent);dragon::labui::pos(b,x,y);dragon::labui::size(b,w,h);lv_obj_set_style_bg_color(b,lv_color_hex(0x15392a),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,lv_color_hex(0x437450),0);lv_obj_set_style_pad_all(b,dragon::labui::px(10),0);lv_obj_set_style_radius(b,dragon::labui::px(12),0);lv_obj_set_style_shadow_width(b,0,0);lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void*)(intptr_t)id);if(!*s)return b;auto* l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_set_style_text_font(l,font18(),0);lv_obj_center(l);return b;}
void row(const char* title,const char* detail,unsigned i,int id){auto* b=btn(body,"",0,i*70,338,64,id);text(b,title,0,-1,308,0xe6ffb2,font18());text(b,detail,0,24,308,0xa1c4b3,font14());}
void change(View v){view=v;redraw=true;}
void updateStatus(){if(!info)return;char line[520];if(view==Recipes){recipeStatus(line,sizeof(line));}else if(view==Missions){missionStatus(line,sizeof(line));}else if(view==Network){networkStatus(line,sizeof(line),true);}else if(view==Csi){csiStatus(line,sizeof(line));}else if(view==Portal){portalStatus(line,sizeof(line),true);}else if(view==Statistics){snprintf(line,sizeof(line),"PASSIVE WI-FI OBSERVATIONS\n\nFrames: %lu   Retries: %lu\nBeacons: %lu   Probes: %lu\nDisconnects: %lu\nEAPOL: %lu   Protected: %lu\nPwnagotchi: %lu   Detections: %lu\n\nEAPOL key-message candidates\nM1 %lu  M2 %lu  M3 %lu  M4 %lu\nCounts do not prove a complete\nhandshake. Export PCAP to inspect.",counters.frames,counters.retries,counters.beacons,counters.probes,counters.disconnect,counters.eapol,counters.protectedFrames,counters.peers,counters.detections,counters.keyMessages[1],counters.keyMessages[2],counters.keyMessages[3],counters.keyMessages[4]);}else if(view==Running){unsigned left=status.active?std::max<int>(0,int(seconds)-int((millis()-status.started)/1000)):0;snprintf(line,sizeof(line),"%s\n\n%s\n\n%us remaining\nAccepted: %lu   Errors: %lu\nAuth replies: %lu   EAPOL: %lu\n\nAccepted = driver accepted.\nEffect requires receiver verification.",attackDefinitions[unsigned(status.attack)].name,status.message,left,status.accepted,status.errors,status.responses,status.eapol);}else{snprintf(line,sizeof(line),"%s\n\n%s\n%lu records / %u KB\n\n%u KB RAM buffer / stops when full.\nSaved files use recon flash.\nBadge and profiles stay separate.",status.recording?"RECORDING":status.saving?"SAVING":"CAPTURE IDLE",status.message,status.records,unsigned(status.bytes/1024),unsigned(reconCaptureCapacity()/1024));}if(strcmp(lv_label_get_text(info),line))lv_label_set_text(info,line);}
void draw(){
 lv_obj_clean(root);info=nullptr;lv_obj_set_style_bg_color(root,lv_color_hex(0x071510),0);
 const char* titles[]={"MISSION MODES","RADIO TOOLKIT","WI-FI ATTACKS","BLE ATTACKS","SELECT NETWORK","SELECT CLIENT","REVIEW TEST","LIVE SESSION","CAPTURE STUDIO","CAPTURE LIBRARY","SAVED CAPTURE","WI-FI ANALYSIS","PROBE REQUESTS","CAPTURE FILTERS","PORTAL LAB","CSI SIGNAL MONITOR","NETWORK TOOLS","SAVED NETWORKS","RECON SESSIONS","RECON RECIPES","ATTACKS"};
 auto* heading=text(root,titles[view],63,40,340,0xc9fc68,font26());lv_obj_set_style_text_align(heading,LV_TEXT_ALIGN_CENTER,0);
 auto* subtitle=text(root,reconModes[unsigned(reconMode())].name,70,78,326,0x9bcfb7,font16());lv_obj_set_style_text_align(subtitle,LV_TEXT_ALIGN_CENTER,0);
 body=lv_obj_create(root);dragon::labui::pos(body,57,110);dragon::labui::size(body,352,274);lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(body,0,0);lv_obj_set_style_pad_all(body,0,0);
 btn(root,"HOME",123,397,102,42,1);btn(root,view==Modes?"TOOLS":"BACK",239,397,102,42,2);
 unsigned n=0;
 if(view==Modes){for(unsigned i=0;i<unsigned(ReconMode::Count);++i)row(reconModes[i].name,reconModes[i].detail,n++,100+i);row("Foxhunt a target","Choose a radar contact / live signal graph",n++,12);row("BLE Lab","GATT discovery / profile emulation",n++,13);row("Radio toolkit","Wi-Fi + BLE tests / capture files",n++,14);}
 else if(view==Attacks){row("Wi-Fi attacks","Select network / client / timed session",n++,20);row("BLE attacks","Pairing advertisements / timed session",n++,21);row("Radio toolkit","Capture / recon sessions / network tools",n++,14);}
 else if(view==Tools){row("Recon recipes","Reviewed steps / timed runs / web editor",n++,67);row("Recon sessions","Persistent observations / RCN1 export",n++,63);row("Connected network","Saved profiles / IP tools / local ping scan",n++,55);row("CSI signal monitor","Passive / selected AP / calibrate first",n++,51);row("Portal / KARMA lab","Local AP / captured probe SSID / timed",n++,47);row("Wi-Fi statistics","Frames / EAPOL / PMKID / SAE",n++,40);row("Probe-request inventory","Requested SSIDs / signal / age",n++,41);row("Wi-Fi attacks","Select target, rate and duration",n++,20);row("BLE attacks","Timed pairing-advertisement tests",n++,21);row("Survey / select network","Passive AP + associated-client inventory",n++,22);row("Capture studio","Wi-Fi PCAP / raw BLE advertisement CSV",n++,23);row("Saved captures","Inspect files / download over USB",n++,24);row("Channel monitor","Channel activity and manual channel lock",n++,25);row("BLE Lab","Discover, save, compare and emulate GATT",n++,13);}
 else if(view==WifiTools||view==BleTools){for(unsigned i=0;i<unsigned(AttackMode::Count);++i){auto& d=attackDefinitions[i];if(d.ble!=(view==BleTools))continue;row(d.name,d.needsClient?"Needs AP + observed client":d.needsAp?"Needs a recently observed AP":d.ble?"10 advertisement updates/sec maximum":"Own source / configured lab SSID",n++,200+i);}}
 else if(view==Networks){apRows=0;int order[ApCapacity];unsigned count=0;for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used&&millis()-aps[i].seen<30000)order[count++]=i;
 for(unsigned i=1;i<count;++i){int x=order[i],j=i;while(j&&aps[order[j-1]].rssi<aps[x].rssi){order[j]=order[j-1];--j;}order[j]=x;}
 row("Refresh list","Wait a complete channel sweep, then refresh",n++,22);
 for(unsigned j=0;j<count;++j){auto& a=aps[order[j]];char mac[18],detail[80];displayMac(a.mac,mac);snprintf(detail,sizeof(detail),"%s / CH%u / %d dBm",mac,a.channel,a.rssi);memcpy(apKeys[apRows],a.mac,6);row(*a.name?displayName(a.name).c_str():"Hidden SSID",detail,n++,300+apRows++);}
 if(!count)text(body,"Listening for AP beacons...",0,78);
 }else if(view==Stations){clientRows=0;row("Use AP / all its clients","AP-directed test; no individual client selected",n++,26);row("Refresh clients","Only clients observed on this AP are shown",n++,27);for(unsigned i=0;clients&&i<ClientCapacity;++i){auto& c=clients[i];if(!c.used||memcmp(c.ap,target.bssid,6)||millis()-c.seen>=30000)continue;char mac[18],detail[80];displayMac(c.mac,mac);snprintf(detail,sizeof(detail),"%d dBm / %lu frames",c.rssi,c.packets);memcpy(clientKeys[clientRows],c.mac,6);row(mac,detail,n++,400+clientRows++);}}
 else if(view==Confirm){char mac[18],client[18];displayMac(target.bssid,mac);displayMac(target.client,client);char summary[420];auto& d=attackDefinitions[unsigned(chosen)];snprintf(summary,sizeof(summary),"%s\n\n%s\nAP: %s\nClient: %s\nCH %u  / destination CH %u\n\n%us / %u %s\n\nStart only for your selected lab.\nHome and Stop end transmission.",d.name,d.ble?"BLE advertising only":target.ssid,target.hasAp?mac:"own lab source",target.hasClient?client:"AP scope / not selected",target.channel,target.destinationChannel,seconds,rate,d.ble?"updates/sec (max 10)":"frames/sec");text(body,summary,3,0,327);int y=360;btn(body,"NETWORK",0,y,164,40,22);btn(body,"CLIENT",174,y,164,40,27);btn(body,"DURATION",0,y+48,164,40,28);btn(body,"RATE",174,y+48,164,40,29);btn(body,"CHANNEL",0,y+96,164,40,30);btn(body,"CSA CHANNEL",174,y+96,164,40,31);btn(body,"START TEST",30,y+152,278,48,32);}
 else if(view==Running){info=text(body,"",3,0,327);updateStatus();btn(body,"STOP NOW",45,300,248,48,33);}
 else if(view==Capture){info=text(body,"",3,0,327);updateStatus();btn(body,"FILTERS",80,248,178,36,42);btn(body,"WI-FI RECORD",0,290,164,44,34);btn(body,"BLE RECORD",174,290,164,44,35);btn(body,"STOP",0,342,164,44,36);btn(body,"SAVE",174,342,164,44,37);btn(body,"SAVED FILES",0,394,164,44,24);btn(body,"DISCARD",174,394,164,44,38);}
 else if(view==Statistics){info=text(body,"",3,0,327,0xd9efe0,font16());updateStatus();btn(body,"PROBE INVENTORY",15,280,308,42,41);}
 else if(view==Probes){row("Refresh inventory","Requested networks are not sender identity",n++,41);for(unsigned i=0;probes&&i<96;++i){auto& p=probes[i];if(!p.used)continue;char mac[18],ssid[33],detail[95];displayMac(p.mac,mac);safeText(ssid,sizeof(ssid),p.ssid,p.length);snprintf(detail,sizeof(detail),"%s / %d dBm / %lus",mac,p.rssi,(millis()-p.seen)/1000);row(p.length?displayName(ssid).c_str():"Wildcard probe",detail,n++,-1);}}
 else if(view==Filters){row(analysis::filterNames[unsigned(captureFilter)],"Tap to cycle packet type",n++,43);row(captureSelected?"Only selected AP":"All observed APs","Tap to switch capture scope",n++,44);row("Hop: all 11 channels","180 ms per channel",n++,45);row("Hop: 1 / 6 / 11","350 ms per channel",n++,46);text(body,"Filter and hop controls apply to Wi-Fi capture/survey. Select a fresh network for AP-only recording. Settings cannot change mid-recording.",4,n*70+5,326,0xa1c4b3,font16());}
 else if(view==Portal){info=text(body,"",3,0,327,0xd9efe0,font16());updateStatus();btn(body,"START 30 SECONDS",0,200,334,44,48);btn(body,"STOP PORTAL",0,252,334,44,49);btn(body,"SAVE SESSION LOG",0,304,334,44,50);text(body,"Choose the AP name, captured probe or custom HTML in web Radio tools. Default: Dragon-Lab, a clearly labeled test page.",4,366,326,0xa1c4b3,font16());}
 else if(view==Csi){info=text(body,"",3,0,327,0xd9efe0,font16());updateStatus();btn(body,"START SELECTED AP",0,280,334,42,52);btn(body,"RECALIBRATE",0,330,334,42,53);btn(body,"STOP CSI",0,380,334,42,54);}
 else if(view==Recipes){info=text(body,"",3,0,327,0xd9efe0,font16());updateStatus();text(body,"Create and review recipes in the web Radio tools. No automatic startup. Home cancels a running recipe and restores the previous radio mode.",3,180,327,0xa1c4b3,font16());btn(body,"STOP RECIPE",0,342,334,44,68);}
 else if(view==Missions){info=text(body,"",3,0,327,0xd9efe0,font16());updateStatus();btn(body,"START WI-FI",0,310,164,42,64);btn(body,"START BLE",174,310,164,42,65);btn(body,"STOP + SAVE",0,362,334,42,66);}
 else if(view==Network){btn(body,"SAVED NETWORKS",0,0,334,42,56);info=text(body,"",3,54,327,0xd9efe0,font16());updateStatus();btn(body,"PING GATEWAY",0,390,334,42,57);btn(body,"SCAN LOCAL /24",0,440,334,42,58);btn(body,"STOP SCAN",0,490,334,42,59);btn(body,"SAVE RESULTS",0,540,334,42,60);btn(body,"DISCONNECT",0,590,334,42,61);btn(body,"CLEAR RESULTS",0,640,334,42,62);text(body,"Add profiles in the web Radio tools. Saved passwords remain on this device. No automatic connection at boot. Session limit: 15 minutes.",4,694,326,0xa1c4b3,font16());}
 else if(view==NetworkProfiles){char names[8][33];unsigned slots[8];unsigned count=networkProfiles(names,slots,8);for(unsigned i=0;i<count;++i)row(names[i],"Tap to join / pauses radar until disconnect",n++,700+slots[i]);if(!count)text(body,"Add a network in the web flasher\'s Radio tools, then return here.",4,8);}
 else if(view==Library){fileCount=fileList(fileNames,fileSizes,32);for(unsigned i=0;i<fileCount;++i){char detail[80];snprintf(detail,sizeof(detail),"%u bytes / tap to inspect",unsigned(fileSizes[i]));row(fileNames[i],detail,n++,600+i);}if(!fileCount)text(body,"No saved captures.\nStop a recording and choose Save.\n\nDownloads are available in the\nGitHub flasher's Radio tools.",3,8);}
 else if(view==FileDetail){text(body,String(fileNames[fileIndex])+"\n\n"+unsigned(fileSizes[fileIndex])+" bytes\n\nUse Radio tools in the web flasher\nto download this capture over USB.\n\nWi-Fi: Radiotap PCAP.\nBLE: raw advertisement CSV.",3,8);btn(body,"DELETE FILE",45,300,248,48,39);}
 redraw=false;updated=millis();
}
void action(lv_event_t* e){int id=(intptr_t)lv_event_get_user_data(e);if(id==1){recipeStop();reconStop();uiShow("radar");return;}
 if(id==2){if(view==Modes)change(Tools);else if(view==Running){stop();change(Tools);}else if(view==Confirm)change(attackDefinitions[unsigned(chosen)].ble?BleTools:WifiTools);else if(view==WifiTools||view==BleTools)change(Attacks);else if(view==Attacks)uiShow("menu");else if(view==Capture||view==Library||view==Statistics||view==Probes||view==Filters||view==Portal||view==Csi||view==Network)change(Tools);else if(view==NetworkProfiles)change(Network);else if(view==Networks||view==Stations)change(Confirm);else if(view==FileDetail)change(Library);else change(Modes);return;}
 if(id>=100&&id<100+unsigned(ReconMode::Count)){if(reconSetMode(ReconMode(id-100)))uiShow("radar");return;}
 if(id>=200&&id<200+unsigned(AttackMode::Count)){chosen=AttackMode(id-200);change(Confirm);return;}
 if(id>=300&&id<300+int(apRows)){for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used&&!memcmp(aps[i].mac,apKeys[id-300],6)){if(selectAp(i))change(Stations);break;}return;}
 if(id>=400&&id<400+int(clientRows)){for(unsigned i=0;clients&&i<ClientCapacity;++i)if(clients[i].used&&!memcmp(clients[i].mac,clientKeys[id-400],6)&&!memcmp(clients[i].ap,target.bssid,6)){if(selectClient(i))change(Confirm);break;}return;}
 if(id>=700&&id<708){networkConnect(id-700);change(Network);return;}
 if(id>=600&&id<600+int(fileCount)){fileIndex=id-600;change(FileDetail);return;}
 switch(id){
 case 12:uiShow("targets");break;case 13:reconStop();uiShow("lab");break;case 14:change(Tools);break;
 case 20:change(WifiTools);break;case 21:change(BleTools);break;case 22:if(!reconOwnsRadio()&&!bleLabOwnsRadio()){reconSetMode(ReconMode::Wifi);change(Networks);}break;
 case 23:change(Capture);break;case 24:change(Library);break;case 25:uiShow("channels");break;
 case 26:target.hasClient=false;change(Confirm);break;case 27:change(Stations);break;
 case 28:seconds=seconds<5?5:seconds<15?15:seconds<30?30:seconds<60?60:5;redraw=true;break;
 case 29:rate=rate<10?10:rate<25?25:rate<50?50:rate<100?100:1;redraw=true;break;
 case 30:if(!target.hasAp)target.channel=target.channel%11+1;redraw=true;break;
 case 31:target.destinationChannel=target.destinationChannel%11+1;redraw=true;break;
 case 32:start(chosen);change(Running);break;case 33:stop();break;
 case 34:captureStart(false);break;case 35:captureStart(true);break;case 36:captureStop();break;case 37:captureSave();break;
 case 67:change(Recipes);break;case 68:recipeStop();break;case 63:change(Missions);break;case 64:case 65:{char name[24];snprintf(name,sizeof(name),"session-%08lx",esp_random());missionStart(id==65,name);break;}case 66:missionStop();break;
 case 62:networkCommand("net clear");break;case 55:change(Network);break;case 56:change(NetworkProfiles);break;case 57:networkPingGateway();break;case 58:networkScanLocal();break;case 59:networkCommand("net scan stop");break;case 60:networkSaveResults();break;case 61:networkStop();break;
 case 51:change(Csi);break;case 52:csiStart();break;case 53:csiCommand("csi calibrate");break;case 54:csiStop();break;
 case 47:change(Portal);break;case 48:seconds=30;portalStart();break;case 49:portalStop();break;case 50:portalCommand("portal save");break;
 case 40:change(Statistics);break;case 41:change(Probes);break;case 42:change(Filters);break;
 case 43:setCaptureFilter((unsigned(captureFilter)+1)%unsigned(analysis::Filter::Count),captureSelected);redraw=true;break;
 case 44:setCaptureFilter(unsigned(captureFilter),!captureSelected);redraw=true;break;
 case 45:reconCommand("recon hop 2047 180");break;case 46:reconCommand("recon hop 1057 350");break;
 case 39:{static const char* buttons[]={"Keep","Delete",""};auto* box=lv_msgbox_create(root,"Delete saved capture?",fileNames[fileIndex],buttons,true);dragon::labui::width(box,330);lv_obj_center(box);lv_obj_add_event_cb(box,[](lv_event_t* ev){auto* box=lv_event_get_current_target(ev);if(lv_msgbox_get_active_btn(box)==1){if(fileRemove(fileNames[fileIndex]))change(Library);else say("Could not delete capture");}lv_msgbox_close(box);},LV_EVENT_VALUE_CHANGED,nullptr);break;}
 case 38:{static const char* buttons[]={"Keep","Discard",""};auto* box=lv_msgbox_create(root,"Discard capture?","Remove the unsaved RAM recording? Saved files stay intact.",buttons,true);dragon::labui::width(box,330);lv_obj_center(box);lv_obj_add_event_cb(box,[](lv_event_t* ev){auto* box=lv_event_get_current_target(ev);if(lv_msgbox_get_active_btn(box)==1)reconCommand("recon capture discard");lv_msgbox_close(box);},LV_EVENT_VALUE_CHANGED,nullptr);break;}
 }
}
}
void reconView(void* p,bool tools){root=(lv_obj_t*)p;view=pageRequested?requested:tools?Tools:Modes;pageRequested=false;redraw=true;draw();}
void reconPage(const String& name){requested=name=="attacks"?Attacks:name=="recipes"?Recipes:name=="missions"?Missions:name=="network"?Network:name=="csi"?Csi:name=="portal"?Portal:name=="analysis"?Statistics:name=="probes"?Probes:name=="filters"?Filters:Capture;pageRequested=true;}
void reconViewLoop(){if(!root)return;if(pageRequested){view=requested;redraw=true;pageRequested=false;}if(redraw)draw();else if(millis()-updated>=300){updated=millis();if(view==Running||view==Capture||view==Statistics||view==Portal||view==Csi||view==Network||view==Missions||view==Recipes)updateStatus();}}
void reconLeave(){reconStop();root=body=info=nullptr;}
#endif
