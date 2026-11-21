#include "badusb.h"
#include "app.h"

#if DRAGON_BADUSB
#if !ARDUINO_USB_MODE
#error BadUSB keyboard mode expects the hardware USB serial console (ARDUINO_USB_MODE=1)
#endif
#include "badusb_script.h"
#include "flash_budget.h"
#include "recon.h"
#include "wardrive.h"
#include "wardrive_upload.h"
#include <LittleFS.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <atomic>
#include "badusb_mode.h"

namespace {
// Normal boots keep the S3's hardware USB serial/JTAG, so flashing and serial
// tools behave as before. Keyboard mode is entered only by a restart the user
// asks for, and lasts until the next restart or power cycle.
// Register the keyboard descriptor globally; TinyUSB starts only in keyboard mode.
USBHIDKeyboard keyboard;
USBHID reports;
dragon::badusb::Runner runner;
constexpr unsigned FileLimit=24;
constexpr char Demo[]="REM Open an empty text editor before pressing Run.\nDELAY 3000\nSTRINGLN Dragon Detector USB keyboard test.\n";
char names[FileLimit][33]{};
unsigned fileCount=1;
char selectedName[33]{},uploadName[33]{},message[128]="Select a script; open a text editor for the demo";
char* script=nullptr;
size_t scriptLength=0,uploadLength=0,uploadReceived=0;
uint32_t uploadAt=0;
bool selected=false,onCard=false,storageReady=false,releasePending=false;
std::atomic<bool> linkLost{false};
void say(const char* text){strlcpy(message,text,sizeof(message));}
bool busy(){return wardriveBusy()||wardriveUploadBusy()||reconStorageBusy();}
bool memory(){if(!script)script=static_cast<char*>(malloc(dragon::badusb::MaxScriptBytes+1));if(!script)say("Script memory unavailable");return script;}
fs::FS* filesystem(){
#if BOARD_HAS_SD
  if(onCard)return sdReady?&CardFS:nullptr;
#endif
  return storageReady?&LittleFS:nullptr;
}
struct CardLock {
  bool locked=false;
  CardLock(){
#if BOARD_HAS_SD
    if(onCard&&sdMutex){xSemaphoreTake(sdMutex,portMAX_DELAY);locked=true;}
#endif
  }
  ~CardLock(){if(locked)xSemaphoreGive(sdMutex);}
};
const char* directory(){return onCard?"/dragon/badusb":"/badusb";}
String path(const char* name){return String(directory())+"/"+name;}
bool validName(const char* name){
  const size_t n=strlen(name);if(n<5||n>32||strcmp(name+n-4,".txt"))return false;
  for(size_t i=0;i<n-4;++i)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='A'&&name[i]<='Z')||(name[i]>='0'&&name[i]<='9')||name[i]=='-'||name[i]=='_'))return false;
  return strcmp(name,"demo.txt")!=0;
}
bool space(size_t bytes){
  uint64_t total=LittleFS.totalBytes(),used=LittleFS.usedBytes();
#if BOARD_HAS_SD
  if(onCard){total=CardFS.totalBytes();used=CardFS.usedBytes();}
#endif
  const uint64_t reserve=onCard?32ULL*1024*1024:dragon::flashReserve(size_t(total));
  return used<=total&&bytes<=total-used&&total-used-bytes>=reserve;
}
bool release(){
  if(!releasePending)return true;
  if(!reports.ready())return false;
  KeyReport empty{};
  if(!reports.SendReport(HID_REPORT_ID_KEYBOARD,&empty,sizeof(empty),20))return false;
  releasePending=false;return true;
}
bool stroke(void*,const dragon::badusb::Stroke& keys){
  if(linkLost.load()||!reports.ready())return false;
  KeyReport report{};report.modifiers=keys.modifiers;memcpy(report.keys,keys.keys,sizeof(report.keys));
  // A timeout can occur after the host received a key-down. Keep trying the
  // empty report on subsequent ticks, and before any later run.
  releasePending=true;
  const bool sent=reports.SendReport(HID_REPORT_ID_KEYBOARD,&report,sizeof(report),20);
  const bool released=release();return sent&&released;
}
bool validate(){dragon::badusb::Error error{};if(dragon::badusb::validate(script,scriptLength,error))return true;
  snprintf(message,sizeof(message),"Line %u: %s",error.line,error.message);return false;}
void cancelUpload(){uploadLength=uploadReceived=0;uploadName[0]=0;}
int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
bool decimal(const char*& text,size_t& value){
  if(*text<'0'||*text>'9')return false;value=0;
  while(*text>='0'&&*text<='9'){
    const unsigned digit=unsigned(*text++-'0');
    if(value>(dragon::badusb::MaxScriptBytes-digit)/10)return false;
    value=value*10+digit;
  }
  return true;
}
void reply(bool ok){Serial.printf("BADUSB_%s %s\n",ok?"OK":"ERROR",message);}
}
bool badusbSupported(){return true;}
const char* badusbUnavailableReason(){return "";}
bool badusbKeyboardMode(){return dragon::badusb::keyboardMode();}
void badusbBegin(){
  if(badusbKeyboardMode()){
    keyboard.begin();
    USB.onEvent([](void*,esp_event_base_t,int32_t event,void*){
      if(event==ARDUINO_USB_STOPPED_EVENT||event==ARDUINO_USB_SUSPEND_EVENT)linkLost=true;
    });
    USB.begin(); // Moves the USB pins from serial/JTAG to the keyboard.
  }
#if BOARD_HAS_SD
  onCard=sdReady;
#endif
  storageReady=onCard||LittleFS.totalBytes()>0;
#ifdef BOARD_M5DIAL
  // Compact firmware has no badge filesystem owner. Never format on failure.
  if(!storageReady)storageReady=LittleFS.begin(false);
#endif
  badusbRefresh();
  if(badusbKeyboardMode()){
    const char* wanted=dragon::badusb::keyboardScript();
    for(unsigned i=0;i<fileCount;++i)if(!strcmp(names[i],wanted)){badusbSelect(i);break;}
    say(selected?"Keyboard mode / press Run to type":"Keyboard mode / select a script");
  }
}
void badusbRefresh(){
  if(runner.running()||uploadLength||busy())return;
  strlcpy(names[0],"demo.txt",sizeof(names[0]));fileCount=1;
  auto* fs=filesystem();if(!fs)return;CardLock lock;
  if(!fs->exists(directory())&&!fs->mkdir(directory()))return;
  File dir=fs->open(directory());if(!dir||!dir.isDirectory())return;
  for(File file=dir.openNextFile();file&&fileCount<FileLimit;file=dir.openNextFile()){
    const char* name=strrchr(file.name(),'/');name=name?name+1:file.name();
    if(!file.isDirectory()&&validName(name)&&file.size()<=dragon::badusb::MaxScriptBytes)strlcpy(names[fileCount++],name,sizeof(names[0]));
    file.close();
  }
  dir.close();
  for(unsigned i=1;i<fileCount;++i)for(unsigned j=i+1;j<fileCount;++j)if(strcmp(names[i],names[j])>0){char tmp[33];memcpy(tmp,names[i],33);memcpy(names[i],names[j],33);memcpy(names[j],tmp,33);}
}
unsigned badusbCount(){return fileCount;}
const char* badusbName(unsigned i){return i<fileCount?names[i]:"";}
bool badusbSelect(unsigned i){
  if(runner.running()||uploadLength||busy()){say("Stop the active session or upload first");return false;}
  selected=false;selectedName[0]=0;scriptLength=0;
  if(i>=fileCount||!memory()){if(i>=fileCount)say("Script not found");return false;}
  if(i==0){scriptLength=sizeof(Demo)-1;memcpy(script,Demo,sizeof(Demo));}
  else {
    auto* fs=filesystem();if(!fs){say("Script storage unavailable");return false;}CardLock lock;
    File file=fs->open(path(names[i]));const size_t length=file?file.size():0;
    if(!file||file.isDirectory()||!length||length>dragon::badusb::MaxScriptBytes){say("Script missing, empty or too large");return false;}
    const size_t got=file.read(reinterpret_cast<uint8_t*>(script),length);file.close();
    if(got!=length){say("Script read failed");return false;}scriptLength=length;script[length]=0;
  }
  if(!validate())return false;
  strlcpy(selectedName,names[i],sizeof(selectedName));selected=true;say("Ready / US keyboard layout / press Run");return true;
}
bool badusbRun(){
  if(runner.running()){say("Script already running");return false;}
  if(!selected||uploadLength||busy()){say("Select a valid script and finish other sessions");return false;}
  // Normal mode: restart as a USB keyboard with this script selected. The
  // script still waits for a second, explicit Run press after the restart.
  if(!badusbKeyboardMode()){say("Restarting as USB keyboard");dragon::badusb::restartIntoKeyboard(selectedName);return false;}
  if(!reports.ready()||!release()){say("Connect native USB to an awake host");return false;}
  linkLost=false;
  dragon::badusb::Error error{};
  if(!runner.begin(script,scriptLength,millis(),error)){snprintf(message,sizeof(message),"Line %u: %s",error.line,error.message);return false;}
  say("Running / Stop releases all keys");return true;
}
void badusbStop(){runner.stop();release();say("Stopped / press Run to start again");}
void badusbExitKeyboard(){
  if(!badusbKeyboardMode())return;
  runner.stop();release();say("Restarting as USB serial");dragon::badusb::restartIntoSerial();
}
const char* badusbPreview(){return selected&&script?script:"";}
BadUsbStatus badusbStatus(){return {badusbKeyboardMode()&&reports.ready(),runner.running(),selected,runner.line(),selectedName,message,onCard?"SD /dragon/badusb":"Internal /badusb",badusbKeyboardMode()};}
void badusbLoop(){
  if(uploadLength&&uint32_t(millis()-uploadAt)>30000){cancelUpload();say("Upload expired; start again");}
  release();
  if(!runner.running())return;
  if(linkLost.load()||!reports.ready()||busy()){badusbStop();say("Stopped / USB disconnected, suspended or device busy");return;}
  runner.tick(millis(),stroke,nullptr);
  if(runner.complete())say("Finished / all keys released");
  else if(runner.failed()){release();snprintf(message,sizeof(message),"Line %u: %s",runner.line(),runner.error().message);}
}
bool badusbCommand(const String& c){
  if(c!="badusb"&&!c.startsWith("badusb "))return false;
  if(c=="badusb status"||c=="badusb"){auto s=badusbStatus();Serial.printf("BADUSB_STATUS supported=1 mode=%s connected=%u running=%u selected=%u line=%u file=%s medium=%s message=%s\n",s.keyboardMode?"keyboard":"serial",s.connected,s.running,s.selected,s.line,s.name,s.medium,s.message);return true;}
  if(c=="badusb stop"){badusbStop();reply(true);return true;}
  if(c=="badusb cancel"){if(runner.running()){say("Stop the script first");reply(false);return true;}cancelUpload();say("Upload cancelled");reply(true);return true;}
  if(runner.running()||busy()){say("Stop the active session first");reply(false);return true;}
  if(c=="badusb list"){badusbRefresh();for(unsigned i=0;i<fileCount;++i)Serial.printf("BADUSB_FILE %s\n",names[i]);Serial.println("BADUSB_END");return true;}
  if(c.startsWith("badusb select ")){String name=c.substring(14);for(unsigned i=0;i<fileCount;++i)if(name==names[i]){reply(badusbSelect(i));return true;}say("Script not found; refresh the list");reply(false);return true;}
  // USB may prepare files and inspect status, but starts are local UI actions.
  if(c=="badusb run"){say("Press Run on the detector");reply(false);return true;}
  // Switching modes never starts a script; Run is still pressed on the detector.
  if(c=="badusb keyboard"){if(badusbKeyboardMode()){say("Already in keyboard mode");reply(false);return true;}
    if(uploadLength){say("Finish or cancel the upload first");reply(false);return true;}
    say("Restarting as USB keyboard");reply(true);Serial.flush();dragon::badusb::restartIntoKeyboard(selected?selectedName:"");}
  if(c.startsWith("badusb upload ")){
    const char* input=c.c_str()+14;const char* split=strchr(input,' ');char name[33]{};size_t length=0;
    bool syntax=split&&size_t(split-input)<sizeof(name);
    if(syntax){memcpy(name,input,split-input);input=split+1;syntax=decimal(input,length)&&!*input;}
    if(!syntax||!validName(name)||!length){say("Use badusb upload NAME.txt BYTES (1..8192)");reply(false);return true;}
    auto* fs=filesystem();if(!fs){say("Script storage unavailable");reply(false);return true;}
    {CardLock lock;if(!space(length+4096)||fs->exists(path(name))){say("Name exists or storage is full");reply(false);return true;}}
    if(!memory()){reply(false);return true;}selected=false;selectedName[0]=0;scriptLength=0;
    strlcpy(uploadName,name,sizeof(uploadName));uploadLength=length;uploadReceived=0;uploadAt=millis();say("Ready for hex chunks");reply(true);return true;
  }
  if(c.startsWith("badusb chunk ")){
    size_t offset=0;const char* data=c.c_str()+13;
    if(!uploadLength||!decimal(data,offset)||*data!=' '||offset!=uploadReceived){say("Upload offset mismatch");reply(false);return true;}
    ++data;size_t n=strlen(data);
    if(!n||n>512||n%2||n/2>uploadLength-uploadReceived){say("Invalid chunk size");reply(false);return true;}
    for(size_t i=0;i<n;++i)if(hex(data[i])<0){say("Invalid hex chunk");reply(false);return true;}
    for(size_t i=0;i<n;i+=2)script[uploadReceived++]=char((hex(data[i])<<4)|hex(data[i+1]));
    uploadAt=millis();Serial.printf("BADUSB_OK offset=%u\n",unsigned(uploadReceived));return true;
  }
  if(c=="badusb save"){
    if(!uploadLength||uploadReceived!=uploadLength){say("Upload incomplete");reply(false);return true;}
    scriptLength=uploadLength;script[scriptLength]=0;
    if(!validate()){cancelUpload();reply(false);return true;}
    auto* fs=filesystem();bool ok=false;
    if(fs){CardLock lock;String dest=path(uploadName),part=dest+".part";
      if(space(scriptLength+4096)&&!fs->exists(dest)&&(fs->exists(directory())||fs->mkdir(directory()))){
        fs->remove(part);File file=fs->open(part,FILE_WRITE);
        if(file){ok=file.write(reinterpret_cast<const uint8_t*>(script),scriptLength)==scriptLength;file.flush();file.close();}
        if(ok)ok=fs->rename(part,dest);if(!ok)fs->remove(part);
      }
    }
    cancelUpload();say(ok?"Saved / select the script on the detector":"Save failed; existing scripts retained");badusbRefresh();reply(ok);return true;
  }
  say("Commands: status, list, select NAME.txt, keyboard, upload, chunk, save, cancel, stop");reply(false);return true;
}
#else
bool badusbSupported(){return false;}
const char* badusbUnavailableReason(){return "This profile's USB port does not expose native HID";}
void badusbBegin(){}
void badusbLoop(){}
void badusbRefresh(){}
unsigned badusbCount(){return 0;}
const char* badusbName(unsigned){return "";}
bool badusbSelect(unsigned){return false;}
bool badusbRun(){return false;}
void badusbStop(){}
const char* badusbPreview(){return "";}
BadUsbStatus badusbStatus(){return {false,false,false,0,"",badusbUnavailableReason(),"unavailable",false};}
bool badusbKeyboardMode(){return false;}
void badusbExitKeyboard(){}
bool badusbCommand(const String& c){if(c!="badusb"&&!c.startsWith("badusb "))return false;Serial.println("BADUSB_ERROR unsupported USB hardware/profile");return true;}
#endif
