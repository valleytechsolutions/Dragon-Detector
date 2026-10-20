#pragma once
#include "wearable.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dragon::wardrive {
constexpr uint32_t FixMaxAgeMs = 3000;
constexpr const char* Columns =
  "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,"
  "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n";

struct Fix {
  double latitude=0, longitude=0, altitude=0, hdop=0;
  uint32_t epoch=0, positionAt=0, qualityAt=0;
  unsigned satellites=0, quality=0, rmcSecond=0, ggaSecond=0;
  bool position=false, altitudeValid=false;
  bool usable(uint32_t now) const {
    const unsigned delta=(rmcSecond+86400-ggaSecond)%86400;
    return position && epoch && quality && uint32_t(now-positionAt)<=FixMaxAgeMs &&
      uint32_t(now-qualityAt)<=FixMaxAgeMs && (delta<=1 || delta>=86399);
  }
};
inline int nibble(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='A'&&c<='F')return c-'A'+10;
  if(c>='a'&&c<='f')return c-'a'+10;
  return -1;
}
inline bool number(const char* text,double& value) {
  if(!text||!*text)return false;
  char* end=nullptr;value=std::strtod(text,&end);
  return end!=text && !*end && std::isfinite(value);
}
inline bool integer(const char* text,unsigned& value,unsigned maximum) {
  if(!text||!*text)return false;value=0;
  for(;*text;++text){if(*text<'0'||*text>'9')return false;
    if(value>maximum/10)return false;value=value*10+unsigned(*text-'0');if(value>maximum)return false;}
  return true;
}
inline bool coordinate(const char* text,const char* direction,bool latitude,double& result) {
  double raw;if(!number(text,raw)||raw<0||raw>18000||!direction||!*direction||direction[1])return false;
  const unsigned degrees=unsigned(raw/100);const double minutes=raw-degrees*100;
  if(minutes>=60||degrees>(latitude?90u:180u))return false;
  result=degrees+minutes/60;
  if(result>(latitude?90:180))return false;
  if(*direction==(latitude?'S':'W'))result=-result;
  else if(*direction!=(latitude?'N':'E'))return false;
  return true;
}
inline bool clockFields(const char* text,Calendar& date,unsigned& seconds) {
  const size_t n=std::strlen(text);
  if(n<6)return false;
  for(unsigned i=0;i<6;++i)if(text[i]<'0'||text[i]>'9')return false;
  if(n>6){if(text[6]!='.'||n==7)return false;for(size_t i=7;i<n;++i)if(text[i]<'0'||text[i]>'9')return false;}
  date.hour=(text[0]-'0')*10+text[1]-'0';date.minute=(text[2]-'0')*10+text[3]-'0';
  date.second=(text[4]-'0')*10+text[5]-'0';
  if(date.hour>23||date.minute>59||date.second>59)return false;
  seconds=date.hour*3600+date.minute*60+date.second;return true;
}
class Nmea {
  char line_[128]{};size_t length_=0;bool collecting_=false;
  void sentence(uint32_t now) {
    char* star=std::strchr(line_,'*');
    if(!star||std::strlen(star)!=3)return;
    int high=nibble(star[1]),low=nibble(star[2]);if(high<0||low<0)return;
    uint8_t check=0;for(char* p=line_+1;p<star;++p)check^=uint8_t(*p);
    if(check!=uint8_t(high*16+low)){++badChecksum;return;}
    *star=0;char* fields[22]{};unsigned count=1;fields[0]=line_+1;
    for(char* p=line_+1;*p;++p)if(*p==','){*p=0;if(count==22)return;fields[count++]=p+1;}
    if(std::strlen(fields[0])!=5)return;
    Calendar date;unsigned seconds;
    if(!std::strcmp(fields[0]+2,"RMC")&&count>=10) {
      if(std::strcmp(fields[2],"A")){fix.position=false;return;}
      double latitude,longitude;
      if(!clockFields(fields[1],date,seconds)||
         !coordinate(fields[3],fields[4],true,latitude)||
         !coordinate(fields[5],fields[6],false,longitude)||std::strlen(fields[9])!=6){fix.position=false;return;}
      for(unsigned i=0;i<6;++i)if(fields[9][i]<'0'||fields[9][i]>'9'){fix.position=false;return;}
      const char* d=fields[9];date.day=(d[0]-'0')*10+d[1]-'0';date.month=(d[2]-'0')*10+d[3]-'0';
      date.year=2000+(d[4]-'0')*10+d[5]-'0';
      if(!validDate(date)){fix.position=false;return;}
      const uint32_t epoch=dateEpoch(date);
      // A frozen or replayed NMEA second must not refresh the age indefinitely.
      if(epoch<=fix.epoch)return;
      fix.latitude=latitude;fix.longitude=longitude;fix.epoch=epoch;
      fix.positionAt=now;fix.rmcSecond=seconds;fix.position=true;++positions;
    } else if(!std::strcmp(fields[0]+2,"GGA")&&count>=11) {
      unsigned quality,satellites;double hdop,altitude;
      if(!clockFields(fields[1],date,seconds)||!integer(fields[6],quality,8)||
         !integer(fields[7],satellites,99)||!number(fields[8],hdop)||hdop<0||
         !number(fields[9],altitude)||std::strcmp(fields[10],"M")){fix.quality=0;return;}
      // Exclude invalid, dead-reckoned, manual and simulated fixes.
      fix.quality=(quality==1||quality==2||quality==4||quality==5)?quality:0;
      fix.satellites=satellites;fix.hdop=hdop;fix.altitude=altitude;
      fix.altitudeValid=fix.quality!=0;fix.qualityAt=now;fix.ggaSecond=seconds;
    }
  }
public:
  Fix fix;uint32_t badChecksum=0,overlong=0,positions=0;
  void discardPartial(){length_=0;collecting_=false;}
  void feed(char c,uint32_t now) {
    if(c=='$'){length_=0;collecting_=true;line_[length_++]=c;return;}
    if(!collecting_)return;
    if(c=='\r')return;
    if(c=='\n'){line_[length_]=0;sentence(now);collecting_=false;return;}
    if(length_==sizeof(line_)-1){collecting_=false;++overlong;return;}
    if(uint8_t(c)<32||uint8_t(c)>126){collecting_=false;return;}
    line_[length_++]=c;
  }
};

inline Calendar calendar(uint32_t epoch) {
  Calendar d;uint32_t days=epoch/86400;
  d.hour=epoch%86400/3600;d.minute=epoch%3600/60;d.second=epoch%60;
  d.year=1970;while(days>=unsigned(leap(d.year)?366:365)){days-=leap(d.year)?366:365;++d.year;}
  d.month=1;while(days>=unsigned(monthDays(d.year,d.month))){days-=monthDays(d.year,d.month);++d.month;}
  d.day=days+1;return d;
}
// Interpret advertised AKM suites rather than labeling every RSN AP WPA2.
// Cipher details not decoded here are deliberately omitted.
inline void authentication(char* out,size_t cap,const uint8_t* frame,size_t size,uint8_t security){
  bool psk=false,eap=false,sae=false,owe=false,other=false,found=false;
  if(security&4){
    for(size_t at=36;at+2<=size;){
      unsigned id=frame[at++],length=frame[at++];if(length>size-at)break;
      const uint8_t* d=frame+at;at+=length;if(id!=48||length<10)continue;
      size_t count=d[6]|size_t(d[7])<<8,pos=8;
      if(count>(length-pos)/4)continue;pos+=count*4;
      if(length-pos<2)continue;count=d[pos]|size_t(d[pos+1])<<8;pos+=2;
      if(!count||count>(length-pos)/4)continue;found=true;
      for(size_t i=0;i<count;++i,pos+=4){
        if(d[pos]||d[pos+1]!=0x0f||d[pos+2]!=0xac){other=true;continue;}
        switch(d[pos+3]){
          case 1:case 3:case 5:eap=true;break;
          case 2:case 4:case 6:psk=true;break;
          case 8:case 9:sae=true;break;
          case 18:owe=true;break;
          default:other=true;
        }
      }
    }
  }
  std::snprintf(out,cap,"%s%s%s%s%s%s%s[ESS]",
    security&2?"[WPA]":"",psk?"[WPA2-PSK]":"",eap?"[WPA2-EAP]":"",
    sae?"[WPA3-SAE]":"",owe?"[OWE]":"",
    (security&4)&&(!found||other)?"[RSN]":"",
    (security&1)&&!(security&6)?"[WEP]":"");
}
// Always quote text. Preserve valid UTF-8; replace invalid/control bytes so
// advertisements cannot insert CSV rows or poison the UTF-8 upload.
inline bool quoted(char* out,size_t cap,size_t& used,const uint8_t* data,size_t length) {
  auto put=[&](char c){if(used+1>=cap)return false;out[used++]=c;return true;};
  if(!put('"'))return false;
  for(size_t i=0;i<length;) {
    uint8_t c=data[i];
    if(c<32||c==127){if(!put(' '))return false;++i;continue;}
    if(c<128){if(c=='"'&&!put('"'))return false;if(!put(char(c)))return false;++i;continue;}
    unsigned n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
    bool valid=n&&i+n<=length;
    for(unsigned j=1;valid&&j<n;++j)valid=(data[i+j]&0xc0)==0x80;
    if(valid&&n==3)valid=!(c==0xe0&&data[i+1]<0xa0)&&!(c==0xed&&data[i+1]>=0xa0);
    if(valid&&n==4)valid=!(c==0xf0&&data[i+1]<0x90)&&!(c==0xf4&&data[i+1]>=0x90);
    if(!valid){if(!put('?'))return false;++i;continue;}
    for(unsigned j=0;j<n;++j)if(!put(char(data[i++])))return false;
  }
  if(!put('"'))return false;out[used]=0;return true;
}
inline size_t csv(char* out,size_t cap,const uint8_t mac[6],const uint8_t* name,size_t length,
                  const char* auth,int rssi,unsigned channel,bool ble,const Fix& fix,uint32_t now) {
  if(!out||cap<2||!mac||(!name&&length)||length>64||!fix.usable(now))return 0;
  int count=std::snprintf(out,cap,"%02X:%02X:%02X:%02X:%02X:%02X,",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  if(count<0||size_t(count)>=cap)return 0;size_t used=count;
  if(!quoted(out,cap,used,name,length))return 0;
  const Calendar d=calendar(fix.epoch);
  count=std::snprintf(out+used,cap-used,",%s,%04d-%02d-%02d %02d:%02d:%02d,%u,%u,%d,%.7f,%.7f,%.2f,0,,,%.4s\n",
    ble?"[BLE]":auth,d.year,d.month,d.day,d.hour,d.minute,d.second,
    ble?0:channel,ble?0:channel>=1&&channel<=13?2407+5*channel:channel==14?2484:0,rssi,
    fix.latitude,fix.longitude,fix.altitudeValid?fix.altitude:0,ble?"BLE":"WIFI");
  return count>0&&size_t(count)<cap-used?used+count:0;
}
// A fixed-memory, whole-record queue; a slow card never overwrites unread data.
class Records {
  uint8_t* data_=nullptr;size_t capacity_=0,head_=0,tail_=0,used_=0;
  void put(uint8_t c){data_[head_]=c;head_=(head_+1)%capacity_;++used_;}
public:
  static constexpr size_t MaxRecord=2304; // 1024-byte BLE payload as hex + CSV fields.
  void reset(uint8_t* data,size_t capacity){data_=data;capacity_=capacity;head_=tail_=used_=0;}
  size_t size() const{return used_;}
  bool push(const uint8_t* data,size_t n) {
    if(!data_||!data||!n||n>MaxRecord||n+2>capacity_-used_)return false;
    put(uint8_t(n));put(uint8_t(n>>8));for(size_t i=0;i<n;++i)put(data[i]);return true;
  }
  size_t front(uint8_t* out,size_t cap) const {
    if(used_<2)return 0;size_t n=data_[tail_]|size_t(data_[(tail_+1)%capacity_])<<8;
    if(n>cap||n+2>used_)return 0;
    for(size_t i=0;i<n;++i)out[i]=data_[(tail_+2+i)%capacity_];return n;
  }
  void pop(size_t n){if(n+2>used_)return;tail_=(tail_+n+2)%capacity_;used_-=n+2;}
};
} // namespace dragon::wardrive
