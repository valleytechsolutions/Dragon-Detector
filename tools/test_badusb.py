"""Fault-inject the actual BadUSB runtime with in-memory FS/USB/serial transports.

Builds the production .cpp without replacing its function bodies. No USB device,
payload process, or physical storage is accessed by these tests.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.work' / 'badusb-tests'
WORK.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
env = os.environ.copy()
env['ZIG_GLOBAL_CACHE_DIR'] = str(WORK / 'zig-global')
env['ZIG_LOCAL_CACHE_DIR'] = str(WORK / 'zig-local')

mock = r'''
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
struct String : std::string {
  using std::string::string;
  String(const std::string& s):std::string(s){}
  bool startsWith(const char* s)const{return compare(0,strlen(s),s)==0;}
  String substring(size_t start)const{return substr(start);}
};
// glibc 2.38+ and macOS both declare strlcpy; a renamed mock avoids clashing with either.
inline size_t testStrlcpy(char* out,const char* in,size_t cap){size_t n=strlen(in);if(cap){size_t count=std::min(n,cap-1);memcpy(out,in,count);out[count]=0;}return n;}
#define strlcpy testStrlcpy
uint32_t now=0;
uint32_t millis(){return now;}
struct SerialPort {
  std::string output;
  void println(const char* text){output+=text;output+='\n';}
  void flush(){}
  template<class... Args> void printf(const char* format,Args... args){char line[512];snprintf(line,sizeof(line),format,args...);output+=line;}
} Serial;
bool driveBusy=false,uploadBusy=false,storageBusy=false;
bool wardriveBusy(){return driveBusy;}
bool wardriveUploadBusy(){return uploadBusy;}
bool reconStorageBusy(){return storageBusy;}
using SemaphoreHandle_t=int;
SemaphoreHandle_t sdMutex=1;
constexpr int portMAX_DELAY=0;
int locks=0;
void xSemaphoreTake(SemaphoreHandle_t,int){assert(locks++==0);}
void xSemaphoreGive(SemaphoreHandle_t){assert(--locks==0);}
constexpr const char* FILE_WRITE="w";
struct Node {std::string name,data;bool dir=false;};
namespace fs {class FS;}
class File {
  std::shared_ptr<Node> node;
  fs::FS* owner=nullptr;
  std::vector<std::shared_ptr<Node>> children;
  size_t child=0;
 public:
  File()=default;
  File(std::shared_ptr<Node> n,fs::FS* fs):node(n),owner(fs){}
  operator bool()const{return bool(node);}
  bool isDirectory()const{return node&&node->dir;}
  size_t size()const{return node?node->data.size():0;}
  const char* name()const{return node?node->name.c_str():"";}
  size_t read(uint8_t*,size_t);
  size_t write(const uint8_t*,size_t);
  void flush(){}
  void close(){node.reset();}
  File openNextFile(){return child<children.size()?File(children[child++],owner):File();}
  friend class fs::FS;
};
namespace fs {
class FS {
 public:
  std::map<std::string,std::shared_ptr<Node>> nodes;
  size_t total=64*1024*1024,used=1024;
  bool failOpen=false,failWrite=false,failRead=false,failRename=false,failMkdir=false;
  unsigned writes=0,begins=0;
  bool begin(bool format){assert(!format);++begins;return total>0;}
  size_t totalBytes(){return total;}
  size_t usedBytes(){return used;}
  bool exists(const String& path){return nodes.count(path);}
  bool mkdir(const String& path){if(failMkdir)return false;nodes[path]=std::make_shared<Node>(Node{path,"",true});return true;}
  void put(const std::string& path,const std::string& data){nodes[path]=std::make_shared<Node>(Node{path,data,false});}
  File open(const String& path,const char* mode=nullptr){
    if(failOpen)return {};
    if(mode){assert(!strcmp(mode,FILE_WRITE));put(path,"");}
    auto found=nodes.find(path);if(found==nodes.end())return {};
    File file(found->second,this);
    if(file.isDirectory())for(auto& entry:nodes){const auto prefix=path+"/";if(entry.first.compare(0,prefix.size(),prefix)==0&&entry.first.find('/',prefix.size())==std::string::npos)file.children.push_back(entry.second);}
    return file;
  }
  bool remove(const String& path){return nodes.erase(path)>0;}
  bool rename(const String& from,const String& to){if(failRename||!exists(from)||exists(to))return false;auto node=nodes.at(from);nodes.erase(from);node->name=to;nodes[to]=node;return true;}
};
}
size_t File::read(uint8_t* out,size_t n){assert(node&&!node->dir);n=std::min(n,node->data.size());if(owner->failRead&&n)--n;memcpy(out,node->data.data(),n);return n;}
size_t File::write(const uint8_t* in,size_t n){assert(node&&!node->dir);++owner->writes;if(owner->failWrite&&n)--n;node->data.assign(reinterpret_cast<const char*>(in),n);return n;}
fs::FS LittleFS,CardFS;
bool sdReady=true;
using esp_event_base_t=const char*;
constexpr int ARDUINO_USB_STOPPED_EVENT=1,ARDUINO_USB_SUSPEND_EVENT=2;
struct USBClass {
  using Callback=void(*)(void*,esp_event_base_t,int32_t,void*);
  Callback callback=nullptr;
  unsigned begins=0;
  void onEvent(Callback cb){callback=cb;}
  void begin(){++begins;}
  void fire(int event){if(callback)callback(nullptr,nullptr,event,nullptr);}
} USB;
struct KeyReport {uint8_t modifiers=0,reserved=0,keys[6]{};};
constexpr int HID_REPORT_ID_KEYBOARD=1;
bool connected=true;
unsigned sends=0,disconnectOnSend=0,failOnSend=0,keyboardBegins=0;
std::vector<KeyReport> sentReports;
struct USBHID {
  bool ready(){return connected;}
  bool SendReport(int id,const void* data,size_t size,unsigned timeout){
    assert(id==HID_REPORT_ID_KEYBOARD&&size==sizeof(KeyReport)&&timeout<=20);++sends;
    if(!connected||sends==failOnSend)return false;
    sentReports.push_back(*static_cast<const KeyReport*>(data));
    if(sends==disconnectOnSend){connected=false;USB.fire(ARDUINO_USB_STOPPED_EVENT);}
    return true;
  }
};
struct USBHIDKeyboard {void begin(){++keyboardBegins;}};
'''
(WORK / 'runtime.h').write_text(mock)
# Keyboard mode restarts the chip; model each restart request as an exception.
(WORK / 'badusb_mode.h').write_text(r'''#pragma once
#include <string>
struct RestartRequested {bool keyboard;std::string script;};
namespace dragon::badusb {
inline bool mockKeyboard=true;
inline std::string mockScript;
inline void bootMode(){}
inline bool keyboardMode(){return mockKeyboard;}
inline const char* keyboardScript(){return mockScript.c_str();}
[[noreturn]] inline void restartIntoKeyboard(const char* name){throw RestartRequested{true,name};}
[[noreturn]] inline void restartIntoSerial(){throw RestartRequested{false,""};}
}
''')
for header in ['Arduino.h', 'app.h', 'recon.h', 'wardrive.h', 'wardrive_upload.h',
               'LittleFS.h', 'USB.h', 'USBHIDKeyboard.h']:
    (WORK / header).write_text('#pragma once\n#include "runtime.h"\n')

tests = r'''
#include "badusb.cpp"
static std::string command(const std::string& text){Serial.output.clear();assert(badusbCommand(String(text)));assert(locks==0);return Serial.output;}
static bool ok(const std::string& text){return command(text).find("BADUSB_OK")!=std::string::npos;}
static bool rejected(const std::string& text){return command(text).find("BADUSB_ERROR")!=std::string::npos;}
static std::string hexadecimal(const std::string& source){std::string out;const char* digits="0123456789abcdef";for(unsigned char c:source){out+=digits[c>>4];out+=digits[c&15];}return out;}
static void stage(const char* name,const std::string& text){assert(ok(std::string("badusb upload ")+name+" "+std::to_string(text.size())));assert(ok("badusb chunk 0 "+hexadecimal(text)));}
static void reset(bool card=true){
  runner.stop();runner=dragon::badusb::Runner{};cancelUpload();selected=false;selectedName[0]=0;scriptLength=0;releasePending=false;linkLost=false;
  onCard=storageReady=false;fileCount=1;memset(names,0,sizeof(names));
  now=0;driveBusy=uploadBusy=storageBusy=false;LittleFS={};CardFS={};sdReady=card;
  connected=true;sends=disconnectOnSend=failOnSend=keyboardBegins=0;sentReports.clear();Serial.output.clear();USB.callback=nullptr;USB.begins=0;
  dragon::badusb::mockKeyboard=true;dragon::badusb::mockScript.clear();
  badusbBegin();assert(keyboardBegins==1&&USB.begins==1&&sends==0&&!runner.running());
}
static void completeRun(){for(unsigned i=0;runner.running()&&i<5000;++i){badusbLoop();now+=10;}assert(!runner.running()&&runner.complete()&&!releasePending);}
static void noHeldKeys(){assert(!sentReports.empty());KeyReport zero{};assert(!memcmp(&sentReports.back(),&zero,sizeof(zero)));}
static void uploads(){
  reset();const std::string text="REM benign\r\nSTRING  Hello \r\nENTER\r\n";
  assert(!badusbCommand("other command"));assert(badusbCount()==1&&!strcmp(badusbName(0),"demo.txt"));
  assert(ok("badusb select demo.txt")&&!runner.running()&&sends==0);assert(rejected("badusb run")&&sends==0);
  stage("hello.txt",text);assert(!badusbStatus().selected&&uploadReceived==text.size());
  assert(ok("badusb save"));assert(CardFS.nodes.at("/dragon/badusb/hello.txt")->data==text);assert(!uploadLength&&!runner.running()&&sends==0);
  assert(badusbCount()==2&&ok("badusb select hello.txt"));assert(std::string(badusbPreview())==text&&sends==0);
  assert(rejected("badusb upload hello.txt 6"));assert(CardFS.nodes.at("/dragon/badusb/hello.txt")->data==text);
  assert(badusbRun());completeRun();noHeldKeys();assert(sentReports.size()==16); // " Hello " (7 keys) + ENTER, each press + release.
  for(const char* request:{"badusb upload ../a.txt 4","badusb upload demo.txt 4","badusb upload a.bin 4","badusb upload .txt 4","badusb upload a.txt 0","badusb upload a.txt -1","badusb upload a.txt +2","badusb upload a.txt 8193","badusb upload a.txt 4294967297","badusb upload a.txt -4294967295","badusb upload a.txt 99999999999999999999999","badusb upload a.txt 2 extra"})assert(rejected(request)&&!uploadLength);
  assert(ok("badusb upload pieces.txt 6"));
  for(const char* request:{"badusb chunk 1 454e","badusb chunk 0 4","badusb chunk 0 gg","badusb chunk 0 454e5445520a00","badusb chunk 0abcd","badusb chunk 4294967296 454e","badusb chunk -4294967296 454e","badusb chunk +0 454e"}){assert(rejected(request));assert(uploadReceived==0);}
  assert(ok("badusb chunk 0 454e"));assert(rejected("badusb save")&&uploadReceived==2);assert(rejected("badusb chunk 0 454e")&&uploadReceived==2);
  assert(ok("badusb chunk 2 5445520a"));assert(ok("badusb save"));assert(CardFS.nodes.at("/dragon/badusb/pieces.txt")->data=="ENTER\n");
  stage("bad.txt","STRING okay\nUNSUPPORTED\n");assert(rejected("badusb save")&&!CardFS.exists("/dragon/badusb/bad.txt")&&!uploadLength);
  assert(ok("badusb upload pending.txt 6"));now+=30001;badusbLoop();assert(!uploadLength&&rejected("badusb chunk 0 454e5445520a"));
  assert(ok("badusb upload cancel.txt 6"));assert(ok("badusb cancel")&&!uploadLength);
}
static void storageFailures(){
  for(unsigned failure=0;failure<4;++failure){
    reset();CardFS.put("/dragon/badusb/keep.txt","STRING existing");stage("new.txt","ENTER\n");
    if(failure==0)CardFS.failWrite=true;if(failure==1)CardFS.failRename=true;if(failure==2)CardFS.failOpen=true;if(failure==3)CardFS.used=CardFS.total-10;
    assert(rejected("badusb save"));assert(!CardFS.exists("/dragon/badusb/new.txt")&&!CardFS.exists("/dragon/badusb/new.txt.part"));assert(CardFS.nodes.at("/dragon/badusb/keep.txt")->data=="STRING existing");assert(!uploadLength&&sends==0);
  }
  reset();CardFS.put("/dragon/badusb/z.txt","ENTER");CardFS.put("/dragon/badusb/a.txt","TAB");CardFS.put("/dragon/badusb/invalid.txt","BROKEN");CardFS.put("/dragon/badusb/oversize.txt",std::string(8193,'a'));CardFS.mkdir("/dragon/badusb/dir.txt");badusbRefresh();
  assert(badusbCount()==4&&!strcmp(badusbName(1),"a.txt")&&!strcmp(badusbName(3),"z.txt"));assert(rejected("badusb select invalid.txt")&&!badusbStatus().selected);
  CardFS.failRead=true;assert(rejected("badusb select a.txt")&&!badusbStatus().selected);CardFS.failRead=false;assert(ok("badusb select a.txt"));
  sdReady=false;assert(!badusbSelect(1));assert(!badusbStatus().selected); // Removed card does not fall back to unrelated flash files.
  reset(false);stage("flash.txt","ENTER\n");assert(ok("badusb save"));assert(LittleFS.exists("/badusb/flash.txt")&&!CardFS.exists("/dragon/badusb/flash.txt"));
  reset(false);LittleFS.total=0;storageReady=false;assert(rejected("badusb upload no.txt 6"));assert(badusbSelect(0)); // Built-in demo needs no filesystem.
}
static void lifecycle(){
  reset();assert(badusbSelect(0));for(now=0;now<10000;now+=10)badusbLoop();assert(sends==0&&!runner.running());
  connected=false;assert(!badusbRun());connected=true;assert(badusbRun());badusbLoop();badusbLoop();assert(runner.running()&&sends==0); // REM then DELAY.
  USB.fire(ARDUINO_USB_SUSPEND_EVENT);badusbLoop();assert(!runner.running()&&sends==0);now+=5000;badusbLoop();assert(sends==0); // No resume after wake.
  assert(badusbRun());badusbStop();now+=5000;badusbLoop();assert(!runner.running()&&sends==0);
  for(unsigned reason=0;reason<3;++reason){assert(badusbRun());if(reason==0)driveBusy=true;if(reason==1)uploadBusy=true;if(reason==2)storageBusy=true;badusbLoop();assert(!runner.running());driveBusy=uploadBusy=storageBusy=false;}
  reset();stage("keys.txt","STRING ab");assert(ok("badusb save")&&ok("badusb select keys.txt"));assert(badusbRun());
  assert(rejected("badusb upload later.txt 6"));assert(rejected("badusb select demo.txt"));assert(!badusbSelect(0));
  disconnectOnSend=1;badusbLoop();assert(!runner.running()&&releasePending&&sentReports.size()==1);assert(sentReports.back().keys[0]==4);
  connected=true;badusbLoop();assert(!releasePending&&!runner.running());noHeldKeys();now+=100;badusbLoop();assert(sentReports.size()==2); // Reconnect only releases; it cannot resume.
  assert(badusbRun());badusbLoop();assert(sentReports.size()==4);assert(ok("badusb stop")&&!runner.running());noHeldKeys();
  reset();stage("keys.txt","ENTER\n");assert(ok("badusb save")&&ok("badusb select keys.txt"));assert(badusbRun());failOnSend=2;badusbLoop();assert(!runner.running());noHeldKeys(); // Failed release retries in the same loop.
  reset();stage("keys.txt","ENTER\n");assert(ok("badusb save")&&ok("badusb select keys.txt"));assert(badusbRun());failOnSend=1;badusbLoop();assert(!runner.running());noHeldKeys(); // Ambiguous key-down failure still sends release.
}
template<class F> static RestartRequested restart(F f){try{f();}catch(const RestartRequested& r){return r;}assert(!"expected a restart");return {};}
static void modes(){
  // Normal boot: USB serial only. TinyUSB and the keyboard are never started.
  reset();dragon::badusb::mockKeyboard=false;keyboardBegins=USB.begins=0;badusbBegin();
  assert(keyboardBegins==0&&USB.begins==0&&!badusbKeyboardMode()&&!badusbStatus().connected&&!badusbStatus().keyboardMode);
  assert(command("badusb status").find("mode=serial connected=0")!=std::string::npos);
  assert(ok("badusb select demo.txt"));
  // Run in serial mode only requests the keyboard restart; nothing is typed.
  auto r=restart([]{badusbRun();});assert(r.keyboard&&r.script=="demo.txt"&&sends==0&&!runner.running());
  r=restart([]{command("badusb keyboard");});assert(r.keyboard&&r.script=="demo.txt"&&sends==0);
  assert(ok("badusb upload pending.txt 6"));assert(rejected("badusb keyboard")&&uploadLength);assert(ok("badusb cancel"));
  badusbExitKeyboard();assert(!runner.running()); // No-op outside keyboard mode.
  // Keyboard boot: the selection is restored, but the script waits for Run.
  reset();dragon::badusb::mockScript="demo.txt";keyboardBegins=USB.begins=0;sentReports.clear();sends=0;selected=false;badusbBegin();
  assert(keyboardBegins==1&&USB.begins==1&&badusbStatus().keyboardMode&&badusbStatus().selected&&!strcmp(badusbStatus().name,"demo.txt"));
  for(int i=0;i<500;++i){badusbLoop();now+=10;}assert(sends==0&&!runner.running());
  assert(command("badusb status").find("mode=keyboard connected=1")!=std::string::npos);assert(rejected("badusb keyboard"));
  assert(badusbRun()&&runner.running());r=restart([]{badusbExitKeyboard();});assert(!r.keyboard&&!runner.running()&&!releasePending);
  // A missing carried selection leaves keyboard mode idle.
  reset();dragon::badusb::mockScript="gone.txt";selected=false;badusbBegin();assert(!badusbStatus().selected&&sends==0);
}
int main(){uploads();storageFailures();lifecycle();modes();free(script);script=nullptr;puts("PASS BadUSB real runtime: strict upload, file selection, failed saves, no auto-run, delay cancellation, stop, disconnect, release retry and one-shot keyboard mode");}
'''
(WORK / 'test.cpp').write_text(tests)
flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
output = WORK / ('badusb-sanitize' if args.sanitize else 'badusb')
subprocess.run(compiler + flags + ['-std=c++17', '-UNDEBUG', '-DDRAGON_BADUSB=1',
    '-DARDUINO_USB_MODE=1', '-DARDUINO_USB_CDC_ON_BOOT=1', '-DBOARD_HAS_SD=1',
    '-I' + str(WORK), '-I' + str(ROOT / 'firmware/include'), '-I' + str(ROOT / 'firmware/src'),
    str(WORK / 'test.cpp'), '-o', str(output)], check=True, env=env)
subprocess.run([str(output)], check=True, env=env)
