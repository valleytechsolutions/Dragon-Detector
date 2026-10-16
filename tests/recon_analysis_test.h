#pragma once
#include "recon_analysis.h"
#include "ble_insights.h"
#include "dns_reply.h"
#include "csi_analysis.h"
static void reconAnalysisTests(){using namespace dragon::analysis;using namespace dragon::lab;
 dragon::csi::Shape shape;int8_t iq[128]{};for(unsigned i=4;i<128;++i)iq[i]=10;assert(shape.observe(iq,127)<0);assert(shape.observe(iq,128)<0);assert(shape.observe(iq,128)<.00001f);for(unsigned i=4;i<128;++i)iq[i]=20;assert(shape.observe(iq,128)<.00001f);for(unsigned i=4;i<64;++i)iq[i]=0;assert(shape.observe(iq,128)>.1f);dragon::csi::Baseline base;for(unsigned i=0;i<30;++i)base.add(.001f);assert(base.score(.001f)==0&&base.score(.5f)==100&&base.threshold()>=.005f);base.add(NAN);assert(base.count==30);int8_t zeros[128]{};assert(shape.observe(zeros,128)<0);
 const uint8_t camera[6]={0,0x1d,0x96,1,2,3};assert(inspectAxonBle(nullptr,0,0,camera,0).signals&BodyCameraVendor);assert(!inspectAxonBle(nullptr,0,0,camera,1).detected());
 uint8_t dns[512]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'l','a','b',0,0,1,0,1};assert(captiveDns(dns,21,sizeof(dns))==37&&dns[7]==1&&dns[33]==192);dns[2]=1;dns[7]=0;dns[12]=0xc0;assert(!captiveDns(dns,21,sizeof(dns)));
 auto b=beacon(1,false);auto f=parseWifi(b.data(),b.size());assert(accepts(Filter::All,f)&&accepts(Filter::Beacons,f)&&!accepts(Filter::Probes,f));
 std::vector<uint8_t> p(24);p[0]=0x40;p[10]=0x10;p.insert(p.end(),{0,4,'a',0,'b',0xff});f=parseWifi(p.data(),p.size());assert(f.valid&&f.ssidLength==4);Probe rows[2]{};observeProbe(rows,2,f,-60,6,100);observeProbe(rows,2,f,-40,6,110);assert(rows[0].count==2&&rows[0].length==4&&rows[0].ssid[3]==255);p[15]=2;f=parseWifi(p.data(),p.size());observeProbe(rows,2,f,-50,1,120);assert(rows[1].used);p[15]=3;f=parseWifi(p.data(),p.size());observeProbe(rows,2,f,-50,1,130);assert(rows[0].mac[5]==3&&rows[0].count==1);
 std::vector<uint8_t> key(24+8+99);key[0]=8;key[1]=2;const uint8_t llc[]={0xaa,0xaa,3,0,0,0,0x88,0x8e};memcpy(key.data()+24,llc,8);key[32]=2;key[33]=3;key[35]=95;key[36]=2;
 for(auto pair:std::vector<std::pair<uint16_t,unsigned>>{{0x88,1},{0x108,2},{0x1c8,3},{0x308,4},{0xc88,0},{0x80,0}}){key[37]=pair.first>>8;key[38]=pair.first;assert(eapolMessage(key.data(),key.size())==pair.second);for(size_t n=0;n<key.size();++n)assert(!eapolMessage(key.data(),n));}
 assert(nextChannel(1,1057)==6&&nextChannel(6,1057)==11&&nextChannel(11,1057)==1);for(unsigned i=1;i<=11;++i)assert(nextChannel(i,1)==1);
 key[37]=0;key[38]=0x88;key[35]=117;key[129]=0;key[130]=22;key.insert(key.end(),{221,20,0,0x0f,0xac,4});key.resize(key.size()+16);assert(hasPmkid(key.data(),key.size()));key[37]=0x10;assert(!hasPmkid(key.data(),key.size()));
 auto advert=ad(0x16,{0xd2,0xfc,0x40,0,1});auto info=inspectAdvertisement(advert.data(),advert.size(),advert.size());assert(info.valid&&!strcmp(info.protocol,"BTHome sensor data"));assert(ble(advert).confidence==Confidence::None);
 append(advert,{2,0x0a,0xf8,3,0x19,0xc1,3});info=inspectAdvertisement(advert.data(),advert.size(),advert.size());assert(info.power==-8&&info.hasPower&&info.appearance==0x3c1);
 assert(!strcmp(uuidName("00002a4b-0000-1000-8000-00805f9b34fb"),"HID Report Map"));assert(!uuidName("ffff2a4b-0000-1000-8000-00805f9b34fb"));
 const uint8_t map[]={0x05,1,0x09,6,0xa1,1,0x85,1,0x75,1,0x95,8,0x81,2,0x75,8,0x95,6,0x81,0,0x95,1,0x91,2,0xc0};auto hid=parseHidMap(map,sizeof(map));assert(hid.valid&&hid.count==1&&hid.reports[0].id==1&&hid.reports[0].bits[0]==56&&hid.reports[0].bits[1]==8);assert(!parseHidMap(map,sizeof(map)-1).valid);
 for(auto v:std::vector<std::vector<uint8_t>>{{0xb4},{0xa4,0xa4,0xa4,0xa4,0xa4},{0xc0},{0x85,0},{0x75,0xff,0x97,0xff,0xff,0xff,0xff,0x81,2},{0xfe,5,0,1}})assert(!parseHidMap(v.data(),v.size()).valid);
 uint8_t decoded[4];size_t length;assert(decodeHex("01:02-03 04",decoded,4,length)&&length==4);for(const char* s:{"0","zz","01 2","0 1","0102030405",""})assert(!decodeHex(s,decoded,4,length));
 for(const char* marker:{"BWCDEVICE","ECIVEDCWB"}){std::vector<uint8_t> bytes(16);memcpy(bytes.data()+3,marker,9);auto a=ad(0x21,bytes);assert(ble(a).family==Family::Axon);uint8_t mac[6]{0x42,1,2,3,4,5};auto evidence=inspectAxonBle(a.data(),a.size(),a.size(),mac,1);assert(evidence.signals&AxonBwc);a[0]++;assert(!inspectAxonBle(a.data(),a.size(),a.size(),mac,1).detected());}
 assert(ble(name("FS-BEC46A")).family==Family::Flock);assert(ble(name("FS-Living Room")).confidence==Confidence::None);assert(ble(name("BodyWorn Remote")).family==Family::Axon);assert(ble({3,0xff,0x8e,5}).confidence==Confidence::None);
 uint32_t rng=4231;uint8_t data[512];for(unsigned i=0;i<25000;++i){for(auto& x:data){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;x=rng;}size_t n=rng%512;parseHidMap(data,n);inspectAdvertisement(data,n,n/2);eapolMessage(data,n);hasPmkid(data,n);captiveDns(data,n,sizeof(data));}
 puts("PASS recon analysis: raw SSIDs, frame filters, EAPOL bounds, channel plans, BLE fingerprints, HID layouts, BWC priority and 25000 malformed inputs");
}
