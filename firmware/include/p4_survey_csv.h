#pragma once
#include "p4_radar_snapshot.h"
#include <cstdio>
#include <cstring>

namespace dragon::p4 {
constexpr char surveyColumns[]="bssid,channel,rssi_dbm,security,sample_age_ms,ssid_hex\n";
inline bool validSurveyAp(const p4_radar_ap_t& ap) {
  static constexpr uint8_t zero[6]{};
  return !(ap.bssid[0]&1) && std::memcmp(ap.bssid,zero,6) && ap.channel>=1 &&
    ap.channel<=P4_RADAR_CHANNELS && ap.rssi<0 && ap.rssi>=-127 && ap.ssid_length<=sizeof(ap.ssid);
}
inline unsigned surveyRowCount(const p4_radar_wifi_snapshot_t& data){
  unsigned count=0;
  for(unsigned i=0;i<data.count && i<P4_RADAR_MAX_APS;++i)count+=validSurveyAp(data.aps[i]);
  return count;
}
template<class Sink> bool writeSurveyCsv(const p4_radar_wifi_snapshot_t& data,uint32_t now,bool privateMode,Sink write){
  if(!write(surveyColumns,sizeof(surveyColumns)-1))return false;
  static constexpr char digits[]="0123456789abcdef";
  for(unsigned i=0;i<data.count && i<P4_RADAR_MAX_APS;++i){
    const auto& ap=data.aps[i];
    if(!validSurveyAp(ap))continue;
    char address[18]{},hex[65]{},line[168];
    if(!privateMode){
      std::snprintf(address,sizeof(address),"%02X:%02X:%02X:%02X:%02X:%02X",ap.bssid[0],ap.bssid[1],ap.bssid[2],ap.bssid[3],ap.bssid[4],ap.bssid[5]);
      for(unsigned j=0;j<ap.ssid_length;++j){hex[2*j]=digits[ap.ssid[j]>>4];hex[2*j+1]=digits[ap.ssid[j]&15];}
    }
    const int n=std::snprintf(line,sizeof(line),"%s,%u,%d,%s,%lu,%s\n",address,unsigned(ap.channel),int(ap.rssi),
      p4_ap_security_name(ap.security),(unsigned long)(now-data.finished_ms),hex);
    if(n<=0 || size_t(n)>=sizeof(line) || !write(line,size_t(n)))return false;
  }
  return true;
}
}
