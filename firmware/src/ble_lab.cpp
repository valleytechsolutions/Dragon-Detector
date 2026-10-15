// Dragon BLE Lab. Original MIT implementation; no third-party firmware code.
#include "ble_lab.h"
#ifdef BOARD_AMOLED_175C
#include "app.h"
#include <NimBLEDevice.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <atomic>
#include <lvgl.h>

using namespace dragon::lab;
namespace {
enum State:uint8_t {Idle,Working,Monitoring,Emulating,Failed};
enum Job:uint8_t {None,Discover,Clone,Stop,Subscribe,Read};
std::atomic<State> state{Idle};std::atomic<bool> owned{false},cancelled{false},finished{false};
std::atomic<uint32_t> revision{1},eventDrops{0};
bool resumeRadio=true,rebind=false,storeReady=false;
Job job=None;unsigned jobIndex=0;TaskHandle_t workerTask=nullptr;
StaticSemaphore_t jobControl;SemaphoreHandle_t jobReady=nullptr;
Profile* profile=nullptr;fs::LittleFSFS recon;
char message[120]="Select an owned BLE peripheral to inspect",selectedName[48]{};
uint8_t selectedMac[6]{},selectedType=0;
NimBLEClient* client=nullptr;
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
Event* events=nullptr;unsigned eventCount=0,eventHead=0;
uint32_t loggedBytes=0;bool logEnabled=false;
ble_gap_event_listener notifyListener{};bool notifyListenerActive=false;
void say(const char* s){strlcpy(message,s,sizeof(message));++revision;}
void queueEvent(unsigned index,const uint8_t* data,size_t length,uint8_t kind){Event e{};e.at=millis();e.index=index;e.length=std::min<size_t>(length,MaxValue);e.kind=kind;if(e.length)memcpy(e.data,data,e.length);if(!eventQueue||xQueueSend(eventQueue,&e,0)!=pdTRUE)++eventDrops;}
int onGap(ble_gap_event* event,void*){if(event->type==BLE_GAP_EVENT_NOTIFY_RX&&client&&event->notify_rx.conn_handle==client->getConnHandle()){for(unsigned i=0;i<profile->count;++i){const auto& a=profile->attributes[i];if(a.kind!=Characteristic||a.handle!=event->notify_rx.attr_handle)continue;uint8_t data[MaxValue];size_t n=std::min<size_t>(MaxValue,OS_MBUF_PKTLEN(event->notify_rx.om));os_mbuf_copydata(event->notify_rx.om,0,n,data);queueEvent(i,data,n,event->notify_rx.indication?1:0);break;}}return 0;}
int onSubscribe(uint16_t,const ble_gatt_error* error,ble_gatt_attr*,void*){readContext.status=error->status;xSemaphoreGive(readDone);return 0;}
bool subscribe(NimBLERemoteCharacteristic* c){
 uint16_t handle=0;for(unsigned i=0;i<profile->count;++i){const auto& a=profile->attributes[i];if(a.kind==Descriptor&&a.parent==jobIndex&&!strcmp(a.uuid,"2902")){handle=a.handle;break;}}if(!handle)return false;
 if(!notifyListenerActive){if(ble_gap_event_listener_register(&notifyListener,onGap,nullptr))return false;notifyListenerActive=true;}
 uint8_t value[2]={uint8_t(c->canNotify()?1:2),0};readContext.status=0;xSemaphoreTake(readDone,0);
 int rc=ble_gattc_write_flat(client->getConnHandle(),handle,value,sizeof(value),onSubscribe,nullptr);if(!rc){xSemaphoreTake(readDone,portMAX_DELAY);rc=readContext.status;}return rc==0;
}
class CloneCallbacks:public NimBLECharacteristicCallbacks {void onWrite(NimBLECharacteristic* c,NimBLEConnInfo&) override{for(unsigned i=0;i<MaxAttributes;++i)if(local[i]==c){auto v=c->getValue();queueEvent(i,v.data(),v.size(),2);break;}}} cloneCallbacks;
class ServerCallbacks:public NimBLEServerCallbacks {void onConnect(NimBLEServer*,NimBLEConnInfo&) override{queueEvent(0,nullptr,0,3);}void onDisconnect(NimBLEServer*,NimBLEConnInfo&,int) override{queueEvent(0,nullptr,0,4);}} serverCallbacks;
bool working(){return state.load()==Working;}
bool memoryOkay(){return ESP.getFreeHeap()>26000&&!cancelled.load();}
void value(Attribute& a,const NimBLEAttValue& v,bool okay){a.length=std::min<size_t>(MaxValue,v.size());if(a.length)memcpy(a.value,v.data(),a.length);a.flags=okay?0:ReadFailed;if(v.size()>MaxValue)a.flags|=ValueTruncated;}
void disconnect(){if(notifyListenerActive){ble_gap_event_listener_unregister(&notifyListener);notifyListenerActive=false;}if(client){if(client->isConnected())client->disconnect();else client->cancelConnect();uint32_t at=millis();while(client->isConnected()&&millis()-at<2500)delay(10);}}
void stopWorker(){disconnect();if(rebind){auto* server=NimBLEDevice::getServer();NimBLEDevice::getAdvertising()->stop();if(server){server->advertiseOnDisconnect(false);for(auto h:server->getPeerDevices())server->disconnect(h);uint32_t at=millis();while(server->getConnectedCount()&&millis()-at<2500)delay(10);}NimBLEDevice::deinit(true);client=nullptr;memset(remote,0,sizeof(remote));memset(local,0,sizeof(local));}finished=true;}
void discover(){
 init(*profile,selectedName);memcpy(profile->mac,selectedMac,6);profile->addressType=selectedType;
 for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(a.used&&a.type==selectedType&&!memcmp(a.mac,selectedMac,6)){profile->advertisementLength=a.length;profile->scanBoundary=a.boundary;memcpy(profile->advertisement,a.data,a.length);break;}}
 char mac[18];macText(selectedMac,mac);client=NimBLEDevice::createClient();if(!client){say("Cannot allocate BLE client");stopWorker();return;}rebind=true;
 client->setConnectTimeout(7000);client->setConnectionParams(24,40,0,200);
 // A peer may negotiate MTU first. NimBLE 2.3.6 otherwise reports EALREADY
 // as a failed connection. Discovery and long reads also work at MTU 23.
 if(!client->connect(NimBLEAddress(std::string(mac),selectedType),true,false,false)){snprintf(message,sizeof(message),"Connection failed (%d). Device may not be connectable.",client->getLastError());++revision;stopWorker();return;}
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
 if(cancelled||!client->isConnected()){say("Disconnected. Partial snapshot retained; radar resumed.");profile->flags|=1;stopWorker();return;}
 snprintf(message,sizeof(message),"Connected / %u attributes%s",profile->count,profile->flags?" / PARTIAL":"");++revision;state=Monitoring;
}
uint32_t properties(uint8_t p){return (p&1?NIMBLE_PROPERTY::BROADCAST:0)|(p&2?NIMBLE_PROPERTY::READ:0)|(p&4?NIMBLE_PROPERTY::WRITE_NR:0)|(p&8?NIMBLE_PROPERTY::WRITE:0)|(p&16?NIMBLE_PROPERTY::NOTIFY:0)|(p&32?NIMBLE_PROPERTY::INDICATE:0);}
void clone(){
 if(!profile->count||profile->flags||!valid(*profile,bytes(*profile))){say("Emulation needs a complete, valid GATT profile");finished=true;return;}
 auto* server=NimBLEDevice::createServer();if(!server){say("Could not allocate GATT server");finished=true;return;}rebind=true;server->setCallbacks(&serverCallbacks,false);server->advertiseOnDisconnect(true);
 NimBLEService* services[MaxAttributes]{};unsigned cloned=0,skipped=0;
 for(unsigned i=0;i<profile->count;++i){auto& a=profile->attributes[i];NimBLEUUID uuid(a.uuid);
  if(!memoryOkay()){say("Emulation stopped: memory reserve reached");stopWorker();return;}
  if(a.kind==Service){if(uuid==NimBLEUUID(uint16_t(0x1800))||uuid==NimBLEUUID(uint16_t(0x1801))){++skipped;continue;}services[i]=server->createService(uuid);}
  else if(a.kind==Characteristic){if(!services[a.parent])continue;local[i]=services[a.parent]->createCharacteristic(uuid,properties(a.properties),MaxValue);if(!local[i]){say("Characteristic allocation failed");stopWorker();return;}local[i]->setValue(a.value,a.length);local[i]->setCallbacks(&cloneCallbacks);++cloned;}
  else{if(!local[a.parent]||uuid==NimBLEUUID(uint16_t(0x2902)))continue;auto* d=local[a.parent]->createDescriptor(uuid,NIMBLE_PROPERTY::READ,MaxValue);if(d)d->setValue(a.value,a.length);}
 }
 if(!cloned){say("No application characteristics in this profile");stopWorker();return;}
 for(auto* s:services)if(s&&!s->start()){say("GATT service registration failed");stopWorker();return;}
 NimBLEDevice::setDeviceName(profile->name);server->start();auto* adv=NimBLEDevice::getAdvertising();adv->reset();adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);adv->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
 NimBLEAdvertisementData data,scan;bool captured=profile->scanBoundary>0&&profile->scanBoundary<=31&&profile->advertisementLength-profile->scanBoundary<=31;
 if(captured){data.addData(profile->advertisement,profile->scanBoundary);scan.addData(profile->advertisement+profile->scanBoundary,profile->advertisementLength-profile->scanBoundary);}
 else{data.setFlags(6);scan.setName(std::string(profile->name).substr(0,29));for(auto* s:services)if(s){data.addServiceUUID(s->getUUID());break;}}
 adv->setAdvertisementData(data);adv->setScanResponseData(scan);adv->enableScanResponse(true);
 if(!adv->start()){say("Advertising failed; radar restored");stopWorker();return;}
 snprintf(message,sizeof(message),"Emulating %u characteristics / %s adverts",cloned,captured?"captured":"generated");++revision;state=Emulating;
}
void worker(void*){for(;;){xSemaphoreTake(jobReady,portMAX_DELAY);Job run=job;
 // NimBLE uses this task's notification slot internally. Jobs must use a
 // separate semaphore, otherwise a late GATT completion replays the last job.
 ulTaskNotifyTake(pdTRUE,0);
 if(run==Stop)stopWorker();else if(run==Discover)discover();else if(run==Clone)clone();
 else if((run==Read||run==Subscribe)&&client&&client->isConnected()&&jobIndex<profile->count&&remote[jobIndex]){
  auto* c=remote[jobIndex];auto& a=profile->attributes[jobIndex];bool ok=false;
  if(run==Read){ok=readAttribute(a);}
  else if(c->canNotify()||c->canIndicate())ok=subscribe(c);
  say(ok?(run==Read?"Value refreshed":"Subscribed; notifications appear in Events"):"Operation rejected / may require pairing or permission");state=Monitoring;
 }else if(run!=None){say("Connection no longer available");stopWorker();}
 }}
bool start(Job next,unsigned index=0){if(!profile||!workerTask||working()||finished)return false;
 if(next==Discover||next==Clone){if(owned)return false;resumeRadio=radioEnabled;setRadio(false);owned=true;cancelled=false;eventCount=eventHead=0;eventDrops=0;}
 else if(next!=Stop&&state!=Monitoring)return false;
 job=next;jobIndex=index;state=Working;say(next==Discover?"Connecting...":next==Clone?"Building local GATT peripheral...":next==Stop?"Stopping lab / restoring radar...":"Request pending...");xSemaphoreGive(jobReady);return true;}
String path(const char* name){return String("/profiles/")+name+".ddg";}
bool save(const char* name){if(!storeReady||working()||!safeName(name)||!valid(*profile,bytes(*profile))||!profile->count)return false;String p=path(name);if(recon.exists(p)){say("Name exists. Choose a new name in the web lab.");return false;}
 size_t length=bytes(*profile);if(recon.totalBytes()-recon.usedBytes()<length+16384)return false;recon.remove(p+".part");File f=recon.open(p+".part",FILE_WRITE);if(!f)return false;uint32_t crc=checksum((uint8_t*)profile,length);bool ok=f.write((uint8_t*)profile,length)==length&&f.write((uint8_t*)&crc,4)==4;f.flush();f.close();if(ok)ok=recon.rename(p+".part",p);if(!ok)recon.remove(p+".part");say(ok?"Profile saved to dedicated recon flash":"Profile save failed");return ok;}
bool readProfile(const char* name,Profile& p){if(!storeReady||!safeName(name))return false;File f=recon.open(path(name));size_t n=f?f.size():0;if(n<HeaderSize+4||n>sizeof(Profile)+4){f.close();return false;}memset(&p,0,sizeof(p));uint32_t crc=0;bool ok=f.read((uint8_t*)&p,n-4)==n-4&&f.read((uint8_t*)&crc,4)==4;f.close();return ok&&checksum((uint8_t*)&p,n-4)==crc&&valid(p,n-4);}
void fileList(){Serial.println("LAB_PROFILES");if(storeReady){File d=recon.open("/profiles");for(File f=d.openNextFile();f;f=d.openNextFile()){if(!f.isDirectory()&&String(f.name()).endsWith(".ddg"))Serial.printf("LAB_PROFILE %s %u\n",f.name(),unsigned(f.size()));f.close();}d.close();}Serial.println("LAB_END");}
bool serialWrite(const uint8_t* p,size_t n){uint32_t at=millis();while(n){size_t k=std::min<size_t>(128,std::min(n,size_t(Serial.availableForWrite())));if(k){k=Serial.write(p,k);p+=k;n-=k;at=millis();}else if(millis()-at>5000)return false;delay(1);}return true;}
void exportLog(bool previous){File f=storeReady?recon.open(previous?"/events/previous.csv":"/events/current.csv"):File();if(!f){Serial.println("LAB_ERROR no saved log");return;}bool r=radioEnabled;setRadio(false);Serial.printf("LAB_LOG_BEGIN %u\n",unsigned(f.size()));Serial.println("uptime_ms,kind,attribute,length,hex");uint8_t b[256];bool ok=true;while(f.available()&&ok){size_t n=f.read(b,sizeof(b));ok=n&&serialWrite(b,n);}f.close();Serial.println(ok?"LAB_LOG_END":"LAB_ERROR log transfer");setRadio(r);}
void exportProfile(){if(working()||!profile->count){Serial.println("LAB_ERROR no stable profile");return;}size_t n=bytes(*profile);uint32_t crc=checksum((uint8_t*)profile,n);bool r=radioEnabled;setRadio(false);Serial.printf("LAB_FILE %u %08lx\n",unsigned(n),crc);bool ok=true;char line[540];
 for(size_t off=0;off<n&&ok;off+=256){size_t count=std::min<size_t>(256,n-off);int pos=snprintf(line,sizeof(line),"LAB_DATA %u ",unsigned(off));for(size_t i=0;i<count;++i)pos+=snprintf(line+pos,sizeof(line)-pos,"%02x",((uint8_t*)profile)[off+i]);line[pos++]='\n';ok=serialWrite((uint8_t*)line,pos);}Serial.println(ok?"LAB_FILE_END":"LAB_ERROR transfer");setRadio(r);}
void importProfile(const String& command){unsigned n=0;unsigned long crc=0;char extra;if(owned||sscanf(command.c_str(),"lab import %u %lx %c",&n,&crc,&extra)!=2||n<HeaderSize||n>sizeof(Profile)){Serial.println("LAB_ERROR import length or busy");return;}
 auto* temp=(Profile*)ps_calloc(1,sizeof(Profile));if(!temp){Serial.println("LAB_ERROR memory");return;}bool r=radioEnabled;setRadio(false);Serial.println("LAB_READY");size_t received=0;uint32_t at=millis(),ack=at;
 while(received<n&&millis()-at<5000){size_t k=std::min<size_t>(256,std::min<size_t>(Serial.available(),n-received));if(k){k=Serial.readBytes((uint8_t*)temp+received,k);received+=k;at=ack=millis();Serial.printf("LAB_ACK %u\n\n",unsigned(received));}else{if(millis()-ack>250){Serial.printf("LAB_ACK %u\n\n",unsigned(received));ack=millis();}delay(1);}}
 bool ok=received==n&&checksum((uint8_t*)temp,n)==crc&&valid(*temp,n);if(ok){memcpy(profile,temp,sizeof(Profile));say("Imported profile / review before emulating");}free(temp);setRadio(r);Serial.println(ok?"LAB_OK imported":"LAB_ERROR invalid profile or CRC");}
String hex(const uint8_t* p,size_t n,size_t limit=32){String s;char b[4];for(size_t i=0;i<std::min(n,limit);++i){snprintf(b,sizeof(b),"%02x",p[i]);s+=b;if(i+1<std::min(n,limit))s+=' ';}if(n>limit)s+=" ...";return s;}
const char* stateName(){switch(state.load()){case Working:return "BUSY";case Monitoring:return "CONNECTED";case Emulating:return "EMULATING";case Failed:return "ERROR";default:return "IDLE";}}
}
bool bleLabOwnsRadio(){return owned.load();}
void bleLabSelect(const uint8_t* mac,uint8_t type,const char* name){if(owned)return;memcpy(selectedMac,mac,6);selectedType=type;strlcpy(selectedName,name,sizeof(selectedName));++revision;}
void bleLabObserve(const uint8_t* mac,uint8_t type,int rssi,const uint8_t* data,size_t len,size_t boundary,uint32_t at){if(!advertisements||owned||len>MaxAdvertisement||boundary>len)return;
 if(!advertisementValid(data,boundary)||!advertisementValid(data+boundary,len-boundary))return;
 unsigned slot=0;uint32_t age=0;for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(!a.used||(a.type==type&&!memcmp(a.mac,mac,6))){slot=i;break;}if(uint32_t(at-a.at)>=age){age=at-a.at;slot=i;}}
 auto& a=advertisements[slot];a.used=true;memcpy(a.mac,mac,6);a.type=type;a.rssi=rssi;a.at=at;a.length=len;a.boundary=boundary;memcpy(a.data,data,len);
}
void bleLabBegin(){
 profile=(Profile*)ps_calloc(1,sizeof(Profile));advertisements=(Advertisement*)ps_calloc(CacheCount,sizeof(Advertisement));events=(Event*)ps_calloc(64,sizeof(Event));eventMemory=(uint8_t*)ps_malloc(16*sizeof(Event));
 if(!profile||!advertisements||!events||!eventMemory){say("BLE Lab memory unavailable");return;}
 init(*profile,"");eventQueue=xQueueCreateStatic(16,sizeof(Event),eventMemory,&eventControl);readDone=xSemaphoreCreateBinaryStatic(&readControl);jobReady=xSemaphoreCreateBinaryStatic(&jobControl);
 if(xTaskCreatePinnedToCore(worker,"ble-lab",8192,nullptr,1,&workerTask,0)!=pdPASS){workerTask=nullptr;say("BLE Lab worker unavailable");return;}
 auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"recon");
 if(p&&p->address==0x1000000&&p->size==0x1000000&&ESP.getFlashChipSize()>=0x2000000){storeReady=recon.begin(false,"/recon",4,"recon");if(storeReady){recon.mkdir("/profiles");recon.mkdir("/events");}}
 Serial.printf("LAB_STORAGE ready=%u total=%u used=%u profile_bytes=%u\n",storeReady,storeReady?recon.totalBytes():0,storeReady?recon.usedBytes():0,unsigned(sizeof(Profile)));
}
void bleLabStop(){if(!owned)return;cancelled=true;if(!working())start(Stop);}
void bleLabLoop(){
 if(finished.exchange(false)){if(rebind){radioRebindBle();rebind=false;}owned=false;cancelled=false;state=Idle;setRadio(resumeRadio);if(!strcmp(message,"Stopping lab / restoring radar..."))say("Lab stopped. Radar restored; workspace retained.");++revision;}
 if(cancelled&&owned&&!working())start(Stop);
 if(state==Monitoring&&client&&!client->isConnected()){say("Peripheral disconnected; returning radio to radar");start(Stop);}
 Event e;for(unsigned i=0;eventQueue&&i<8&&xQueueReceive(eventQueue,&e,0)==pdTRUE;++i){events[eventHead]=e;eventHead=(eventHead+1)%64;eventCount=std::min(64u,eventCount+1);++revision;
  if(logEnabled&&storeReady){String line=String(e.at)+","+e.kind+","+e.index+","+e.length+","+hex(e.data,e.length,MaxValue)+"\n";File f=recon.open("/events/current.csv",FILE_APPEND);if(f){if(f.size()+line.length()>1024*1024){f.close();recon.remove("/events/previous.csv");recon.rename("/events/current.csv","/events/previous.csv");f=recon.open("/events/current.csv",FILE_WRITE);}if(f&&recon.totalBytes()-recon.usedBytes()>32768){size_t n=f.print(line);loggedBytes+=n;if(n!=line.length())logEnabled=false;}else logEnabled=false;f.close();}}
 }
}
bool bleLabCommand(const String& c){if(!c.startsWith("lab "))return false;if(!profile||!workerTask){Serial.println("LAB_ERROR unavailable");return true;}
 if(c=="lab status"){Serial.printf("LAB_STATUS state=%s owned=%u storage=%u total=%u used=%u attributes=%u partial=%u events=%u drops=%lu logging=%u logged=%lu heap=%u\n",stateName(),owned.load(),storeReady,storeReady?recon.totalBytes():0,storeReady?recon.usedBytes():0,working()?0:profile->count,working()?0:profile->flags,eventCount,eventDrops.load(),logEnabled,loggedBytes,ESP.getFreeHeap());Serial.printf("LAB_MESSAGE %s\nLAB_END\n",working()?"BLE operation in progress":message);}
 else if(c=="lab stop"){bleLabStop();Serial.println("LAB_OK stopping");}
 else if(c=="lab demo"&&!owned){demo(*profile);selectedName[0]=0;say("Demo loaded: battery + read/write/notify loopback");Serial.println("LAB_OK demo");}
 else if(c=="lab clone"){Serial.println(start(Clone)?"LAB_OK starting":"LAB_ERROR busy");}
 else if(c.startsWith("lab inspect ")){char mac[18];unsigned t;char tail;unsigned m[6];if(sscanf(c.c_str(),"lab inspect %17s %u %c",mac,&t,&tail)==2&&strlen(mac)==17&&t<=3&&sscanf(mac,"%2x:%2x:%2x:%2x:%2x:%2x",m,m+1,m+2,m+3,m+4,m+5)==6&&!owned){uint8_t b[6];for(int i=0;i<6;++i)b[i]=m[i];bleLabSelect(b,t,mac);Serial.println(start(Discover)?"LAB_OK connecting":"LAB_ERROR busy");}else Serial.println("LAB_ERROR address/type or busy");}
 else if(c.startsWith("lab read ")||c.startsWith("lab subscribe ")){bool read=c.startsWith("lab read ");unsigned i;char tail;bool parse=sscanf(c.c_str(),read?"lab read %u %c":"lab subscribe %u %c",&i,&tail)==1;Serial.println(parse&&i<profile->count&&remote[i]&&start(read?Read:Subscribe,i)?"LAB_OK pending":"LAB_ERROR attribute or connection");}
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
 else if(c=="lab events"){for(unsigned i=0;i<eventCount;++i){auto& e=events[(eventHead+64-eventCount+i)%64];Serial.printf("LAB_EVENT %lu %u %u %u %s\n",e.at,e.kind,e.index,e.length,hex(e.data,e.length,MaxValue).c_str());}Serial.println("LAB_END");}
 else Serial.println("LAB_ERROR unknown command or busy");return true;
}

// Touch UI is built on the app task only. BLE callbacks never touch LVGL.
namespace {
enum View {Overview,Nearby,AdvertisementView,Gatt,AttributeView,EventView,Saved};View view=Overview;
lv_obj_t* viewRoot=nullptr;lv_obj_t* viewBody=nullptr;lv_obj_t* viewStatus=nullptr;uint32_t drawn=0,lastView=0;unsigned attrIndex=0;bool redraw=true;
dragon::DeviceKey nearbyKeys[20]{};char savedNames[48][33]{};unsigned savedCount=0;
void drawLab();
String props(uint8_t p){String s;if(p&2)s+="READ ";if(p&4)s+="WRITE-NR ";if(p&8)s+="WRITE ";if(p&16)s+="NOTIFY ";if(p&32)s+="INDICATE ";return s.length()?s:"No read/write access";}
const char* valueState(uint8_t flags){return flags&ReadFailed?"Read unavailable / protected or rejected":flags&ValueTruncated?"Value truncated":flags&NotRead?"Not readable / no value captured":"Value read successfully";}
lv_obj_t* text(lv_obj_t* p,const String& s,int x,int y,int w,const lv_font_t* f=&lv_font_montserrat_18,uint32_t c=0xd7ecdf){auto* o=lv_label_create(p);lv_label_set_text(o,s.c_str());lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_font(o,f,0);lv_obj_set_style_text_color(o,lv_color_hex(c),0);return o;}
void tap(lv_event_t* e);
void btn(lv_obj_t* p,const String& name,int x,int y,int w,int h,int id){auto* b=lv_btn_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);lv_obj_set_style_bg_color(b,lv_color_hex(0x183c31),0);lv_obj_set_style_border_color(b,lv_color_hex(0x447b60),0);lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_radius(b,12,0);lv_obj_add_event_cb(b,tap,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* l=text(b,name,0,0,w-12);lv_obj_center(l);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);}
void entry(const String& a,const String& b,unsigned row,int id){auto* button=lv_btn_create(viewBody);lv_obj_set_pos(button,0,row*72);lv_obj_set_size(button,334,66);lv_obj_set_style_bg_color(button,lv_color_hex(0x183c31),0);lv_obj_set_style_radius(button,10,0);lv_obj_set_style_pad_all(button,9,0);lv_obj_add_event_cb(button,tap,LV_EVENT_CLICKED,(void*)(intptr_t)id);auto* top=text(button,a,0,0,312,&lv_font_montserrat_16);lv_label_set_long_mode(top,LV_LABEL_LONG_DOT);lv_obj_set_height(top,21);auto* detail=text(button,b,0,27,312,&lv_font_montserrat_14,0xa4c5b3);lv_label_set_long_mode(detail,LV_LABEL_LONG_DOT);lv_obj_set_height(detail,18);}
void tap(lv_event_t* e){int id=(intptr_t)lv_event_get_user_data(e);if(id==1){bleLabStop();uiShow("radar");return;}if(id==2){view=Overview;redraw=true;return;}if(id==3){bleLabStop();redraw=true;return;}
 if(working())return;
 if(id>=100&&id<120&&!owned){auto k=nearbyKeys[id-100];int i=findContact(k);if(i>=0)bleLabSelect(k.mac,k.addressType,contacts[i].name);view=AdvertisementView;}
 else if(id>=1000&&id<1000+int(profile->count)){attrIndex=id-1000;view=AttributeView;}
 else if(id>=2000&&id<2000+int(savedCount)&&!owned){if(readProfile(savedNames[id-2000],*profile)){say("Saved profile loaded");view=Gatt;}else say("Profile CRC/read failed");}
 else switch(id){case 10:view=Nearby;break;case 11:view=Gatt;break;case 12:view=EventView;break;case 13:view=Saved;break;case 14:if(!owned){demo(*profile);selectedName[0]=0;say("Demo profile ready");view=Gatt;}break;
 case 15:logEnabled=!logEnabled&&storeReady;break;case 16:{if(!owned){auto* box=lv_msgbox_create(viewRoot,"Emulate this profile?","Creates a connectable local BLE peripheral. Radar pauses until Stop or Home. Bond keys and original device behavior are not copied.",nullptr,true);lv_obj_set_size(box,340,265);lv_obj_center(box);btn(box,"START EMULATION",25,175,270,45,17);}return;}
 case 17:start(Clone);view=Overview;break;case 18:{char n[33];snprintf(n,sizeof(n),"profile-%08lx",esp_random());save(n);break;}
 case 19:view=AdvertisementView;break;case 20:start(Discover);view=Overview;break;case 21:{if(!owned){dragon::DeviceKey k{};k.radio=1;k.addressType=selectedType;memcpy(k.mac,selectedMac,6);if(huntDevice(k,0))uiShow("hunt");}return;}
 case 22:start(Read,attrIndex);break;case 23:start(Subscribe,attrIndex);break;
 case 24:if(state==Emulating&&local[attrIndex]){if(profile->attributes[attrIndex].properties&16)local[attrIndex]->notify();else if(profile->attributes[attrIndex].properties&32)local[attrIndex]->indicate();}break;
 }redraw=true;
}
void drawLab(){
 int scroll=!redraw&&viewBody?lv_obj_get_scroll_y(viewBody):0;
 lv_obj_clean(viewRoot);text(viewRoot,"BLE LAB",137,42,192,&lv_font_montserrat_26,0xc9fc68);viewStatus=text(viewRoot,String(stateName())+" / "+(owned?"RADAR PAUSED":"RADAR LISTENING"),69,79,328,&lv_font_montserrat_16,0x9bcfb7);
 viewBody=lv_obj_create(viewRoot);lv_obj_set_pos(viewBody,57,111);lv_obj_set_size(viewBody,352,267);lv_obj_set_style_bg_opa(viewBody,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(viewBody,0,0);lv_obj_set_style_pad_all(viewBody,0,0);
 btn(viewRoot,"HOME",123,397,102,42,1);btn(viewRoot,view==Overview?"STOP":"BACK",239,397,102,42,view==Overview?3:2);
 if(working()){text(viewBody,"Working...\n\nThe interface remains responsive.\nStop or Home cancels after the current Bluetooth request.",5,5,322);drawn=revision;redraw=false;return;}
 if(view==Overview){unsigned n=0;text(viewBody,message,3,0,327,&lv_font_montserrat_16,0xe5dca2);auto* panel=lv_obj_create(viewBody);lv_obj_remove_style_all(panel);lv_obj_set_pos(panel,0,66);lv_obj_set_size(panel,334,8*72);auto* original=viewBody;viewBody=panel;
  if(*selectedName)entry("Selected peripheral",selectedName,n++,19);entry("Nearby BLE","Advertisement inspector / GATT",n++,10);entry("GATT workspace",String(profile->count)+" attributes / "+profile->name,n++,11);entry("Live events",String(eventCount)+" retained / "+eventDrops.load()+" dropped",n++,12);entry("Saved profiles",storeReady?String(recon.usedBytes()/1024)+" KB used / 16 MB recon flash":"Storage needs initialization",n++,13);entry("Emulate profile","Local peripheral / explicit start",n++,16);entry("Save workspace","Unique filename; never overwrites",n++,18);entry(logEnabled?"Event logging: ON":"Event logging: OFF","CSV / two 1 MB rotating logs",n++,15);entry("Load demo profile","Battery + read/write/notify",n++,14);lv_obj_set_height(panel,n*72);viewBody=original;
 }else if(view==Nearby){unsigned n=0;int order[MAX_CONTACTS];unsigned total=0;for(unsigned i=0;i<MAX_CONTACTS;++i)if(contacts[i].used&&contacts[i].ble&&millis()-contacts[i].seen<60000)order[total++]=i;std::sort(order,order+total,[](int a,int b){return contacts[a].rssi>contacts[b].rssi;});for(unsigned j=0;j<total&&n<20;++j){auto& c=contacts[order[j]];nearbyKeys[n]=contactKey(c);char mac[18];macText(c.mac,mac);entry(*c.name?String(c.name):String(mac),String(c.rssi)+" dBm / "+addressName(c.addressType,c.mac),n,100+n);++n;}if(!n)text(viewBody,"Waiting for advertisements.\nStop the connection to scan again.",4,15,326);
 }else if(view==AdvertisementView){char mac[18];macText(selectedMac,mac);String s=String(selectedName)+"\n"+mac+"\n"+addressName(selectedType,selectedMac)+"\n";for(unsigned i=0;i<CacheCount;++i){auto& a=advertisements[i];if(a.used&&a.type==selectedType&&!memcmp(a.mac,selectedMac,6)){s+=String(a.rssi)+" dBm / "+((millis()-a.at)/1000)+"s ago\n";for(size_t off=0;off<a.length;){if(off==a.boundary)s+="SCAN RESPONSE\n";uint8_t n=a.data[off++];if(!n)break;uint8_t type=a.data[off++];s+="AD 0x"+String(type,HEX)+" / "+String(n-1)+" bytes\n"+hex(a.data+off,n-1)+"\n";off+=n-1;}break;}}auto* l=text(viewBody,s,4,0,326,&lv_font_montserrat_16);lv_obj_update_layout(l);int y=lv_obj_get_height(l)+18;btn(viewBody,"CONNECT + DISCOVER",0,y,334,48,20);btn(viewBody,"HUNT THIS SIGNAL",0,y+59,334,48,21);
 }else if(view==Gatt){unsigned n=0;for(unsigned i=0;i<profile->count;++i){auto& a=profile->attributes[i];String top=String(a.kind==Service?"SERVICE ":a.kind==Characteristic?"CHAR ":"DESC ")+a.uuid;entry(top,a.kind==Service?"Primary service":a.kind==Characteristic?props(a.properties):String(a.length)+" bytes / "+valueState(a.flags),n++,1000+i);}if(!n)text(viewBody,"Connect to a peripheral, load a saved profile, or load the demo.",4,16,326);
 }else if(view==AttributeView&&attrIndex<profile->count){auto& a=profile->attributes[attrIndex];String s=String(a.uuid)+"\nIndex "+attrIndex+" / handle "+a.handle+"\n"+props(a.properties)+"\n"+valueState(a.flags)+"\nLength "+a.length+" bytes\n\n"+hex(a.value,a.length,128);auto* l=text(viewBody,s,4,0,326,&lv_font_montserrat_16);lv_obj_update_layout(l);int y=lv_obj_get_height(l)+16;if(a.kind==Characteristic){if(state==Monitoring){if(a.properties&2)btn(viewBody,"READ VALUE",0,y,334,46,22);if(a.properties&48)btn(viewBody,"SUBSCRIBE",0,y+56,334,46,23);}if(state==Emulating&&(a.properties&48))btn(viewBody,"SEND CURRENT VALUE",0,y,334,46,24);}
 }else if(view==EventView){String s;for(unsigned i=0;i<std::min(eventCount,16u);++i){auto& e=events[(eventHead+63-i)%64];s+=String(e.at/1000)+"s / #"+e.index+" / "+(e.kind==0?"notify":e.kind==1?"indicate":e.kind==2?"local write":e.kind==3?"peer connected":"peer left")+"\n"+hex(e.data,e.length,24)+"\n\n";}text(viewBody,s.length()?s:"No notifications or local writes yet.",4,0,326,&lv_font_montserrat_16);
 }else if(view==Saved){savedCount=0;if(storeReady){File d=recon.open("/profiles");for(File f=d.openNextFile();f&&savedCount<48;f=d.openNextFile()){String n=f.name();if(!f.isDirectory()&&n.endsWith(".ddg")){n.remove(n.length()-4);if(safeName(n.c_str())){strlcpy(savedNames[savedCount],n.c_str(),33);entry(n,String(f.size())+" bytes",savedCount,2000+savedCount);++savedCount;}}f.close();}d.close();}if(!savedCount)text(viewBody,"No saved profiles.\nSave a discovered workspace first.\nUSB export/import is in the web BLE Lab.",4,15,326);}
 lv_obj_update_layout(viewBody);if(scroll)lv_obj_scroll_to_y(viewBody,scroll,LV_ANIM_OFF);drawn=revision;redraw=false;
}
}
void bleLabView(void* parent){viewRoot=(lv_obj_t*)parent;viewBody=nullptr;view=Overview;redraw=true;drawLab();}
void bleLabViewLoop(){if(!viewRoot)return;uint32_t now=millis();if((redraw||drawn!=revision)&&now-lastView>500){lastView=now;drawLab();}}
#endif
