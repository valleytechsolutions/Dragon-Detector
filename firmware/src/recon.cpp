// Radio modes and bounded lab sessions. Original MIT adapter; packet builders
// originate in Valleytech HackerPhone. Marauder MIT notice is retained.
#include "recon_internal.h"
#include "background_radio.h"
#include "portal.h"
#include "csi_monitor.h"
#include "network_tools.h"
#ifdef BOARD_AMOLED_175C
#include "app.h"
#include "ble_lab.h"
#include "ble_insights.h"
#include "packet_helpers.h"
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <mbedtls/ecp.h>
#include <atomic>
using namespace dragon;
using namespace dragon::tx;
namespace dragon::toolkit {
analysis::Counters counters;analysis::Probe* probes=nullptr;
analysis::Filter captureFilter=analysis::Filter::All;bool captureSelected=false;uint16_t hopMask=0x7ff,hopDwell=180;
static analysis::Filter recordingFilter=analysis::Filter::All;static bool recordingSelected=false;static uint8_t recordingAp[6]{};
Ap* aps=nullptr;Client* clients=nullptr;Status status;AttackTarget target;
uint16_t seconds=15,rate=10;
static ReconMode mode=ReconMode::Radar;static Preferences prefs;
static std::atomic<bool> wifiRecording{false};
static bool captureModeOwned=false;static ReconMode capturePreviousMode=ReconMode::Radar;static uint8_t capturePreviousLock=0;
static bool resumeRadio=false,captureEapol=false;
static uint8_t previousLock=0,ownMac[6]{},saeMaterial[96]{},saeClient[6]{},token[128]{};
static uint8_t customAdvertisement[31]{};static size_t customLength=0;
static size_t tokenSize=0;static uint16_t sequence=0;static uint32_t lastTx=0,captureAt=0;
static AttackTarget running;static uint16_t runSeconds=15,runRate=10;
static uint8_t* packet=nullptr;static uint8_t* beacon=nullptr;static size_t beaconLength=0;
static std::atomic<int> crypto{0};
constexpr size_t CaptureCapacity=2*1024*1024;
static uint8_t* capture=nullptr;static bool captureDirty=false;
static File saveFile;static String savePath;static size_t saveOffset=0;static uint32_t saveCrc=0xffffffffu;
static char ssids[32][33]{};static unsigned ssidCount=0;
void say(const char* s){strlcpy(status.message,s,sizeof(status.message));}
static bool busy(){return status.active||status.saving||portalActive()||csiActive()||networkActive()||bleLabOwnsRadio();}
static bool saveList(){auto* fs=bleLabStorage();if(!fs||!bleLabStorageSpace(8192))return false;fs->mkdir("/lists");File f=fs->open("/lists/ssids.part",FILE_WRITE);if(!f)return false;bool ok=true;
 for(unsigned i=0;i<ssidCount&&ok;++i)ok=f.print(ssids[i])==strlen(ssids[i])&&f.write('\n')==1;
 f.flush();f.close();ok=ok&&fs->rename("/lists/ssids.part","/lists/ssids.txt");if(!ok)fs->remove("/lists/ssids.part");return ok;
}
static void loadList(){auto* fs=bleLabStorage();if(!fs)return;File f=fs->open("/lists/ssids.txt");if(!f)return;if(f.size()>32*33){f.close();say("Saved SSID list is invalid");return;}unsigned len=0;bool ok=true;
 while(f.available()&&ok){int c=f.read();if(c=='\n'){if(!len||ssidCount>=32){ok=false;break;}ssids[ssidCount++][len]=0;len=0;}else if(c<32||c>126||len>=32||ssidCount>=32)ok=false;else ssids[ssidCount][len++]=char(c);}
 f.close();if(!ok||len){ssidCount=0;say("Saved SSID list is invalid");}
}
bool listAdd(const char* name){size_t n=strlen(name);if(busy()||!n||n>32||ssidCount>=32)return false;for(size_t i=0;i<n;++i)if(name[i]<32||name[i]>126)return false;for(unsigned i=0;i<ssidCount;++i)if(!strcmp(ssids[i],name))return true;
 strlcpy(ssids[ssidCount++],name,33);if(saveList())return true;--ssidCount;return false;
}
bool listClear(){if(busy())return false;unsigned old=ssidCount;ssidCount=0;if(saveList())return true;ssidCount=old;return false;}
unsigned listCount(){return ssidCount;}
bool setCaptureFilter(unsigned f,bool selectedOnly){if(f>=unsigned(analysis::Filter::Count)||busy()||status.recording)return false;captureFilter=analysis::Filter(f);captureSelected=selectedOnly;return true;}
bool selectAp(unsigned i){if(busy()||!aps||i>=ApCapacity||!aps[i].used||millis()-aps[i].seen>=30000)return false;auto& a=aps[i];target={};target.hasAp=true;memcpy(target.bssid,a.mac,6);target.channel=a.channel;target.destinationChannel=a.channel==6?1:6;strlcpy(target.ssid,a.name,33);return true;}
bool selectClient(unsigned i){if(busy()||!clients||i>=ClientCapacity||!clients[i].used||millis()-clients[i].seen>=30000||!target.hasAp||memcmp(clients[i].ap,target.bssid,6))return false;memcpy(target.client,clients[i].mac,6);target.hasClient=true;return true;}
static Ap* selected(){if(aps&&target.hasAp)for(unsigned i=0;i<ApCapacity;++i)if(aps[i].used&&!memcmp(aps[i].mac,target.bssid,6))return &aps[i];return nullptr;}
static bool cloneMode(AttackMode m){return m==AttackMode::CloneBeacon||m==AttackMode::ApSpam||m==AttackMode::ChannelSwitch||m==AttackMode::Quiet;}
bool available(AttackMode m,char* why,size_t cap){
 const char* error=nullptr;auto* ap=selected();
 if(unsigned(m)>=unsigned(AttackMode::Count))error="Unknown tool";
 else if(busy()||crypto==1)error="Stop active tool / wait for cleanup";
 else if(status.recording)error="Stop recording first; keep or save capture";
 else if(!packet||!beacon)error="Packet memory unavailable";
 else if(!validAttackTarget(m,target))error="Select required AP / client / channel";
 else if(attackDefinitions[unsigned(m)].needsAp&&(!ap||millis()-ap->seen>=30000||ap->channel!=target.channel))error="AP is stale; survey and reselect it";
 else if(cloneMode(m)&&(!ap||!ap->length))error="Scan a complete beacon first";
 else if(m==AttackMode::SsidList&&!ssidCount)error="Add SSIDs in web radio tools";
 else if(m==AttackMode::CustomBle&&!customLength)error="Set a valid legacy payload in web Radio tools";
 else if(m==AttackMode::ActiveEapol&&captureDirty)error="Save or discard the current capture first";
 else if(seconds<1||seconds>60||rate<1||rate>100)error="Duration 1-60s / rate 1-100 per second";
 if(!error&&attackDefinitions[unsigned(m)].needsAp&&target.hasClient){bool fresh=false;for(unsigned i=0;clients&&i<ClientCapacity;++i)if(clients[i].used&&!memcmp(clients[i].ap,target.bssid,6)&&!memcmp(clients[i].mac,target.client,6)&&millis()-clients[i].seen<30000)fresh=true;if(!fresh)error="Client is stale; survey and reselect it";}
 if(why&&cap)strlcpy(why,error?error:"Ready; transmitted frames do not prove effect",cap);return !error;
}
static int randomBytes(void*,unsigned char* p,size_t n){esp_fill_random(p,n);return 0;}
static void prepareSae(void*){
 mbedtls_ecp_group g;mbedtls_ecp_point q;mbedtls_mpi d;mbedtls_ecp_group_init(&g);mbedtls_ecp_point_init(&q);mbedtls_mpi_init(&d);uint8_t encoded[65];size_t n=0;
 bool ok=mbedtls_ecp_group_load(&g,MBEDTLS_ECP_DP_SECP256R1)==0&&mbedtls_ecp_gen_keypair(&g,&d,&q,randomBytes,nullptr)==0&&mbedtls_mpi_write_binary(&d,saeMaterial,32)==0&&mbedtls_ecp_point_write_binary(&g,&q,MBEDTLS_ECP_PF_UNCOMPRESSED,&n,encoded,65)==0&&n==65;
 if(ok)memcpy(saeMaterial+32,encoded+1,64);mbedtls_mpi_free(&d);mbedtls_ecp_point_free(&q);mbedtls_ecp_group_free(&g);crypto=ok?2:-1;vTaskDelete(nullptr);
}
static bool captureInit(bool ble,bool eapol){
 if(captureDirty){say("Unsaved capture: Save or Discard first");return false;}
 if(!capture)capture=(uint8_t*)ps_malloc(CaptureCapacity);if(!capture){say("Capture memory unavailable");return false;}
 status.bytes=0;status.records=0;captureEapol=eapol;recordingFilter=captureFilter;recordingSelected=captureSelected;memcpy(recordingAp,target.bssid,6);status.bleCapture=ble;
 if(ble){const char* header="uptime_ms,address,address_type,rssi,advertisement_length,payload_length,payload_hex\n";status.bytes=strlen(header);memcpy(capture,header,status.bytes);}else{pcapHeader(capture);status.bytes=24;}
 captureAt=millis();status.recording=true;wifiRecording=!ble;captureDirty=true;return true;
}
bool captureStart(bool ble,bool eapol){if(busy()||status.recording||captureDirty){say("Stop and save or discard the current capture first");return false;}
 if(!ble&&captureSelected&&(!target.hasAp||!selected()||millis()-selected()->seen>=30000||selected()->channel!=target.channel)){say("Selected-AP capture requires a fresh selected network");return false;}
 if(!captureInit(ble,eapol))return false;capturePreviousMode=mode;capturePreviousLock=channelLock;captureModeOwned=true;mode=ble?ReconMode::BleCapture:ReconMode::WifiCapture;channelLock=!ble&&captureSelected?target.channel:0;trackedContact=-1;radioApplyMode();say("Recording to RAM; Stop, then Save to recon flash");return true;
}
void captureStop(){bool was=status.recording;status.recording=false;wifiRecording=false;if(captureModeOwned){captureModeOwned=false;mode=capturePreviousMode;channelLock=capturePreviousLock;radioApplyMode();}if(was)say("Capture stopped; Save to retain it across restart");}
static bool appendCapture(const uint8_t* data,size_t n){if(n>CaptureCapacity-status.bytes){captureStop();say("2 MB capture full; stopped without overwriting");return false;}memcpy(capture+status.bytes,data,n);status.bytes+=n;++status.records;return true;}
bool start(AttackMode m){
 char why[128];if(!available(m,why,sizeof(why))){if(!status.active&&unsigned(m)<unsigned(AttackMode::Count)){status.attack=m;status.accepted=status.errors=status.responses=status.eapol=0;status.error=0;}say(why);return false;}
 running=target;runSeconds=seconds;runRate=rate;beaconLength=0;if(auto* a=selected()){beaconLength=a->length;memcpy(beacon,a->beacon,beaconLength);}
 if(m==AttackMode::ActiveEapol&&!captureInit(false,true))return false;
 resumeRadio=radioEnabled;previousLock=channelLock;setRadio(false);
 status.active=true;status.attack=m;status.accepted=status.errors=status.responses=status.eapol=0;status.error=0;sequence=0;tokenSize=0;
 bool ble=attackDefinitions[unsigned(m)].ble;esp_err_t err=ESP_OK;
 if(!ble){err=esp_wifi_set_mode(WIFI_MODE_STA);if(err==ESP_OK)err=esp_wifi_get_mac(WIFI_IF_STA,ownMac);if(err==ESP_OK)err=esp_wifi_set_channel(running.channel,WIFI_SECOND_CHAN_NONE);if(err==ESP_OK)err=esp_wifi_set_promiscuous(true);}
 else{auto* a=NimBLEDevice::getAdvertising();a->stop();a->reset();a->setConnectableMode(BLE_GAP_CONN_MODE_NON);a->setAdvertisingInterval(160);a->enableScanResponse(false);}
 if(err!=ESP_OK){stop();status.error=err;say("Radio setup failed");return false;}
 if(m==AttackMode::SaeCommit){esp_fill_random(saeClient,6);saeClient[0]=(saeClient[0]&0xfc)|2;crypto=1;if(xTaskCreatePinnedToCore(prepareSae,"dd-sae",6144,nullptr,1,nullptr,0)!=pdPASS){crypto=-1;stop();say("SAE worker unavailable");return false;}}
 status.started=millis();lastTx=status.started-1000;say("Running timed test / Home stops and restores radar");return true;
}
void stop(){
 if(!status.active)return;bool ble=attackDefinitions[unsigned(status.attack)].ble;
 if(ble){auto* a=NimBLEDevice::getAdvertising();a->stop();a->reset();NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);}
 else{esp_wifi_set_promiscuous(false);esp_wifi_set_mode(WIFI_MODE_NULL);}
 if(status.attack==AttackMode::ActiveEapol)captureStop();
 status.active=false;channelLock=previousLock;setRadio(resumeRadio);say("Stopped / previous radio mode restored");
}
static void tick(uint32_t now){
 if(!status.active)return;if(now-status.started>=uint32_t(runSeconds)*1000){stop();return;}
 auto m=status.attack;bool ble=attackDefinitions[unsigned(m)].ble;
 uint32_t interval=ble?std::max<uint32_t>(100,1000/runRate):1000/runRate;if(now-lastTx<interval)return;lastTx=now;
 uint8_t entropy[96],source[6];esp_fill_random(entropy,sizeof(entropy));memcpy(source,ownMac,6);
 if(m==AttackMode::RandomBeacons||m==AttackMode::Probe||m==AttackMode::Authentication||m==AttackMode::ApSpam){memcpy(source,entropy+48,6);source[0]=(source[0]&0xfc)|2;}
 if(m==AttackMode::SaeCommit){if(crypto==1)return;if(crypto!=2){stop();say("SAE key generation failed");return;}memcpy(entropy,saeMaterial,96);memcpy(source,saeClient,6);}
 if(m==AttackMode::SsidList)strlcpy(running.ssid,ssids[sequence%ssidCount],33);
 size_t n=m==AttackMode::CustomBle?copyBleAdvertisement(packet,1200,customAdvertisement,customLength):cloneMode(m)?cloneBeacon(packet,1200,beacon,beaconLength,sequence++,m==AttackMode::ChannelSwitch?37:m==AttackMode::Quiet?40:0,running.destinationChannel):ble?buildBleAttack(packet,1200,m,entropy):buildWifiAttack(packet,1200,m,running,source,sequence++,entropy);
 if(n&&m==AttackMode::ApSpam){memcpy(packet+10,source,6);memcpy(packet+16,source,6);}
 if(n==128&&m==AttackMode::SaeCommit&&tokenSize){memmove(packet+32+tokenSize,packet+32,96);memcpy(packet+32,token,tokenSize);n+=tokenSize;}
 int result=ESP_ERR_INVALID_ARG;
 if(n&&ble){auto* a=NimBLEDevice::getAdvertising();a->stop();NimBLEAdvertisementData data;bool identity=false;
  if(bleRotatesAddress(m)){uint8_t address[6];memcpy(address,entropy+64,6);address[0]|=0xc0;identity=NimBLEDevice::setOwnAddr(NimBLEAddress(address,BLE_ADDR_RANDOM))&&NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);}
  else identity=NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  bool ok=identity&&data.addData(packet,n)&&a->setAdvertisementData(data)&&a->start(interval+20);result=ok?ESP_OK:ESP_FAIL;}
 else if(n){if(packet[0]==0x80){uint64_t stamp=esp_timer_get_time();for(unsigned i=0;i<8;++i)packet[24+i]=stamp>>(8*i);}result=esp_wifi_80211_tx(WIFI_IF_STA,packet,n,false);}
 if(result==ESP_OK)++status.accepted;else{++status.errors;status.error=result;stop();snprintf(status.message,sizeof(status.message),"Driver rejected frame (%d); stopped",result);}
}
static bool safeFile(const char* p){size_t n=strlen(p);if(n<6||n>=48||strstr(p,".."))return false;for(size_t i=0;i<n;++i)if(!isalnum(uint8_t(p[i]))&&p[i]!='-'&&p[i]!='.'&&p[i]!='_')return false;return true;}
static bool serialWrite(const char* p,size_t n){RadioTransfer transfer;uint32_t at=millis();while(n){size_t k=std::min<size_t>(128,std::min(n,size_t(Serial.availableForWrite())));if(k){k=Serial.write((const uint8_t*)p,k);p+=k;n-=k;at=millis();}else if(millis()-at>5000)return false;radioTransferTick();delay(1);}return true;}
unsigned fileList(char names[][48],size_t* sizes,unsigned max){auto* fs=bleLabStorage();if(!fs||status.saving)return 0;File dir=fs->open("/captures");unsigned n=0;for(File f=dir.openNextFile();f&&n<max;f=dir.openNextFile()){String name=f.name();if(!f.isDirectory()&&(name.endsWith(".pcap")||name.endsWith(".csv"))){strlcpy(names[n],name.c_str(),48);sizes[n++]=f.size();}f.close();}dir.close();return n;}
bool fileRemove(const char* name){auto* fs=bleLabStorage();return !busy()&&safeFile(name)&&fs&&fs->remove(String("/captures/")+name);}
bool fileExport(const char* name){auto* fs=bleLabStorage();if(busy()||status.recording||!fs||!safeFile(name))return false;File f=fs->open(String("/captures/")+name);if(!f||f.size()>CaptureCapacity){f.close();return false;}
 RadioTransfer transfer;Serial.printf("RECON_FILE %u\n",unsigned(f.size()));uint8_t data[128];size_t offset=0;uint32_t crc=0xffffffffu;bool ok=true;
 while(f.available()){size_t n=f.read(data,sizeof(data));if(!n){ok=false;break;}char line[300];int k=snprintf(line,sizeof(line),"RECON_DATA %u ",unsigned(offset));for(size_t i=0;i<n;++i){snprintf(line+k,3,"%02x",data[i]);k+=2;}line[k++]='\n';if(!serialWrite(line,k)){ok=false;break;}crc=dragon::crc32(data,n,crc);offset+=n;}f.close();Serial.printf("RECON_END %u %08lx\n",unsigned(offset),crc^0xffffffffu);return ok;}
bool captureSave(){
 if(busy()||status.recording||!captureDirty||!capture||!status.records){say("Stop a nonempty capture before saving");return false;}
 auto* fs=bleLabStorage();if(!fs){say("Recon flash unavailable; initialize it in BLE Lab");return false;}
 // FS capacity is checked by the storage adapter; no format or overwrite.
 if(!bleLabStorageSpace(status.bytes+65536)){say("Recon storage full; download and remove old captures");return false;}
 File dir=fs->open("/captures");unsigned saved=0;for(File f=dir.openNextFile();f;f=dir.openNextFile()){String name=f.name();if(!f.isDirectory()&&(name.endsWith(".pcap")||name.endsWith(".csv")))++saved;f.close();}dir.close();
 if(saved>=32){say("32 capture slots full; download and delete old files");return false;}
 fs->mkdir("/captures");char name[48];snprintf(name,sizeof(name),"/captures/%s-%08lx.%s",status.bleCapture?"ble":"wifi",esp_random(),status.bleCapture?"csv":"pcap");savePath=name;
 if(fs->exists(savePath)||fs->exists(savePath+".part")){say("Name collision; retry Save");return false;}saveFile=fs->open(savePath+".part",FILE_WRITE);if(!saveFile){say("Cannot create capture file");return false;}
 status.saving=true;saveOffset=0;saveCrc=0xffffffffu;say("Saving capture to recon flash...");return true;
}
static void saveTick(){if(!status.saving)return;size_t n=std::min<size_t>(4096,status.bytes-saveOffset);bool ok=saveFile.write(capture+saveOffset,n)==n;if(ok){saveCrc=dragon::crc32(capture+saveOffset,n,saveCrc);saveOffset+=n;}
 if(!ok||saveOffset==status.bytes){saveFile.flush();saveFile.close();auto* fs=bleLabStorage();ok=ok&&fs&&fs->rename(savePath+".part",savePath);if(ok){captureDirty=false;snprintf(status.message,sizeof(status.message),"Saved %s / CRC %08lx",savePath.c_str()+10,saveCrc^0xffffffffu);}else{if(fs)fs->remove(savePath+".part");say("Save failed; capture remains in RAM");}status.saving=false;}
}
}
using namespace dragon::toolkit;
ReconMode reconMode(){return mode;}
uint8_t reconRadio(){return reconModes[unsigned(mode)].radio;}
bool reconAll(){return reconModes[unsigned(mode)].all;}
bool reconAccepts(Family family,bool ble){return modeAccepts(mode,family,ble);}
bool reconStorageBusy(){return status.saving;}
bool reconOwnsRadio(){return status.active||portalActive()||csiActive()||networkActive();}
bool reconRecording(){return wifiRecording.load();}
bool reconSetMode(ReconMode next){if(!validMode(unsigned(next))||busy())return false;if(status.recording&&next!=mode)captureStop();mode=next;prefs.putUChar("mode",unsigned(mode));trackedContact=-1;channelLock=0;radioApplyMode();return true;}
void reconStop(){networkStop();csiStop();portalStop();stop();captureStop();}
uint16_t reconHopDwell(){return hopDwell;}
uint8_t reconNextChannel(uint8_t current){return analysis::nextChannel(current,hopMask);}
void reconBegin(){networkBegin();prefs.begin("dd-recon",false);loadList();unsigned m=prefs.getUChar("mode",0);mode=validMode(m)?ReconMode(m):ReconMode::Radar;probes=(analysis::Probe*)ps_calloc(96,sizeof(analysis::Probe));aps=(Ap*)ps_calloc(ApCapacity,sizeof(Ap));clients=(dragon::toolkit::Client*)ps_calloc(ClientCapacity,sizeof(dragon::toolkit::Client));packet=(uint8_t*)ps_malloc(1200);beacon=(uint8_t*)ps_malloc(1024);radioApplyMode();}
void reconLoop(){networkLoop();csiLoop();portalLoop();tick(millis());saveTick();if(status.recording&&millis()-captureAt>=15*60*1000){captureStop();say("15-minute capture limit; Save to retain");}}
void reconWifi(const uint8_t* p,size_t n,size_t original,uint64_t us,int rssi,uint8_t channel){
 if(status.saving)return;
 auto frame=dragon::parseWifi(p,n);bool eapol=frame.valid&&frame.eapol;
 if(!status.active){counters.observe(p,n,frame);analysis::observeProbe(probes,96,frame,rssi,channel,millis());}
 if(status.active&&targetsFrame(p,n,running.bssid)){if(eapol)++status.eapol;if(n>=30&&p[0]==0xb0&&!(p[1]&0x40)){++status.responses;if(status.attack==AttackMode::SaeCommit){size_t t=saeChallenge(p,n,running.bssid,saeClient,token,sizeof(token));if(t)tokenSize=t;}}}
 if(status.recording&&!status.bleCapture&&(captureEapol?(eapol&&targetsFrame(p,n,running.bssid)):(analysis::accepts(recordingFilter,frame)&&(!recordingSelected||targetsFrame(p,n,recordingAp))))){uint8_t record[1053];size_t z=pcapRecord(record,sizeof(record),p,n,original,us,rssi,channel);if(z)appendCapture(record,z);}
 if(status.active||!aps||!clients)return;uint32_t now=millis();
 if(frame.valid&&frame.beacon&&beaconValid(p,n)&&n==original&&unicastAddress(p+10)){
  unsigned slot=0;uint32_t oldest=0;for(unsigned i=0;i<ApCapacity;++i){if(!aps[i].used||!memcmp(aps[i].mac,p+10,6)){slot=i;break;}if(now-aps[i].seen>=oldest){oldest=now-aps[i].seen;slot=i;}}
  auto& a=aps[slot];if(!a.used||memcmp(a.mac,p+10,6))a={};a.used=true;memcpy(a.mac,p+10,6);a.channel=beaconChannel(p,n,channel);a.security=frame.security;a.rssi=rssi;a.seen=now;++a.packets;strlcpy(a.name,frame.name,33);a.length=n;memcpy(a.beacon,p,n);
 }
 uint8_t ap[6],client[6];if(stationPair(p,n,ap,client)){unsigned slot=0;uint32_t oldest=0;for(unsigned i=0;i<ClientCapacity;++i){if(!clients[i].used||(!memcmp(clients[i].mac,client,6)&&!memcmp(clients[i].ap,ap,6))){slot=i;break;}if(now-clients[i].seen>=oldest){oldest=now-clients[i].seen;slot=i;}}
 auto& c=clients[slot];if(!c.used||memcmp(c.mac,client,6)||memcmp(c.ap,ap,6))c={};c.used=true;memcpy(c.mac,client,6);memcpy(c.ap,ap,6);c.rssi=rssi;c.seen=now;++c.packets;}
}
void reconBle(const uint8_t* mac,uint8_t type,int rssi,const uint8_t* p,size_t n,size_t boundary,uint32_t at){if(!status.recording||!status.bleCapture||n>1024||boundary>n)return;char line[2200],address[18];macText(mac,address);int used=snprintf(line,sizeof(line),"%lu,%s,%u,%d,%u,%u,",at,address,type,rssi,unsigned(boundary),unsigned(n));for(size_t i=0;i<n;++i){snprintf(line+used,3,"%02x",p[i]);used+=2;}line[used++]='\n';appendCapture((const uint8_t*)line,used);}
static bool number(const String& s,unsigned& n){if(!s.length())return false;n=0;for(unsigned i=0;i<s.length();++i){if(!isDigit(s[i])||n>100000)return false;n=n*10+s[i]-'0';}return true;}
bool reconCommand(const String& c){
 if(networkCommand(c)||portalCommand(c)||csiCommand(c))return true;
 if(!c.startsWith("mode ")&&!c.startsWith("tool ")&&!c.startsWith("recon "))return false;unsigned n=0;
 if(c=="mode list"){for(unsigned i=0;i<unsigned(ReconMode::Count);++i)Serial.printf("MODE %u %s / %s\n",i,reconModes[i].name,reconModes[i].detail);Serial.println("RECON_OK");}
 else if(c=="mode status"||c=="tool status")Serial.printf("MODE_STATUS mode=%u radio=%u active=%u tool=%u accepted=%lu errors=%lu error=%d responses=%lu eapol=%lu recording=%u records=%lu bytes=%u saving=%u ssids=%u message=%s\n",unsigned(mode),reconRadio(),status.active,unsigned(status.attack),status.accepted,status.errors,status.error,status.responses,status.eapol,status.recording,status.records,unsigned(status.bytes),status.saving,ssidCount,status.message);
 else if(c.startsWith("mode set ")&&number(c.substring(9),n)&&validMode(n))Serial.println(reconSetMode(ReconMode(n))?"RECON_OK":"RECON_ERROR busy");
 else if(c=="recon stats"){Serial.printf("RECON_STATS frames=%lu beacons=%lu probes=%lu disconnect=%lu eapol=%lu pwnagotchi=%lu detections=%lu protected=%lu retries=%lu m1=%lu m2=%lu m3=%lu m4=%lu other_eapol=%lu pmkid=%lu sae=%lu\n",counters.frames,counters.beacons,counters.probes,counters.disconnect,counters.eapol,counters.peers,counters.detections,counters.protectedFrames,counters.retries,counters.keyMessages[1],counters.keyMessages[2],counters.keyMessages[3],counters.keyMessages[4],counters.keyMessages[0],counters.pmkid,counters.sae);Serial.println("RECON_OK");}
 else if(c=="recon stats clear"&&!busy()&&!status.recording){counters={};if(probes)memset(probes,0,96*sizeof(*probes));Serial.println("RECON_OK");}
 else if(c=="recon probes"){for(unsigned i=0;probes&&i<96;++i){auto& p=probes[i];if(!p.used)continue;char mac[18];macText(p.mac,mac);char line[200];int k=snprintf(line,sizeof(line),"RECON_PROBE %s ch=%u rssi=%d age=%lu count=%lu ssid_hex=",mac,p.channel,p.rssi,(millis()-p.seen)/1000,p.count);for(unsigned j=0;j<p.length;++j)k+=snprintf(line+k,sizeof(line)-k,"%02x",p.ssid[j]);line[k++]='\n';serialWrite(line,k);}Serial.println("RECON_OK");}
 else if(c=="recon filter"){Serial.printf("RECON_FILTER kind=%u selected=%u mask=%u dwell=%u\n",unsigned(captureFilter),captureSelected,hopMask,hopDwell);Serial.println("RECON_OK");}
 else if(c.startsWith("recon filter ")){unsigned kind,selected;char tail;bool ok=sscanf(c.c_str(),"recon filter %u %u %c",&kind,&selected,&tail)==2&&selected<2&&setCaptureFilter(kind,selected);Serial.println(ok?"RECON_OK":"RECON_ERROR filter or busy");}
 else if(c.startsWith("recon hop ")&&!busy()&&!status.recording){unsigned mask,dwell;char tail;bool ok=sscanf(c.c_str(),"recon hop %u %u %c",&mask,&dwell,&tail)==2&&mask&&mask<=0x7ff&&dwell>=80&&dwell<=2000;if(ok){hopMask=mask;hopDwell=dwell;channelLock=0;}Serial.println(ok?"RECON_OK":"RECON_ERROR hop mask/dwell");}
 else if(c=="tool list"){for(unsigned i=0;i<unsigned(AttackMode::Count);++i){auto& d=attackDefinitions[i];Serial.printf("TOOL %u ble=%u ap=%u client=%u %s\n",i,d.ble,d.needsAp,d.needsClient,d.name);}Serial.println("RECON_OK");}
 else if(c.startsWith("tool payload ")&&!busy()){uint8_t data[31],validated[31];size_t count=0;bool ok=lab::decodeHex(c.substring(13).c_str(),data,sizeof(data),count)&&copyBleAdvertisement(validated,sizeof(validated),data,count);if(ok){memcpy(customAdvertisement,validated,count);customLength=count;}Serial.println(ok?"RECON_OK custom advertisement stored in RAM; select BLE custom advertisement to transmit":"RECON_ERROR complete 1-31 byte AD payload required");}
 else if(c=="tool payload"){Serial.print("RECON_PAYLOAD ");for(size_t i=0;i<customLength;++i)Serial.printf("%02x",customAdvertisement[i]);Serial.println();Serial.println("RECON_OK");}
 else if(c=="tool aps"){for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used){char mac[18];macText(aps[i].mac,mac);Serial.printf("TOOL_AP %u %s ch=%u rssi=%d age=%lu security=%u name=%s\n",i,mac,aps[i].channel,aps[i].rssi,(millis()-aps[i].seen)/1000,aps[i].security,aps[i].name);}Serial.println("RECON_OK");}
 else if(c=="tool clients"){for(unsigned i=0;clients&&i<ClientCapacity;++i)if(clients[i].used&&target.hasAp&&!memcmp(clients[i].ap,target.bssid,6)){char mac[18];macText(clients[i].mac,mac);Serial.printf("TOOL_CLIENT %u %s rssi=%d age=%lu\n",i,mac,clients[i].rssi,(millis()-clients[i].seen)/1000);}Serial.println("RECON_OK");}
 else if(c=="tool target"){char ap[18],client[18];macText(target.bssid,ap);macText(target.client,client);Serial.printf("RECON_TARGET ap=%s client=%s channel=%u destination=%u name=%s\n",target.hasAp?ap:"none",target.hasClient?client:"AP-scope",target.channel,target.destinationChannel,target.ssid);Serial.println("RECON_OK");}
 else if(c.startsWith("tool ap ")){char address[18],extra;unsigned ch;uint8_t mac[6];bool ok=false;if(sscanf(c.c_str(),"tool ap %17s %u %c",address,&ch,&extra)==2&&parseMac(address,mac))for(unsigned i=0;aps&&i<ApCapacity;++i)if(aps[i].used&&aps[i].channel==ch&&!memcmp(aps[i].mac,mac,6)){ok=selectAp(i);break;}Serial.println(ok?"RECON_OK":"RECON_ERROR AP not fresh / channel changed");}
 else if(c.startsWith("tool client ")&&c!="tool client clear"){uint8_t mac[6];bool ok=false;if(parseMac(c.substring(12).c_str(),mac))for(unsigned i=0;clients&&i<ClientCapacity;++i)if(clients[i].used&&!memcmp(clients[i].ap,target.bssid,6)&&!memcmp(clients[i].mac,mac,6)){ok=selectClient(i);break;}Serial.println(ok?"RECON_OK":"RECON_ERROR client not fresh / wrong AP");}
 else if(c=="tool client clear"&&!busy()){target.hasClient=false;memset(target.client,0,6);Serial.println("RECON_OK");}
 else if(c.startsWith("tool seconds ")&&number(c.substring(13),n)&&n>=1&&n<=60&&!busy()){seconds=n;Serial.println("RECON_OK");}
 else if(c.startsWith("tool rate ")&&number(c.substring(10),n)&&n>=1&&n<=100&&!busy()){rate=n;Serial.println("RECON_OK");}
 else if(c.startsWith("tool channel ")&&number(c.substring(13),n)&&n>=1&&n<=11&&!busy()&&!target.hasAp){target.channel=n;Serial.println("RECON_OK");}
 else if(c=="tool target clear"&&!busy()){target={};Serial.println("RECON_OK");}
 else if(c.startsWith("tool destination ")&&number(c.substring(17),n)&&n>=1&&n<=11&&!busy()){target.destinationChannel=n;Serial.println("RECON_OK");}
 else if(c.startsWith("tool name ")&&!busy()&&c.length()>10&&c.length()<=42){strlcpy(target.ssid,c.substring(10).c_str(),33);Serial.println("RECON_OK");}
 else if(c.startsWith("tool ssid add "))Serial.println(listAdd(c.substring(14).c_str())?"RECON_OK":"RECON_ERROR playlist / invalid name");
 else if(c=="tool ssid clear")Serial.println(listClear()?"RECON_OK":"RECON_ERROR storage or busy");
 else if(c=="tool ssid list"){for(unsigned i=0;i<ssidCount;++i)Serial.printf("RECON_SSID %u %s\n",i,ssids[i]);Serial.println("RECON_OK");}
 else if(c.startsWith("tool start ")&&number(c.substring(11),n)&&n<unsigned(AttackMode::Count)){bool ok=start(AttackMode(n));Serial.printf("RECON_%s %s\n",ok?"OK":"ERROR",status.message);}
 else if(c=="tool stop"){reconStop();Serial.println("RECON_OK");}
 else if(c=="recon capture wifi"||c=="recon capture ble")Serial.println(captureStart(c.endsWith("ble"))?"RECON_OK":"RECON_ERROR capture busy / unsaved");
 else if(c=="recon capture stop"){captureStop();Serial.println("RECON_OK");}
 else if(c=="recon capture discard"&&!busy()&&!status.recording){captureDirty=false;status.bytes=status.records=0;Serial.println("RECON_OK");}
 else if(c=="recon capture save")Serial.println(captureSave()?"RECON_OK saving":"RECON_ERROR cannot save");
 else if(c=="recon files"){char names[32][48];size_t sizes[32];unsigned count=fileList(names,sizes,32);for(unsigned i=0;i<count;++i)Serial.printf("RECON_SAVED %s %u\n",names[i],unsigned(sizes[i]));Serial.println("RECON_OK");}
 else if(c.startsWith("recon get ")){if(!fileExport(c.substring(10).c_str()))Serial.println("RECON_ERROR cannot export");}
 else if(c.startsWith("recon remove "))Serial.println(fileRemove(c.substring(13).c_str())?"RECON_OK":"RECON_ERROR cannot remove");
 else Serial.println("RECON_ERROR invalid command or busy");return true;
}
#endif
