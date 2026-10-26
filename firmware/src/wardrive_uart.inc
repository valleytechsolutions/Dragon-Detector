// Included inside wardrive.cpp's namespace; no transmit pin is attached.
#ifdef BOARD_GPS_UART
HardwareSerial externalGpsSerial(1);
Preferences externalGpsPrefs;
dragon::gpsuart::Config externalGpsConfig;
bool externalGpsOpen=false,externalGpsPrefsReady=false;
bool externalGpsSuspended=false;
uint32_t externalGpsLastByte=0,externalGpsBytes=0;
std::atomic<bool> externalGpsOverflow{false};
void externalGpsApply(){
  externalGpsSerial.end();externalGpsOpen=false;externalGpsOverflow=false;
  gps={};present=false;externalGpsLastByte=externalGpsBytes=0;
  if(!externalGpsConfig.enabled||externalGpsSuspended)return;
  if(externalGpsSerial.setRxBufferSize(4096)!=4096)return;
  externalGpsSerial.begin(externalGpsConfig.baud,SERIAL_8N1,externalGpsConfig.rx,-1);
  externalGpsOpen=bool(externalGpsSerial);
  externalGpsSerial.onReceiveError([](hardwareSerial_error_t){externalGpsOverflow=true;});
}
void externalGpsBegin(){
  externalGpsPrefsReady=externalGpsPrefs.begin("dd-gps-uart",false);
  dragon::gpsuart::Config stored;
  if(externalGpsPrefsReady&&dragon::gpsuart::decode(externalGpsPrefs.getUInt("config",0),stored))externalGpsConfig=stored;
  externalGpsApply();
}
void gpsLoop(){
  if(!externalGpsConfig.enabled||!externalGpsOpen)return;
  uint32_t now=millis();unsigned waiting=externalGpsSerial.available();
  if(externalGpsOverflow.exchange(false)||waiting>2048){
    // A queued position from before a stall/error must not become a fresh fix.
    gps.fix.position=false;gps.discardPartial();++gpsErrors;
    unsigned discard=waiting;while(discard--&&externalGpsSerial.available())externalGpsSerial.read();
    waiting=0;
  }
  unsigned budget=512;while(budget--&&externalGpsSerial.available()){
    int c=externalGpsSerial.read();if(c<0)break;gps.feed(char(c),now);externalGpsLastByte=now;++externalGpsBytes;
  }
  present=externalGpsBytes&&uint32_t(now-externalGpsLastByte)<3000;
  if(!present)gps.fix.position=false;
}
#endif
