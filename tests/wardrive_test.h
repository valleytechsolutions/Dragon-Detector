#pragma once
#include "wardrive_format.h"
#include "radar_interaction.h"
#include "storage_replace_test.h"
#include <deque>
static void wardriveTests(){
  storageReplaceTests();
  using namespace dragon::wardrive;
  dragon::RadarTapGesture gesture;dragon::DeviceKey first{},second{};first.mac[0]=1;second.mac[0]=2;
  assert(!gesture.tap(first,100)&&gesture.tap(first,400));
  assert(!gesture.tap(first,500));assert(!gesture.tap(first,1200)); // slow repeat remains a summary
  assert(!gesture.tap(second,1300));assert(gesture.tap(second,1450)); // same cache index cannot substitute a different identity
  gesture.clear();assert(!gesture.tap(first,2000));gesture.clear();assert(!gesture.tap(first,2100));
  gesture.clear();assert(!gesture.tap(first,0xfffffff0));assert(gesture.tap(first,100));
  gesture.clear();assert(!gesture.tap(first,0));assert(gesture.tap(first,0)); // queued clicks in the same LVGL frame

  auto send=[](Nmea& parser,const std::string& body,uint32_t now){
    uint8_t sum=0;for(char c:body)sum^=uint8_t(c);
    char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",sum);
    for(char c:std::string("$")+body+tail)parser.feed(c,now);
  };
  const std::string rmc="GNRMC,123519.00,A,4807.038,N,01131.000,E,0,0,191026,,,A";
  const std::string gga="GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";
  Nmea parser;send(parser,rmc,100);assert(!parser.fix.usable(100));
  send(parser,gga,200);assert(parser.fix.usable(200));
  assert(parser.rmcSentences==1&&parser.ggaSentences==1&&parser.rmcStatus=='A');
  send(parser,"GPGSV,1,1,02,01,45,120,35,02,60,220,41",220);
  assert(parser.gsvSentences==1&&parser.inView==2&&parser.bestSnr==41&&parser.fix.usable(220));
  // Reception telemetry cannot create a position or refresh an expired fix.
  send(parser,"GPGSV,1,1,01,03,45,120,20",6000);
  assert(parser.gsvSentences==2&&parser.inView==1&&parser.bestSnr==20&&!parser.fix.usable(6000));
  assert(std::abs(parser.fix.latitude-48.1173)<0.0000001);
  assert(std::abs(parser.fix.longitude-11.5166666667)<0.0000001);
  assert(parser.fix.epoch==dragon::dateEpoch({2026,10,19,12,35,19}));
  assert(!parser.fix.usable(3101)); // RMC age, not just GGA age.
  send(parser,rmc,4000);send(parser,gga,4000);
  assert(!parser.fix.usable(4000)); // Frozen data cannot renew a fix.
  Nmea invalid;send(invalid,rmc,100);send(invalid,gga,100);
  send(invalid,"GNRMC,123520,V,,,,,,,191026,,,N",101);
  assert(!invalid.fix.usable(101));
  Nmea quality;send(quality,rmc,10);
  send(quality,"GNGGA,123519,4807.038,N,01131.000,E,6,08,0.9,1,M,0,M,,",10);
  assert(!quality.fix.usable(10)); // Dead reckoning is not a satellite fix.
  send(quality,"GNGGA,123522,4807.038,N,01131.000,E,1,08,0.9,1,M,0,M,,",10);
  assert(!quality.fix.usable(10)); // RMC/GGA epochs must agree.
  Nmea wrap;send(wrap,rmc,0xfffffff0);send(wrap,gga,0xfffffff0);
  assert(wrap.fix.usable(20));assert(!wrap.fix.usable(4000));
  Nmea midnight;
  send(midnight,"GNRMC,235959,A,0000.000,S,00000.000,W,0,0,311226,,,A",10);
  send(midnight,"GNGGA,000000,0000.000,S,00000.000,W,1,09,1.2,-3,M,0,M,,",20);
  assert(midnight.fix.usable(20)); // Zero coordinates are valid if actually fixed.
  send(midnight,"GNRMC,000000,A,0000.000,S,00000.000,W,0,0,010127,,,A",30);
  assert(midnight.fix.usable(30));
  auto date=calendar(midnight.fix.epoch);
  assert(date.year==2027&&date.month==1&&date.day==1&&date.hour==0);
  Nmea corrupt;for(char c:std::string("$GNRMC,123519,A*00\n"))corrupt.feed(c,0);
  assert(!corrupt.fix.usable(0)&&corrupt.badChecksum==1);
  for(char c:std::string("$")+std::string(200,'x')+"\n")corrupt.feed(c,0);
  assert(corrupt.overlong==1);
  send(corrupt,rmc,100);send(corrupt,gga,100);assert(corrupt.fix.usable(100));
  double coordinateResult;
  assert(!coordinate("1e300","N",true,coordinateResult));
  assert(!coordinate("4860.000","N",true,coordinateResult));
  assert(!coordinate("9000.001","N",true,coordinateResult));
  assert(!coordinate("1200","",true,coordinateResult));
  assert(coordinate("18000","W",false,coordinateResult)&&coordinateResult==-180);
  char auth[96];std::vector<uint8_t> beacon(36);
  beacon.insert(beacon.end(),{48,22,1,0,0,0x0f,0xac,4,1,0,0,0x0f,0xac,4,2,0,0,0x0f,0xac,2,0,0x0f,0xac,8});
  authentication(auth,sizeof(auth),beacon.data(),beacon.size(),5);
  assert(std::string(auth)=="[WPA2-PSK][WPA3-SAE][ESS]");
  beacon.back()=18;authentication(auth,sizeof(auth),beacon.data(),beacon.size(),5);assert(std::string(auth)=="[WPA2-PSK][OWE][ESS]");
  for(size_t n=0;n<beacon.size();++n)authentication(auth,sizeof(auth),beacon.data(),n,5);
  authentication(auth,sizeof(auth),beacon.data(),0,1);assert(std::string(auth)=="[WEP][ESS]");
  authentication(auth,sizeof(auth),beacon.data(),0,0);assert(std::string(auth)=="[ESS]");
  const uint8_t mac[]={1,2,3,4,5,6};
  char row[384];const uint8_t ssid[]={'a',',','"','b','\n',0xc3,0xa9,0xff};
  size_t n=csv(row,sizeof(row),mac,ssid,sizeof(ssid),"[ESS]",-73,6,false,corrupt.fix,100);
  assert(n&&std::string(row).find("\"a,\"\"b é?\"")!=std::string::npos);
  assert(std::string(row).find(",2026-10-19 12:35:19,6,2437,-73,48.1173000,11.5166667,545.40,0,,,WIFI\n")!=std::string::npos);
  assert(csv(row,sizeof(row),mac,nullptr,0,"",-41,0,true,corrupt.fix,100));
  assert(std::string(row).find(",\"\",[BLE],")!=std::string::npos);
  assert(std::string(row).find(",0,0,-41,")!=std::string::npos);
  assert(std::string(row).find(",,,BLE\n")!=std::string::npos);
  assert(!csv(row,10,mac,nullptr,0,"[ESS]",-40,1,false,corrupt.fix,100));
  assert(!csv(row,sizeof(row),mac,nullptr,0,"[ESS]",-40,1,false,corrupt.fix,5000));
  // Model-based queue test covers capacity, wraparound and preserving order
  // when a card cannot consume incoming records quickly enough.
  uint8_t memory[127],output[100];Records records;records.reset(memory,sizeof(memory));
  std::deque<std::vector<uint8_t>> model;uint32_t rng=1;
  for(unsigned i=0;i<10000;++i){
    rng=rng*1664525+1013904223;
    if(rng&1){
      std::vector<uint8_t> value(1+(rng>>8)%100,uint8_t(i));size_t occupied=0;
      for(const auto& v:model)occupied+=v.size()+2;
      bool accepted=records.push(value.data(),value.size());
      assert(accepted==(occupied+value.size()+2<=sizeof(memory)));
      if(accepted)model.push_back(value);
    }else if(!model.empty()){
      size_t count=records.front(output,sizeof(output));assert(count==model.front().size());
      assert(!memcmp(output,model.front().data(),count));records.pop(count);model.pop_front();
    }else assert(records.front(output,sizeof(output))==0);
  }
  puts("PASS: GNSS checksum/fix freshness/time rollover, WiGLE UTF-8 CSV, bounded whole-record queue");
}
