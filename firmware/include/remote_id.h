#pragma once
#include <opendroneid.h>
#include <stddef.h>
#include <math.h>

namespace dragon {
// Only broadcast data is decoded. No connection, pairing, or active query.
struct DroneInfo {
  bool valid=false, hasId=false, hasLocation=false, hasOperator=false;
  char id[41]{};
  double latitude=0,longitude=0,operatorLatitude=0,operatorLongitude=0;
  float altitude=-1000,speed=255;
  uint32_t idAt=0,locationAt=0,operatorAt=0;
};
inline void droneFields(DroneInfo& out,const ODID_UAS_Data& data) {
  out.valid=true;
  for(unsigned i=0;i<ODID_BASIC_ID_MAX_MESSAGES;++i)if(data.BasicIDValid[i]) {
    // Binary IDs are represented in hex; never mistake them for a C string.
    auto& b=data.BasicID[i];out.hasId=b.IDType!=ODID_IDTYPE_NONE;if(!out.hasId)continue;
    if(b.IDType==ODID_IDTYPE_UTM_ASSIGNED_UUID||b.IDType==ODID_IDTYPE_SPECIFIC_SESSION_ID){const char* hex="0123456789ABCDEF";for(unsigned k=0;k<20;++k){uint8_t c=b.UASID[k];out.id[2*k]=hex[c>>4];out.id[2*k+1]=hex[c&15];}out.id[40]=0;}
    else {for(unsigned k=0;k<20;++k){unsigned char c=b.UASID[k];out.id[k]=c==0?0:(c>=32&&c<127?c:'?');}out.id[20]=0;}break;
  }
  if(data.LocationValid) {
    auto& l=data.Location;
    if(isfinite(l.Latitude)&&isfinite(l.Longitude)&&fabs(l.Latitude)<=90&&fabs(l.Longitude)<=180&&(l.Latitude!=0||l.Longitude!=0)) {
      out.hasLocation=true;out.latitude=l.Latitude;out.longitude=l.Longitude;out.altitude=l.AltitudeGeo;out.speed=l.SpeedHorizontal;
    }
  }
  if(data.SystemValid) {
    auto& s=data.System;
    if(isfinite(s.OperatorLatitude)&&isfinite(s.OperatorLongitude)&&fabs(s.OperatorLatitude)<=90&&fabs(s.OperatorLongitude)<=180&&(s.OperatorLatitude!=0||s.OperatorLongitude!=0)) {
      out.hasOperator=true;out.operatorLatitude=s.OperatorLatitude;out.operatorLongitude=s.OperatorLongitude;
    }
  }
}
inline bool decodeDrone(const uint8_t* p,size_t n,DroneInfo& out) {
  if(!p || !n)return false;
  ODID_UAS_Data data;odid_initUasData(&data);
  if((p[0]>>4)==0xF) {
    if(n<3 || p[1]!=25 || p[2]==0 || p[2]>ODID_PACK_MAX_MESSAGES || n!=3+size_t(p[2])*25)return false;
    ODID_MessagePack_encoded pack{};memcpy(&pack,p,n);
    if(decodeMessagePack(&data,&pack)!=ODID_SUCCESS)return false;
  } else {
    if(n!=25 || (p[0]>>4)>5)return false;
    uint8_t message[25];memcpy(message,p,25);
    if(decodeOpenDroneID(&data,message)==ODID_MESSAGETYPE_INVALID)return false;
  }
  bool decoded=data.LocationValid||data.SystemValid||data.SelfIDValid||data.OperatorIDValid;
  for(auto flag:data.BasicIDValid)decoded|=flag;for(auto flag:data.AuthValid)decoded|=flag;
  if(!decoded)return false;droneFields(out,data);return true;
}
// Public NAN service-discovery attribute format used by OpenDroneID. Every
// nested length is checked before the core decoder sees a message pack.
inline bool decodeDroneNan(const uint8_t* p,size_t n,DroneInfo& out) {
  const uint8_t header[]={4,9,0x50,0x6f,0x9a,0x13};
  const uint8_t service[]={0x88,0x69,0x19,0x9d,0x92,9};
  if(n<30 || memcmp(p+24,header,6))return false;
  DroneInfo result;bool found=false;
  for(size_t at=30;at<n;) {
    if(n-at<3)return false;
    uint8_t type=p[at];size_t len=p[at+1]|size_t(p[at+2])<<8;at+=3;
    if(len>n-at)return false;
    const uint8_t* d=p+at;
    if(type==3 && len>=11 && !memcmp(d,service,6) && d[8]==0x10) {
      size_t info=d[9];
      if(info!=len-10 || info<4 || !decodeDrone(d+11,info-1,result))return false;
      found=true;
    }
    at+=len;
  }
  if(found)out=result;return found;
}
inline void mergeDrone(DroneInfo& stored,const DroneInfo& fresh,uint32_t now) {
  if(!fresh.valid)return;stored.valid=true;
  if(fresh.hasId){memcpy(stored.id,fresh.id,sizeof(stored.id));stored.hasId=true;stored.idAt=now;}
  if(fresh.hasLocation){stored.hasLocation=true;stored.latitude=fresh.latitude;stored.longitude=fresh.longitude;stored.altitude=fresh.altitude;stored.speed=fresh.speed;stored.locationAt=now;}
  if(fresh.hasOperator){stored.hasOperator=true;stored.operatorLatitude=fresh.operatorLatitude;stored.operatorLongitude=fresh.operatorLongitude;stored.operatorAt=now;}
}
}
