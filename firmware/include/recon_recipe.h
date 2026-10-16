#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
namespace dragon::recipe {
enum Kind:uint8_t{Wait,Mode,CaptureWifi,CaptureBle,CaptureStop,CaptureSave,MissionWifi,MissionBle,MissionStop,Tool,Stop};
struct Step {Kind kind;unsigned a=0,b=0,c=0;};struct Plan {Step steps[24]{};unsigned count=0;uint32_t duration=0;const char* error="empty recipe";};
inline bool parse(const char* text,size_t size,Plan& plan){plan={};if(!text||!size||size>512||memchr(text,0,size))return false;size_t pos=0;
 while(pos<size){char line[64]{};size_t n=0;while(pos<size&&text[pos]!='\n'){char c=text[pos++];if(c=='\r')continue;if(n==sizeof(line)-1){plan.error="line too long";return false;}line[n++]=c;}if(pos<size)++pos;while(n&&(line[n-1]==' '||line[n-1]=='\t'))line[--n]=0;char* p=line;while(*p==' '||*p=='\t')++p;if(!*p||*p=='#')continue;if(plan.count==24){plan.error="24 step limit";return false;}Step s{};char extra;unsigned a,b,c;
 if(sscanf(p,"WAIT %u %c",&a,&extra)==1&&a>=1&&a<=600){s={Wait,a};plan.duration+=a;}
 else if(sscanf(p,"MODE %u %c",&a,&extra)==1&&a<8)s={Mode,a};
 else if(!strcmp(p,"CAPTURE WIFI"))s={CaptureWifi};else if(!strcmp(p,"CAPTURE BLE"))s={CaptureBle};else if(!strcmp(p,"CAPTURE STOP"))s={CaptureStop};else if(!strcmp(p,"CAPTURE SAVE"))s={CaptureSave};
 else if(!strcmp(p,"MISSION WIFI"))s={MissionWifi};else if(!strcmp(p,"MISSION BLE"))s={MissionBle};else if(!strcmp(p,"MISSION STOP"))s={MissionStop};
 else if(sscanf(p,"TOOL %u %u %u %c",&a,&b,&c,&extra)==3&&a<29&&b>=1&&b<=60&&c>=1&&c<=100){s={Tool,a,b,c};plan.duration+=b;}
 else if(!strcmp(p,"STOP"))s={Stop};else{plan.error="unknown command or range";return false;}
 if(plan.duration>3600){plan.error="one hour limit";return false;}plan.steps[plan.count++]=s;
 }plan.error=plan.count?"ok":"empty recipe";return plan.count>0;
}
}
