#pragma once
#include "contacts.h"

namespace dragon::p4 {

inline const char* signatureFamily(const Contact& c){
  return c.match.confidence!=Confidence::None && c.match.family!=Family::Wifi && c.match.family!=Family::Ble
      ?familyName(c.match.family):nullptr;
}

// Returns the suffix length so a narrow callout can keep unnamed contacts distinct.
inline size_t contactTitle(char (&out)[64],const Contact& c,unsigned slot,bool privacy){
  const char* first=c.name;
  const char* end=first+strlen(first);
  while(first<end && *first==' ')++first;
  while(end>first && end[-1]==' ')--end;
  if(end!=first){
    const int length=int(end-first);
    if(privacy)snprintf(out,sizeof(out),strlen(c.name)>3?"%.3s...":"%s",c.name);
    else snprintf(out,sizeof(out),"%.*s",length,first);
    return 0;
  }
  const char* family=signatureFamily(c);
  if(!family)family=c.ble?"BLE":"Hidden AP";
  char id[12];
  if(privacy)snprintf(id,sizeof(id)," #%u",slot+1);
  else snprintf(id,sizeof(id)," %02X%02X",c.mac[4],c.mac[5]);
  snprintf(out,sizeof(out),"%s%s",family,id);
  return strlen(id);
}

// Received names in Contact have already passed the radio's printable-ASCII filter.
template<class Width> inline void fitTitle(char (&text)[64],size_t suffix,int pixels,Width width){
  const size_t length=strlen(text);
  if(int(width(text,length))<=pixels)return;
  char ending[16]="...";
  if(suffix)strncat(ending,text+length-suffix,sizeof(ending)-strlen(ending)-1);
  const int available=pixels-int(width(ending,strlen(ending)));
  size_t low=0,high=length-suffix;
  while(low<high){
    const size_t mid=(low+high+1)/2;
    if(int(width(text,mid))<=available)low=mid;else high=mid-1;
  }
  snprintf(text+low,sizeof(text)-low,"%s",ending);
}

} // namespace dragon::p4
