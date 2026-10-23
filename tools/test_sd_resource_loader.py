"""Compile the production startup loader against a fault-injecting SD adapter."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];work=root/'.work/resource-loader';work.mkdir(parents=True,exist_ok=True)
source=(root/'firmware/src/sd_resources.cpp').read_text()
for include in ['#include "sd_resources.h"','#include "app.h"','#include <esp_heap_caps.h>']:source=source.replace(include,'')
fake=r'''
#define BOARD_HAS_SD 1
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstdint>
bool sdReady=true,exists=true,shortRead=false,locked=false;size_t freeBytes=8*1024*1024;unsigned reads=0;
void* sdMutex=(void*)1;constexpr int pdTRUE=1,FILE_READ=0,MALLOC_CAP_SPIRAM=0;
unsigned micros(){static unsigned t=0;return ++t;}unsigned pdMS_TO_TICKS(unsigned t){return t;}void delay(unsigned){}
int xSemaphoreTake(void*,unsigned){locked=true;return pdTRUE;}void xSemaphoreGive(void*){locked=false;}
size_t heap_caps_get_free_size(int){return freeBytes;}void* ps_malloc(size_t n){return malloc(n);}
struct {template<class... T> void printf(const char* f,T... t){std::printf(f,t...);}} Serial;
std::vector<uint8_t> card;
struct File {size_t at=0;explicit operator bool()const{return exists;}size_t size(){return card.size();}
 size_t read(uint8_t* out,size_t n){assert(locked);++reads;if(shortRead)return 0;assert(n<=4096&&at+n<=card.size());memcpy(out,card.data()+at,n);at+=n;return n;}void close(){} };
struct {File open(const char* path,int){assert(!strcmp(path,"/dragon/resources/vendors.ddr"));return {};}} CardFS;
void sdResourcesStatus();
'''
tests=r'''
int main(int argc,char** argv){
 std::ifstream f(argv[1],std::ios::binary);card={std::istreambuf_iterator<char>(f),{}};
 const char* mode=argv[2];
 if(!strcmp(mode,"missing"))exists=false;
 if(!strcmp(mode,"nocard"))sdReady=false;
 if(!strcmp(mode,"memory"))freeBytes=0;
 if(!strcmp(mode,"short"))shortRead=true;
 if(!strcmp(mode,"corrupt"))card.back()^=1;
 if(!strcmp(mode,"oversize"))card.resize(dragon::ResourceIndex::MaxBytes+1);
 sdResourcesBegin();assert(!locked);
 if(!strcmp(mode,"valid")){assert(loadedBytes==card.size());assert(resourceIndex.count()==54282);unsigned saved=reads;card.clear();sdReady=false;uint8_t mac[]={0,0,0,0,0,0};sdResourceVendor(mac,false,0);assert(reads==saved&&resourceIndex.count()==54282);}
 else assert(!memory&&!loadedBytes&&!resourceIndex.count());
}
'''
p=work/'loader.cpp';p.write_text(fake+source+tests);exe=work/'loader.exe'
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG','-I'+str(root/'firmware/include'),str(p),'-o',str(exe)],check=True)
for mode in ['valid','missing','nocard','memory','short','corrupt','oversize']:
    subprocess.run([str(exe),str(root/'.work/resource-tests/full.ddr'),mode],check=True)
print('PASS production SD loader: missing card/file, short read, corruption, allocation, size, removal after load')
