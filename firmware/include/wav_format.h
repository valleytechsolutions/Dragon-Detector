#pragma once
// Original MIT code: RIFF/WAVE PCM header writer and tolerant reader, the
// Voice Recorder's file names, and the small sample helpers its capture and
// playback use (mono mix, level meter, rate conversion, a one-reader /
// one-writer ring). No Arduino or ESP-IDF types, so the host tests compile it.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <atomic>

namespace dragon {
namespace wav {
constexpr uint32_t HeaderBytes=44;
// FAT32 stops at 4 GiB - 1 and RIFF sizes are 32-bit: a recording ends 1 MiB before.
constexpr uint32_t FileLimit=0xFFF00000u; // 4 GiB - 1 MiB
constexpr uint32_t Unknown=0xFFFFFFFFu; // the "streaming, read to the end" size
inline void put16(uint8_t* p,uint16_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
inline void put32(uint8_t* p,uint32_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);p[2]=uint8_t(v>>16);p[3]=uint8_t(v>>24);}
inline uint16_t get16(const uint8_t* p){return uint16_t(p[0]|p[1]<<8);}
inline uint32_t get32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
// The RIFF size field for a 44-byte header followed by dataBytes of samples.
inline uint32_t riffSize(uint32_t dataBytes){return dataBytes>=Unknown-36?Unknown:dataBytes+36;}
// A canonical 16-bit PCM header. dataBytes Unknown writes the streaming placeholder.
inline void header(uint8_t out[HeaderBytes],uint32_t rate,uint16_t channels,uint32_t dataBytes){
  const uint16_t align=uint16_t(channels*2);
  memcpy(out,"RIFF",4);put32(out+4,riffSize(dataBytes));memcpy(out+8,"WAVEfmt ",8);put32(out+16,16);
  put16(out+20,1);put16(out+22,channels);put32(out+24,rate);put32(out+28,rate*align);put16(out+32,align);put16(out+34,16);
  memcpy(out+36,"data",4);put32(out+40,dataBytes);
}
struct Format {uint16_t format=0,channels=0,bits=0,blockAlign=0;uint32_t rate=0,byteRate=0,dataOffset=0,dataBytes=0;bool sizeGuessed=false;};
enum class Status:uint8_t {Ok,ReadError,NotRiff,NotWave,NoFormat,NoData};
inline const char* statusText(Status s){
  switch(s){case Status::Ok:return "ok";case Status::ReadError:return "Cannot read the file";case Status::NotRiff:return "Not a WAV file (no RIFF header)";
    case Status::NotWave:return "Not a WAV file (RIFF but not WAVE)";case Status::NoFormat:return "WAV file has no format chunk";default:return "WAV file has no audio data";}
}
// read(offset,buffer,length) returns true only when all length bytes were read.
// Chunks other than "fmt " and "data" (LIST, fact, bext, JUNK, id3 ...) are
// skipped with their pad byte. A data size of 0, Unknown, or one that runs
// past the end (a recording cut off before its last header update) becomes
// the bytes the file really holds.
template<class Reader> Status parse(Reader&& read,uint32_t fileSize,Format& out){
  out=Format{};uint8_t b[40];
  if(fileSize<12||!read(0,b,12))return fileSize<12?Status::NotRiff:Status::ReadError;
  if(memcmp(b,"RIFF",4))return Status::NotRiff;
  if(memcmp(b+8,"WAVE",4))return Status::NotWave;
  bool haveFormat=false,haveData=false;uint64_t at=12;
  for(unsigned chunks=0;chunks<256&&at+8<=fileSize;++chunks){
    if(!read(uint32_t(at),b,8))return Status::ReadError;
    const uint32_t size=get32(b+4);const uint64_t body=at+8;
    if(!memcmp(b,"fmt ",4)){
      if(size<16||body+16>fileSize)return Status::NoFormat;
      const uint32_t take=size<40?size:40;if(!read(uint32_t(body),b,take))return Status::ReadError;
      out.format=get16(b);out.channels=get16(b+2);out.rate=get32(b+4);out.byteRate=get32(b+8);out.blockAlign=get16(b+12);out.bits=get16(b+14);
      // WAVE_FORMAT_EXTENSIBLE: the sub-format GUID starts with the real tag.
      if(out.format==0xFFFE&&take>=26)out.format=get16(b+24);
      haveFormat=true;if(haveData)break;
    }else if(!memcmp(b,"data",4)){
      out.dataOffset=uint32_t(body);const uint64_t room=fileSize-body;
      if(size==0||size==Unknown||size>room){out.dataBytes=uint32_t(room);out.sizeGuessed=size!=room;}else out.dataBytes=size;
      haveData=true;if(haveFormat)break;
      if(out.sizeGuessed)break; // no reliable end to look past
    }
    at=body+uint64_t(size)+(size&1);
  }
  if(!haveFormat)return Status::NoFormat;
  if(!haveData)return Status::NoData;
  if(out.blockAlign)out.dataBytes-=out.dataBytes%out.blockAlign;
  return Status::Ok;
}
constexpr uint32_t MinRate=8000,MaxRate=48000;
// Why this player cannot play the format, or nullptr when it can.
inline const char* unsupported(const Format& f){
  if(f.format!=1)return f.format==3?"Float WAV: save as 16-bit PCM":"Compressed WAV: save as 16-bit PCM";
  if(f.bits!=16)return "Only 16-bit PCM WAV plays here";
  if(f.channels<1||f.channels>2)return "Only mono or stereo WAV plays here";
  if(f.rate<MinRate||f.rate>MaxRate)return "Sample rate must be 8000-48000 Hz";
  if(f.blockAlign!=f.channels*2)return "WAV block size does not match its format";
  return nullptr;
}
inline uint32_t durationMs(const Format& f){return f.rate&&f.blockAlign?uint32_t(uint64_t(f.dataBytes/f.blockAlign)*1000/f.rate):0;}

// /recordings/REC_2026-11-20_15-33-07.wav from the local time, or
// /recordings/REC_UPTIME_000754s.wav when the clock is not set. attempt 2, 3 ...
// add _2, _3 ... so an existing file is never replaced. False if it did not fit.
inline bool recordingName(char* out,size_t size,bool clockValid,int year,int month,int day,int hour,int minute,int second,uint32_t uptimeSeconds,unsigned attempt){
  char suffix[12]="";if(attempt>1)snprintf(suffix,sizeof(suffix),"_%u",attempt);
  const int n=clockValid?snprintf(out,size,"/recordings/REC_%04d-%02d-%02d_%02d-%02d-%02d%s.wav",year,month,day,hour,minute,second,suffix):
                         snprintf(out,size,"/recordings/REC_UPTIME_%06lus%s.wav",(unsigned long)uptimeSeconds,suffix);
  return n>0&&size_t(n)<size;
}
inline bool isWavName(const char* name){size_t n=strlen(name);return n>4&&name[n-4]=='.'&&(name[n-3]|32)=='w'&&(name[n-2]|32)=='a'&&(name[n-1]|32)=='v';}

// Peak sample to a 0-100 meter on a -60..0 dBFS scale.
inline uint8_t meterPercent(int peak){
  if(peak<0)peak=-peak;if(peak<=32)return 0;if(peak>=32767)return 100;
  const float db=20.f*log10f(float(peak)/32767.f);return uint8_t(db<=-60.f?0:lroundf((db+60.f)*100.f/60.f));
}
inline int16_t mix(int16_t left,int16_t right){return int16_t((int32_t(left)+int32_t(right))/2);}
inline int16_t scale(int16_t sample,unsigned volume){return volume>=100?sample:int16_t(int32_t(sample)*int32_t(volume)/100);}

// Streaming rate converter for one channel. Down: each output is the average
// of the input it covers (a box filter, which keeps most aliasing out). Up:
// linear interpolation. Exact integer time, so long files do not drift.
struct Resampler {
  uint32_t from=16000,to=16000;uint32_t phase=0;int32_t acc=0,prev=0;bool primed=false;
  static uint32_t gcd(uint32_t a,uint32_t b){while(b){uint32_t t=a%b;a=b;b=t;}return a;}
  void begin(uint32_t inputRate,uint32_t outputRate){uint32_t g=gcd(inputRate,outputRate);if(!g)g=1;from=inputRate/g;to=outputRate/g;phase=0;acc=0;prev=0;primed=false;}
  // Outputs one input sample makes: at most to/from+1 (2 at 8 kHz -> 16 kHz).
  template<class Emit> void push(int16_t x,Emit&& emit){
    if(from==to){emit(x);return;}
    if(from>to){ // phase: input units already in this output (0..from)
      uint32_t left=to;
      while(left){uint32_t take=from-phase<left?from-phase:left;acc+=int32_t(x)*int32_t(take);phase+=take;left-=take;
        if(phase==from){emit(int16_t(acc/int32_t(from)));acc=0;phase=0;}}
      return;
    }
    if(!primed){prev=x;primed=true;return;}
    // phase: where the next output falls after prev, in 1/to of an input step
    while(phase<to){emit(int16_t(prev+(int32_t(x)-prev)*int32_t(phase)/int32_t(to)));phase+=from;}
    phase-=to;prev=x;
  }
};

// One producer task, one consumer task. Capacity is a power of two.
template<class T> struct Ring {
  T* data=nullptr;uint32_t mask=0;std::atomic<uint32_t> head{0},tail{0};
  void attach(T* storage,uint32_t capacity){data=storage;mask=capacity-1;head=0;tail=0;}
  uint32_t used()const{return head.load(std::memory_order_acquire)-tail.load(std::memory_order_acquire);}
  uint32_t space()const{return data?mask+1-used():0;}
  void reset(){tail.store(head.load());}
  uint32_t write(const T* in,uint32_t n){
    uint32_t h=head.load(std::memory_order_relaxed),room=mask+1-(h-tail.load(std::memory_order_acquire));if(n>room)n=room;
    for(uint32_t i=0;i<n;++i)data[(h+i)&mask]=in[i];head.store(h+n,std::memory_order_release);return n;
  }
  bool put(T value){return write(&value,1)==1;}
  uint32_t read(T* out,uint32_t n){
    uint32_t t=tail.load(std::memory_order_relaxed),have=head.load(std::memory_order_acquire)-t;if(n>have)n=have;
    for(uint32_t i=0;i<n;++i)out[i]=data[(t+i)&mask];tail.store(t+n,std::memory_order_release);return n;
  }
};
}
}
