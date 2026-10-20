"""Compile the production SD writer against a failing in-memory filesystem."""
from pathlib import Path
import argparse, subprocess, sys
root=Path(__file__).resolve().parents[1]
work=root/'.work'/'sd-tests';work.mkdir(parents=True,exist_ok=True)
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
header=(root/'firmware/include/sd_records.h').read_text()
source=(root/'firmware/src/sd_records.cpp').read_text()
for include in ('#pragma once','#include "app.h"','#include "recon.h"','#include "sd_records.h"'):
    header=header.replace(include,'');source=source.replace(include,'')
fake=r'''
#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
using String=std::string;
size_t strlcpy(char* to,const char* from,size_t capacity){size_t n=strlen(from);if(capacity){memcpy(to,from,std::min(n,capacity-1));to[std::min(n,capacity-1)]=0;}return n;}
uint32_t clockMs=0,storageDrops=0,randomValue=0;
bool sdReady=true,onCard=true,space=true,shortWrite=false,badRename=false;
uint32_t millis(){return clockMs;}uint32_t micros(){return clockMs*1000;}
uint32_t esp_random(){return ++randomValue;}
void* ps_malloc(size_t n){return malloc(n);}
constexpr int pdTRUE=1,FILE_WRITE=1;void* sdMutex=(void*)1;
int xSemaphoreTake(void*,int){return pdTRUE;}void xSemaphoreGive(void*){}
struct Content {std::vector<uint8_t> data;unsigned flushes=0;};
struct File {
 std::shared_ptr<Content> content;
 explicit operator bool()const{return bool(content);}
 size_t write(const uint8_t* data,size_t n){if(!content)return 0;size_t count=shortWrite?n/2:n;content->data.insert(content->data.end(),data,data+count);return count;}
 void close(){content.reset();}void flush(){if(content)++content->flushes;}
};
namespace fs {struct FS {
 std::map<String,std::shared_ptr<Content>> files;
 bool exists(const String& path){return files.count(path);}
 bool mkdir(const char* path){files[path]=std::make_shared<Content>();return true;}
 File open(const String& path,int){auto data=std::make_shared<Content>();files[path]=data;return {data};}
 bool rename(const String& from,const String& to){if(badRename||!files.count(from)||files.count(to))return false;files[to]=files[from];files.erase(from);return true;}
};}
fs::FS storage;
fs::FS* bleLabStorage(){return &storage;}
bool bleLabStorageOnCard(){return onCard;}
bool bleLabStorageSpace(size_t){return space;}
'''
tests=r'''
int main(){
 SdRecords writer;const uint8_t header[]={0x11,0x22,0x33},record[]={1,2,3,4,5};
 onCard=false;assert(!writer.begin("/captures","wifi","pcap",header,3));onCard=true;
 assert(writer.begin("/captures","wifi","pcap",header,3));
 assert(!writer.begin("/captures","wifi","pcap",header,3));
 const String first=writer.path();
 assert(writer.append(record,sizeof(record)));assert(writer.records==0);
 writer.stop();assert(writer.busy()&&!writer.active());
 assert(!writer.append(record,sizeof(record)));writer.loop();
 assert(!writer.busy()&&!writer.failed()&&writer.records==1&&writer.bytes==8);
 assert(storage.files[first]->data==std::vector<uint8_t>({0x11,0x22,0x33,1,2,3,4,5}));
 assert(!storage.exists(first+".part"));
 assert(writer.begin("/captures","ble","csv",header,3));
 std::vector<uint8_t> large(2200,0xab);
 while(writer.append(large.data(),large.size())){}
 assert(writer.dropped==1&&writer.pending()<=64*1024);
 writer.stop();while(writer.busy())writer.loop();
 assert(!writer.failed()&&writer.records>0);
 // A failed write stops the stream and preserves partial data under .part.
 assert(writer.begin("/captures","wifi","pcap",header,3));String partial=writer.path();
 assert(writer.append(record,5));assert(writer.append(record,5));shortWrite=true;writer.loop();shortWrite=false;
 assert(writer.failed()&&!writer.busy()&&writer.dropped==2);
 assert(storage.exists(partial+".part")&&!storage.exists(partial));
 assert(writer.begin("/captures","wifi","pcap",header,3));String rename=writer.path();
 writer.append(record,5);writer.stop();badRename=true;writer.loop();badRename=false;
 assert(writer.failed()&&storage.exists(rename+".part")&&!storage.exists(rename));
 assert(writer.begin("/captures","wifi","pcap",header,3));writer.append(record,5);
 sdReady=false;writer.loop();sdReady=true;assert(writer.failed()&&!writer.busy());
 space=false;assert(!writer.begin("/captures","wifi","pcap",header,3));space=true;
 assert(writer.begin("/captures","wifi","pcap",header,3));clockMs+=10001;space=false;writer.loop();space=true;
 assert(writer.failed()&&!writer.active());
 // Cross the production 32 MiB boundary; headers recur and records are intact.
 assert(writer.begin("/captures","wifi","pcap",header,3));String segment=writer.path();
 std::vector<uint8_t> block(2048,0x5a);
 for(unsigned i=0;i<16385;++i){assert(writer.append(block.data(),block.size()));writer.loop();}
 writer.stop();while(writer.busy())writer.loop();
 assert(!writer.failed()&&writer.records==16385&&writer.dropped==0);
 assert(String(writer.path())!=segment);
 for(const auto& path:{segment,String(writer.path())}){
  const auto& data=storage.files[path]->data;
  assert(data.size()<=32*1024*1024&&(data.size()-3)%2048==0);
  assert(std::equal(data.begin(),data.begin()+3,header));
  assert(std::all_of(data.begin()+3,data.end(),[](uint8_t n){return n==0x5a;}));
 }
 puts("PASS production SD writer: queue pressure, short writes, card loss, full card, rename failure, finalization, 32 MiB rotation");
}
'''
path=work/'test.cpp';path.write_text(fake+header+source+tests,encoding='utf-8')
exe=work/('test.exe' if sys.platform=='win32' else 'test')
subprocess.run(compiler+['-O2','-UNDEBUG','-std=c++17','-I'+str(root/'firmware/include'),str(path),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
