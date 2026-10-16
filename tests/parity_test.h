#pragma once
#include "monitor.h"
#include <vector>
#include <cassert>
using namespace dragon;
static std::vector<uint8_t> ad(uint8_t type,const std::vector<uint8_t>& bytes){std::vector<uint8_t> p{uint8_t(bytes.size()+1),type};p.insert(p.end(),bytes.begin(),bytes.end());return p;}
static std::vector<uint8_t> beacon(uint8_t identity,bool secure){
  std::vector<uint8_t> p(36);p[0]=0x80;p[10]=p[16]=0x10;p[15]=p[21]=identity;
  p[34]=secure?0x10:0;append(p,{0,4,'t','e','s','t'});
  if(secure)append(p,{48,18,1,0,0,0x0f,0xac,4,1,0,0,0x0f,0xac,4,1,0,0,0x0f,0xac,2});
  return p;
}
static void parityTests(){
  for(uint16_t uuid:{0x3100,0x3200,0x3300,0x3400,0x3500})assert(ble(ad(3,{uint8_t(uuid),uint8_t(uuid>>8)})).family==Family::Raven);
  for(uint16_t uuid:{0x3101,0x3102,0x3180,0x3301,0x34ff})assert(ble(ad(3,{uint8_t(uuid),uint8_t(uuid>>8)})).family==Family::Ble);
  assert(ble(ad(3,{0,0x31,0,0x32,0,0x33,0,0x34,0,0x35})).confidence==Confidence::Strong);
  assert(ble(ad(0x16,{0x12,0x18,0,0x31})).family==Family::Ble); // payload is not a service list
  for(size_t n:{23,24,35,36}){std::vector<uint8_t> d(n);d[0]=0xaa;d[1]=0xfe;d[2]=0x40;assert(ble(ad(0x16,d)).family==Family::GoogleTag);d[2]=0x41;assert(ble(ad(0x16,d)).family==Family::GoogleTag);for(uint8_t type:{0,0x10,0x20,0x30}){d[2]=type;assert(ble(ad(0x16,d)).confidence==Confidence::Low);}}
  assert(ble({3,3,0xaa,0xfe}).confidence==Confidence::Low);
  assert(ble({4,0x16,0xaa,0xfe,0x40}).confidence==Confidence::Low);
  assert(ble({3,3,0xed,0xfe}).family==Family::Tile);assert(ble({3,3,0xec,0xfe}).family==Family::Tile);
  assert(ble({3,0xff,0xc2,3}).confidence==Confidence::Low);
  assert(ble({3,0xff,0xc2,3,3,3,0x45,0xfe}).confidence==Confidence::Medium);
  assert(ble({3,0xff,0x29,0x0e}).confidence==Confidence::None);
  std::vector<uint8_t> ib(25);ib[0]=0x4c;ib[2]=2;ib[3]=0x15;
  assert(ble(ad(0xff,ib)).family==Family::IBeacon);ib.pop_back();assert(ble(ad(0xff,ib)).confidence==Confidence::None);
  assert(DetectorSettings{}.enabled(Family::IBeacon));
  assert(DetectorSettings{}.dimSeconds==0 && DetectorSettings{}.offSeconds==0);
  assert(named("Flockhart Home",Family::Wifi).confidence==Confidence::None);
  for(const char* s:{"HC-03","HC-05","HC-06","RN42","BT04-A"})assert(named(s,Family::Ble).family==Family::Skimmer);
  const uint8_t flockMac[]={0xb4,0x1e,0x52,1,2,3},ringMac[]={0xac,0x9f,0xc3,1,2,3},flipperMac[]={0x0c,0xfa,0x22,1,2,3};
  Match none{Family::Ble,Confidence::None,"none"};
  assert(addressHint(flockMac,true,none).confidence==Confidence::Medium);
  assert(addressHint(ringMac,true,none).family==Family::Ring);assert(addressHint(flipperMac,true,none).confidence==Confidence::None);
  assert(addressHint(ringMac,false,none).confidence==Confidence::None);
  const uint8_t samsung[]={0x48,0x27,0xea,1,2,3},sonos[]={0,0x0e,0x58,1,2,3},local[]={0x82,0x6b,0xf2,1,2,3};
  assert(addressHint(samsung,true,none).confidence==Confidence::None);assert(addressHint(sonos,true,none).confidence==Confidence::None);assert(addressHint(local,true,none).confidence==Confidence::Low);
  auto a=beacon(1,true),b=beacon(2,true),open=beacon(3,false);
  auto fa=parseWifi(a.data(),a.size()),fb=parseWifi(b.data(),b.size()),fo=parseWifi(open.data(),open.size());
  assert(fa.valid&&fa.security==5&&fo.valid&&fo.security==0);
  SsidMonitor mesh;uint8_t peer[6];
  for(unsigned t=0;t<3000;t+=250){assert(!mesh.observe(fa,t,peer));assert(!mesh.observe(fb,t,peer));}
  assert(!mesh.observe(fo,3100,peer));assert(!mesh.observe(fo,3300,peer));assert(mesh.observe(fo,3600,peer));assert(!mesh.observe(fo,3900,peer));
  SsidMonitor stale;for(unsigned t=0;t<1000;t+=250)stale.observe(fa,t,peer);for(unsigned t=31000;t<33000;t+=250)assert(!stale.observe(fo,t,peer));
  SsidMonitor raw;auto similar=fo;similar.ssid[0]='?';similar.ssid[1]=0xff;auto different=fa;different.ssid[0]='?';different.ssid[1]=0xfe;
  for(unsigned t=0;t<1500;t+=250){assert(!raw.observe(similar,t,peer));assert(!raw.observe(different,t,peer));}
  a[45]=255;assert(!parseWifi(a.data(),a.size()).valid); // impossible pairwise count
  AlertGate gate;assert(gate.allow(0,-80,5));assert(!gate.allow(100,-70,5));for(int i=1;i<5;++i)assert(gate.allow(i*30000,-80,5));assert(!gate.allow(150000,-80,5));assert(gate.allow(150001,-73,5));assert(!gate.allow(180001,-73,5));assert(gate.allow(180002,-80,5,true));assert(gate.allow(1800000,-90,5));
  SignalHistory history;for(int i=0;i<65;++i)history.add(-90+i,i*500);assert(history.count==48&&history.at(0)==-73&&history.at(47)==-26&&history.peak==-26);
  history.add(0,40000);assert(history.peak==-26);
  DeviceKey key1,key2;key1.radio=1;assert(!(key1==key2));key2.radio=1;key2.addressType=1;assert(!(key1==key2));
  // Encode with upstream OpenDroneID, then exercise our bounded transport framing.
  ODID_BasicID_data id;odid_initBasicIDData(&id);id.IDType=ODID_IDTYPE_SERIAL_NUMBER;id.UAType=ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;strcpy(id.UASID,"TEST-DRONE-123");
  ODID_BasicID_encoded encoded;assert(encodeBasicIDMessage(&encoded,&id)==ODID_SUCCESS);
  std::vector<uint8_t> message((uint8_t*)&encoded,(uint8_t*)&encoded+25),service{0xfa,0xff,0x0d,7};append(service,message);
  auto drone=parseBle(ad(0x16,service).data(),ad(0x16,service).size());assert(drone.valid&&drone.match.family==Family::Drone&&drone.drone.hasId&&!strcmp(drone.drone.id,"TEST-DRONE-123"));
  service[2]=0;assert(ble(ad(0x16,service)).confidence==Confidence::Low);service[2]=0x0d;service.pop_back();assert(ble(ad(0x16,service)).confidence==Confidence::Low);
  std::vector<uint8_t> pack{0xf2,25,1};append(pack,message);DroneInfo decoded;assert(decodeDrone(pack.data(),pack.size(),decoded));
  auto bad=pack;bad[2]=9;assert(!decodeDrone(bad.data(),bad.size(),decoded));bad=pack;bad[1]=24;assert(!decodeDrone(bad.data(),bad.size(),decoded));
  for(size_t n=0;n<pack.size();++n)assert(!decodeDrone(pack.data(),n,decoded));
  auto rid=beacon(8,false);std::vector<uint8_t> vendor{0xfa,0x0b,0xbc,0x0d,0};append(vendor,pack);append(rid,{221,uint8_t(vendor.size())});append(rid,vendor);assert(parseWifi(rid.data(),rid.size()).match.family==Family::Drone);auto malformedRid=rid;++malformedRid[43];assert(!parseWifi(malformedRid.data(),malformedRid.size()).valid);
  auto nan=std::vector<uint8_t>(24);nan[0]=0xd0;append(nan,{4,9,0x50,0x6f,0x9a,0x13,3,uint8_t(11+pack.size()),0,0x88,0x69,0x19,0x9d,0x92,9,1,0,0x10,uint8_t(1+pack.size()),7});append(nan,pack);
  assert(parseWifi(nan.data(),nan.size()).match.family==Family::Drone);nan[31]=255;assert(parseWifi(nan.data(),nan.size()).match.family!=Family::Drone);
  ODID_Location_data location;odid_initLocationData(&location);location.Latitude=40.1234567;location.Longitude=-73.7654321;location.AltitudeGeo=42;location.SpeedHorizontal=6.5;
  ODID_Location_encoded loc;assert(encodeLocationMessage(&loc,&location)==ODID_SUCCESS);DroneInfo pos;assert(decodeDrone((uint8_t*)&loc,25,pos)&&pos.hasLocation);assert(fabs(pos.latitude-40.1234567)<0.0000002);
  mergeDrone(decoded,pos,4000);assert(decoded.hasId&&decoded.hasLocation&&decoded.locationAt==4000);
  const std::vector<uint8_t> fox={0x90,0x4b,0x5a,0xc6,0x44,0x5f,0x26,0x8e,0x2b,0x45,0x14,0x71,1,0,4,0xdd,1,4,3,2,1};
  assert(ble(ad(0x21,fox)).family==Family::Fox);assert(ble(ad(7,std::vector<uint8_t>(fox.begin(),fox.begin()+16))).confidence==Confidence::None);
  puts("PASS: parity families, vendor false positives, Remote ID BLE/beacon/NAN, SSID conflicts, alert gating, history and display defaults");
}
