"""Fault-inject the production, one-shot 1.85B SD format/reboot workflow.

The real sdMaintenanceBoot and sdMaintenanceCommand bodies are compiled with
fake NVS, SD and restart transports. No serial device or disk is accessed.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.work' / 'sd-maintenance-tests'
WORK.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
build_env = os.environ.copy()
build_env['ZIG_GLOBAL_CACHE_DIR'] = str(WORK / 'zig-global')
build_env['ZIG_LOCAL_CACHE_DIR'] = str(WORK / 'zig-local')

production = (ROOT / 'firmware/src/sd_maintenance.cpp').read_text(encoding='utf-8')
assert production.startswith('#ifdef BOARD_WAVESHARE_185B\n')
body = production[production.index('namespace {'):production.rindex('#endif')]
adapter = production[production.index('unsigned DragonSdMmc::clusterBytes()'):production.index('namespace {')]
source = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <map>
#include <string>
#include <vector>

constexpr int ESP_OK=0;
static std::map<std::string,std::vector<uint8_t>> nvs;
static std::vector<std::string> operations;
static unsigned beginCalls=0,putCalls=0,failBeginAt=0,failPutAt=0;
static std::string failRemove;
static bool readShort=false;
static void operation(const std::string& text){operations.push_back(text);}
struct Preferences {
  bool opened=false;
  bool begin(const char* name,bool readonly){
    assert(!strcmp(name,"dd185-sd")&&!readonly);operation("begin");
    opened=++beginCalls!=failBeginAt;return opened;
  }
  bool isKey(const char* key){assert(opened);return nvs.count(key);}
  size_t getBytesLength(const char* key){assert(opened);return nvs.at(key).size();}
  size_t getBytes(const char* key,void* target,size_t size){
    assert(opened);auto found=nvs.find(key);if(found==nvs.end())return 0;
    size=std::min(size,found->second.size());if(readShort&&size)--size;
    memcpy(target,found->second.data(),size);return size;
  }
  bool remove(const char* key){
    assert(opened);operation(std::string("remove:")+key);
    if(failRemove==key)return false;
    return nvs.erase(key)==1; // A successful remove models the committed erase.
  }
  size_t putBytes(const char* key,const void* data,size_t size){
    assert(opened);operation(std::string("put:")+key);
    if(++putCalls==failPutAt)return 0;
    const auto* first=static_cast<const uint8_t*>(data);
    nvs[key]=std::vector<uint8_t>(first,first+size);return size;
  }
  void end(){assert(opened);opened=false;operation("end");}
};
struct FakeCard {
  uint64_t id=0x123456789abcdef0ULL,bytes=8000000000ULL;
  unsigned fat=32,identityCalls=0,sizeCalls=0,fatCalls=0,formatCalls=0;
  unsigned clusterSize=32768;
  int formatError=ESP_OK;
  bool poisoned=false,interruptFormat=false;
  void check()const {assert(!poisoned&&"card accessed after IDF may have freed its handle");}
  uint64_t identity(){check();++identityCalls;operation("card:identity");return id;}
  uint64_t cardSize(){check();++sizeCalls;operation("card:size");return bytes;}
  uint64_t totalBytes(){check();return bytes-1024;}
  uint64_t usedBytes(){check();return 1024;}
  unsigned fatType(){check();++fatCalls;operation("card:fat");return fat;}
  unsigned clusterBytes(){check();operation("card:cluster");return clusterSize;}
  int formatMountedCard();
} dragonSdMmc;
struct SimulatedRestart{};
struct SimulatedPowerCut{};
static unsigned restarts=0;
struct {void restart(){operation("restart");++restarts;throw SimulatedRestart{};}} ESP;
void delay(unsigned){}
struct {
  std::string output;
  void println(const char* text){output+=text;output+='\n';}
  template<class... Args> void printf(const char* format,Args... args){
    char buffer[512];snprintf(buffer,sizeof(buffer),format,args...);output+=buffer;
  }
  void flush(){}
} Serial;
struct String : std::string {
  using std::string::string;
  bool startsWith(const char* prefix)const{return compare(0,strlen(prefix),prefix)==0;}
};
bool sdReady=true,loggingEnabled=false,captureEnabled=false;
struct {bool running=false;} stopwatch;
bool reconRadio=false,bleRadio=false,reconStorage=false,wardrive=false,upload=false;
bool reconOwnsRadio(){return reconRadio;} bool bleLabOwnsRadio(){return bleRadio;}
bool reconStorageBusy(){return reconStorage;} bool wardriveBusy(){return wardrive;}
bool wardriveUploadBusy(){return upload;}
enum class AudioMode {Idle,Playing,Recording};
struct AudioStatus {AudioMode mode=AudioMode::Idle;} audio;
AudioStatus audioStatus(){return audio;}
''' + body + r'''

template<class T> void store(const char* key,const T& value){
  const auto* data=reinterpret_cast<const uint8_t*>(&value);
  nvs[key]=std::vector<uint8_t>(data,data+sizeof(value));
}
template<class T> T stored(const char* key){
  T value{};assert(nvs.count(key)&&nvs.at(key).size()==sizeof(value));
  memcpy(&value,nvs.at(key).data(),sizeof(value));return value;
}
int FakeCard::formatMountedCard(){
  check();++formatCalls;operation("card:format");
  assert(!nvs.count("format")&&"request must be durably consumed before any erase");
  const auto pending=stored<FormatVerification>("verify");
  assert(pending.stage==1&&pending.request.identity==id&&pending.request.bytes==bytes);
  poisoned=true; // Even ESP_OK can follow a failed remount/free in IDF 5.1.4.
  if(interruptFormat)throw SimulatedPowerCut{};
  return formatError;
}
static FormatRequest matchingRequest(){
  FormatRequest request{};request.identity=dragonSdMmc.id;request.bytes=dragonSdMmc.bytes;return request;
}
static void resetRuntime(bool clearNvs=true){
  if(clearNvs)nvs.clear();operations.clear();beginCalls=putCalls=failBeginAt=failPutAt=0;
  failRemove.clear();readShort=false;dragonSdMmc={};restarts=0;Serial.output.clear();
  formatResult="not_requested";sdReady=true;loggingEnabled=captureEnabled=stopwatch.running=false;
  reconRadio=bleRadio=reconStorage=wardrive=upload=false;audio={};
}
static size_t indexOf(const char* text){
  auto found=std::find(operations.begin(),operations.end(),text);
  assert(found!=operations.end());return size_t(found-operations.begin());
}
static void noErase(){assert(dragonSdMmc.formatCalls==0&&restarts==0);}
static void expectStatus(const char* expected){assert(!strcmp(formatResult,expected));}
static void bootRestarts(){
  bool restarted=false;
  try{sdMaintenanceBoot(true);}catch(const SimulatedRestart&){restarted=true;}
  assert(restarted&&restarts==1&&dragonSdMmc.formatCalls==1);
  assert(indexOf("remove:format")<indexOf("put:verify"));
  assert(indexOf("put:verify")<indexOf("card:format"));
  assert(indexOf("card:format")<indexOf("restart"));
  // Poisoned card methods assert individually. This also forbids new methods
  // being added to a post-format path without explicitly extending the fake.
}
static void requestChecks(){
  resetRuntime();assert(sdMaintenanceBoot(true));noErase();assert(operations.size()==2);
  resetRuntime();assert(!sdMaintenanceBoot(false));noErase();
  resetRuntime();store("format",matchingRequest());failBeginAt=1;
  assert(!sdMaintenanceBoot(true));noErase();assert(nvs.count("format"));
  resetRuntime();store("format",matchingRequest());failRemove="format";
  assert(!sdMaintenanceBoot(true));expectStatus("request_clear_failed");noErase();
  assert(nvs.count("format")&&dragonSdMmc.identityCalls==0);

  resetRuntime();store("format",matchingRequest());
  assert(!sdMaintenanceBoot(false));expectStatus("card_not_mounted");
  assert(!nvs.count("format")&&dragonSdMmc.identityCalls==0);noErase();
  resetRuntime(false);assert(sdMaintenanceBoot(true));noErase(); // later insertion cannot retry

  for(unsigned malformed=0;malformed<4;++malformed){
    resetRuntime();auto request=matchingRequest();
    if(malformed==0)request.magic^=1;
    if(malformed==1)request.reserved=1;
    store("format",request);
    if(malformed==2)nvs["format"].pop_back();
    if(malformed==3)readShort=true;
    assert(sdMaintenanceBoot(true));expectStatus("invalid_request");
    assert(!nvs.count("format")&&dragonSdMmc.identityCalls==0);noErase();
  }
  for(unsigned mismatch=0;mismatch<3;++mismatch){
    resetRuntime();auto request=matchingRequest();
    if(mismatch==0)request.identity^=1;
    if(mismatch==1)++request.bytes;
    if(mismatch==2)request.identity=0;
    store("format",request);assert(sdMaintenanceBoot(true));
    expectStatus("card_changed");assert(!nvs.count("format"));noErase();
  }
  for(unsigned failure=0;failure<2;++failure){
    resetRuntime();store("format",matchingRequest());
    if(failure==0)failBeginAt=2;else failPutAt=1;
    assert(!sdMaintenanceBoot(true));expectStatus("verification_storage_failed");
    assert(!nvs.count("format")&&!nvs.count("verify"));noErase();
  }
}
static void twoBootChecks(){
  resetRuntime();store("format",matchingRequest());bootRestarts();
  assert(stored<FormatVerification>("verify").stage==2&&!nvs.count("format"));
  resetRuntime(false);assert(sdMaintenanceBoot(true));expectStatus("complete");
  assert(!nvs.count("verify")&&dragonSdMmc.fatCalls==1);noErase();
  resetRuntime(false);assert(sdMaintenanceBoot(true));expectStatus("not_requested");noErase();

  resetRuntime();store("format",matchingRequest());dragonSdMmc.formatError=-1;bootRestarts();
  resetRuntime(false);assert(sdMaintenanceBoot(true));expectStatus("format_failed");
  assert(dragonSdMmc.fatCalls==0&&!nvs.count("verify"));noErase();

  resetRuntime();store("format",matchingRequest());failPutAt=2;bootRestarts();
  assert(stored<FormatVerification>("verify").stage==1);
  resetRuntime(false);assert(sdMaintenanceBoot(true));expectStatus("interrupted_result_unknown");noErase();

  resetRuntime();store("format",matchingRequest());dragonSdMmc.interruptFormat=true;
  bool interrupted=false;
  try{sdMaintenanceBoot(true);}catch(const SimulatedPowerCut&){interrupted=true;}
  assert(interrupted&&dragonSdMmc.formatCalls==1&&!nvs.count("format"));
  resetRuntime(false);assert(sdMaintenanceBoot(true));expectStatus("interrupted_result_unknown");noErase();

  for(unsigned failure=0;failure<6;++failure){
    resetRuntime();store("format",matchingRequest());bootRestarts();resetRuntime(false);
    if(failure==1)dragonSdMmc.id^=1;
    if(failure==2)++dragonSdMmc.bytes;
    if(failure==3)dragonSdMmc.fat=16;
    if(failure==4)dragonSdMmc.fat=0;
    if(failure==5)dragonSdMmc.clusterSize=512;
    assert(sdMaintenanceBoot(failure!=0)==(failure!=0));
    expectStatus(failure<3?"verification_card_missing_or_changed":
      failure<5?"fresh_mount_not_fat32":"fresh_mount_cluster_mismatch");
    assert(!nvs.count("verify"));noErase();
  }
  resetRuntime();FormatVerification verification{};verification.request=matchingRequest();
  verification.stage=2;verification.result=ESP_OK;store("verify",verification);failRemove="verify";
  assert(!sdMaintenanceBoot(true));expectStatus("verification_clear_failed");
  assert(dragonSdMmc.identityCalls==0&&nvs.count("verify"));noErase();
  resetRuntime();store("verify",verification);nvs["verify"].pop_back();
  assert(sdMaintenanceBoot(true));expectStatus("verification_card_missing_or_changed");noErase();
}
static String formatCommand(){
  char text[160];snprintf(text,sizeof(text),"sd format %016" PRIx64 " %" PRIu64 " ERASE",dragonSdMmc.id,dragonSdMmc.bytes);
  return text;
}
static void commandChecks(){
  resetRuntime();assert(!sdMaintenanceCommand("not sd"));noErase();
  assert(sdMaintenanceCommand("sd status"));assert(Serial.output.find("SD_STATUS")!=std::string::npos);noErase();
  for(bool* busy:{&stopwatch.running,&reconRadio,&bleRadio,&reconStorage,&wardrive,&upload,&loggingEnabled,&captureEnabled}){
    resetRuntime();*busy=true;assert(sdMaintenanceCommand(formatCommand()));
    assert(nvs.empty()&&dragonSdMmc.identityCalls==0);noErase();
  }
  for(AudioMode mode:{AudioMode::Playing,AudioMode::Recording}){
    resetRuntime();audio.mode=mode;assert(sdMaintenanceCommand(formatCommand()));assert(nvs.empty());noErase();
  }
  resetRuntime();sdReady=false;assert(sdMaintenanceCommand(formatCommand()));assert(nvs.empty());noErase();
  for(unsigned malformed=0;malformed<5;++malformed){
    resetRuntime();String command=formatCommand();
    if(malformed==0)command="sd format";
    if(malformed==1)command+=" extra";
    if(malformed==2)command.replace(command.find("ERASE"),5,"erase");
    if(malformed==3)dragonSdMmc.id^=1;
    if(malformed==4)++dragonSdMmc.bytes;
    assert(sdMaintenanceCommand(command));assert(nvs.empty());noErase();
  }
  resetRuntime();dragonSdMmc.fat=16;assert(sdMaintenanceCommand(formatCommand()));assert(nvs.empty());noErase();
  for(unsigned failure=0;failure<2;++failure){
    resetRuntime();if(failure==0)failBeginAt=1;else failPutAt=1;
    assert(sdMaintenanceCommand(formatCommand()));assert(nvs.empty());noErase();
  }
  resetRuntime();bool restarted=false;
  try{sdMaintenanceCommand(formatCommand());}catch(const SimulatedRestart&){restarted=true;}
  assert(restarted&&restarts==1&&dragonSdMmc.formatCalls==0);
  auto request=stored<FormatRequest>("format");
  assert(request.magic==FormatMagic&&request.identity==dragonSdMmc.id&&request.bytes==dragonSdMmc.bytes);
  assert(indexOf("put:format")<indexOf("restart"));
}
int main(){requestChecks();twoBootChecks();commandChecks();}
'''
adapter_source = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define FF_VOLUMES 2
#define FF_MULTI_PARTITION 1
#define FF_MIN_SS 512
#define FF_MAX_SS 4096
using BYTE=uint8_t;using DWORD=uint32_t;using UINT=unsigned;
enum FRESULT {FR_OK,FR_DISK_ERR};
struct FATFS {uint16_t csize=64,ssize=512;};
struct MKFS_PARM {BYTE fmt,n_fat;UINT align,n_root;DWORD au_size;};
struct PARTITION {BYTE pd,pt;};
constexpr int FM_FAT32=2,ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=0x103,ESP_ERR_NO_MEM=0x101;
constexpr unsigned MALLOC_CAP_INTERNAL=1,MALLOC_CAP_DMA=2,MALLOC_CAP_8BIT=4;
static PARTITION VolToPart[FF_VOLUMES]={{0,0},{1,0}};
static std::vector<std::string> operations;
static BYTE registeredDrive=0;
static bool allocationFails=false,unmounted=false,returnNullFs=false;
static FRESULT unmountResult=FR_OK,formatResult=FR_OK,freeResult=FR_OK;
static unsigned allocCalls=0,freeCalls=0,mkfsCalls=0,unmountCalls=0;
static std::string expectedVolume="0:";
alignas(16) static uint8_t workspace[4096];
static uint8_t card;
static FATFS fs;
struct DragonSdMmc {
  const uint8_t* _card=&card;
  BYTE _pdrv=0;
  unsigned clusterBytes() const;
  int formatMountedCard();
};
BYTE ff_diskio_get_pdrv_card(const uint8_t* ptr){
  assert(ptr==&card&&!unmounted);operations.push_back("lookup");return registeredDrive;
}
void* heap_caps_malloc(size_t size,unsigned caps){
  assert(size==4096&&caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT));
  ++allocCalls;operations.push_back("allocate");return allocationFails?nullptr:workspace;
}
void heap_caps_free(void* pointer){assert(pointer==workspace);++freeCalls;operations.push_back("free");}
FRESULT f_mount(FATFS* mounted,const char* volume,BYTE immediate){
  assert(!mounted&&!immediate&&volume==expectedVolume&&allocCalls==1&&!allocationFails);
  ++unmountCalls;operations.push_back("unmount");unmounted=unmountResult==FR_OK;return unmountResult;
}
FRESULT f_mkfs(const char* volume,const MKFS_PARM* options,void* work,UINT size){
  assert(unmounted&&volume==expectedVolume&&work==workspace&&size==4096);
  assert(options->fmt==FM_FAT32&&options->n_fat==2&&options->align==0&&options->n_root==0&&options->au_size==32768);
  ++mkfsCalls;operations.push_back("mkfs");return formatResult;
}
FRESULT f_getfree(const char* volume,DWORD* freeClusters,FATFS** result){
  assert(!unmounted&&volume==expectedVolume);*freeClusters=100;*result=returnNullFs?nullptr:&fs;return freeResult;
}
''' + adapter + r'''
static void reset(){
  VolToPart[0]={0,0};VolToPart[1]={1,0};operations.clear();registeredDrive=0;
  allocationFails=unmounted=returnNullFs=false;unmountResult=formatResult=freeResult=FR_OK;
  allocCalls=freeCalls=mkfsCalls=unmountCalls=0;expectedVolume="0:";fs={};
}
static void formatChecks(){
  for(unsigned invalid=0;invalid<5;++invalid){
    reset();DragonSdMmc sd;
    if(invalid==0)sd._card=nullptr;
    if(invalid==1)sd._pdrv=0xff;
    if(invalid==2)registeredDrive=1;
    if(invalid==3)VolToPart[0].pd=1;
    if(invalid==4)VolToPart[0].pt=1;
    assert(sd.formatMountedCard()==ESP_ERR_INVALID_STATE);
    assert(!allocCalls&&!unmountCalls&&!mkfsCalls&&!freeCalls);
  }
  reset();DragonSdMmc sd;allocationFails=true;
  assert(sd.formatMountedCard()==ESP_ERR_NO_MEM&&allocCalls==1&&!unmountCalls&&!mkfsCalls&&!freeCalls&&!unmounted);
  reset();unmountResult=FR_DISK_ERR;
  assert(sd.formatMountedCard()==ESP_FAIL&&unmountCalls==1&&!mkfsCalls&&freeCalls==1&&!unmounted);
  reset();formatResult=FR_DISK_ERR;
  assert(sd.formatMountedCard()==ESP_FAIL&&unmountCalls==1&&mkfsCalls==1&&freeCalls==1&&unmounted);
  for(BYTE drive:{BYTE(0),BYTE(1)}){
    reset();sd._pdrv=registeredDrive=drive;expectedVolume=drive==0?"0:":"1:";
    assert(sd.formatMountedCard()==ESP_OK&&unmounted&&mkfsCalls==1&&freeCalls==1);
    assert((operations==std::vector<std::string>{"lookup","allocate","unmount","mkfs","free"}));
  }
}
static void clusterChecks(){
  reset();DragonSdMmc sd;
  assert(sd.clusterBytes()==32768);
  fs.ssize=4096;fs.csize=8;assert(sd.clusterBytes()==32768);
  fs.ssize=512;fs.csize=1;assert(sd.clusterBytes()==512);
  returnNullFs=true;assert(!sd.clusterBytes());returnNullFs=false;
  freeResult=FR_DISK_ERR;assert(!sd.clusterBytes());freeResult=FR_OK;
  sd._pdrv=0xff;assert(!sd.clusterBytes());sd._pdrv=0;
  sd._card=nullptr;assert(!sd.clusterBytes());
}
int main(){formatChecks();clusterChecks();}
'''

def run_fixture(name, contents):
    path = WORK / (name + '.cpp')
    path.write_text(contents, encoding='utf-8')
    exe = WORK / (name + '.exe' if sys.platform == 'win32' else name)
    log_path = WORK / (name + '-build.log')
    with log_path.open('w', encoding='utf-8') as log:
        result = subprocess.run(compiler + ['-std=c++17', '-UNDEBUG', str(path), '-o', str(exe)],
                                stdout=log, stderr=subprocess.STDOUT, env=build_env)
    if result.returncode:
        print(log_path.read_text(encoding='utf-8', errors='replace')[-16000:], file=sys.stderr)
        raise SystemExit(result.returncode)
    subprocess.run([str(exe)], check=True)

run_fixture('sd_maintenance', source)
run_fixture('sd_adapter', adapter_source)
print('PASS production SD format request consumption, identity/capacity checks, NVS faults, no card access after format, interrupted/reboot verification, FAT32/32KB completion, busy/confirmation gates; actual format adapter drive mapping, pre-unmount allocation, two FATs, 32KB clusters and failure cleanup')
