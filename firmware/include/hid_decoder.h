#pragma once
// Independent HID 1.11 short-item decoder. GATT Report values exclude Report ID;
// the caller must supply the ID and direction from the 0x2908 descriptor.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
namespace dragon::hid {
struct Field {uint16_t offset,count,usageStart,usageCount;uint8_t id,kind,width,flags;int32_t minimum;uint32_t first,last;};
struct Report {uint8_t id;uint16_t bits[3];};
struct Map {Field fields[128];Report reports[16];uint32_t usages[512];unsigned count=0,reportCount=0,usageCount=0;bool valid=false;const char* error="empty map";};
inline int32_t signedValue(uint32_t v,unsigned bits){if(bits&&bits<32&&(v&(1u<<(bits-1))))v|=(~0u)<<bits;return int32_t(v);}
inline bool parse(Map& m,const uint8_t* p,size_t n){
 m={};auto fail=[&](const char* reason){m.error=reason;return false;};
 if(!p||!n||n>512)return fail("empty or oversized map");
 struct Global {uint32_t page=0,size=0,count=0;int32_t minimum=0;uint8_t id=0;} g,stack[4];
 uint32_t uses[64]{},low=0,high=0;unsigned useCount=0,sp=0,depth=0;bool range=false,rangeEnd=false,numbered=false,unnumbered=false;
 for(size_t at=0;at<n;){uint8_t h=p[at++];if(h==0xfe)return fail("long item unsupported");unsigned z=(h&3)==3?4:h&3;if(z>n-at)return fail("truncated item");uint32_t v=0;for(unsigned j=0;j<z;++j)v|=uint32_t(p[at++])<<(8*j);unsigned type=(h>>2)&3,tag=h>>4;
  if(type==1){switch(tag){case 0:if(v>65535)return fail("usage page overflow");g.page=v;break;case 1:g.minimum=signedValue(v,z*8);break;case 7:g.size=v;break;case 8:if(!v||v>255)return fail("invalid report ID");g.id=v;numbered=true;break;case 9:g.count=v;break;case 10:if(sp==4)return fail("global stack overflow");stack[sp++]=g;break;case 11:if(!sp)return fail("global stack underflow");g=stack[--sp];break;default:break;}}
  else if(type==2){uint32_t u=z==4?v:(g.page<<16)|v;if(tag==0){if(useCount==64)return fail("usage list limit");uses[useCount++]=u;}else if(tag==1){low=u;range=true;}else if(tag==2){high=u;rangeEnd=true;}else return fail("local item unsupported");}
  else if(type==0){
   if(tag==10){if(++depth>16)return fail("collection depth limit");}else if(tag==12){if(!depth)return fail("collection underflow");--depth;}
   else if(tag==8||tag==9||tag==11){
    if(!depth)return fail("report outside collection");if(!g.size||g.size>32||!g.count||g.count>1024||g.count*g.size>8192)return fail("report size limit");
    if(range!=rangeEnd||(range&&(high<low||(high>>16)!=(low>>16)))|| (range&&useCount))return fail("unsupported usage sequence");
    unsigned ri=0;for(;ri<m.reportCount&&m.reports[ri].id!=g.id;++ri){}if(ri==m.reportCount){if(ri==16)return fail("report count limit");m.reports[m.reportCount++].id=g.id;}
    unsigned kind=tag==8?0:tag==9?1:2;auto& bits=m.reports[ri].bits[kind];unsigned total=g.size*g.count;if(total>8192-bits)return fail("report bits limit");
    if(!(v&1)){
     if(v&0x100)return fail("buffered bytes unsupported");if(m.count==128||m.usageCount+useCount>512)return fail("field capacity limit");
     auto& f=m.fields[m.count++];f={bits,uint16_t(g.count),uint16_t(m.usageCount),uint16_t(useCount),g.id,uint8_t(kind),uint8_t(g.size),uint8_t(v),g.minimum,range?low:0,range?high:0};
     for(unsigned j=0;j<useCount;++j)m.usages[m.usageCount++]=uses[j];
    }
    bits+=total;if(!g.id)unnumbered=true;
   }else return fail("main item unsupported");
   useCount=0;range=rangeEnd=false;
  }else return fail("reserved item");
 }
 if(sp||depth)return fail("unclosed stack or collection");if(numbered&&unnumbered)return fail("mixed report IDs");if(!m.reportCount)return fail("no reports");m.valid=true;m.error="ok";return true;
}
inline uint32_t bits(const uint8_t* p,unsigned start,unsigned width){uint32_t v=0;for(unsigned b=0;b<width;++b)v|=uint32_t((p[(start+b)/8]>>((start+b)%8))&1)<<b;return v;}
inline void usageName(uint32_t usage,char* out,size_t cap){unsigned page=usage>>16,u=usage&65535;const char* name=nullptr;
 if(page==7){static const char* special[]={"LeftCtrl","LeftShift","LeftAlt","LeftGUI","RightCtrl","RightShift","RightAlt","RightGUI"};if(u>=4&&u<=29){snprintf(out,cap,"Key%c",'A'+u-4);return;}if(u>=0xe0&&u<=0xe7)name=special[u-0xe0];else if(u==0x28)name="Enter";else if(u==0x29)name="Escape";else if(u==0x2a)name="Backspace";else if(u==0x2b)name="Tab";else if(u==0x2c)name="Space";else if(u>=1&&u<=3)name="KeyboardError";}
 else if(page==1){if(u==0x30)name="X";else if(u==0x31)name="Y";else if(u==0x32)name="Z";else if(u==0x38)name="Wheel";else if(u==0x39)name="Hat";}
 else if(page==0xc){if(u==0xe9)name="VolumeUp";else if(u==0xea)name="VolumeDown";else if(u==0xe2)name="Mute";else if(u==0xcd)name="PlayPause";else if(u==0xb5)name="NextTrack";else if(u==0xb6)name="PreviousTrack";}
 if(name)snprintf(out,cap,"%s",name);else if(page==9)snprintf(out,cap,"Button%u",u);else snprintf(out,cap,"%04x:%04x",page,u);
}
inline bool decode(const Map& m,uint8_t id,unsigned kind,const uint8_t* data,size_t n,char* out,size_t cap){
 if(!out||!cap)return false;out[0]=0;auto fail=[&](const char* s){snprintf(out,cap,"Raw only: %s",s);return false;};if(!m.valid)return fail(m.error);if(!data||kind>2)return fail("invalid report");
 unsigned ri=0;for(;ri<m.reportCount&&m.reports[ri].id!=id;++ri){}if(ri==m.reportCount||!m.reports[ri].bits[kind])return fail("unknown report ID/direction");if(n!=(m.reports[ri].bits[kind]+7u)/8)return fail("report length mismatch");
 size_t used=0;bool any=false;for(unsigned i=0;i<m.count;++i){auto& f=m.fields[i];if(f.id!=id||f.kind!=kind)continue;
  for(unsigned j=0;j<f.count;++j){uint32_t raw=bits(data,f.offset+j*f.width,f.width);int32_t value=f.minimum<0?signedValue(raw,f.width):int32_t(raw);uint32_t usage=0;
   if(f.flags&2){if(f.usageCount)usage=m.usages[f.usageStart+(j<f.usageCount?j:f.usageCount-1)];else if(f.first)usage=f.first+(j<f.last-f.first?j:f.last-f.first);}
   else {int64_t index=int64_t(value)-f.minimum;if(index<0)continue;if(f.usageCount){if(uint64_t(index)>=f.usageCount)continue;usage=m.usages[f.usageStart+unsigned(index)];}else if(f.first&&uint64_t(index)<=f.last-f.first)usage=f.first+uint32_t(index);value=1;}
   if(!usage||!(usage&65535)||value==0)continue;char name[40];usageName(usage,name,sizeof(name));char line[70];snprintf(line,sizeof(line),"%s%s=%ld%s",any?" ":"",name,long(value),(f.flags&4)?"(rel)":"");size_t len=strlen(line);if(len>=cap-used){if(cap-used>=4)memcpy(out+used,"...",4);return true;}memcpy(out+used,line,len+1);used+=len;any=true;
  }
 }if(!any)snprintf(out,cap,"All controls released / zero");return true;
}
}
