// Original Dragon Detector adapter. Piglet's asynchronous network scan workflow
// is a behavior reference; the ESP-IDF driver owns and releases its AP list.
#ifdef BOARD_GPS_LC76G
namespace {
enum class DriveScanPhase {Off,Wifi,Results,Ble};
DriveScanPhase driveScanPhase=DriveScanPhase::Off;
esp_event_handler_instance_t driveScanEvent=nullptr;
std::atomic<bool> driveScanWaiting{false};
std::atomic<int> driveScanResult{-1};
wifi_mode_t drivePreviousMode=WIFI_MODE_NULL;
wifi_ps_type_t drivePreviousPs=WIFI_PS_MIN_MODEM;
uint32_t driveScanAt=0,driveBleAt=0,driveResultAt=0;
uint32_t driveScans=0,driveScanErrors=0,driveScanResets=0,driveScanMaxMs=0;
unsigned driveFailures=0,driveResultsLeft=0;
void driveScanDone(void*,esp_event_base_t,int32_t,void* data){
  if(data&&driveScanWaiting.load())driveScanResult.store(static_cast<wifi_event_sta_scan_done_t*>(data)->status);
}
const char* driveAuth(wifi_auth_mode_t auth){
  switch(auth){
    case WIFI_AUTH_OPEN:return "[ESS]";case WIFI_AUTH_WEP:return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:return "[WPA-PSK][ESS]";case WIFI_AUTH_WPA2_PSK:return "[WPA2-PSK][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:return "[WPA-PSK][WPA2-PSK][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE:return "[WPA2-EAP][ESS]";
    case WIFI_AUTH_WPA3_PSK:return "[WPA3-SAE][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:return "[WPA2-PSK][WPA3-SAE][ESS]";
    default:return "[UNKNOWN][ESS]";
  }
}
void driveCancelScan(){
  driveScanWaiting=false;esp_wifi_scan_stop();esp_wifi_clear_ap_list();driveResultsLeft=0;driveScanResult=-1;
}
void driveBeginBle(uint32_t now){
  driveScanPhase=DriveScanPhase::Ble;blePhase=true;driveBleAt=now;
  if(!scanCommand(ScanAction::Start,1500)){++driveScanErrors;}
}
void driveScanFailure(uint32_t now){
  ++driveScanErrors;driveCancelScan();
  if(++driveFailures>=3){
    // Recreate just the STA control block; never deinitialize the shared radio
    // or NimBLE stack. The BLE interval provides recovery backoff.
    esp_wifi_set_mode(WIFI_MODE_NULL);esp_wifi_set_mode(WIFI_MODE_STA);
    driveFailures=0;++driveScanResets;
  }
  driveBeginBle(now);
}
void driveStartScan(uint32_t now){
  scanCommand(ScanAction::Stop);blePhase=false;driveScanAt=now;
  wifi_scan_config_t config{};config.show_hidden=true;config.scan_type=WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min=0;config.scan_time.active.max=100;
  driveScanResult=-1;driveScanWaiting=true;driveScanPhase=DriveScanPhase::Wifi;
  if(esp_wifi_scan_start(&config,false)!=ESP_OK)driveScanFailure(now);
}
bool driveRadioBegin(){
  driveScans=driveScanErrors=driveScanResets=driveScanMaxMs=0;driveFailures=0;
  esp_wifi_get_mode(&drivePreviousMode);esp_wifi_get_ps(&drivePreviousPs);
  if(esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,driveScanDone,nullptr,&driveScanEvent)!=ESP_OK)return false;
  if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK){esp_event_handler_instance_unregister(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,driveScanEvent);driveScanEvent=nullptr;return false;}
  esp_wifi_set_ps(WIFI_PS_NONE);radioEnabled=true;lastAccounting=millis();driveStartScan(lastAccounting);return true;
}
void driveRadioEnd(){
  driveCancelScan();driveScanPhase=DriveScanPhase::Off;
  if(driveScanEvent){esp_event_handler_instance_unregister(WIFI_EVENT,WIFI_EVENT_SCAN_DONE,driveScanEvent);driveScanEvent=nullptr;}
  esp_wifi_set_mode(drivePreviousMode);esp_wifi_set_ps(drivePreviousPs);
}
void driveRadioLoop(uint32_t now,uint32_t elapsed){
  if(!wardriveActive()){
    if(driveScanPhase!=DriveScanPhase::Off){driveCancelScan();scanCommand(ScanAction::Stop);driveScanPhase=DriveScanPhase::Off;}
    return;
  }
  if(blePhase)bleListenMs+=elapsed;else wifiListenMs+=elapsed;
  if(driveScanPhase==DriveScanPhase::Wifi){
    int result=driveScanResult.load();
    if(result<0){if(now-driveScanAt>=8000)driveScanFailure(now);return;}
    driveScanWaiting=false;
    if(result!=0){driveScanFailure(now);return;}
    ++driveScans;driveFailures=0;driveScanMaxMs=std::max(driveScanMaxMs,now-driveScanAt);
    uint16_t count=0;if(esp_wifi_scan_get_ap_num(&count)!=ESP_OK){driveScanFailure(now);return;}
    driveResultsLeft=count;driveResultAt=now;driveBeginBle(now);driveScanPhase=DriveScanPhase::Results;
  }
  if(driveScanPhase==DriveScanPhase::Results){
    // Consume and free four driver records per pass; SD/GPS/UI get a turn.
    for(unsigned i=0;i<4&&driveResultsLeft;++i){
      wifi_ap_record_t ap{};if(esp_wifi_scan_get_ap_record(&ap)!=ESP_OK){driveScanFailure(now);return;}
      --driveResultsLeft;
      if(ap.primary>=1&&ap.primary<=11)wardriveWifiResult(ap.bssid,ap.ssid,strnlen((char*)ap.ssid,32),driveAuth(ap.authmode),ap.rssi,ap.primary,driveResultAt);
    }
    if(driveResultsLeft)return;
    esp_wifi_clear_ap_list();driveScanPhase=DriveScanPhase::Ble;
  }
  if(driveScanPhase==DriveScanPhase::Ble&&now-driveBleAt>=1500)driveStartScan(now);
}
}
#endif
