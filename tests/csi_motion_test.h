#pragma once
#include "csi_motion.h"
// The first seven cases are ported from ESP32 Marauder develop 79cc6bc,
// test/test_csi_motion_model/test_main.cpp (MIT, Copyright (c) 2020 Just Call
// Me Koko; full notice in firmware/include/csi_motion.h and
// docs/licenses/MARAUDER-LICENSE.txt); the rest are Dragon Detector's own.
namespace csimotiontest {
using dragon::csi::MotionModel;using dragon::csi::State;using dragon::csi::Encoding;
static void put12(int8_t* d,int16_t v){uint16_t e=uint16_t(v)&0xfff;d[0]=int8_t(e&0xff);d[1]=int8_t(e>>8);}
static void fill12(int8_t* s,int16_t re,int16_t im){for(int i=0;i<128;i+=4){put12(s+i,re);put12(s+i+2,im);}}
static void fillLegacy(int8_t* s,int8_t re,int8_t im){for(int i=0;i<128;i+=2){s[i]=im;s[i+1]=re;}}
static void feed(MotionModel& m,const int8_t* s,uint32_t from,uint32_t to,uint32_t& accepted,bool fwi=false,Encoding e=Encoding::Signed12LE,int8_t rssi=-55){for(uint32_t t=from;t<=to;t+=20){m.addSample(s,128,rssi,t,fwi,e);++accepted;m.tick(t,accepted,0);}}
static void marauderTests(){
 {MotionModel m;int8_t s[128];fill12(s,20,10);m.reset(0,1000);uint32_t a=0;
  for(uint32_t t=0;t<=4200;t+=20){m.addSample(s,128,-55,t);++a;m.tick(t,a,0);}
  // Ported with MinCalibrationWindows: a 1 s calibration still needs 15 windows.
  assert(!m.calibrated()&&m.state()==State::LowData&&m.motionScore()<20);}
 {MotionModel m;int8_t s[128];fill12(s,20,10);m.reset(0,500);uint32_t a=0;feed(m,s,0,16200,a);assert(m.calibrated()&&m.reliability()>=25&&m.motionScore()<20&&m.state()==State::Still);
  for(uint32_t t=16220;t<=20200;t+=20){bool d=(t%140)<60;for(int i=0;i<128;i+=4){put12(s+i,d&&((i/4)%4==0)?650:d?40:20);put12(s+i+2,d&&((i/4)%3==0)?-450:d?300:10);}m.addSample(s,128,-55,t);++a;m.tick(t,a,0);}
  assert(m.amplitudeScore()>20&&m.correlationScore()>20&&m.motionScore()>40&&m.state()==State::Motion);}
 {MotionModel m;int8_t s[128];fill12(s,20,10);m.reset(0,100);m.addSample(s,128,-80,200);m.tick(1200,1,8);assert(m.reliability()<=25&&(m.state()==State::Calibrating||m.state()==State::LowData));}
 {MotionModel m;int8_t s[128];fill12(s,20,10);m.reset(0,500);uint32_t a=0;feed(m,s,0,16200,a);
  for(uint32_t t=16220;t<=20200;t+=20){bool e=(t%180)<60;for(int i=0;i<128;i+=4){bool r=e&&((i/4)%2);put12(s+i,r?-10:20);put12(s+i+2,r?20:10);}m.addSample(s,128,-55,t);++a;m.tick(t,a,0);}
  // Fusion uses a median, so a phase-only disturbance does not declare motion.
  assert(m.phaseScore()>50&&m.amplitudeScore()<15&&m.motionScore()<15&&m.state()!=State::Motion);}
 {MotionModel m;int8_t s[128];fill12(s,20,10);m.reset(0,500);uint32_t a=0;feed(m,s,0,16200,a);
  for(uint32_t t=16220;t<=20200;t+=20){for(int i=0;i<128;i+=4){put12(s+i,((i/4)+(t/20))%4==0?700:30);put12(s+i+2,30);}m.addSample(s,128,-55,t);++a;m.tick(t,a,0);}
  assert(m.correlationScore()>20);}
 {MotionModel m;int8_t s[128];fill12(s,-61,15);put12(s,2047);put12(s+2,-2048);m.reset(0,1000);uint32_t a=0;feed(m,s,0,17200,a,true);assert(m.calibrated()&&m.motionScore()<20&&m.carriers()==31);}
 {MotionModel m;int8_t s[128];fillLegacy(s,-61,15);s[0]=s[1]=s[2]=s[3]=127;m.reset(0,1000);uint32_t a=0;feed(m,s,0,17200,a,true,Encoding::Signed8ImagReal);assert(m.calibrated()&&m.motionScore()<20&&m.carriers()==51);}
}
// Synthetic LLTF channel: three static paths, optional moving path whose
// phase and strength change with time, Gaussian receiver noise.
struct Channel{
 uint32_t rng=12345;float noise=1.2f,mover=0;bool guardJunk=false;
 float uniform(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (rng&0xffffff)/float(0x1000000);}
 float gauss(){float s=0;for(int i=0;i<6;++i)s+=uniform();return (s-3)*1.41f;}
 void make(int8_t* out,uint32_t t){
  const float paths[3][3]={{24,0,.3f},{11,3.5f,1.9f},{7,7.2f,4.1f}};
  for(int b=0;b<64;++b){int k=b<32?b:b-64;float re=0,im=0;
   for(auto& p:paths){float a=-6.2832f*k*p[1]/64+p[2];re+=p[0]*std::cos(a);im+=p[0]*std::sin(a);}
   if(mover>0){float ph=6.2832f*t/170.0f,g=mover*(.6f+.4f*std::sin(t/530.0f)),a=-6.2832f*k*5.1f/64+ph;re+=g*std::cos(a);im+=g*std::sin(a);}
   re+=noise*gauss();im+=noise*gauss();
   bool valid=MotionModel::validTone(b);
   if(!valid){re=guardJunk?120*(uniform()-.5f)*2:0;im=guardJunk?120*(uniform()-.5f)*2:0;if(b>=2&&!guardJunk){re=0;im=0;}}
   out[b*2]=int8_t(std::lround(std::fmax(-127,std::fmin(127,im))));out[b*2+1]=int8_t(std::lround(std::fmax(-127,std::fmin(127,re))));}
 }
};
static void run(MotionModel& m,Channel& c,uint32_t from,uint32_t to,uint32_t& a,uint32_t step=50,int rssi=-55){int8_t s[128];for(uint32_t t=from;t<to;t+=step){c.make(s,t);m.addSample(s,128,int8_t(rssi+(c.uniform()<.5f?0:-1)),t,true,Encoding::Signed8ImagReal);++a;m.tick(t,a,0);}m.tick(to,a,0);}
static void ownTests(){
 // Still stream stays STILL with a positive margin; a moving path enters
 // MOTION, is logged once, then HOLD lasts 15 s and returns to STILL.
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);run(m,c,0,30000,a);assert(m.calibrated()&&m.calibrationWindows()>=15);
  run(m,c,30000,90000,a);assert(m.state()==State::Still&&m.intervalCount()==0&&m.noiseWindows()==30&&m.margin()>0);
  c.mover=9;run(m,c,90000,100000,a);assert(m.state()==State::Motion&&m.intervalCount()==1&&m.interval(0).open&&m.interval(0).peak>=MotionModel::MotionThreshold);
  c.mover=0;run(m,c,100000,104000,a);assert(m.state()==State::Hold&&!m.interval(0).open&&m.interval(0).start>=89000&&m.interval(0).end<=103000&&!m.interval(0).moved);
  run(m,c,104000,110000,a);assert(m.state()==State::Hold);run(m,c,110000,118000,a);assert(m.state()==State::Still&&m.intervalCount()==1&&m.movedCount()==0);}
 // Junk on DC and guard tones does not change the result.
 {MotionModel clean,junk;Channel c1,c2;c2.guardJunk=true;uint32_t a1=0,a2=0;clean.reset(0);junk.reset(0);run(clean,c1,0,90000,a1);run(junk,c2,0,90000,a2);
  assert(junk.state()==State::Still&&junk.intervalCount()==0&&junk.carriers()==51);assert(std::abs(int(junk.motionScore())-int(clean.motionScore()))<=3);}
 // Too few calibration windows: LOW DATA, not calibrated, until enough arrive.
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);run(m,c,0,45000,a,300);assert(!m.calibrated()&&m.state()==State::LowData&&m.calibrationWindows()==0&&m.motionScore()==0);
  run(m,c,45000,62000,a);assert(m.calibrated()&&m.calibrationWindows()>=15);}
 // A lone corrupt packet every two seconds is rejected and never declares motion.
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);int8_t s[128];
  for(uint32_t t=0;t<120000;t+=50){c.make(s,t);if(t%2000==1000)for(int b=4;b<128;++b)s[b]=int8_t(c.uniform()*200-100);m.addSample(s,128,-55,t,true,Encoding::Signed8ImagReal);++a;m.tick(t,a,0);}
  assert(m.rejected()>=50&&m.state()==State::Still&&m.intervalCount()==0);}
 // One window over the limit is not MOTION (two are needed).
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);run(m,c,0,60000,a);c.mover=9;run(m,c,60000,60900,a);c.mover=0;run(m,c,60900,75000,a);assert(m.intervalCount()==0&&m.state()!=State::Motion);}
 // Detector moved: the static channel and RSSI change; recalibrate, no MOTION.
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);run(m,c,0,40000,a);assert(m.state()==State::Still);
  c.rng=999;run(m,c,40000,43000,a,50,-70);assert(m.movedCount()==1&&m.state()==State::Calibrating&&m.intervalCount()==0);
  run(m,c,43000,80000,a,50,-70);assert(m.calibrated()&&m.state()==State::Still&&m.movedCount()==1);}
 // Moving during calibration (RSSI range over 10 dB) restarts calibration.
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);run(m,c,0,15000,a);run(m,c,15000,31000,a,50,-72);assert(!m.calibrated()&&m.movedCount()==1&&m.state()==State::Calibrating);}
 // Interval log keeps the last 12 and survives recalibration.
 {MotionModel m;Channel c;uint32_t a=0;m.reset(0);run(m,c,0,40000,a);uint32_t t=40000;
  for(int i=0;i<14;++i){c.mover=9;run(m,c,t,t+5000,a);c.mover=0;run(m,c,t+5000,t+25000,a);t+=25000;}
  assert(m.intervalCount()==12&&m.interval(11).start>m.interval(0).start&&!m.interval(11).open);m.reset(t);assert(m.intervalCount()==12&&!m.calibrated());m.clearLog();assert(!m.intervalCount());}
 // PHY classes and traffic diagnosis.
 assert(MotionModel::phyClass(0,0)==0&&MotionModel::phyClass(0,7)==0&&MotionModel::phyClass(0,0x0b)==1&&MotionModel::phyClass(1,0)==2);
 using namespace dragon::csi;
 assert(diagnose({50,50,0,0,0},5000)==Supply::NoCsi&&diagnose({0,0,0,0,0},5000)==Supply::NotHeard&&diagnose({60,40,20,0,10},5000)==Supply::Sparse&&diagnose({120,40,80,0,60},5000)==Supply::Ok);
 assert(!strcmp(supplyMessage(Supply::NoCsi),"This network's beacons carry no motion data; choose another network"));
 for(int v=0;v<16;++v)assert(MotionModel::validTone(v)==(v>=2));for(int v=27;v<38;++v)assert(!MotionModel::validTone(v));assert(MotionModel::validTone(38)&&MotionModel::validTone(63));
}
}
static void csiMotionTests(){csimotiontest::marauderTests();csimotiontest::ownTests();puts("PASS CSI room motion: Marauder model port, subcarrier mask, Hampel spikes, hysteresis/hold, calibration floor, moved gate, traffic diagnosis");}
