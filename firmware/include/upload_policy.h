#pragma once
// Design prototype only: deliberately not connected to SD, radio or transport.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace dragon { namespace upload {
struct Config {
 bool enabled=false,terms=false,wigle=false,wdgwars=false;
 char apiName[65]{},apiToken[129]{},wdgKey[65]{};
};
inline bool parse(const char* input,size_t size,bool sd,bool gps,Config& output){
 output=Config{};Config next;
 if(!sd||!gps||!input||size>2048)return false;
 const char* keys[]={"config_version","uploads_enabled","terms_reviewed","wigle_enabled","wdgwars_enabled","wigle_api_name","wigle_api_token","wdgwars_api_key"};
 unsigned seen=0;
 for(size_t at=0;at<size;){
  char line[192];size_t n=0;
  while(at<size&&input[at]!='\n'){char c=input[at++];if(c=='\r'){if(at<size&&input[at]!='\n')return false;break;}if(c<32||c>126||n>=sizeof(line)-1)return false;line[n++]=c;}
  if(at<size&&input[at]=='\n')++at;line[n]=0;
  if(!n||line[0]=='#')continue;
  char* equal=strchr(line,'=');if(!equal)return false;*equal++=0;
  unsigned k=0;while(k<8&&strcmp(line,keys[k]))++k;
  if(k==8||(seen&(1u<<k)))return false;seen|=1u<<k;
  if(k==0){if(strcmp(equal,"1"))return false;}
  else if(k<5){if(strcmp(equal,"true")&&strcmp(equal,"false"))return false;bool value=!strcmp(equal,"true");if(k==1)next.enabled=value;else if(k==2)next.terms=value;else if(k==3)next.wigle=value;else next.wdgwars=value;}
  else{
   size_t length=strlen(equal),capacity=k==6?sizeof(next.apiToken):sizeof(next.apiName);if(length>=capacity)return false;
   for(size_t i=0;i<length;++i){char c=equal[i];bool alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');if(!alnum&&c!='-'&&c!='_')return false;}
   memcpy(k==5?next.apiName:k==6?next.apiToken:next.wdgKey,equal,length+1);
  }
 }
 if(!(seen&1))return false;
 if(next.enabled){
  if(!next.terms||(!next.wigle&&!next.wdgwars))return false;
  if(next.wigle&&(!next.apiName[0]||!next.apiToken[0]))return false;
  if(next.wdgwars){if(strlen(next.wdgKey)!=64)return false;for(char c:next.wdgKey){if(!c)break;if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}}
 }else{next.wigle=next.wdgwars=false;}
 output=next;return true;
}
enum class State:uint8_t {Accepted,Retry,Blocked,Rejected,Uncertain};
struct Decision {State state;uint32_t delaySeconds;};
inline Decision result(unsigned status,bool serviceAccepted,bool transmitted,unsigned attempts,uint32_t retryAfter=0){
 if(status>=200&&status<300)return {serviceAccepted?State::Accepted:State::Uncertain,0};
 if(!status&&transmitted)return {State::Uncertain,0};
 if(status==401||status==403)return {State::Blocked,0};
 if(status>=400&&status<500&&status!=429)return {State::Rejected,0};
 if(status>=300&&status<400)return {State::Blocked,0}; // Never redirect credentials.
 if(attempts>=5)return {State::Blocked,0};
 uint32_t backoff=60u<<(attempts>4?4:attempts);
 if(retryAfter>86400)return {State::Blocked,0}; // Never retry earlier than a long server limit.
 return {State::Retry,retryAfter>backoff?retryAfter:backoff};
}
}}
