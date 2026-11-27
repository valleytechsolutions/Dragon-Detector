"""Keep persistent media allocations out of the P4 radio's internal DMA heap."""
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
work=root/'.work/p4-media-memory-tests';work.mkdir(parents=True,exist_ok=True)
driver=(root/'firmware/src/voice_recorder.cpp').read_text()
start=driver.index('bool ready(){');end=driver.index('// Serial replies wait',start)
source=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#define BOARD_WAVESHARE_P4_KNOB 1
#define BOARD_HAS_MP3 1
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,pdPASS=1;
constexpr unsigned RecRingSamples=16384,PlayRingSamples=16384,PlayOutMax=7000,Mp3BufBytes=4096;
struct Ring{int16_t* data=nullptr;void attach(int16_t* p,unsigned){data=p;}} recRing,playRing;
int16_t *outBuffer=nullptr,*mp3Pcm=nullptr;uint8_t* mp3Buf=nullptr;
using TaskHandle_t=void*;TaskHandle_t ioTask=nullptr,micTask=nullptr;void* commands=nullptr;
struct Command{char bytes[148];};
unsigned calls=0,failAt=0,micStarts=0,ioStarts=0;bool failWorker=false;std::vector<void*> blocks;
void* heap_caps_malloc(size_t n,int caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(++calls==failAt)return nullptr;void* p=malloc(n);assert(p);blocks.push_back(p);return p;}
void* xQueueCreate(unsigned count,size_t n){assert(count==6&&n==sizeof(Command));return (void*)1;}
void micLoop(void*){}void ioLoop(void*){}
int xTaskCreate(void(*fn)(void*),const char* name,unsigned stack,void*,int priority,TaskHandle_t* out){assert(fn==micLoop&&!strcmp(name,"dd-mic")&&stack==3072&&priority==5);++micStarts;*out=(void*)2;return pdPASS;}
int xTaskCreateWithCaps(void(*fn)(void*),const char* name,unsigned stack,void*,int priority,TaskHandle_t* out,int caps){assert(fn==ioLoop&&!strcmp(name,"dd-voice")&&stack==6144&&priority==2&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++ioStarts;if(failWorker)return 0;*out=(void*)3;return pdPASS;}
void reset(){for(auto p:blocks)free(p);blocks.clear();recRing={};playRing={};outBuffer=mp3Pcm=nullptr;mp3Buf=nullptr;ioTask=micTask=commands=nullptr;calls=micStarts=ioStarts=0;failWorker=false;}
''' + driver[start:end] + r'''
int main(){
 for(failAt=1;failAt<=5;++failAt){reset();assert(!ready());unsigned failure=failAt;failAt=0;assert(ready());assert(micStarts==1&&ioStarts==1);auto before=calls;assert(ready()&&calls==before);failAt=failure;}
 reset();failAt=0;failWorker=true;assert(!ready()&&micStarts==1&&!ioTask);failWorker=false;assert(ready()&&micStarts==1&&ioStarts==2);reset();
 puts("PASS production media setup: external buffers/SD-worker stack, internal microphone task, allocation failures, retries and idempotence");
}
'''
(work/'ready.cpp').write_text(source)
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-UNDEBUG',str(work/'ready.cpp'),'-o',str(work/'ready.exe')],check=True)
subprocess.run([str(work/'ready.exe')],check=True)

# Exercise the actual Helix allocation and partial-failure cleanup with the
# same forced include used by its P4 component build.
(work/'esp_heap_caps.h').write_text('''#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void* heap_caps_malloc(size_t,int);
''')
(work/'helix.c').write_text(r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "mp3common.h"
static unsigned calls,failAt;
void* heap_caps_malloc(size_t n,int caps){assert(caps==3);if(++calls==failAt)return NULL;return malloc(n);}
int main(void){
 for(unsigned i=1;i<=8;++i){calls=0;failAt=i;assert(AllocateBuffers()==NULL);}
 calls=failAt=0;MP3DecInfo* state=AllocateBuffers();assert(state&&calls==8);FreeBuffers(state);
 puts("PASS Helix state uses external memory and handles each allocation failure");
}
''')
helix=root/'managed_components/chmorgan__esp-libhelix-mp3/libhelix-mp3'
includes=['-I'+str(work),'-I'+str(helix/'real'),'-I'+str(helix/'pub')]
subprocess.run([sys.executable,'-m','ziglang','cc','-UNDEBUG',*includes,'-include',str(root/'firmware/include/p4_mp3_alloc.h'),'-c',str(helix/'real/buffers.c'),'-o',str(work/'buffers.o')],check=True)
subprocess.run([sys.executable,'-m','ziglang','cc','-UNDEBUG',*includes,str(work/'helix.c'),str(work/'buffers.o'),'-o',str(work/'helix.exe')],check=True)
subprocess.run([str(work/'helix.exe')],check=True)
