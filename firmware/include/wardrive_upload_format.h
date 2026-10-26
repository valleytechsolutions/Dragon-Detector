#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace dragon::upload {
enum class Service:uint8_t {Wigle,WdgWars};
enum class ReceiptState:uint8_t {None,Accepted,Pending,Done,Uncertain,Failed};
struct Config {
  char ssid[33]{},password[64]{},wigle[385]{},wdg[129]{};
  int profile=-1;
};
inline bool plain(const char* s){for(;*s;++s)if(uint8_t(*s)<32||uint8_t(*s)>126)return false;return true;}
template<size_t N> inline bool copy(char (&out)[N],const char* s){size_t n=strlen(s);if(n>=N||!plain(s))return false;memcpy(out,s,n+1);return true;}
inline bool token(const char* s,bool base64){
  size_t n=strlen(s);if(!n)return true;if(n<8)return false;
  for(;*s;++s)if(!((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||
    (base64?(*s=='+'||*s=='/'||*s=='='):(*s=='-'||*s=='_'))))return false;
  return true;
}
// Parse one bounded INI-style line. Unknown Piglet options are ignored; this
// firmware never enables automatic uploads from a reused configuration file.
inline bool configLine(Config& c,char* line){
  size_t n=strlen(line);while(n&&(line[n-1]=='\r'||line[n-1]==' '||line[n-1]=='\t'))line[--n]=0;
  while(*line==' '||*line=='\t')++line;
  if(!*line||*line=='#'||*line==';'||*line=='[')return true;
  char* equal=strchr(line,'=');if(!equal)return false;*equal++=0;
  char* end=line+strlen(line);while(end>line&&(end[-1]==' '||end[-1]=='\t'))*--end=0;
  while(*equal==' '||*equal=='\t')++equal;
  if(!strcmp(line,"homeSsid"))return copy(c.ssid,equal);
  if(!strcmp(line,"homePsk"))return copy(c.password,equal);
  if(!strcmp(line,"wigleBasicToken"))return token(equal,true)&&copy(c.wigle,equal);
  if(!strcmp(line,"wdgwarsApiKey"))return token(equal,false)&&copy(c.wdg,equal);
  if(!strcmp(line,"uploadNetworkProfile")){
    if(!*equal){c.profile=-1;return true;}if(strlen(equal)!=1||*equal<'0'||*equal>'7')return false;c.profile=*equal-'0';return true;
  }
  return true;
}
inline bool networkConfigured(const Config& c){size_t n=strlen(c.password);return c.profile>=0||(*c.ssid&&(n==0||(n>=8&&n<=63)));}
inline bool leaf(const char* name){
  size_t n=strlen(name);if(n<10||n>47||strncmp(name,"wigle-",6)||strcmp(name+n-4,".csv"))return false;
  for(size_t i=0;i<n;++i)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='0'&&name[i]<='9')||name[i]=='-'||name[i]=='.'))return false;
  return strstr(name,"..")==nullptr;
}
struct Receipt {uint32_t bytes=0,crc=0,job=0;ReceiptState state=ReceiptState::None;};
inline bool receipt(const char* text,Receipt& r){
  unsigned state=0,bytes=0,crc=0,job=0;char extra;
  if(sscanf(text,"DDUP1 %u %x %u %u %c",&bytes,&crc,&state,&job,&extra)!=4||state<1||state>unsigned(ReceiptState::Failed))return false;
  if(state==unsigned(ReceiptState::Pending)&&!job)return false;
  r={bytes,crc,job,ReceiptState(state)};return true;
}
inline bool confirmed(const Receipt& r){return r.state==ReceiptState::Accepted||r.state==ReceiptState::Done;}
enum class ReceiptRead:uint8_t {Missing,Valid,Invalid};
enum class UploadAction:uint8_t {Skip,Blocked,Check,Submit};
inline UploadAction uploadAction(ReceiptRead read,const Receipt& r,uint32_t bytes,uint32_t crc,bool checkOnly,bool retry){
  // A damaged receipt may represent a successful upload. Require the same
  // explicit review as a connection lost after sending the CSV.
  if(read==ReceiptRead::Invalid)return retry&&!checkOnly?UploadAction::Submit:UploadAction::Blocked;
  const bool matching=read==ReceiptRead::Valid&&r.bytes==bytes&&r.crc==crc;
  if(matching&&confirmed(r))return UploadAction::Skip;
  if(matching&&r.state==ReceiptState::Uncertain&&!retry)return UploadAction::Blocked;
  if(matching&&r.state==ReceiptState::Pending)return UploadAction::Check;
  return checkOnly?UploadAction::Skip:UploadAction::Submit;
}
inline size_t multipart(char* out,size_t cap,const char* boundary,const char* name){
  if(!leaf(name))return 0;
  int n=snprintf(out,cap,"--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: text/csv\r\n\r\n",boundary,name);
  return n>0&&size_t(n)<cap?size_t(n):0;
}
}
