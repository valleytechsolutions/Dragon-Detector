// Wardriving node radio. Included by radio.cpp, which owns the shared scanner,
// observation pool and radar schedule. Original Dragon Detector code; the
// protocol itself lives in node_link.h.
//
// Life of a session:
//   begin  radar paused (setRadio(false)), Wi-Fi switched from its passive NULL
//          mode to STA, power save off, ESP-NOW up on channel 6, BLE passive.
//   run    radioLoop() hands every pass to nodeRadioLoop(); process() hands BLE
//          advertisements to the link. Nothing reaches the detector or alerts.
//   end    everything above undone in reverse, then the radar resumes as it was.
namespace noderadio {
using namespace dragon::node;

struct Received {uint8_t from[6];int8_t rssi;uint8_t length;uint8_t data[ESP_NOW_MAX_DATA_LEN];};
// The node's working memory is taken when a node starts and given back when it
// stops (PSRAM where the board has it), so a board that never runs a node keeps
// all of its internal RAM. Only the small receive queue stays once created,
// because the Wi-Fi task may still be finishing a callback as ESP-NOW stops.
#ifdef BOARD_M5DIAL
constexpr unsigned ReceiveDepth=8;
constexpr size_t SeenEntries=2048;         // internal RAM while running: 16 KB
constexpr uint32_t NodeMemory=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
#else
constexpr unsigned ReceiveDepth=16;
constexpr size_t SeenEntries=32768;        // PSRAM while running: 256 KB. Counts are exact while the table has room (see Link::remember)
constexpr uint32_t NodeMemory=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#endif
StaticQueue_t receiveControl;
uint8_t* receiveStorage=nullptr;
QueueHandle_t receiveQueue=nullptr;
std::atomic<uint32_t> receiveDrops{0};
// Send reports: only the report for the newest frame counts. A report for a
// frame the link already gave up on arrives while a newer one is outstanding.
std::atomic<int> sendResult{-1};
std::atomic<uint32_t> sendsOut{0};
std::atomic<bool> scanWaiting{false};
std::atomic<int> scanResult{-1};
esp_event_handler_instance_t scanEvent=nullptr;
bool owned=false,espNow=false,keyed=false,scanning=false;
wifi_mode_t previousMode=WIFI_MODE_NULL;
wifi_ps_type_t previousPs=WIFI_PS_MIN_MODEM;
int previousTracked=-1;uint8_t previousLock=0;bool previousRadio=true;
uint8_t lmk[ESP_NOW_KEY_LEN]{},ownMac[6]{};
Seen* seen=nullptr;
Link* link=nullptr;
const Status idle{};

void received(const esp_now_recv_info_t* info,const uint8_t* data,int length){
  // Wi-Fi task: copy and hand over, nothing else.
  if(!info||!info->src_addr||!data||length<5||length>int(ESP_NOW_MAX_DATA_LEN)||memcmp(data,"ENOW",4))return;
  Received r;memcpy(r.from,info->src_addr,6);r.rssi=int8_t(info->rx_ctrl?info->rx_ctrl->rssi:0);
  r.length=uint8_t(length);memcpy(r.data,data,size_t(length));
  if(xQueueSend(receiveQueue,&r,0)!=pdTRUE)++receiveDrops;
}
void reported(const uint8_t*,esp_now_send_status_t status){
  uint32_t out=sendsOut.load();
  while(out&&!sendsOut.compare_exchange_weak(out,out-1)){}
  if(out==1)sendResult.store(status==ESP_NOW_SEND_SUCCESS?1:0);
}
void scanDone(void*,esp_event_base_t,int32_t,void* data){
  if(data&&scanWaiting.load())scanResult.store(int(static_cast<wifi_event_sta_scan_done_t*>(data)->status));
}
Auth auth(wifi_auth_mode_t mode){
  switch(mode){
    case WIFI_AUTH_OPEN:return Auth::Open;case WIFI_AUTH_WEP:return Auth::Wep;
    case WIFI_AUTH_WPA_PSK:return Auth::Wpa;case WIFI_AUTH_WPA2_PSK:return Auth::Wpa2;
    case WIFI_AUTH_WPA_WPA2_PSK:return Auth::WpaWpa2;case WIFI_AUTH_WPA2_ENTERPRISE:return Auth::Wpa2Enterprise;
    case WIFI_AUTH_WPA3_PSK:return Auth::Wpa3;case WIFI_AUTH_WPA2_WPA3_PSK:return Auth::Wpa2Wpa3;
    case WIFI_AUTH_WAPI_PSK:return Auth::Wapi;default:return Auth::Other;
  }
}
void stopScan(){
  scanWaiting=false;
  if(scanning){esp_wifi_scan_stop();scanning=false;}
  esp_wifi_clear_ap_list();scanResult=-1;
}

// NODE_EVENT lines waiting for the end of a binary USB transfer. Printed in order.
char noteLines[768];size_t noteLength=0;uint32_t noteDrops=0;
void flushNotes(){
  if(!noteLength||radioTransferDepth)return;
  Serial.write(reinterpret_cast<const uint8_t*>(noteLines),noteLength);noteLength=0;
  if(noteDrops){Serial.printf("NODE_EVENT dropped=%lu\n",(unsigned long)noteDrops);noteDrops=0;}
}

struct RadioPort final : Port {
  bool tune(uint8_t channel) override {
    if(esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK)return false;
    wifiChannel=channel;return true;
  }
  bool peer(const uint8_t* mac,Peer mode) override {
    if(mode==Peer::Remove){esp_now_del_peer(mac);return true;}
    esp_now_peer_info_t info{};memcpy(info.peer_addr,mac,6);info.channel=0;info.ifidx=WIFI_IF_STA;
    info.encrypt=mode==Peer::Encrypted;
    if(info.encrypt){if(!keyed)return false;memcpy(info.lmk,lmk,ESP_NOW_KEY_LEN);}
    return (esp_now_is_peer_exist(mac)?esp_now_mod_peer(&info):esp_now_add_peer(&info))==ESP_OK;
  }
  bool transmit(const uint8_t* destination,const uint8_t* frame,size_t length) override {
    sendResult=-1;++sendsOut;
    if(esp_now_send(destination,frame,length)==ESP_OK)return true;
    uint32_t out=sendsOut.load();while(out&&!sendsOut.compare_exchange_weak(out,out-1)){}
    return false;
  }
  bool scan(uint8_t channel,uint16_t dwellMs) override {
    wifi_scan_config_t config{};config.channel=channel;config.show_hidden=true;config.scan_type=WIFI_SCAN_TYPE_ACTIVE;
    config.scan_time.active.min=0;config.scan_time.active.max=dwellMs;
    scanResult=-1;scanWaiting=true;
    if(esp_wifi_scan_start(&config,false)!=ESP_OK){scanWaiting=false;esp_wifi_clear_ap_list();return false;}
    scanning=true;wifiChannel=channel;return true;
  }
  void scanStop() override {stopScan();}
  void ble(bool on,uint32_t durationMs) override {
    blePhase=on;
    if(on)scanCommand(ScanAction::Start,durationMs);else scanCommand(ScanAction::Stop);
  }
  // Runs on the main loop, but also inside a binary USB transfer (radioTransferTick
  // between screenshot chunks): lines wait in noteLines until no transfer runs.
  void note(Note what,Core core,const uint8_t* text,size_t length) override {
    static const char* const names[]={"linked","core_lost","mode_changed","key_lost","other_core","role","channels"};
    static_assert(sizeof(names)/sizeof(names[0])==size_t(Note::Channels)+1,"one name per Note");
    char shown[81];size_t n=0;
    for(size_t i=0;text&&i<length&&n+1<sizeof(shown);++i)shown[n++]=text[i]>=32&&text[i]<127&&text[i]!='"'?char(text[i]):'.';
    shown[n]=0;
    const unsigned index=unsigned(what);
    char line[192];
    const int used=snprintf(line,sizeof(line),"NODE_EVENT %s core=%s text=\"%s\" length=%u at_ms=%lu\n",index<sizeof(names)/sizeof(names[0])?names[index]:"?",
      dragon::node::coreName(core),shown,unsigned(length),(unsigned long)millis());
    if(used<=0)return;
    const size_t size=size_t(used)<sizeof(line)?size_t(used):sizeof(line)-1;
    if(noteLength+size>sizeof(noteLines)){++noteDrops;return;}
    memcpy(noteLines+noteLength,line,size);noteLength+=size;
    flushNotes();
  }
};
RadioPort port;

// Gives the session's working memory back. Only called while no node runs.
void release(){
  if(link){link->~Link();heap_caps_free(link);link=nullptr;}
  if(seen){heap_caps_free(seen);seen=nullptr;}
}

// Called with the session's settings already chosen. Unwinds completely on failure.
bool begin(const Config& requested,const uint8_t* pmk,const uint8_t* linkKey,char* why,size_t whyLength){
  auto fail=[&](const char* reason){snprintf(why,whyLength,"%s",reason);return false;};
  if(owned)return fail("Node already running");
  if(!queue||!scanner)return fail("Radio did not start");
  // Scan results arrive as a driver event. Lite builds never needed the
  // default event loop until now; creating it twice is harmless. It comes
  // first, so a failure here leaves no node memory taken.
  const esp_err_t loop=esp_event_loop_create_default();
  if(loop!=ESP_OK&&loop!=ESP_ERR_INVALID_STATE)return fail("Wi-Fi events unavailable");
  if(!receiveQueue){
    receiveStorage=static_cast<uint8_t*>(heap_caps_malloc(ReceiveDepth*sizeof(Received),NodeMemory));
    if(!receiveStorage)return fail("Not enough memory");
    receiveQueue=xQueueCreateStatic(ReceiveDepth,sizeof(Received),receiveStorage,&receiveControl);
  }
  if(!seen)seen=static_cast<Seen*>(heap_caps_calloc(SeenEntries,sizeof(Seen),NodeMemory));
  if(!link){void* room=heap_caps_malloc(sizeof(Link),NodeMemory);if(room)link=new(room) Link();}
  if(!seen||!link){release();return fail("Not enough memory");}

  wardriveTransition=true;
  previousTracked=trackedContact;previousLock=channelLock;previousRadio=radioEnabled;
  setRadio(false);
  wardriveScanning=true;trackedContact=-1;channelLock=0;
  scanCommand(ScanAction::Configure,0,false);   // a node listens, it never asks BLE devices to answer
  esp_wifi_get_mode(&previousMode);esp_wifi_get_ps(&previousPs);
  bool ok=esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,scanDone,nullptr,&scanEvent)==ESP_OK;
  if(ok)ok=esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK;
  if(ok){esp_wifi_set_ps(WIFI_PS_NONE);ok=esp_wifi_set_channel(HomeChannel,WIFI_SECOND_CHAN_NONE)==ESP_OK;}
  if(ok)wifiChannel=HomeChannel;
  if(ok)ok=esp_now_init()==ESP_OK;
  if(ok){
    espNow=true;
    keyed=pmk&&linkKey;
    if(keyed){memcpy(lmk,linkKey,ESP_NOW_KEY_LEN);ok=esp_now_set_pmk(pmk)==ESP_OK;}
  }
  if(ok)ok=esp_now_register_recv_cb(received)==ESP_OK&&esp_now_register_send_cb(reported)==ESP_OK;
  if(ok){esp_now_peer_info_t broadcast{};memset(broadcast.peer_addr,0xff,6);broadcast.ifidx=WIFI_IF_STA;ok=esp_now_add_peer(&broadcast)==ESP_OK;}
  if(ok)ok=esp_wifi_get_mac(WIFI_IF_STA,ownMac)==ESP_OK;
  xQueueReset(receiveQueue);sendResult=-1;sendsOut=0;scanResult=-1;scanWaiting=false;scanning=false;receiveDrops=0;
  Config config=requested;
  config.seen=seen;config.seenCount=SeenEntries;config.encrypted=requested.encrypted&&keyed;
  config.seed=esp_random()^(uint32_t(ownMac[2])<<24|uint32_t(ownMac[3])<<16|uint32_t(ownMac[4])<<8|ownMac[5]);
  if(ok){owned=true;ok=link->begin(port,config,millis());}
  if(!ok){
    owned=false;
    if(espNow){esp_now_unregister_recv_cb();esp_now_unregister_send_cb();esp_now_deinit();espNow=false;}
    if(scanEvent){esp_event_handler_instance_unregister(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,scanEvent);scanEvent=nullptr;}
    esp_wifi_set_ps(previousPs);esp_wifi_set_mode(previousMode);
    wardriveScanning=false;trackedContact=previousTracked;channelLock=previousLock;
    scanCommand(ScanAction::Configure,0,activeBle&&reconMode()==dragon::ReconMode::Radar);
    setRadio(previousRadio);wardriveTransition=false;release();
    return fail("Wi-Fi could not switch to node mode");
  }
  radioEnabled=false;blePhase=false;lastAccounting=millis();wardriveTransition=false;
  return true;
}

void end(){
  if(!owned)return;
  link->end();
  flushNotes();noteLength=0;   // what a transfer still holds back would only mislead the next session
  wardriveTransition=true;
  scanCommand(ScanAction::Stop);blePhase=false;
  stopScan();
  if(espNow){esp_now_unregister_recv_cb();esp_now_unregister_send_cb();esp_now_deinit();espNow=false;}
  if(scanEvent){esp_event_handler_instance_unregister(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,scanEvent);scanEvent=nullptr;}
  esp_wifi_set_ps(previousPs);esp_wifi_set_mode(previousMode);
  keyed=false;memset(lmk,0,sizeof(lmk));
  if(receiveQueue)xQueueReset(receiveQueue);
  owned=false;wardriveScanning=false;
  trackedContact=previousTracked;channelLock=previousLock;
  scanCommand(ScanAction::Configure,0,activeBle&&reconMode()==dragon::ReconMode::Radar);
  setRadio(previousRadio);wardriveTransition=false;
  release();
}

// One pass of the application loop. Everything the link sees arrives here, in order.
void loop(uint32_t now){
  flushNotes();
  Received r;
  for(unsigned i=0;i<ReceiveDepth&&xQueueReceive(receiveQueue,&r,0)==pdTRUE;++i)link->received(r.from,r.data,r.length,r.rssi,now);
  const int sent=sendResult.exchange(-1);
  if(sent>=0)link->sent(sent==1,now);
  const int scanned=scanResult.load();
  if(scanning&&scanned>=0){
    scanWaiting=false;scanning=false;scanResult=-1;
    uint16_t count=0;bool ok=scanned==0&&esp_wifi_scan_get_ap_num(&count)==ESP_OK;
    for(uint16_t i=0;ok&&i<count;++i){
      wifi_ap_record_t ap{};
      if(esp_wifi_scan_get_ap_record(&ap)!=ESP_OK)break;
      link->wifi(ap.bssid,ap.ssid,strnlen(reinterpret_cast<const char*>(ap.ssid),32),auth(ap.authmode),ap.primary,ap.rssi);
    }
    esp_wifi_clear_ap_list();
    link->scanned(ok,now);
  }
  link->loop(now);
}
void bleSeen(const uint8_t* address,int rssi){if(owned)link->ble(address,rssi);}
}

bool radioNodeBegin(const dragon::node::Config& config,const uint8_t* pmk,const uint8_t* lmk,char* why,size_t whyLength){
  return noderadio::begin(config,pmk,lmk,why,whyLength);
}
void radioNodeEnd(){noderadio::end();}
bool radioNodeOwned(){return noderadio::owned;}
const dragon::node::Status& radioNodeStatus(){return noderadio::owned?noderadio::link->status():noderadio::idle;}
void radioNodeMac(uint8_t* mac){esp_read_mac(mac,ESP_MAC_WIFI_STA);}
uint32_t radioNodeReceiveDrops(){return noderadio::receiveDrops.load();}
