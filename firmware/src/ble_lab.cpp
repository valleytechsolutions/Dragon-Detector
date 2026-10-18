// Dragon BLE Lab. Original MIT implementation; no third-party firmware code.
#include "ble_lab.h"
#include "background_radio.h"
#include "recon.h"
#include "missions.h"
#include "ble_insights.h"
#include "hid_decoder.h"
#include "flash_budget.h"
#include "attack_frames.h"
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "ui_theme.h"
#include <NimBLEDevice.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <atomic>
#include <lvgl.h>
#include "lab_ui_scale.h"

using namespace dragon::lab;
namespace {
using namespace dragon::labui;
enum State:uint8_t {Idle,Working,Monitoring,Emulating,Bridging,Failed};
enum Job:uint8_t {None,Discover,Clone,Stop,Subscribe,Read,Unsubscribe,Write,WriteNr,Mtu,Secure,Replay,Bridge,BridgeWrite};
std::atomic<State> state{Idle};std::atomic<bool> owned{false},cancelled{false},finished{false};
std::atomic<uint32_t> revision{1},eventDrops{0};
bool resumeRadio=true,rebind=false,storeReady=false;
std::atomic<bool> bridgeActive{false};uint32_t bridgeStarted=0;std::atomic<uint32_t> bridgeWrites{0},bridgeErrors{0};
bool replayActive=false,replayIdentity=false;uint32_t replayStarted=0;
uint8_t writeData[MaxValue]{};size_t writeLength=0;bool subscriptions[MaxAttributes]{};uint16_t requestedMtu=247;
Job job=None;unsigned jobIndex=0;TaskHandle_t workerTask=nullptr;
StaticSemaphore_t jobControl;SemaphoreHandle_t jobReady=nullptr;
Profile* profile=nullptr;
dragon::hid::Map* hidMap=nullptr;
#ifdef BOARD_AMOLED_175C
fs::LittleFSFS recon;
#else
fs::LittleFSFS& recon=LittleFS;
#endif
char message[120]="Select an owned BLE peripheral to inspect",selectedName[48]{};
uint8_t selectedMac[6]{},selectedType=0;
NimBLEClient* client=nullptr;
bool pairingEnabled=false,pairingMitm=true,pairingBond=false;
std::atomic<unsigned> pairRequest{0};std::atomic<uint32_t> pairCode{0},pairAt{0};
std::atomic<bool> encrypted{false},authenticated{false},bonded{false};
std::atomic<int> disconnectReason{0};std::atomic<bool> pairingRequested{false};
class ClientCallbacks:public NimBLEClientCallbacks {
 void onDisconnect(NimBLEClient*,int reason) override{disconnectReason=reason;pairRequest=0;}
 // User-controlled pairing. Never accept an unseen numeric comparison or default PIN.
 void onPassKeyEntry(NimBLEConnInfo&) override{pairingRequested=true;if(!pairingEnabled){if(client)client->disconnect();return;}pairAt=millis();pairRequest=1;++revision;}
 void onConfirmPasskey(NimBLEConnInfo& info,uint32_t pin) override{pairingRequested=true;if(!pairingEnabled){NimBLEDevice::injectConfirmPasskey(info,false);if(client)client->disconnect();return;}pairCode=pin;pairAt=millis();pairRequest=2;++revision;}
 void onAuthenticationComplete(NimBLEConnInfo& info) override{encrypted=info.isEncrypted();authenticated=info.isAuthenticated();bonded=info.isBonded();pairRequest=0;++revision;}
} clientCallbacks;
NimBLERemoteCharacteristic* remote[MaxAttributes]{};
NimBLECharacteristic* local[MaxAttributes]{};
StaticSemaphore_t readControl;SemaphoreHandle_t readDone=nullptr;
struct ReadContext {Attribute* attribute=nullptr;int status=0;bool longRead=true;} readContext;
int onRead(uint16_t,const ble_gatt_error* error,ble_gatt_attr* data,void*){
 auto& a=*readContext.attribute;
 if(error->status){readContext.status=error->status;xSemaphoreGive(readDone);return 0;}
 if(data&&data->om){size_t n=OS_MBUF_PKTLEN(data->om),offset=data->offset;if(offset<MaxValue){size_t take=std::min<size_t>(n,MaxValue-offset);os_mbuf_copydata(data->om,0,take,a.value+offset);a.length=std::max<size_t>(a.length,offset+take);}if(offset+n>MaxValue)a.flags|=ValueTruncated;}
 if(!data||!readContext.longRead)xSemaphoreGive(readDone);return 0;
}
bool readAttribute(Attribute& a){
 a.length=0;a.flags=0;memset(a.value,0,sizeof(a.value));readContext={&a,0};xSemaphoreTake(readDone,0);
 int rc=ble_gattc_read_long(client->getConnHandle(),a.handle,0,onRead,nullptr);
 if(!rc){xSemaphoreTake(readDone,portMAX_DELAY);rc=readContext.status;}
 if(rc==BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG)){a.length=0;readContext.status=0;readContext.longRead=false;rc=ble_gattc_read(client->getConnHandle(),a.handle,onRead,nullptr);if(!rc){xSemaphoreTake(readDone,portMAX_DELAY);rc=readContext.status;}}
 bool ok=rc==0||rc==BLE_HS_EDONE;if(!ok)a.flags|=ReadFailed;return ok;
}
struct Advertisement {bool used;uint8_t mac[6],type;int8_t rssi;uint16_t length,boundary;uint32_t at;uint8_t data[MaxAdvertisement];};
Advertisement* advertisements=nullptr;
constexpr unsigned CacheCount=64;
struct Event {uint32_t at;uint16_t index,length;uint8_t kind;uint8_t data[MaxValue];};
QueueHandle_t eventQueue=nullptr;StaticQueue_t eventControl;uint8_t* eventMemory=nullptr;
QueueHandle_t bridgeQueue=nullptr;StaticQueue_t bridgeControl;uint8_t* bridgeMemory=nullptr;
Event* events=nullptr;unsigned eventCount=0,eventHead=0;
uint32_t loggedBytes=0;bool logEnabled=false;
ble_gap_event_listener notifyListener{};bool notifyListenerActive=false;
void say(const char* s){strlcpy(message,s,sizeof(message));++revision;}
void queueEvent(unsigned index,const uint8_t* data,size_t length,uint8_t kind){Event e{};e.at=millis();e.index=index;e.length=std::min<size_t>(length,MaxValue);e.kind=kind;if(e.length)memcpy(e.data,data,e.length);if(!eventQueue||xQueueSend(eventQueue,&e,0)!=pdTRUE)++eventDrops;}
int onGap(ble_gap_event* event,void*){if(event->type==BLE_GAP_EVENT_NOTIFY_RX&&client&&event->notify_rx.conn_handle==client->getConnHandle()){for(unsigned i=0;i<profile->count;++i){const auto& a=profile->attributes[i];if(a.kind!=Characteristic||a.handle!=event->notify_rx.attr_handle)continue;uint8_t data[MaxValue];size_t n=std::min<size_t>(MaxValue,OS_MBUF_PKTLEN(event->notify_rx.om));os_mbuf_copydata(event->notify_rx.om,0,n,data);queueEvent(i,data,n,event->notify_rx.indication?1:0);break;}}return 0;}
int onSubscribe(uint16_t,const ble_gatt_error* error,ble_gatt_attr*,void*){readContext.status=error->status;xSemaphoreGive(readDone);return 0;}
bool subscribe(NimBLERemoteCharacteristic* c,bool enable=true){
 uint16_t handle=0;for(unsigned i=0;i<profile->count;++i){const auto& a=profile->attributes[i];if(a.kind==Descriptor&&a.parent==jobIndex&&!strcmp(a.uuid,"2902")){handle=a.handle;break;}}if(!handle)return false;
 if(!notifyListenerActive){if(ble_gap_event_listener_register(&notifyListener,onGap,nullptr))return false;notifyListenerActive=true;}
 uint8_t value[2]={uint8_t(enable?(c->canNotify()?1:2):0),0};readContext.status=0;xSemaphoreTake(readDone,0);
 int rc=ble_gattc_write_flat(client->getConnHandle(),handle,value,sizeof(value),onSubscribe,nullptr);if(!rc){xSemaphoreTake(readDone,portMAX_DELAY);rc=readContext.status;}return rc==0;
}
class CloneCallbacks:public NimBLECharacteristicCallbacks {void onWrite(NimBLECharacteristic* c,NimBLEConnInfo&) override{for(unsigned i=0;i<MaxAttributes;++i)if(local[i]==c){auto v=c->getValue();queueEvent(i,v.data(),v.size(),2);break;}}} cloneCallbacks;
class ServerCallbacks:public NimBLEServerCallbacks {void onConnect(NimBLEServer*,NimBLEConnInfo&) override{queueEvent(0,nullptr,0,3);}void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int) override{queueEvent(0,nullptr,0,4);}} serverCallbacks;
bool working(){return state.load()==Working;}
bool memoryOkay(){return ESP.getFreeHeap()>26000&&!cancelled.load();}
void value(Attribute& a,const NimBLEAttValue& v,bool okay){a.length=std::min<size_t>(MaxValue,v.size());if(a.length)memcpy(a.value,v.data(),a.length);a.flags=okay?0:ReadFailed;if(v.size()>MaxValue)a.flags|=ValueTruncated;}
void disconnect(){pairRequest=0;pairCode=0;encrypted=false;authenticated=false;bonded=false;memset(subscriptions,0,sizeof(subscriptions));if(notifyListenerActive){ble_gap_event_listener_unregister(&notifyListener);notifyListenerActive=false;}if(client){if(client->isConnected())client->disconnect();else client->cancelConnect();uint32_t at=millis();while(client->isConnected()&&millis()-at<2500)delay(10);}}
void stopWorker(){bridgeActive=false;if(bridgeQueue)xQueueReset(bridgeQueue);replayActive=false;disconnect();if(rebind){auto* server=NimBLEDevice::getServer();NimBLEDevice::getAdvertising()->stop();if(server){server->advertiseOnDisconnect(false);for(auto h:server->getPeerDevices())server->disconnect(h);uint32_t at=millis();while(server->getConnectedCount()&&millis()-at<2500)delay(10);}NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);NimBLEDevice::deinit(true);client=nullptr;memset(remote,0,sizeof(remote));memset(local,0,sizeof(local));}finished=true;}
void discover(){
 init(*profile,selectedName);memcpy(profile->mac,selectedMac,6);profile->addressType=selectedType;
 for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(a.used&&a.type==selectedType&&!memcmp(a.mac,selectedMac,6)){profile->advertisementLength=a.length;profile->scanBoundary=a.boundary;memcpy(profile->advertisement,a.data,a.length);break;}}
 char mac[18];macText(selectedMac,mac);client=NimBLEDevice::createClient();if(!client){say("Cannot allocate BLE client");stopWorker();return;}rebind=true;
 disconnectReason=0;pairingRequested=false;pairRequest=0;encrypted=false;authenticated=false;bonded=false;NimBLEDevice::setSecurityAuth(pairingBond,pairingMitm,true);NimBLEDevice::setSecurityIOCap(pairingEnabled?BLE_HS_IO_KEYBOARD_DISPLAY:BLE_HS_IO_NO_INPUT_OUTPUT);client->setClientCallbacks(&clientCallbacks,false);
 client->setConnectTimeout(7000);client->setConnectionParams(24,40,0,200);
 // A peer may negotiate MTU first. NimBLE 2.3.6 otherwise reports EALREADY
 // as a failed connection. Discovery and long reads also work at MTU 23.
 if(!client->connect(NimBLEAddress(std::string(mac),selectedType),true,false,false)){snprintf(message,sizeof(message),"Connection failed (%d). Device may not be connectable.",client->getLastError());++revision;stopWorker();return;}
 if(pairingEnabled&&!client->secureConnection()){say("Pairing rejected or timed out; radar resumed");stopWorker();return;}
 say("Discovering services and readable values...");
 unsigned services=0,chars=0;uint32_t began=millis();
 for(auto* s:client->getServices(true)){
  if(!memoryOkay()||++services>16||millis()-began>60000){profile->flags|=1;break;}
  unsigned si=profile->count;if(!append(*profile,Service,0xffff,s->getUUID().toString().c_str(),s->getStartHandle()))break;
  for(auto* c:s->getCharacteristics(true)){
   if(!memoryOkay()||++chars>64||millis()-began>60000){profile->flags|=1;break;}
   unsigned ci=profile->count;auto* a=append(*profile,Characteristic,si,c->getUUID().toString().c_str(),c->getHandle());if(!a)break;
   a->properties=(c->canBroadcast()?1:0)|(c->canRead()?2:0)|(c->canWriteNoResponse()?4:0)|(c->canWrite()?8:0)|(c->canNotify()?16:0)|(c->canIndicate()?32:0)|(c->canWriteSigned()?64:0);
   remote[ci]=c;a->flags=NotRead;if(c->canRead())readAttribute(*a);
   for(auto* d:c->getDescriptors(true)){
    // NimBLE's discovery range can extend to the end of the service. A new
    // characteristic declaration ends THIS characteristic's descriptors.
    if(d->getUUID()==NimBLEUUID(uint16_t(0x2803)))break;
    if(!memoryOkay()||millis()-began>60000){profile->flags|=1;break;}
    auto* da=append(*profile,Descriptor,ci,d->getUUID().toString().c_str(),d->getHandle());if(!da)break;
    readAttribute(*da);
   }
   c->deleteDescriptors(); // do not retain overlapping discovery vectors
  }
 }
 if(cancelled||!client->isConnected()){if(pairingRequested)say("Pairing requires user input; partial snapshot retained. Radar resumed.");else{snprintf(message,sizeof(message),"Disconnected (%d). Partial snapshot retained; radar resumed.",disconnectReason.load());++revision;}profile->flags|=1;stopWorker();return;}
 snprintf(message,sizeof(message),"Connected / %u attributes%s",profile->count,profile->flags?" / PARTIAL":"");++revision;state=Monitoring;
}
uint32_t properties(uint8_t p){return (p&1?NIMBLE_PROPERTY::BROADCAST:0)|(p&2?NIMBLE_PROPERTY::READ:0)|(p&4?NIMBLE_PROPERTY::WRITE_NR:0)|(p&8?NIMBLE_PROPERTY::WRITE:0)|(p&16?NIMBLE_PROPERTY::NOTIFY:0)|(p&32?NIMBLE_PROPERTY::INDICATE:0);}
void clone(){
 if(!profile->count||profile->flags||!valid(*profile,bytes(*profile))){say("Emulation needs a complete, valid GATT profile");finished=true;return;}
 auto* server=NimBLEDevice::createServer();if(!server){say("Could not allocate GATT server");finished=true;return;}rebind=true;server->setCallbacks(&serverCallbacks,false);server->advertiseOnDisconnect(!bridgeActive);
 NimBLEService* services[MaxAttributes]{};unsigned cloned=0,skipped=0;
 for(unsigned i=0;i<profile->count;++i){auto& a=profile->attributes[i];NimBLEUUID uuid(a.uuid);
  if(!memoryOkay()){say("Emulation stopped: memory reserve reached");stopWorker();return;}
  if(a.kind==Service){if(uuid==NimBLEUUID(uint16_t(0x1800))||uuid==NimBLEUUID(uint16_t(0x1801))){++skipped;continue;}services[i]=server->createService(uuid);}
  else if(a.kind==Characteristic){if(!services[a.parent])continue;local[i]=services[a.parent]->createCharacteristic(uuid,properties(a.properties),MaxValue);if(!local[i]){say("Characteristic allocation failed");stopWorker();return;}local[i]->setValue(a.value,a.length);local[i]->setCallbacks(&cloneCallbacks);++cloned;}
  else{if(!local[a.parent]||uuid==NimBLEUUID(uint16_t(0x2902)))continue;auto* d=local[a.parent]->createDescriptor(uuid,NIMBLE_PROPERTY::READ,MaxValue);if(d)d->setValue(a.value,a.length);}
 }
 if(!cloned){say("No application characteristics in this profile");stopWorker();return;}
 for(auto* s:services)if(s&&!s->start()){say("GATT service registration failed");stopWorker();return;}
 NimBLEDevice::setDeviceName(profile->name);server->start();for(auto* s:services)if(s&&s->getHandle()==0xffff){say("GATT server could not start; radar restored");stopWorker();return;}auto* adv=NimBLEDevice::getAdvertising();adv->reset();adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);adv->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
 NimBLEAdvertisementData data,scan;bool captured=profile->scanBoundary>0&&profile->scanBoundary<=31&&profile->advertisementLength-profile->scanBoundary<=31;
 if(captured){data.addData(profile->advertisement,profile->scanBoundary);scan.addData(profile->advertisement+profile->scanBoundary,profile->advertisementLength-profile->scanBoundary);}
 else{data.setFlags(6);scan.setName(std::string(profile->name).substr(0,29));for(auto* s:services)if(s){data.addServiceUUID(s->getUUID());break;}}
 adv->setAdvertisementData(data);adv->setScanResponseData(scan);adv->enableScanResponse(true);
 if(!adv->start()){say("Advertising failed; radar restored");stopWorker();return;}
 snprintf(message,sizeof(message),"Emulating %u characteristics / %s adverts",cloned,captured?"captured":"generated");++revision;state=bridgeActive?Working:Emulating;
}
void bridge(){
 if(!client||!client->isConnected()||!profile->count||profile->flags||!valid(*profile,bytes(*profile))){say("Bridge needs a connected, complete profile");state=Monitoring;return;}
 // NimBLE cannot change its GATT database while any connection exists.
 // Disconnect without deleting remote objects, register the local server,
 // then reconnect using the existing discovered handles.
 disconnect();if(client->isConnected()){say("Could not suspend connection for bridge");stopWorker();return;}
 bridgeActive=true;bridgeStarted=millis();bridgeWrites=0;bridgeErrors=0;if(bridgeQueue)xQueueReset(bridgeQueue);clone();if(finished||!bridgeActive)return;
 char mac[18];macText(selectedMac,mac);
 if(!client->connect(NimBLEAddress(std::string(mac),selectedType),false,false,false)){say("Bridge peer reconnection failed; radar restored");stopWorker();return;}
 if(pairingEnabled&&!client->secureConnection()){say("Bridge pairing rejected; radar restored");stopWorker();return;}
 unsigned subscribed=0,failed=0;for(unsigned i=0;i<profile->count&&!cancelled;++i)if(local[i]&&remote[i]&&(remote[i]->canNotify()||remote[i]->canIndicate())){jobIndex=i;if(subscriptions[i]||subscribe(remote[i])){subscriptions[i]=true;++subscribed;}else ++failed;}
 if(cancelled||!client->isConnected()){stopWorker();return;}snprintf(message,sizeof(message),"Bridge / cached reads / %u subscriptions / %u rejected / 10 min limit",subscribed,failed);++revision;state=Bridging;
}
void replay(){
 Advertisement* found=nullptr;for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(a.used&&a.type==selectedType&&!memcmp(a.mac,selectedMac,6)&&millis()-a.at<30000){found=&a;break;}}
 if(!found||!found->boundary||found->boundary>31||found->length-found->boundary>31||(replayIdentity&&(!(found->type&1)||(found->mac[0]&0xc0)!=0xc0))){say("Replay needs a fresh legacy advert; identity requires random-static address");finished=true;return;}
 auto* a=NimBLEDevice::getAdvertising();a->stop();a->reset();a->setConnectableMode(BLE_GAP_CONN_MODE_NON);a->setAdvertisingInterval(160);rebind=true;
 if(replayIdentity&&!NimBLEDevice::setOwnAddr(NimBLEAddress(found->mac,BLE_ADDR_RANDOM))){say("Replay address rejected");stopWorker();return;}
 if(replayIdentity)NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
 NimBLEAdvertisementData adv,response;bool ok=adv.addData(found->data,found->boundary)&&a->setAdvertisementData(adv);size_t size=found->length-found->boundary;
 if(size){ok=ok&&response.addData(found->data+found->boundary,size)&&a->setScanResponseData(response);a->enableScanResponse(true);}else a->enableScanResponse(false);
 if(!ok||!a->start()){say("Replay advertising failed");stopWorker();return;}replayActive=true;replayStarted=millis();say(replayIdentity?"Replaying captured random-static identity / 30s maximum":"Replaying advertisement with detector address / 30s maximum");state=Emulating;
}
bool start(Job next,unsigned index);
bool tagSound(){if(state!=Monitoring||!profile||!client||!client->isConnected())return false;
 struct Sound {const char* service;const char* characteristic;uint8_t data[3];unsigned length;};
 static constexpr Sound sounds[]={
  {"7dfc9000-7d1c-4951-86aa-8d9728f8d66c","7dfc9001-7d1c-4951-86aa-8d9728f8d66c",{0xaf,0,0},1},
  {"fd44","4f860003-943b-49ef-bed4-2f730304427a",{1,0,3},3},
  {"15190001-12f4-c226-88ed-2ac5579f2a85","8e0c0001-1d68-fb92-bf61-48377421680e",{0,3,0},2}
 };
 for(unsigned i=0;i<profile->count;++i){auto& a=profile->attributes[i];if(a.kind!=Characteristic||!(a.properties&8)||!remote[i]||a.parent>=i)continue;for(auto& sound:sounds)if(uuidEqual(a.uuid,sound.characteristic)&&uuidEqual(profile->attributes[a.parent].uuid,sound.service)){memcpy(writeData,sound.data,sound.length);writeLength=sound.length;return start(Write,i);}}
 return false;
}
void worker(void*){for(;;){xSemaphoreTake(jobReady,portMAX_DELAY);Job run=job;
 // NimBLE uses this task's notification slot internally. Jobs must use a
 // separate semaphore, otherwise a late GATT completion replays the last job.
 ulTaskNotifyTake(pdTRUE,0);
 if(run==Stop)stopWorker();else if(run==Discover)discover();else if(run==Clone)clone();else if(run==Replay)replay();else if(run==Bridge)bridge();
 else if(run==Secure&&client&&client->isConnected()&&pairingEnabled){bool ok=client->secureConnection();say(ok?"Link encrypted; inspect security status":"Pairing failed");state=Monitoring;}
 else if(run==Mtu&&client&&client->isConnected()){NimBLEDevice::setMTU(requestedMtu);bool ok=client->exchangeMTU();say(ok?"MTU exchange complete":"MTU exchange rejected");state=Monitoring;}
 else if((run==Read||run==Subscribe||run==Unsubscribe||run==Write||run==WriteNr||run==BridgeWrite)&&client&&client->isConnected()&&jobIndex<profile->count){
  auto* c=remote[jobIndex];auto& a=profile->attributes[jobIndex];bool ok=false;
  if(run==Read){ok=readAttribute(a);}
  else if((run==Write||run==WriteNr||run==BridgeWrite)&&c&&((run==Write&&c->canWrite())||(run==WriteNr&&c->canWriteNoResponse())||(run==BridgeWrite&&(c->canWrite()||c->canWriteNoResponse())))){ok=c->writeValue(writeData,writeLength,run==Write||(run==BridgeWrite&&c->canWrite()));if(ok){memcpy(a.value,writeData,writeLength);a.length=writeLength;a.flags=0;queueEvent(jobIndex,writeData,writeLength,5);}}
  else if((run==Subscribe||run==Unsubscribe)&&c&&(c->canNotify()||c->canIndicate())){ok=subscribe(c,run!=Unsubscribe);if(ok)subscriptions[jobIndex]=run!=Unsubscribe;}
  if(run==BridgeWrite){if(ok)++bridgeWrites;else{++bridgeErrors;queueEvent(jobIndex,nullptr,0,6);}state=Bridging;continue;}
  say(ok?(run==Read?"Value refreshed":run==Write?"Write acknowledged by peer":run==WriteNr?"Write command queued; no peer acknowledgement":run==Unsubscribe?"Unsubscribed":"Subscribed; notifications appear in Events"):"Operation rejected / may require pairing or permission");state=Monitoring;
 }else if(run!=None){say("Connection no longer available");stopWorker();}
 }}
bool start(Job next,unsigned index=0){if((radioStandby()&&next!=Stop)||!profile||!workerTask||working()||finished||reconOwnsRadio())return false;
 if(next==Discover||next==Clone||next==Replay){if(owned||reconStorageBusy())return false;missionStop();reconStop();resumeRadio=radioEnabled;setRadio(false);owned=true;cancelled=false;eventCount=eventHead=0;eventDrops=0;}
 else if(next==BridgeWrite){if(state!=Bridging)return false;}else if(next!=Stop&&state!=Monitoring)return false;
 job=next;jobIndex=index;state=Working;say(next==Discover?"Connecting...":next==Clone?"Building local GATT peripheral...":next==Stop?"Stopping lab / restoring radar...":"Request pending...");xSemaphoreGive(jobReady);return true;}
String path(const char* name){return String("/profiles/")+name+".ddg";}
bool save(const char* name){if(!storeReady||working()||!safeName(name)||!valid(*profile,bytes(*profile))||!profile->count)return false;String p=path(name);if(recon.exists(p)){say("Name exists. Choose a new name in the web lab.");return false;}
 size_t length=bytes(*profile);if(!dragon::storageFits(recon.totalBytes(),recon.usedBytes(),length+16384))return false;recon.remove(p+".part");File f=recon.open(p+".part",FILE_WRITE);if(!f)return false;uint32_t crc=checksum((uint8_t*)profile,length);bool ok=f.write((uint8_t*)profile,length)==length&&f.write((uint8_t*)&crc,4)==4;f.flush();f.close();if(ok)ok=recon.rename(p+".part",p);if(!ok)recon.remove(p+".part");say(ok?"Profile saved to recon storage":"Profile save failed");return ok;}
bool readProfile(const char* name,Profile& p){if(!storeReady||!safeName(name))return false;File f=recon.open(path(name));size_t n=f?f.size():0;if(n<HeaderSize+4||n>sizeof(Profile)+4){f.close();return false;}memset(&p,0,sizeof(p));uint32_t crc=0;bool ok=f.read((uint8_t*)&p,n-4)==n-4&&f.read((uint8_t*)&crc,4)==4;f.close();return ok&&checksum((uint8_t*)&p,n-4)==crc&&valid(p,n-4);}
bool hidEvent(const Event& e,char* output,size_t capacity){
 if(!hidMap||working()||e.index>=profile->count||e.kind>2)return false;const auto& a=profile->attributes[e.index];
 if(a.kind!=Characteristic||!uuidEqual(a.uuid,"2a4d"))return false;
 unsigned id=0,kind=0;bool reference=false;for(unsigned i=0;i<profile->count;++i){auto& d=profile->attributes[i];if(d.kind==Descriptor&&d.parent==e.index&&uuidEqual(d.uuid,"2908")&&d.length==2&&!d.flags&&d.value[1]>=1&&d.value[1]<=3){id=d.value[0];kind=d.value[1]-1;reference=true;break;}}
 if(!reference){snprintf(output,capacity,"Raw only: missing Report Reference");return true;}
 for(unsigned i=0;i<profile->count;++i){auto& map=profile->attributes[i];if(map.kind==Characteristic&&map.parent==a.parent&&uuidEqual(map.uuid,"2a4b")){
  if(map.flags){snprintf(output,capacity,"Raw only: incomplete Report Map");return true;}
  dragon::hid::parse(*hidMap,map.value,map.length);dragon::hid::decode(*hidMap,id,kind,e.data,e.length,output,capacity);return true;
 }}snprintf(output,capacity,"Raw only: missing Report Map");return true;
}
void fileList(){Serial.println("LAB_PROFILES");if(storeReady){File d=recon.open("/profiles");for(File f=d.openNextFile();f;f=d.openNextFile()){if(!f.isDirectory()&&String(f.name()).endsWith(".ddg"))Serial.printf("LAB_PROFILE %s %u\n",f.name(),unsigned(f.size()));f.close();}d.close();}Serial.println("LAB_END");}
bool serialWrite(const uint8_t* p,size_t n){RadioTransfer transfer;uint32_t at=millis();while(n){size_t k=std::min<size_t>(128,std::min(n,size_t(Serial.availableForWrite())));if(k){k=Serial.write(p,k);p+=k;n-=k;at=millis();}else if(millis()-at>5000)return false;radioTransferTick();delay(1);}return true;}
void exportLog(bool previous){File f=storeReady?recon.open(previous?"/events/previous.csv":"/events/current.csv"):File();if(!f){Serial.println("LAB_ERROR no saved log");return;}RadioTransfer transfer;Serial.printf("LAB_LOG_BEGIN %u\n",unsigned(f.size()));Serial.println("uptime_ms,kind,attribute,length,hex");uint8_t b[256];bool ok=true;while(f.available()&&ok){size_t n=f.read(b,sizeof(b));ok=n&&serialWrite(b,n);}f.close();Serial.println(ok?"LAB_LOG_END":"LAB_ERROR log transfer");}
void exportProfile(){if(working()||!profile->count){Serial.println("LAB_ERROR no stable profile");return;}size_t n=bytes(*profile);uint32_t crc=checksum((uint8_t*)profile,n);RadioTransfer transfer;Serial.printf("LAB_FILE %u %08lx\n",unsigned(n),crc);bool ok=true;char line[540];
 for(size_t off=0;off<n&&ok;off+=256){size_t count=std::min<size_t>(256,n-off);int pos=snprintf(line,sizeof(line),"LAB_DATA %u ",unsigned(off));for(size_t i=0;i<count;++i)pos+=snprintf(line+pos,sizeof(line)-pos,"%02x",((uint8_t*)profile)[off+i]);line[pos++]='\n';ok=serialWrite((uint8_t*)line,pos);}Serial.println(ok?"LAB_FILE_END":"LAB_ERROR transfer");}
void importProfile(const String& command){unsigned n=0;unsigned long crc=0;char extra;if(owned||reconStorageBusy()||sscanf(command.c_str(),"lab import %u %lx %c",&n,&crc,&extra)!=2||n<HeaderSize||n>sizeof(Profile)){Serial.println("LAB_ERROR import length or busy");return;}
 auto* temp=(Profile*)ps_calloc(1,sizeof(Profile));if(!temp){Serial.println("LAB_ERROR memory");return;}RadioTransfer transfer;Serial.println("LAB_READY");size_t received=0;uint32_t at=millis(),ack=at;
 while(received<n&&millis()-at<5000){size_t k=std::min<size_t>(256,std::min<size_t>(Serial.available(),n-received));if(k){k=Serial.readBytes((uint8_t*)temp+received,k);received+=k;at=ack=millis();Serial.printf("LAB_ACK %u\n\n",unsigned(received));}else{if(millis()-ack>250){Serial.printf("LAB_ACK %u\n\n",unsigned(received));ack=millis();}}radioTransferTick();delay(1);}
 bool ok=received==n&&checksum((uint8_t*)temp,n)==crc&&valid(*temp,n);if(ok){memcpy(profile,temp,sizeof(Profile));say("Imported profile / review before emulating");}free(temp);Serial.println(ok?"LAB_OK imported":"LAB_ERROR invalid profile or CRC");}
String hex(const uint8_t* p,size_t n,size_t limit=32){String s;char b[4];for(size_t i=0;i<std::min(n,limit);++i){snprintf(b,sizeof(b),"%02x",p[i]);s+=b;if(i+1<std::min(n,limit))s+=' ';}if(n>limit)s+=" ...";return s;}
const char* stateName(){switch(state.load()){case Working:return "BUSY";case Monitoring:return "CONNECTED";case Emulating:return "EMULATING";case Bridging:return "BRIDGING";case Failed:return "ERROR";default:return "IDLE";}}
}
bool bleLabOwnsRadio(){return owned.load();}
fs::FS* bleLabStorage(){return storeReady&&!owned?&recon:nullptr;}
bool bleLabStorageSpace(size_t n){return storeReady&&!owned&&dragon::storageFits(recon.totalBytes(),recon.usedBytes(),n);}
void bleLabSelect(const uint8_t* mac,uint8_t type,const char* name){if(owned)return;memcpy(selectedMac,mac,6);selectedType=type;strlcpy(selectedName,name,sizeof(selectedName));++revision;}
void bleLabObserve(const uint8_t* mac,uint8_t type,int rssi,const uint8_t* data,size_t len,size_t boundary,uint32_t at){if(!advertisements||owned||len>MaxAdvertisement||boundary>len)return;
 if(!advertisementValid(data,boundary)||!advertisementValid(data+boundary,len-boundary))return;
 unsigned slot=0;uint32_t age=0;for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(!a.used||(a.type==type&&!memcmp(a.mac,mac,6))){slot=i;break;}if(uint32_t(at-a.at)>=age){age=at-a.at;slot=i;}}
 auto& a=advertisements[slot];a.used=true;memcpy(a.mac,mac,6);a.type=type;a.rssi=rssi;a.at=at;a.length=len;a.boundary=boundary;memcpy(a.data,data,len);
}
void bleLabBegin(){
 hidMap=(dragon::hid::Map*)ps_calloc(1,sizeof(dragon::hid::Map));profile=(Profile*)ps_calloc(1,sizeof(Profile));advertisements=(Advertisement*)ps_calloc(CacheCount,sizeof(Advertisement));events=(Event*)ps_calloc(64,sizeof(Event));eventMemory=(uint8_t*)ps_malloc(16*sizeof(Event));
 if(!hidMap||!profile||!advertisements||!events||!eventMemory){say("BLE Lab memory unavailable");return;}
 bridgeMemory=(uint8_t*)ps_malloc(16*sizeof(Event));bridgeQueue=bridgeMemory?xQueueCreateStatic(16,sizeof(Event),bridgeMemory,&bridgeControl):nullptr;if(!bridgeQueue){say("Bridge queue memory unavailable");return;}
 init(*profile,"");eventQueue=xQueueCreateStatic(16,sizeof(Event),eventMemory,&eventControl);readDone=xSemaphoreCreateBinaryStatic(&readControl);jobReady=xSemaphoreCreateBinaryStatic(&jobControl);
 if(xTaskCreatePinnedToCore(worker,"ble-lab",8192,nullptr,1,&workerTask,0)!=pdPASS){workerTask=nullptr;say("BLE Lab worker unavailable");return;}
 #ifdef BOARD_AMOLED_175C
 auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"recon");
 if(p&&p->address==0x1000000&&p->size==0x1000000&&ESP.getFlashChipSize()>=0x2000000)storeReady=recon.begin(false,"/recon",4,"recon");
#else
 // Reuse the already-mounted badge filesystem. Never format or repartition.
 storeReady=recon.totalBytes()>0;
#endif
 if(storeReady){recon.mkdir("/profiles");recon.mkdir("/events");}
 Serial.printf("LAB_STORAGE ready=%u total=%u used=%u profile_bytes=%u\n",storeReady,storeReady?recon.totalBytes():0,storeReady?recon.usedBytes():0,unsigned(sizeof(Profile)));
}
void bleLabStop(){if(!owned)return;cancelled=true;if(!working())start(Stop);}
void bleLabLoop(){
 if(bridgeActive&&millis()-bridgeStarted>=600000)bleLabStop();
 if(replayActive&&millis()-replayStarted>=30000)bleLabStop();
 if(pairRequest&&millis()-pairAt.load()>30000){pairRequest=0;cancelled=true;if(client)client->disconnect();say("Pairing prompt timed out; request cancelled");}
 if(finished.exchange(false)){if(rebind){radioRebindBle();rebind=false;}owned=false;cancelled=false;state=Idle;setRadio(resumeRadio);if(!strcmp(message,"Stopping lab / restoring radar..."))say("Lab stopped. Radar restored; workspace retained.");++revision;}
 if(cancelled&&owned&&!working())start(Stop);
 if((state==Monitoring||state==Bridging)&&client&&!client->isConnected()){say("Peripheral disconnected; returning radio to radar");start(Stop);}
 Event e;for(unsigned i=0;eventQueue&&i<8&&xQueueReceive(eventQueue,&e,0)==pdTRUE;++i){if(bridgeActive){if(e.kind==4)cancelled=true;else if(e.kind<=2&&xQueueSend(bridgeQueue,&e,0)!=pdTRUE)++bridgeErrors;}
  events[eventHead]=e;eventHead=(eventHead+1)%64;eventCount=std::min(64u,eventCount+1);++revision;
  if(logEnabled&&storeReady){String line=String(e.at)+","+e.kind+","+e.index+","+e.length+","+hex(e.data,e.length,MaxValue)+"\n";File f=recon.open("/events/current.csv",FILE_APPEND);if(f){if(f.size()+line.length()>1024*1024){f.close();recon.remove("/events/previous.csv");recon.rename("/events/current.csv","/events/previous.csv");f=recon.open("/events/current.csv",FILE_WRITE);}if(f&&dragon::storageFits(recon.totalBytes(),recon.usedBytes(),32768)){size_t n=f.print(line);loggedBytes+=n;if(n!=line.length())logEnabled=false;}else logEnabled=false;f.close();}}
 }
 if(bridgeActive&&state==Bridging&&!cancelled){Event pending;for(unsigned i=0;i<8&&state==Bridging&&xQueueReceive(bridgeQueue,&pending,0)==pdTRUE;++i){if(pending.index>=profile->count||!local[pending.index])continue;if(pending.kind==2){memcpy(writeData,pending.data,pending.length);writeLength=pending.length;if(!start(BridgeWrite,pending.index))++bridgeErrors;}else{auto& a=profile->attributes[pending.index];memcpy(a.value,pending.data,pending.length);a.length=pending.length;a.flags=0;local[pending.index]->setValue(pending.data,pending.length);if(a.properties&16)local[pending.index]->notify();else if(a.properties&32)local[pending.index]->indicate();}}}
}
bool bleLabCommand(const String& c){if(!c.startsWith("lab "))return false;if(!profile||!workerTask){Serial.println("LAB_ERROR unavailable");return true;}
 if(c=="lab status"){Serial.printf("LAB_STATUS state=%s owned=%u storage=%u total=%u used=%u attributes=%u partial=%u events=%u drops=%lu logging=%u logged=%lu heap=%u\n",stateName(),owned.load(),storeReady,storeReady?recon.totalBytes():0,storeReady?recon.usedBytes():0,working()?0:profile->count,working()?0:profile->flags,eventCount,eventDrops.load(),logEnabled,loggedBytes,ESP.getFreeHeap());Serial.printf("LAB_MESSAGE %s\nLAB_END\n",working()?"BLE operation in progress":message);}
 else if(c=="lab stop"){bleLabStop();Serial.println("LAB_OK stopping");}
 else if(c=="lab demo"&&!owned){demo(*profile);selectedName[0]=0;say("Demo loaded: battery + read/write/notify loopback");Serial.println("LAB_OK demo");}
 else if(c=="lab bridge"){Serial.println(start(Bridge)?"LAB_OK bridge starting / cached reads / local write acknowledgements":"LAB_ERROR connect and discover first");}
 else if(c=="lab bridge status"){Serial.printf("LAB_BRIDGE active=%u writes=%lu errors=%lu queued=%u\nLAB_END\n",bridgeActive.load(),bridgeWrites.load(),bridgeErrors.load(),bridgeQueue?uxQueueMessagesWaiting(bridgeQueue):0);}
 else if(c=="lab clone"){Serial.println(start(Clone)?"LAB_OK starting":"LAB_ERROR busy");}
 else if(c.startsWith("lab inspect ")){char mac[18];unsigned t;char tail;unsigned m[6];if(sscanf(c.c_str(),"lab inspect %17s %u %c",mac,&t,&tail)==2&&strlen(mac)==17&&t<=3&&sscanf(mac,"%2x:%2x:%2x:%2x:%2x:%2x",m,m+1,m+2,m+3,m+4,m+5)==6&&!owned){uint8_t b[6];for(int i=0;i<6;++i)b[i]=m[i];bleLabSelect(b,t,mac);Serial.println(start(Discover)?"LAB_OK connecting":"LAB_ERROR busy");}else Serial.println("LAB_ERROR address/type or busy");}
 else if(c=="lab security"){Serial.printf("LAB_SECURITY enabled=%u mitm=%u bond=%u encrypted=%u authenticated=%u bonded=%u prompt=%u code=%06lu mtu=%u\n",pairingEnabled,pairingMitm,pairingBond,encrypted.load(),authenticated.load(),bonded.load(),pairRequest.load(),pairCode.load(),client&&owned&&client->isConnected()?client->getMTU():0);Serial.println("LAB_END");}
 else if(c.startsWith("lab pairing ")&&!owned){unsigned enabled,mitm,bond;char tail;bool ok=sscanf(c.c_str(),"lab pairing %u %u %u %c",&enabled,&mitm,&bond,&tail)==3&&enabled<2&&mitm<2&&bond<2;if(ok){pairingEnabled=enabled;pairingMitm=mitm;pairingBond=bond;}Serial.println(ok?"LAB_OK pairing configured for next connection":"LAB_ERROR pairing flags");}
 else if(c=="lab secure")Serial.println(pairingEnabled&&start(Secure)?"LAB_OK pending":"LAB_ERROR enable pairing before connecting");
 else if(c=="lab confirm yes"||c=="lab confirm no"){bool ok=pairRequest==2&&client&&client->isConnected();if(ok){ok=NimBLEDevice::injectConfirmPasskey(client->getConnInfo(),c.endsWith("yes"));pairRequest=0;}Serial.println(ok?"LAB_OK comparison submitted":"LAB_ERROR no numeric comparison");}
 else if(c.startsWith("lab passkey ")){String pin=c.substring(12);bool ok=pin.length()==6&&pairRequest==1&&client&&client->isConnected();for(unsigned i=0;ok&&i<6;++i)ok=isDigit(pin[i]);if(ok){ok=NimBLEDevice::injectPassKey(client->getConnInfo(),pin.toInt());pairRequest=0;}Serial.println(ok?"LAB_OK passkey submitted":"LAB_ERROR six digits / no prompt");}
 else if(c=="lab bonds"&&!owned){for(int i=0;i<NimBLEDevice::getNumBonds();++i){auto a=NimBLEDevice::getBondedAddress(i);Serial.printf("LAB_BOND %s %u\n",a.toString().c_str(),a.getType());}Serial.println("LAB_END");}
 else if(c.startsWith("lab forget ")&&!owned){char address[18],extra;unsigned type;uint8_t mac[6];bool ok=sscanf(c.c_str(),"lab forget %17s %u %c",address,&type,&extra)==2&&type<=3&&dragon::tx::parseMac(address,mac,false)&&NimBLEDevice::deleteBond(NimBLEAddress(std::string(address),type));Serial.println(ok?"LAB_OK bond removed":"LAB_ERROR address/type/bond");}
 else if(c.startsWith("lab select ")&&!owned){char mac[18],tail;unsigned type;uint8_t bytes[6];bool ok=sscanf(c.c_str(),"lab select %17s %u %c",mac,&type,&tail)==2&&type<=3&&dragon::tx::parseMac(mac,bytes,false);bool found=false;if(ok)for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(a.used&&a.type==type&&!memcmp(a.mac,bytes,6)&&millis()-a.at<30000){bleLabSelect(bytes,type,mac);found=true;break;}}Serial.println(found?"LAB_OK selected":"LAB_ERROR select a fresh observed address/type");}
 else if(c=="lab replay"||c=="lab replay identity"){if(!owned){replayIdentity=c.endsWith("identity");Serial.println(start(Replay)?"LAB_OK starting":"LAB_ERROR busy");}else Serial.println("LAB_ERROR busy");}
 else if(c=="lab sound")Serial.println(tagSound()?"LAB_OK sound request pending; audibility unverified":"LAB_ERROR no connected supported tracker sound characteristic");
 else if(c=="lab subscriptions"&&!working()){for(unsigned i=0;i<MaxAttributes;++i)if(subscriptions[i])Serial.printf("LAB_SUB %u\n",i);Serial.println("LAB_END");}
 else if(c.startsWith("lab unsubscribe ")){unsigned i;char tail;bool ok=sscanf(c.c_str(),"lab unsubscribe %u %c",&i,&tail)==1&&i<profile->count&&remote[i]&&start(Unsubscribe,i);Serial.println(ok?"LAB_OK pending":"LAB_ERROR attribute or connection");}
 else if(c.startsWith("lab write ")||c.startsWith("lab writenr ")){bool nr=c.startsWith("lab writenr ");unsigned i;int pos=0;bool ok=state==Monitoring&&sscanf(c.c_str(),nr?"lab writenr %u %n":"lab write %u %n",&i,&pos)==1&&pos>0&&i<profile->count&&remote[i]&&(profile->attributes[i].properties&(nr?4:8));if(ok)ok=decodeHex(c.c_str()+pos,writeData,sizeof(writeData),writeLength)&&start(nr?WriteNr:Write,i);Serial.println(ok?"LAB_OK pending":"LAB_ERROR invalid value, permission, or connection");}
 else if(c.startsWith("lab mtu ")){unsigned n;char tail;bool ok=state==Monitoring&&sscanf(c.c_str(),"lab mtu %u %c",&n,&tail)==1&&n>=23&&n<=517;if(ok){requestedMtu=n;ok=start(Mtu);}Serial.println(ok?"LAB_OK pending":"LAB_ERROR MTU/connection");}
 else if(c.startsWith("lab delete ")){String n=c.substring(11);Serial.println(!owned&&storeReady&&safeName(n.c_str())&&recon.remove(path(n.c_str()))?"LAB_OK deleted":"LAB_ERROR file/name/busy");}
 else if(c=="lab hid"&&!working()){bool any=false;for(unsigned i=0;i<profile->count;++i){auto& a=profile->attributes[i];if(a.kind!=Characteristic||!uuidEqual(a.uuid,"2a4b"))continue;any=true;auto h=parseHidMap(a.value,a.length);Serial.printf("LAB_HID index=%u valid=%u partial=%u reports=%u collections=%u items=%u status=%s\n",i,h.valid,h.unsupported||a.flags,h.count,h.collections,h.items,h.error);if(h.valid)for(unsigned j=0;j<h.count;++j){auto& r=h.reports[j];Serial.printf("LAB_HID_REPORT id=%u input_bits=%lu output_bits=%lu feature_bits=%lu\n",r.id,r.bits[0],r.bits[1],r.bits[2]);}}if(!any)Serial.println("LAB_HID no captured Report Map");Serial.println("LAB_END");}
 else if(c.startsWith("lab hid fields ")&&!working()){
  unsigned index;char extra;bool ok=sscanf(c.c_str(),"lab hid fields %u %c",&index,&extra)==1&&index<profile->count&&uuidEqual(profile->attributes[index].uuid,"2a4b")&&!profile->attributes[index].flags;
  if(ok){auto& a=profile->attributes[index];ok=dragon::hid::parse(*hidMap,a.value,a.length);if(ok)for(unsigned i=0;i<hidMap->count;++i){auto& f=hidMap->fields[i];Serial.printf("LAB_HID_FIELD id=%u kind=%u offset=%u width=%u count=%u variable=%u relative=%u usage_min=%08lx usage_max=%08lx usages=%u\n",f.id,f.kind+1,f.offset,f.width,f.count,!!(f.flags&2),!!(f.flags&4),f.first,f.last,f.usageCount);}}
  Serial.println(ok?"LAB_END":"LAB_ERROR no complete supported map");
 }
 else if(c.startsWith("lab hid decode ")&&!working()){
  unsigned index,id,kind;int pos=0;uint8_t data[512];size_t count=0;char output[400];bool ok=sscanf(c.c_str(),"lab hid decode %u %u %u %n",&index,&id,&kind,&pos)==3&&pos>0&&index<profile->count&&id<=255&&kind>=1&&kind<=3&&uuidEqual(profile->attributes[index].uuid,"2a4b")&&!profile->attributes[index].flags&&decodeHex(c.c_str()+pos,data,sizeof(data),count);
  if(ok){auto& a=profile->attributes[index];dragon::hid::parse(*hidMap,a.value,a.length);ok=dragon::hid::decode(*hidMap,id,kind-1,data,count,output,sizeof(output));Serial.printf("LAB_HID_DECODE %s\n",output);}Serial.println(ok?"LAB_END":"LAB_ERROR map or report");
 }
 else if(c=="lab hid events"&&!working()){char decoded[400];for(unsigned i=0;i<eventCount;++i){auto& e=events[(eventHead+64-eventCount+i)%64];if(hidEvent(e,decoded,sizeof(decoded)))Serial.printf("LAB_HID_EVENT at=%lu index=%u %s\n",e.at,e.index,decoded);}Serial.println("LAB_END");}
 else if(c=="lab fingerprints"){for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(!a.used||millis()-a.at>=60000)continue;auto insight=inspectAdvertisement(a.data,a.length,a.boundary);char mac[18];macText(a.mac,mac);Serial.printf("LAB_FINGERPRINT %s %u rssi=%d company=%04x appearance=%u tx=%s%d protocol=%s / vendor=%s / address=%s\n",mac,a.type,a.rssi,insight.company,insight.appearance,insight.hasPower?"":"unknown/",insight.power,insight.protocol,companyName(insight.company),addressName(a.type,a.mac));}Serial.println("LAB_END");}
 else if(c.startsWith("lab read ")||c.startsWith("lab subscribe ")){bool read=c.startsWith("lab read ");unsigned i;char tail;bool parse=sscanf(c.c_str(),read?"lab read %u %c":"lab subscribe %u %c",&i,&tail)==1;Serial.println(parse&&i<profile->count&&(read?profile->attributes[i].kind!=Service:remote[i]!=nullptr)&&start(read?Read:Subscribe,i)?"LAB_OK pending":"LAB_ERROR attribute or connection");}
 else if(c.startsWith("lab notify ")&&state==Emulating){unsigned i;char tail;if(sscanf(c.c_str(),"lab notify %u %c",&i,&tail)==1&&i<profile->count&&local[i]){bool ok=profile->attributes[i].properties&16?local[i]->notify():local[i]->indicate();Serial.println(ok?"LAB_OK notified":"LAB_ERROR no subscribed client");}else Serial.println("LAB_ERROR attribute");}
 else if(c=="lab storage initialize"&&!owned&&!storeReady){auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"recon");if(p&&p->address==0x1000000&&p->size==0x1000000&&ESP.getFlashChipSize()>=0x2000000){storeReady=recon.begin(true,"/recon",4,"recon");if(storeReady){recon.mkdir("/profiles");recon.mkdir("/events");}}Serial.println(storeReady?"LAB_OK storage initialized":"LAB_ERROR storage");}
 else if(c=="lab adverts"){for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(!a.used||millis()-a.at>=60000)continue;char mac[18];macText(a.mac,mac);String line=String("LAB_AD ")+mac+" "+a.type+" "+a.rssi+" "+a.boundary+" "+hex(a.data,a.length,MaxAdvertisement)+"\n";serialWrite((const uint8_t*)line.c_str(),line.length());}Serial.println("LAB_END");}
 else if(c=="lab nearby"){for(auto& d:contacts)if(d.used&&d.ble&&millis()-d.seen<60000){char mac[18];macText(d.mac,mac);String name=d.name;name.replace('\n',' ');name.replace('\r',' ');Serial.printf("LAB_DEVICE %s %u %d %s\n",mac,d.addressType,d.rssi,name.c_str());}Serial.println("LAB_END");}
 else if(c=="lab list")fileList();
 else if(c.startsWith("lab save "))Serial.println(save(c.substring(9).c_str())?"LAB_OK saved":"LAB_ERROR save/name/storage/busy");
 else if((c.startsWith("lab load ")||c.startsWith("lab compare "))&&!owned){bool cmp=c.startsWith("lab compare ");auto* temp=(Profile*)ps_calloc(1,sizeof(Profile));bool ok=temp&&readProfile(c.substring(cmp?12:9).c_str(),*temp);if(ok){if(cmp){auto d=compare(*profile,*temp);Serial.printf("LAB_DIFF added=%u removed=%u changed=%u\n",d.added,d.removed,d.changed);}else{memcpy(profile,temp,sizeof(Profile));say("Saved profile loaded");}Serial.println("LAB_OK");}else Serial.println("LAB_ERROR file or CRC");free(temp);}
 else if(c=="lab export")exportProfile();
 else if(c.startsWith("lab import "))importProfile(c);
 else if(c=="lab log on"||c=="lab log off"){logEnabled=c.endsWith("on")&&storeReady;Serial.printf("LAB_LOG %u\n",logEnabled);}
 else if(c=="lab log export current"||c=="lab log export previous")exportLog(c.endsWith("previous"));
 else if(c=="lab events"){for(unsigned i=0;i<eventCount;++i){auto& e=events[(eventHead+64-eventCount+i)%64];String line=String("LAB_EVENT ")+e.at+" "+e.kind+" "+e.index+" "+e.length+" "+hex(e.data,e.length,MaxValue)+"\n";if(!serialWrite((const uint8_t*)line.c_str(),line.length()))break;}Serial.println("LAB_END");}
 else Serial.println("LAB_ERROR unknown command or busy");return true;
}

// Touch UI is built on the app task only. BLE callbacks never touch LVGL.
namespace {
enum View {Overview,Nearby,AdvertisementView,Gatt,AttributeView,EventView,Saved,WriteView};View view=Overview;
lv_obj_t* viewRoot=nullptr;lv_obj_t* viewBody=nullptr;lv_obj_t* viewStatus=nullptr;uint32_t drawn=0,lastView=0;unsigned attrIndex=0;bool redraw=true;lv_obj_t* hexInput=nullptr;bool writeNoResponse=false;char writeEditor[65]{};
dragon::DeviceKey nearbyKeys[20]{};char savedNames[48][33]{};unsigned savedCount=0;
void drawLab();
String props(uint8_t p){String s;if(p&2)s+="READ ";if(p&4)s+="WRITE-NR ";if(p&8)s+="WRITE ";if(p&16)s+="NOTIFY ";if(p&32)s+="INDICATE ";return s.length()?s:"No read/write access";}
const char* valueState(uint8_t flags){return flags&ReadFailed?"Read unavailable / protected or rejected":flags&ValueTruncated?"Value truncated":flags&NotRead?"Not readable / no value captured":"Value available in workspace";}
lv_obj_t* text(lv_obj_t* p,const String& s,int x,int y,int w,const lv_font_t* f=font18(),uint32_t c=uiColors().ink){auto* o=lv_label_create(p);lv_label_set_text(o,s.c_str());dragon::labui::pos(o,x,y);dragon::labui::width(o,w);lv_obj_set_style_text_font(o,uiFont(f),0);lv_obj_set_style_text_color(o,lv_color_hex(c),0);return o;}
void tap(lv_event_t* e);
void btn(lv_obj_t* p,const String& name,int x,int y,int w,int h,int id){auto* b=lv_btn_create(p);dragon::labui::pos(b,x,y);dragon::labui::size(b,w,h);lv_obj_set_style_bg_color(b,lv_color_hex(uiColors().surface),0);lv_obj_set_style_border_color(b,lv_color_hex(uiColors().border),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_radius(b,uiAscii()?0:12,0);lv_obj_add_event_cb(b,tap,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* l=text(b,name,0,0,w-12);lv_obj_center(l);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);}
void entry(const String& a,const String& b,unsigned row,int id){auto* button=lv_btn_create(viewBody);dragon::labui::pos(button,0,row*72);dragon::labui::size(button,334,66);lv_obj_set_style_bg_color(button,lv_color_hex(uiColors().surface),0);lv_obj_set_style_radius(button,uiAscii()?0:10,0);lv_obj_set_style_pad_all(button,px(9),0);lv_obj_add_event_cb(button,tap,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* top=text(button,a,0,0,312,font16());lv_label_set_long_mode(top,LV_LABEL_LONG_DOT);dragon::labui::height(top,21);auto* detail=text(button,b,0,27,312,font14(),uiColors().muted);lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);dragon::labui::height(detail,18);}
void tap(lv_event_t* e){int id=(intptr_t)lv_event_get_user_data(e);if(id==1){bleLabStop();uiShow("radar");return;}if(id==2){view=Overview;redraw=true;return;}if(id==3){bleLabStop();redraw=true;return;}
 if(id==4020||id==4021){bleLabCommand(id==4020?"lab confirm yes":"lab confirm no");redraw=true;return;}
 if(id>=4000&&id<4010&&hexInput){char c[2]={char('0'+id-4000),0};lv_textarea_add_text(hexInput,c);return;}
 if(id==4010&&hexInput){lv_textarea_del_char(hexInput);return;}
 if(id==4011&&hexInput){bleLabCommand(String("lab passkey ")+lv_textarea_get_text(hexInput));redraw=true;return;}
 if(working())return;
 if(id>=3000&&id<=3015&&hexInput){const char chars[]="0123456789ABCDEF";char c[2]={chars[id-3000],0};lv_textarea_add_text(hexInput,c);return;}
 if(id==3016&&hexInput){lv_textarea_del_char(hexInput);return;}
 if(id==3017&&hexInput){String command=String(writeNoResponse?"lab writenr ":"lab write ")+attrIndex+" "+lv_textarea_get_text(hexInput);bleLabCommand(command);view=AttributeView;redraw=true;return;}
 if(id==3018){if(hexInput)strlcpy(writeEditor,lv_textarea_get_text(hexInput),sizeof(writeEditor));writeNoResponse=!writeNoResponse;redraw=true;return;}
 if(id>=100&&id<120&&!owned){auto k=nearbyKeys[id-100];int i=findContact(k);if(i>=0)bleLabSelect(k.mac,k.addressType,contacts[i].name);view=AdvertisementView;}
 else if(id>=1000&&id<1000+int(profile->count)){attrIndex=id-1000;view=AttributeView;}
 else if(id>=2000&&id<2000+int(savedCount)&&!owned){if(readProfile(savedNames[id-2000],*profile)){say("Saved profile loaded");view=Gatt;}else say("Profile CRC/read failed");}
 else switch(id){case 10:view=Nearby;break;case 11:view=Gatt;break;case 12:view=EventView;break;case 13:view=Saved;break;case 14:if(!owned){demo(*profile);selectedName[0]=0;say("Demo profile ready");view=Gatt;}break;
 case 15:logEnabled=!logEnabled&&storeReady;break;case 16:{if(!owned){auto* box=uiMessageBox(viewRoot,"Emulate this profile?","Creates a connectable local BLE peripheral. Radar pauses until Stop or Home. Bond keys and original device behavior are not copied.",nullptr,true);dragon::labui::size(box,340,265);lv_obj_center(box);btn(box,"START EMULATION",25,175,270,45,17);}return;}
 case 17:start(Clone);view=Overview;break;case 18:{char n[33];snprintf(n,sizeof(n),"profile-%08lx",esp_random());save(n);break;}
 case 19:view=AdvertisementView;break;case 20:start(Discover);view=Overview;break;case 21:{if(!owned){dragon::DeviceKey k{};k.radio=1;k.addressType=selectedType;memcpy(k.mac,selectedMac,6);if(huntDevice(k,0))uiShow("hunt");}return;}
 case 26:if(!owned){pairingEnabled=!pairingEnabled;pairingMitm=true;pairingBond=false;say(pairingEnabled?"Pairing enabled / authenticated / no stored bond":"Pairing disabled");}break;
 case 27:if(!owned){replayIdentity=false;start(Replay);view=Overview;}break;
 case 29:start(Bridge);view=Overview;break;
 case 28:if(!tagSound())say("Connect + discover a supported tracker sound service first");break;
 case 25:writeEditor[0]=0;writeNoResponse=!(profile->attributes[attrIndex].properties&8);view=WriteView;break;
 case 22:start(Read,attrIndex);break;case 23:start(subscriptions[attrIndex]?Unsubscribe:Subscribe,attrIndex);break;
 case 24:if(state==Emulating&&local[attrIndex]){if(profile->attributes[attrIndex].properties&16)local[attrIndex]->notify();else if(profile->attributes[attrIndex].properties&32)local[attrIndex]->indicate();}break;
 }redraw=true;
}
void drawLab(){
 int scroll=!redraw&&viewBody?lv_obj_get_scroll_y(viewBody):0;
 lv_obj_clean(viewRoot);hexInput=nullptr;text(viewRoot,"BLE LAB",137,42,192,font26(),uiColors().accent);viewStatus=text(viewRoot,String(stateName())+" / "+(owned?"RADAR PAUSED":"RADAR LISTENING"),69,79,328,font16(),uiColors().muted);
 viewBody=lv_obj_create(viewRoot);dragon::labui::pos(viewBody,57,111);dragon::labui::size(viewBody,352,267);lv_obj_set_style_bg_opa(viewBody,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(viewBody,0,0);lv_obj_set_style_pad_all(viewBody,0,0);
 btn(viewRoot,"HOME",123,397,102,42,1);btn(viewRoot,view==Overview?"STOP":"BACK",239,397,102,42,view==Overview?3:2);
 if(pairRequest){char pairingCode[7];snprintf(pairingCode,sizeof(pairingCode),"%06lu",pairCode.load());text(viewBody,pairRequest==2?String("Confirm matching code on BOTH devices\n\n")+String(pairingCode):String("Enter the six-digit code shown by your peripheral"),4,4,326);if(pairRequest==2){btn(viewBody,"MATCH",0,145,160,44,4020);btn(viewBody,"REJECT",174,145,160,44,4021);}else{hexInput=lv_textarea_create(viewBody);dragon::labui::pos(hexInput,4,80);dragon::labui::size(hexInput,326,48);lv_textarea_set_max_length(hexInput,6);lv_textarea_set_accepted_chars(hexInput,"0123456789");lv_textarea_set_one_line(hexInput,true);lv_textarea_set_text(hexInput,"");for(int i=0;i<10;++i){char label[2]={char('0'+i),0};btn(viewBody,label,(i%5)*67,142+(i/5)*46,61,40,4000+i);}btn(viewBody,"DELETE",0,242,160,44,4010);btn(viewBody,"SUBMIT",174,242,160,44,4011);}drawn=revision;redraw=false;return;}
 if(working()){text(viewBody,"Working...\n\nThe interface remains responsive.\nStop or Home cancels after the current Bluetooth request.",5,5,322);drawn=revision;redraw=false;return;}
 if(view==Overview){unsigned n=0;text(viewBody,message,3,0,327,font16(),uiColors().accent);auto* panel=lv_obj_create(viewBody);lv_obj_remove_style_all(panel);dragon::labui::pos(panel,0,66);dragon::labui::size(panel,334,8*72);auto* original=viewBody;viewBody=panel;
  if(*selectedName)entry("Selected peripheral",selectedName,n++,19);entry("Nearby BLE","Advertisement inspector / GATT",n++,10);entry(pairingEnabled?"Pairing enabled":"Pairing disabled","Toggle before connecting / no stored bonds",n++,26);entry("Tracker sound request","Connected Find My / DULT device only",n++,28);entry("GATT workspace",String(profile->count)+" attributes / "+profile->name,n++,11);entry("Live events",String(eventCount)+" retained / "+eventDrops.load()+" dropped",n++,12);entry("Saved profiles",storeReady?String(recon.usedBytes()/1024)+" / "+String(recon.totalBytes()/1024)+" KB flash used":"Storage needs initialization",n++,13);if(state==Monitoring)entry("Start GATT bridge","Cached reads / forwards writes + notifications",n++,29);entry("Emulate profile","Local peripheral / explicit start",n++,16);entry("Save workspace","Unique filename; never overwrites",n++,18);entry(logEnabled?"Event logging: ON":"Event logging: OFF","CSV / two rotating CSV logs",n++,15);entry("Load demo profile","Battery + read/write/notify",n++,14);dragon::labui::height(panel,n*72);viewBody=original;
 }else if(view==Nearby){unsigned n=0;int order[MAX_CONTACTS];unsigned total=0;for(unsigned i=0;i<MAX_CONTACTS;++i)if(contacts[i].used&&contacts[i].ble&&millis()-contacts[i].seen<60000)order[total++]=i;std::sort(order,order+total,[](int a,int b){return contacts[a].rssi>contacts[b].rssi;});for(unsigned j=0;j<total&&n<20;++j){auto& c=contacts[order[j]];nearbyKeys[n]=contactKey(c);char mac[18];macText(c.mac,mac);entry(*c.name?String(c.name):String(mac),String(c.rssi)+" dBm / "+addressName(c.addressType,c.mac),n,100+n);++n;}if(!n)text(viewBody,"Waiting for advertisements.\nStop the connection to scan again.",4,15,326);
 }else if(view==AdvertisementView){char mac[18];macText(selectedMac,mac);String s=String(selectedName)+"\n"+mac+"\n"+addressName(selectedType,selectedMac)+"\n";for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(a.used&&a.type==selectedType&&!memcmp(a.mac,selectedMac,6)){s+=String(a.rssi)+" dBm / "+((millis()-a.at)/1000)+"s ago\n";auto insight=inspectAdvertisement(a.data,a.length,a.boundary);s+=String(insight.protocol)+"\n"+companyName(insight.company)+" / CID "+String(insight.company,HEX)+"\nAppearance "+insight.appearance+"\n";if(insight.hasPower)s+="TX power "+String(insight.power)+" dBm (not distance)\n";for(size_t off=0;off<a.length;){if(off==a.boundary)s+="SCAN RESPONSE\n";uint8_t n=a.data[off++];if(!n){off=off<=a.boundary?a.boundary:a.length;continue;}uint8_t type=a.data[off++];s+="AD 0x"+String(type,HEX)+" / "+String(n-1)+" bytes\n"+hex(a.data+off,n-1)+"\n";off+=n-1;}break;}}auto* l=text(viewBody,s,4,0,326,font16());lv_obj_update_layout(l);int y=logical(lv_obj_get_height(l))+18;btn(viewBody,"CONNECT + DISCOVER",0,y,334,48,20);btn(viewBody,"HUNT THIS SIGNAL",0,y+59,334,48,21);btn(viewBody,"REPLAY ADVERT / 30s",0,y+118,334,48,27);
 }else if(view==Gatt){unsigned n=0;for(unsigned i=0;i<profile->count;++i){auto& a=profile->attributes[i];String top=String(a.kind==Service?"SERVICE ":a.kind==Characteristic?"CHAR ":"DESC ")+(uuidName(a.uuid)?uuidName(a.uuid):a.uuid);entry(top,a.kind==Service?"Primary service":a.kind==Characteristic?props(a.properties):String(a.length)+" bytes / "+valueState(a.flags),n++,1000+i);}if(!n)text(viewBody,"Connect to a peripheral, load a saved profile, or load the demo.",4,16,326);
 }else if(view==AttributeView&&attrIndex<profile->count){auto& a=profile->attributes[attrIndex];String s=String(uuidName(a.uuid)?uuidName(a.uuid):"Custom attribute")+"\n"+a.uuid+"\nIndex "+attrIndex+" / handle "+a.handle+"\n"+props(a.properties)+"\n"+valueState(a.flags)+"\nLength "+a.length+" bytes\n\n"+hex(a.value,a.length,128);auto* l=text(viewBody,s,4,0,326,font16());lv_obj_update_layout(l);int y=logical(lv_obj_get_height(l))+16;if(uuidEqual(a.uuid,"2a4b")&&!a.flags){auto h=parseHidMap(a.value,a.length);String layout=String("HID map: ")+h.error;for(unsigned i=0;h.valid&&i<h.count;++i){auto& r=h.reports[i];layout+="\nID "+String(r.id)+" / bits I:"+r.bits[0]+" O:"+r.bits[1]+" F:"+r.bits[2];}auto* report=text(viewBody,layout,4,y,326,font16());lv_obj_update_layout(report);y+=logical(lv_obj_get_height(report))+14;}if(a.kind==Characteristic){if(state==Monitoring){if(a.properties&2)btn(viewBody,"READ VALUE",0,y,334,46,22);if(a.properties&12)btn(viewBody,"WRITE HEX VALUE",0,y+112,334,46,25);if(a.properties&48)btn(viewBody,subscriptions[attrIndex]?"UNSUBSCRIBE":"SUBSCRIBE",0,y+56,334,46,23);}if(state==Emulating&&(a.properties&48))btn(viewBody,"SEND CURRENT VALUE",0,y,334,46,24);}
 }else if(view==WriteView){text(viewBody,String("WRITE #")+attrIndex+" / "+(writeNoResponse?"NO RESPONSE":"WITH RESPONSE"),3,0,328,font16());hexInput=lv_textarea_create(viewBody);dragon::labui::pos(hexInput,3,30);dragon::labui::size(hexInput,328,66);lv_textarea_set_max_length(hexInput,64);lv_textarea_set_accepted_chars(hexInput,"0123456789ABCDEF");lv_textarea_set_placeholder_text(hexInput,"Hex bytes / 32 bytes maximum");lv_textarea_set_text(hexInput,writeEditor);const char chars[]="0123456789ABCDEF";for(int i=0;i<16;++i){char label[2]={chars[i],0};btn(viewBody,label,(i%4)*84,108+(i/4)*42,78,38,3000+i);}btn(viewBody,"DELETE",0,284,162,42,3016);btn(viewBody,"SEND",174,284,162,42,3017);if((profile->attributes[attrIndex].properties&12)==12)btn(viewBody,"TOGGLE RESPONSE",0,336,334,42,3018);
 }else if(view==EventView){String s;for(unsigned i=0;i<std::min(eventCount,16u);++i){auto& e=events[(eventHead+63-i)%64];s+=String(e.at/1000)+"s / #"+e.index+" / "+(e.kind==0?"notify":e.kind==1?"indicate":e.kind==2?"local write":e.kind==3?"peer connected":e.kind==5?"remote write":"peer left")+"\n"+hex(e.data,e.length,24)+"\n";char decoded[400];if(hidEvent(e,decoded,sizeof(decoded)))s+=String(decoded)+"\n";s+="\n";}text(viewBody,s.length()?s:"No notifications or local writes yet.",4,0,326,font16());
 }else if(view==Saved){savedCount=0;if(storeReady){File d=recon.open("/profiles");for(File f=d.openNextFile();f&&savedCount<48;f=d.openNextFile()){String n=f.name();if(!f.isDirectory()&&n.endsWith(".ddg")){n.remove(n.length()-4);if(safeName(n.c_str())){strlcpy(savedNames[savedCount],n.c_str(),33);entry(n,String(f.size())+" bytes",savedCount,2000+savedCount);++savedCount;}}f.close();}d.close();}if(!savedCount)text(viewBody,"No saved profiles.\nSave a discovered workspace first.\nUSB export/import is in the web BLE Lab.",4,15,326);}
 lv_obj_update_layout(viewBody);if(scroll)lv_obj_scroll_to_y(viewBody,scroll,LV_ANIM_OFF);drawn=revision;redraw=false;
}
}
void bleLabView(void* parent){viewRoot=(lv_obj_t*)parent;viewBody=nullptr;view=Overview;redraw=true;drawLab();}
void bleLabViewLoop(){if(!viewRoot)return;uint32_t now=millis();if((redraw||(view!=WriteView&&drawn!=revision))&&now-lastView>500){lastView=now;drawLab();}}
#endif
