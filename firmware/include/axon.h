#pragma once
#include "detection.h"

namespace dragon {
// Independent interpretation of AXEOFF's public signatures. A vendor identifier
// indicates Axon/TASER equipment, not a camera model or its recording state.
enum AxonSignal : uint8_t { AxonCompany=1, AxonService=2, AxonOui=4, AxonCompatibility=8, AxonName=16, AxonResearch=32 };
struct AxonEvidence {
  uint8_t signals=0;
  bool detected()const{return signals!=0;}
  bool registered()const{return signals&(AxonCompany|AxonService|AxonOui);}
  Match match()const{
    if((signals&(AxonCompany|AxonService))==(AxonCompany|AxonService))
      return {Family::Axon,Confidence::Strong,"Axon/TASER CID 034D + assigned service; model/recording unknown"};
    if(signals&AxonCompany)return {Family::Axon,Confidence::Medium,"Axon/TASER manufacturer CID 034D; model/recording unknown"};
    if(signals&AxonService)return {Family::Axon,Confidence::Medium,"Axon/TASER service FC81/FE6B/FE6C; model/recording unknown"};
    if(signals&AxonOui)return {Family::Axon,Confidence::Medium,"Axon public/source OUI 00:25:DF; model/recording unknown"};
    if(signals&AxonCompatibility)return {Family::Axon,Confidence::Low,"AXEOFF UUID/address match; custom UUID or random address"};
    if(signals&AxonName)return {Family::Axon,Confidence::Low,"Axon / AB2 / AB3 / AB4 advertised name match"};
    if(signals&AxonResearch)return {Family::Axon,Confidence::Low,"SquachWatch Axon address match; shared radio vendor"};
    return {Family::Ble,Confidence::None,"no Axon signature"};
  }
};
inline bool axonServiceId(uint16_t id){return id==0xfc81||id==0xfe6b||id==0xfe6c;}
inline bool axonPrefix(const uint8_t* mac){return mac&&mac[0]==0&&mac[1]==0x25&&mac[2]==0xdf;}
inline bool axonResearchPrefix(const uint8_t* mac){return mac&&((mac[0]==0x28&&mac[1]==0x24&&mac[2]==0xff)||(mac[0]==0xe4&&mac[1]==5&&mac[2]==0x40));}
inline bool axonName(const uint8_t* data,size_t size){char name[64];safeText(name,sizeof(name),data,size);return contains(name,"axon")||namedPrefix(name,"AB2")||namedPrefix(name,"AB3")||namedPrefix(name,"AB4");}
inline AxonEvidence inspectAxonBle(const uint8_t* p,size_t n,size_t advLength,const uint8_t* mac,uint8_t addressType){
  AxonEvidence result;
  // Public identity addresses (type 2) are public too. Random address prefixes
  // and arbitrary UUID substrings are only compatibility clues, never vendors.
  if(axonPrefix(mac))result.signals|=(addressType==0||addressType==2)?AxonOui:AxonCompatibility;
  if((addressType==0||addressType==2)&&axonResearchPrefix(mac))result.signals|=AxonResearch;
  if(!p||advLength>n)return result;
  for(size_t offset=0;offset<n;){
    size_t end=advLength&&offset<advLength?advLength:n;
    uint8_t length=p[offset++];
    if(!length){offset=end;continue;}
    if(length>end-offset){offset=end;continue;}
    uint8_t type=p[offset];const uint8_t* data=p+offset+1;size_t size=length-1;
    if(type==0xff&&size>=2&&data[0]==0x4d&&data[1]==3)result.signals|=AxonCompany;
    if((type==8||type==9)&&axonName(data,size))result.signals|=AxonName;
    size_t width=(type==2||type==3||type==0x16)?2:(type==4||type==5||type==0x20)?4:(type==6||type==7||type==0x21)?16:0;
    bool serviceData=type==0x16||type==0x20||type==0x21;
    if(width&&size>=width&&(serviceData||size%width==0)){
      for(size_t at=0;at+width<=size;at+=width){
        uint16_t id=0;
        if(assignedUuid16(data+at,width,id)&&axonServiceId(id))result.signals|=AxonService;
        else if(width>=4){
          const uint8_t* shortPart=data+at+(width==16?12:0);
          if(axonServiceId(shortPart[0]|uint16_t(shortPart[1])<<8))result.signals|=AxonCompatibility;
        }
        if(serviceData)break;
      }
    }
    offset+=length;
  }
  return result;
}
inline AxonEvidence inspectAxonWifi(const uint8_t* p,size_t n){
  AxonEvidence result;
  // Address evidence is independent of SSID/security IE decoding. The hardware
  // receive callback rejects CRC/RX errors before this header is inspected.
  if(!p||n<24||(p[0]&3))return result;
  const unsigned type=(p[0]>>2)&3;
  if(type!=0&&type!=2)return result;
  if(axonPrefix(p+10))result.signals=AxonOui;
  else if(axonResearchPrefix(p+10))result.signals=AxonResearch;
  // A directed probe's SSID names its destination, not its transmitter.
  if(p[0]==0x80||p[0]==0x50)for(size_t at=36;at+2<=n;){
    uint8_t id=p[at++],length=p[at++];if(length>n-at)break;
    if(id==0&&length<=32&&axonName(p+at,length))result.signals|=AxonName;
    at+=length;
  }
  return result;
}
inline Match withAxon(Match current,AxonEvidence clue){
  auto next=clue.match();
  if(next.confidence==Confidence::None)return current;
  return current.family==Family::Axon&&current.confidence>next.confidence?current:next;
}
// Used by both the receive pipeline and host tests. Complete, bounded Axon
// fields survive a damaged unrelated AD tail; malformed bytes never supply IDs.
inline BleFrame parseBleObservation(const uint8_t* p,size_t n,size_t advLength,const uint8_t* mac,uint8_t addressType,bool& payloadValid){
  auto frame=parseBle(p,n,advLength);payloadValid=frame.valid;
  auto clue=inspectAxonBle(p,n,advLength,mac,addressType);
  if(!frame.valid){frame={};if(clue.signals)frame.valid=true;}
  if(frame.valid)frame.match=addressHint(mac,addressType==0||addressType==2,frame.match);
  frame.match=withAxon(frame.match,clue);
  return frame;
}
inline WifiFrame parseWifiObservation(const uint8_t* p,size_t n,bool truncated,bool& payloadValid){
  auto frame=parseWifi(p,n);payloadValid=frame.valid&&!truncated;
  auto clue=inspectAxonWifi(p,n);
  if(!payloadValid){
    frame={};if(!clue.signals)return frame;
    frame.valid=true;memcpy(frame.source,p+10,6);frame.frameType=p[0];
    // Do not treat a damaged payload as an SSID, security, traffic or RID claim.
  }
  frame.match=withAxon(frame.match,clue);return frame;
}
}
