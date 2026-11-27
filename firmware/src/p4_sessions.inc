namespace {
struct P4SessionGuard {
  bool locked;
  P4SessionGuard():locked(sdReady && !usbDriveOn() && sdMutex &&
    xSemaphoreTake(sdMutex,pdMS_TO_TICKS(120))==pdTRUE){}
  ~P4SessionGuard(){if(locked)xSemaphoreGive(sdMutex);}
  explicit operator bool()const{return locked && sdReady && !usbDriveOn();}
};
uint32_t durableBytes=4,sessionEnded=0,retryAt=0;
bool finalizing=false,privateSession=false;
char sessionKey[17];
bool sessionSpace(size_t needed){
  const uint64_t total=CardFS.totalBytes(),used=CardFS.usedBytes();
  return total>=used && total-used>=32ULL*1024*1024+needed;
}
bool originalSessionCard(){
  const String path=directory(session->name)+"/session.json";
  if(!dragon::recoverFile(CardFS,path))return false;
  File manifest=CardFS.open(path);
  char prefix[50],actual[50];
  int len=snprintf(prefix,sizeof(prefix),"{\"schema\":1,\"session_id\":\"%s\",",sessionKey);
  return manifest && len>0 && size_t(len)<sizeof(prefix) &&
         manifest.read(reinterpret_cast<uint8_t*>(actual),len)==size_t(len) && !memcmp(prefix,actual,len);
}
bool p4SessionFlush(){
  auto& buffer=session->buffers[0];
  if(!buffer.size)return true;
  P4SessionGuard guard;
  if(!guard || !originalSessionCard() || !sessionSpace(buffer.size+32768))return false;
  File f=CardFS.open(directory(session->name)+"/obs.rlog","r+");
  if(!dragon::p4::commitRecords(f,durableBytes,buffer.data,buffer.size))return false;
  durableBytes+=buffer.size;
  buffer.size=0;
  return true;
}
bool p4SessionManifest(bool complete,bool initial=false){
  P4SessionGuard guard;
  if(!guard || (!initial && !originalSessionCard()) || !sessionSpace(2048))return false;
  auto& s=*session;
  fs::FS* fs=&CardFS;
  const String path=directory(s.name)+"/session.json";
  const String temp=path+".part";
  File f=fs->open(temp,FILE_WRITE);
  if(!f)return false;
  char json[680];
  const uint32_t elapsed=(complete?sessionEnded:millis())-s.start;
  const int n=snprintf(json,sizeof(json),
    "{\"schema\":1,\"session_id\":\"%s\",\"state\":\"%s\",\"mode\":\"%s\",\"start_ms\":%lu,\"duration_ms\":%lu,"
    "\"ap\":%lu,\"station\":0,\"ble\":%lu,\"ibeacon\":%lu,\"probe\":0,\"repeat\":%lu,\"deauth\":0,"
    "\"dropped\":%lu,\"gps_fix\":false,\"observations\":\"obs.rlog\",\"probes\":\"probes.rlog\","
    "\"relationships\":\"relations.rlog\",\"capture\":\"\",\"producer\":\"Dragon Detector\","
    "\"reason\":\"%s\",\"bytes\":%lu}\n",
    sessionKey,complete?"complete":"active",s.ble?"ble":"wifi",(unsigned long)s.start,(unsigned long)elapsed,
    (unsigned long)s.counts[0],(unsigned long)s.counts[2],(unsigned long)s.counts[3],
    (unsigned long)s.counts[5],(unsigned long)s.dropped,s.reason,(unsigned long)(durableBytes+8));
  const bool wrote=n>0 && size_t(n)<sizeof(json) && f.write(reinterpret_cast<uint8_t*>(json),n)==size_t(n);
  f.flush();const bool ok=wrote && !f.getWriteError() && f.size()==size_t(n);f.close();
  if(!ok){fs->remove(temp);return false;}
  return dragon::replaceFile(*fs,temp,path);
}
void p4FinishSession(const char* reason){
  if(!session || !session->active)return;
  if(!finalizing){finalizing=true;sessionEnded=millis();strlcpy(session->reason,reason,sizeof(session->reason));}
  retryAt=millis();
  if(p4SessionFlush() && p4SessionManifest(true)){session->active=false;finalizing=false;}
}
}
const char* p4SessionUnavailable(bool ble){
  if(missionActive())return finalizing?"Finish saving the current session.":"A session is already recording.";
  if(usbDriveOn())return "Eject USB Drive first.";
  if(!sdReady || !sdMutex)return "Mount the microSD card before starting.";
  if(!p4CoprocReady() || !radioEnabled || radioStandby() || p4GattBusy() || p4CoprocBusy())return "Return to the radar or finish the current radio operation.";
  const auto mode=p4RadarScanMode();
  if((ble && mode==1)||(!ble && mode==2))return ble?"Enable Bluetooth in Modes.":"Enable Wi-Fi in Modes.";
  return nullptr;
}
static bool p4StartSession(bool ble,const char* requested){
  if(p4SessionUnavailable(ble) || !requested || !safe(requested))return false;
  const String dir=directory(requested);
  if(!session)session=(Session*)ps_calloc(1,sizeof(Session));
  if(!session)return false;
  fs::FS* fs=&CardFS;
  auto cleanup=[&](){for(auto* name:files)fs->remove(dir+"/"+name);fs->remove(dir+"/session.json.part");fs->rmdir(dir);};
  {
    P4SessionGuard guard;
    if(!guard || !fs || !sessionSpace(65536) || fs->exists(dir))return false;
    if(!fs->exists("/dragon") && !fs->mkdir("/dragon"))return false;
    if(!fs->exists(missionsRoot) && !fs->mkdir(missionsRoot))return false;
    File list=fs->open(missionsRoot);
    if(!list || !list.isDirectory())return false;
    unsigned count=0;
    for(File entry=list.openNextFile();entry && count<512;entry=list.openNextFile()){if(entry.isDirectory())++count;entry.close();}
    list.close();
    if(count>=512 || !fs->mkdir(dir))return false;
    for(unsigned i=0;i<3;++i){
      File f=fs->open(dir+"/"+files[i],FILE_WRITE);
      bool ok=f && f.write(reinterpret_cast<const uint8_t*>(headers[i]),4)==4;
      if(f){f.flush();ok=ok && !f.getWriteError() && f.size()==4;f.close();}
      if(!ok){cleanup();return false;}
    }
  }
  memset(session,0,sizeof(*session));strlcpy(session->name,requested,sizeof(session->name));
  session->ble=ble;session->start=session->lastFlush=session->lastManifest=millis();session->bytes=12;
  strlcpy(session->reason,"Recording",sizeof(session->reason));
  durableBytes=4;privateSession=detector.privacy;finalizing=false;
  snprintf(sessionKey,sizeof(sessionKey),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
  if(!p4SessionManifest(false,true)){sessionEnded=session->start;P4SessionGuard guard;if(guard)cleanup();strlcpy(session->reason,"Start failed",sizeof(session->reason));return false;}
  session->active=true;return true;
}
void p4RecordSessionContact(bool ble,const uint8_t* address,uint8_t addressType,int rssi,uint8_t channel,
                            const uint8_t* advert,size_t length,uint32_t seen){
  if(!missionActive() || finalizing || session->ble!=ble || !dragon::p4::inSession(session->start,seen))return;
  static constexpr uint8_t zero[6]={};
  if(!address || !memcmp(address,zero,6) || rssi>=0 || rssi<-127 || addressType>3)return;
  privateSession|=detector.privacy;
  const char type=ble?(dragon::p4::isIBeacon(advert,length)?'i':'b'):'a';
  observe(type,addressType,address,rssi,channel,seen,privateSession);
}
static void p4SessionLoop(){
  if(!missionActive())return;
  const uint32_t now=millis();
  if(finalizing){if(now-retryAt>=1000)p4FinishSession(session->reason);return;}
  const auto mode=p4RadarScanMode();
  if(!p4CoprocReady() || p4CoprocBusy() || !radioEnabled || radioStandby() || (session->ble?mode==1:mode==2)){p4FinishSession("Radio paused or mode changed");return;}
  if(!dragon::p4::inSession(session->start,now)){p4FinishSession("Time limit");return;}
  if(session->bytes>=sessionLimit()-256){p4FinishSession("Size limit");return;}
  if(now-session->lastFlush>=250){session->lastFlush=now;if(!p4SessionFlush()){p4FinishSession("Storage interrupted");return;}}
  if(now-session->lastManifest>=10000){session->lastManifest=now;if(!p4SessionManifest(false))p4FinishSession("Storage interrupted");}
}
void p4SessionStatus(char* out,size_t size){
  if(!session || !session->name[0]){snprintf(out,size,"Record 5 minutes from the current radio schedule. Files are saved in /dragon/missions.");return;}
  const auto& s=*session;
  const unsigned long seconds=((s.active && !finalizing?millis():sessionEnded)-s.start)/1000;
  snprintf(out,size,"%s / %s\n%s\n%lu s / %lu unique / %lu updates\n%lu dropped / %lu bytes saved",
    s.ble?"Bluetooth":"Wi-Fi",s.name,finalizing?"Saving / retrying SD":s.reason,seconds,
    (unsigned long)(s.counts[0]+s.counts[2]+s.counts[3]),(unsigned long)s.counts[5],
    (unsigned long)s.dropped,(unsigned long)(durableBytes+8));
}
