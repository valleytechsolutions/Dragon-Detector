// Original MIT RTTTL parser and detector melodies. No copyrighted song melodies.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "detection.h"
namespace dragon {
constexpr size_t TONE_TEXT_MAX=512,TONE_NOTES_MAX=96;
struct ToneNote {uint16_t hz=0,ms=0;};
struct Melody {ToneNote notes[TONE_NOTES_MAX];uint16_t count=0;uint32_t duration=0;};
inline bool toneNumber(const char*& p,unsigned& n){n=0;const char* start=p;while(*p>='0'&&*p<='9'){n=n*10+unsigned(*p++-'0');if(n>10000)return false;}return p!=start;}
inline bool toneDuration(unsigned n){return n==1||n==2||n==4||n==8||n==16||n==32;}
inline bool parseTone(const char* text,Melody& out){
  out.count=0;out.duration=0;if(!text)return false;
  size_t len=0;while(len<=TONE_TEXT_MAX&&text[len])++len;if(len>TONE_TEXT_MAX)return false;
  if(!len)return true; // Explicit silence is a valid per-family choice.
  for(size_t i=0;i<len;++i)if(text[i]<32||text[i]>126)return false;
  const char* p=strchr(text,':');if(!p||p==text||p-text>32)return false;++p;
  unsigned d=4,o=6,b=120,seen=0;
  while(*p&&*p!=':'){
    char key=*p++;if(*p++!='=')return false;unsigned v;if(!toneNumber(p,v))return false;
    unsigned bit=key=='d'?1:key=='o'?2:key=='b'?4:0;if(!bit||(seen&bit))return false;seen|=bit;
    if(key=='d'){if(!toneDuration(v))return false;d=v;}else if(key=='o'){if(v<4||v>7)return false;o=v;}else{if(v<25||v>900)return false;b=v;}
    if(*p==','){++p;if(*p==':')return false;}else if(*p!=':')return false;
  }
  if(*p++!=':'||!*p)return false;
  static constexpr uint16_t octave4[]={262,277,294,311,330,349,370,392,415,440,466,494};
  while(*p){
    if(out.count>=TONE_NOTES_MAX)return false;
    unsigned duration=d,octave=o;if(*p>='0'&&*p<='9'){if(!toneNumber(p,duration)||!toneDuration(duration))return false;}
    char note=*p;if(!note)return false;++p;if(note>='A'&&note<='Z')note+=32;
    int semitone=note=='c'?0:note=='d'?2:note=='e'?4:note=='f'?5:note=='g'?7:note=='a'?9:note=='b'?11:note=='p'?-1:-2;
    if(semitone==-2)return false;
    if(*p=='#'){if(semitone<0||semitone==4||semitone==11)return false;++semitone;++p;}
    bool dotted=false;if(*p=='.'){dotted=true;++p;}
    if(*p>='0'&&*p<='9'){octave=unsigned(*p++-'0');if(octave<4||octave>7)return false;}
    if(*p=='.'){if(dotted)return false;dotted=true;++p;}
    unsigned ms=240000u/b/duration;if(dotted)ms+=ms/2;
    if(!ms||out.duration+ms>15000)return false;
    out.notes[out.count++]={uint16_t(semitone<0?0:octave4[semitone]<<(octave-4)),uint16_t(ms)};out.duration+=ms;
    if(*p==','){++p;if(!*p)return false;}else if(*p)return false;
  }
  return out.count>0;
}
// Indices match the stable Family enum. Each class has its own short pattern.
static constexpr const char* defaultTones[]={
 "Wifi:d=16,o=5,b=180:c,e", "BLE:d=16,o=5,b=180:d,f",
 "Flock:d=16,o=6,b=180:c,g,c7,8g", "Axon:d=16,o=5,b=180:c,e,g,c6,g,e,8c6",
 "Glasses:d=16,o=6,b=180:e,8g", "Pineapple:d=16,o=5,b=180:g,g,p,c6",
 "Pwn:d=16,o=5,b=180:d,f,a,f", "Nyan:d=16,o=6,b=180:g,e,c,g",
 "Hale:d=16,o=5,b=180:a,f,d,8a", "FindMy:d=16,o=6,b=180:c,p,e,p,c",
 "Serial:d=16,o=5,b=180:c#,g#,c#", "Flipper:d=16,o=6,b=180:c,f#,c7,f#",
 "Biscuit:d=16,o=5,b=180:e,b,e6,b", "SmartTag:d=16,o=6,b=180:d,p,f#,a",
 "Raven:d=16,o=5,b=180:g,c6,e6,8c6", "Google:d=16,o=6,b=180:e,g,d,g",
 "Tile:d=16,o=6,b=180:f,a,8f", "Ring:d=16,o=6,b=180:g,8c,8e",
 "Camera:d=16,o=5,b=180:f,c6,f,8c6", "ALPR:d=16,o=6,b=180:d,a,d7,8a",
 "Drone:d=16,o=6,b=180:c,p,g,p,c7", "Beacon:d=16,o=5,b=180:b,e6,b",
 "Disconnect:d=16,o=5,b=180:a,a,p,d#,d#", "Conflict:d=16,o=5,b=180:c,f#,c,f#",
 "Burst:d=32,o=6,b=180:c,d,e,f,g", "Fox:d=16,o=6,b=180:g,e,8c"
};
struct ToneQueue {
 uint32_t pending=0,played=0,requested[26]{},last[26]{};
 void clear(){pending=0;}
 void request(unsigned family,uint32_t now){if(family>=26||(played&(1u<<family)&&now-last[family]<4000))return;if(!(pending&(1u<<family)))requested[family]=now;pending|=1u<<family;}
 int take(uint32_t now){int best=-1;for(unsigned i=0;i<26;++i)if(pending&(1u<<i)){
   if(now-requested[i]>10000){pending&=~(1u<<i);continue;}
   if(best<0||familyPriority(Family(i))>familyPriority(Family(best)))best=i;
 }if(best>=0){pending&=~(1u<<best);played|=1u<<best;last[best]=now;}return best;}
};
inline unsigned huntInterval(int rssi){rssi=rssi<-95?-95:rssi>-35?-35:rssi;return 2000-unsigned(rssi+95)*1750/60;}
inline unsigned huntPitch(int rssi){rssi=rssi<-95?-95:rssi>-35?-35:rssi;return 440+unsigned(rssi+95)*1320/60;}
}
