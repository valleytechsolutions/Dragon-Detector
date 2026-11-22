#pragma once
// Room motion model for the CSI monitor (host-testable, no Arduino headers).
//
// The window features, median/MAD calibration, median-of-four fusion, 85th
// percentile threshold, 0.55 score smoothing and reliability figure are ported
// from ESP32 Marauder develop 79cc6bc, esp32_marauder/CsiMotionModel.{h,cpp}
// under this notice (also in docs/licenses/MARAUDER-LICENSE.txt):
//
// MIT License
//
// Copyright (c) 2020 Just Call Me Koko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Dragon Detector additions: the LLTF valid-subcarrier mask with phase fitted
// against real tone indices, Hampel rejection of isolated packet spikes, a
// minimum number of calibration windows, STILL/MOTION/HOLD hysteresis, the
// still-noise margin, the last 12 motion intervals, the detector-moved
// heuristic and the PHY traffic diagnosis.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
namespace dragon::csi {
enum class State:uint8_t{Calibrating,LowData,Still,Motion,Hold};
inline const char* stateName(State s){switch(s){case State::Calibrating:return "CALIBRATING";case State::LowData:return "LOW DATA";case State::Still:return "STILL";case State::Motion:return "MOTION";case State::Hold:return "HOLD";}return "?";}
enum class Encoding:uint8_t{Signed12LE,Signed8ImagReal};
struct Interval{uint32_t start=0,end=0;uint8_t peak=0;bool open=false,moved=false;};

class MotionModel{
public:
 static constexpr size_t MaxCarriers=64,MinCalibrationWindows=15,MaxCalibrationWindows=32,Intervals=12,NoiseWindows=30,HampelWindow=9;
 static constexpr uint8_t MotionThreshold=28;
 static constexpr uint16_t MinWindowSamples=5;
 static constexpr uint32_t HoldMs=15000;
 // Marauder uses the 85th percentile of calibration z (floor .75) as the limit,
 // so about one still window in seven crosses it; on synthetic still CSI that
 // entered MOTION 10-30 times an hour. 2.5x that percentile (z floor 2) gave
 // 0 and 1 entries per 1000 simulated still minutes at two noise levels and 4
 // per 500 at a higher one. Synthetic, not measured.
 static constexpr float ThresholdGain=2.5f,ThresholdFloor=2;
 // Detector-moved heuristic: MovedWindows windows in a row whose mean RSSI sits
 // this far from the calibrated RSSI, or with a spread jump on nearly every
 // subcarrier and a smaller RSSI shift, mean the radio path itself changed;
 // they are not scored. MOTION/HOLD whose RSSI stays LevelRssiDb off the
 // calibrated level for LevelWindows windows (AP power step, raised noise) is a
 // changed path too. Both recalibrate.
 static constexpr float MovedRssiDb=10,BroadRssiDb=5,BroadFraction=.9f,BroadFactor=4,BroadFloor=.02f,LevelRssiDb=4;
 static constexpr uint8_t MovedWindows=3,LevelWindows=30;
 // LLTF in a 128-byte ESP32 buffer: tones 0..31 then -32..-1. Keep 1..26 and
 // -26..-1 (buffer 38..63); drop DC and guards. Tone 1 shares the first word,
 // which the driver may mark invalid, so it is dropped on every packet to keep
 // one layout.
 static bool validTone(size_t b){return (b>=2&&b<=26)||(b>=38&&b<=63);}
 static int toneIndex(size_t b){return b<32?int(b):int(b)-64;}
 // 0 DSSS/CCK (11b, carries no LLTF), 1 legacy OFDM, 2 HT/VHT. rate is the
 // non-HT wifi_phy_rate_t code: 0..7 are 1-11 Mb/s DSSS/CCK.
 static unsigned phyClass(unsigned sigMode,unsigned rate){return sigMode?2:rate<8?0:1;}

 void reset(uint32_t now,uint32_t calibrationMs=30000){
  closeInterval(false);
  uint32_t moved=movedCount_,movedAt=movedAt_,accepted=previousAccepted_,dropped=previousDropped_;Interval log[Intervals];size_t count=intervalCount_,head=intervalHead_;memcpy(log,intervals_,sizeof(log));
  this->~MotionModel();new(this) MotionModel();movedCount_=moved;movedAt_=movedAt;previousAccepted_=accepted;previousDropped_=dropped;memcpy(intervals_,log,sizeof(log));intervalCount_=count;intervalHead_=head;
  started_=lastTick_=now;calibrationMs_=calibrationMs;clearWindow(now);
 }
 // Also zeroes the rate baseline: call at session start, when the caller's
 // accepted/dropped totals restart from zero.
 void clearLog(){intervalCount_=intervalHead_=0;movedCount_=0;movedAt_=0;previousAccepted_=previousDropped_=0;}

 void addSample(const int8_t* bytes,size_t length,int8_t rssi,uint32_t now,bool firstWordInvalid=false,Encoding encoding=Encoding::Signed12LE){
  if(!bytes||length<16)return;
  const bool legacy=encoding==Encoding::Signed8ImagReal;
  size_t tones=((legacy?length:length/2)&~size_t(1))/2;if(tones>MaxCarriers)tones=MaxCarriers;
  const bool masked=tones==64;
  const size_t first=masked?0:firstWordInvalid?(legacy?2:1):0;
  float re[MaxCarriers],im[MaxCarriers],mag[MaxCarriers],x[MaxCarriers],total=0;size_t n=0;
  for(size_t t=first;t<tones;++t){
   if(masked&&!validTone(t))continue;
   const size_t s=t*2;
   if(legacy){im[n]=bytes[s];re[n]=bytes[s+1];}else{re[n]=decode12(bytes+s*2);im[n]=decode12(bytes+(s+1)*2);}
   mag[n]=std::sqrt(re[n]*re[n]+im[n]*im[n]);x[n]=masked?float(toneIndex(t)):float(n);total+=mag[n];++n;
  }
  while(int32_t(now-windowStarted_)>=1000)finishWindow(windowStarted_+1000);
  if(n<2||total<1)return;
  float norm[MaxCarriers];const float scale=float(n)/total;for(size_t i=0;i<n;++i)norm[i]=mag[i]*scale;
  float features[3]{};bool pair=previousCount_==n;
  if(pair){
   float dot=0,energy=0,previousEnergy=0,temporal=0,sx=0,sy=0,sxx=0,sxy=0,delta[MaxCarriers];
   for(size_t i=0;i<n;++i){
    dot+=norm[i]*previousNorm_[i];energy+=norm[i]*norm[i];previousEnergy+=previousNorm_[i]*previousNorm_[i];temporal+=std::fabs(norm[i]-previousNorm_[i]);
    delta[i]=std::atan2(im[i]*previousRe_[i]-re[i]*previousIm_[i],re[i]*previousRe_[i]+im[i]*previousIm_[i]);
    sx+=x[i];sy+=delta[i];sxx+=x[i]*x[i];sxy+=x[i]*delta[i];
   }
   const float denominator=n*sxx-sx*sx,slope=std::fabs(denominator)>1e-6f?(n*sxy-sx*sy)/denominator:0,intercept=(sy-slope*sx)/n;
   float phase=0;for(size_t i=0;i<n;++i){float r=delta[i]-(slope*x[i]+intercept);phase+=r*r;}
   features[0]=energy>0&&previousEnergy>0?1-clamp(dot/std::sqrt(energy*previousEnergy),-1,1):0;
   features[1]=std::sqrt(phase/n);features[2]=temporal/n;
   // Hampel identifier on decorrelation: an isolated corrupt packet (AGC step,
   // collision) is skipped once so it neither counts nor becomes the reference.
   // A second outlier in a row is a real change and is accepted.
   if(hampel(features[0])&&!skippedLast_){skippedLast_=true;++rejected_;return;}
  }
  skippedLast_=false;
  carriers_=n;lastRssi_=rssi;rssiSum_+=rssi;
  const float d=float(rssi)-rssiMean_;rssiMean_+=d*.03f;rssiDeviation_+=(std::fabs(d)-rssiDeviation_)*.03f;
  for(size_t i=0;i<n;++i){magnitudeSum_[i]+=norm[i];magnitudeSq_[i]+=norm[i]*norm[i];}
  ++windowSamples_;
  if(pair){for(size_t i=0;i<3;++i){featureSum_[i]+=features[i];featureSq_[i]+=features[i]*features[i];}++pairSamples_;}
  memcpy(previousRe_,re,n*sizeof(float));memcpy(previousIm_,im,n*sizeof(float));memcpy(previousNorm_,norm,n*sizeof(float));previousCount_=n;
 }

 void tick(uint32_t now,uint32_t accepted,uint32_t dropped){
  while(int32_t(now-windowStarted_)>=1000)finishWindow(windowStarted_+1000);
  if(!calibrated_&&now-started_>=calibrationMs_)finishCalibration(now);
  const uint32_t elapsed=now-lastTick_;
  if(elapsed>=1000){
   const uint32_t fresh=accepted-previousAccepted_,drops=dropped-previousDropped_;
   samplesPerSecond_=uint16_t(fresh>65535?65535:fresh*1000u/elapsed);previousAccepted_=accepted;previousDropped_=dropped;lastTick_=now;
   int q=samplesPerSecond_>=20?45:samplesPerSecond_>=10?35:samplesPerSecond_>=5?24:samplesPerSecond_>=2?12:0;
   q+=rssiMean_>-75?20:rssiMean_>-85?12:4;q+=rssiDeviation_<3?20:rssiDeviation_<7?12:3;
   if(fresh+drops){q+=int(fresh*15u/(fresh+drops));q-=int(drops*60u/(fresh+drops));}
   if(samplesPerSecond_<2&&q>20)q=20;
   reliability_=calibrated_?uint8_t(clamp(float(q),0,100)):calibrationPercent(now)/4;
  }
  if(!calibrated_)state_=now-started_<calibrationMs_?State::Calibrating:State::LowData;
  else if(samplesPerSecond_<MinWindowSamples||reliability_<25)state_=State::LowData;
  else if(inMotion_)state_=State::Motion;
  else if(hasMotion_&&now-lastMotion_<HoldMs)state_=State::Hold;
  else state_=State::Still;
 }

 State state()const{return state_;}
 bool calibrated()const{return calibrated_;}
 uint8_t calibrationPercent(uint32_t now)const{if(calibrated_||!calibrationMs_)return 100;uint32_t e=now-started_;return e>=calibrationMs_?100:uint8_t(e*100u/calibrationMs_);}
 size_t calibrationWindows()const{return calibrationCount_;}
 uint8_t motionScore()const{return motionScore_;}
 uint8_t windowScore()const{return windowScore_;}
 uint8_t amplitudeScore()const{return amplitudeScore_;}
 uint8_t correlationScore()const{return correlationScore_;}
 uint8_t phaseScore()const{return phaseScore_;}
 uint8_t reliability()const{return reliability_;}
 uint16_t samplesPerSecond()const{return samplesPerSecond_;}
 float fused()const{return fused_;}
 float thresholdZ()const{return thresholdZ_;}
 size_t carriers()const{return carriers_;}
 uint32_t rejected()const{return rejected_;}
 uint32_t movedCount()const{return movedCount_;}
 uint32_t movedAt()const{return movedAt_;}
 uint32_t lastMotion()const{return hasMotion_?lastMotion_:0;}
 // Highest score of recent non-motion windows; margin = limit - that peak.
 // Large positive: quiet placement. Near zero or negative: false alarms likely.
 size_t noiseWindows()const{return noiseCount_;}
 uint8_t stillPeak()const{uint8_t p=0;for(size_t i=0;i<noiseCount_;++i)p=noise_[i]>p?noise_[i]:p;return p;}
 int margin()const{return int(MotionThreshold)-int(stillPeak());}
 size_t intervalCount()const{return intervalCount_;}
 // 0 is the oldest retained interval.
 const Interval& interval(size_t i)const{return intervals_[(intervalHead_+Intervals-intervalCount_+i)%Intervals];}

private:
 static float clamp(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
 static float median4(float a,float b,float c,float d){float lo=std::fmin(std::fmin(a,b),std::fmin(c,d)),hi=std::fmax(std::fmax(a,b),std::fmax(c,d));return (a+b+c+d-lo-hi)*.5f;}
 static int16_t decode12(const int8_t* b){uint16_t w=uint8_t(b[0])|uint16_t(uint8_t(b[1]))<<8;if(w&0x800)w|=0xF000;return int16_t(w);}
 static float median(float* v,size_t n){for(size_t i=1;i<n;++i){float x=v[i];size_t j=i;while(j&&v[j-1]>x){v[j]=v[j-1];--j;}v[j]=x;}if(!n)return 0;return n&1?v[n/2]:(v[n/2-1]+v[n/2])*.5f;}
 static float featureStd(float sum,float sq,uint16_t n){if(n<2)return 0;float m=sum/n;return std::sqrt(clamp(sq/n-m*m,0,1e6f));}
 bool hampel(float v){
  ring_[ringHead_]=v;ringHead_=(ringHead_+1)%HampelWindow;if(ringCount_<HampelWindow)++ringCount_;
  if(ringCount_<5)return false;
  float w[HampelWindow];memcpy(w,ring_,ringCount_*sizeof(float));const float m=median(w,ringCount_);
  for(size_t i=0;i<ringCount_;++i)w[i]=std::fabs(ring_[i]-m);
  return std::fabs(v-m)>std::fmax(3*1.4826f*median(w,ringCount_),.01f);
 }
 void clearWindow(uint32_t now){memset(magnitudeSum_,0,sizeof(magnitudeSum_));memset(magnitudeSq_,0,sizeof(magnitudeSq_));memset(featureSum_,0,sizeof(featureSum_));memset(featureSq_,0,sizeof(featureSq_));windowSamples_=pairSamples_=0;rssiSum_=0;windowStarted_=now;}
 void closeInterval(bool moved){if(!inMotion_)return;Interval& i=intervals_[(intervalHead_+Intervals-1)%Intervals];i.end=lastMotion_;i.open=false;i.moved=moved;inMotion_=false;}
 void detectorMoved(uint32_t now){closeInterval(true);++movedCount_;movedAt_=now;reset(now,calibrationMs_);}
 void finishCalibration(uint32_t now){
  if(calibrationCount_<MinCalibrationWindows)return;
  // 10th-90th percentile RSSI range, so one odd window does not restart it.
  float work[MaxCalibrationWindows];memcpy(work,calibrationRssi_,sizeof(work));rssiBase_=median(work,calibrationCount_);
  const size_t edge=calibrationCount_/10;if(work[calibrationCount_-1-edge]-work[edge]>=MovedRssiDb){detectorMoved(now);return;}
  for(size_t i=0;i<calibrationCarriers_;++i)calibrationStd_[i]/=calibrationCount_;
  for(size_t f=0;f<4;++f){
   for(size_t i=0;i<calibrationCount_;++i)work[i]=calibrationFeatures_[i][f];
   featureMedian_[f]=median(work,calibrationCount_);
   for(size_t i=0;i<calibrationCount_;++i)work[i]=std::fabs(calibrationFeatures_[i][f]-featureMedian_[f]);
   const float mad=median(work,calibrationCount_)*1.4826f,floor=featureMedian_[f]*.05f;
   featureScale_[f]=clamp(mad>floor?mad:floor,1e-4f,1e6f);
  }
  for(size_t i=0;i<calibrationCount_;++i){float z[4];for(size_t f=0;f<4;++f)z[f]=clamp((calibrationFeatures_[i][f]-featureMedian_[f])/featureScale_[f],0,10);work[i]=median4(z[0],z[1],z[2],z[3]);}
  median(work,calibrationCount_);size_t p=(calibrationCount_*85+99)/100;if(p)--p;
  thresholdZ_=clamp(work[p]*ThresholdGain,ThresholdFloor,6);calibrated_=true;state_=State::LowData;
 }
 void finishWindow(uint32_t end){
  const bool usable=windowSamples_>=MinWindowSamples&&pairSamples_>=2&&carriers_;
  if(usable){
   float spread[MaxCarriers],amplitude=0;
   for(size_t i=0;i<carriers_;++i){float m=magnitudeSum_[i]/windowSamples_;spread[i]=std::sqrt(clamp(magnitudeSq_[i]/windowSamples_-m*m,0,1e6f));amplitude+=spread[i];}
   const float features[4]={amplitude/carriers_,featureStd(featureSum_[0],featureSq_[0],pairSamples_),featureStd(featureSum_[1],featureSq_[1],pairSamples_),featureStd(featureSum_[2],featureSq_[2],pairSamples_)};
   const float rssi=rssiSum_/windowSamples_;
   if(!calibrated_){
    if(calibrationCount_<MaxCalibrationWindows&&(!calibrationCount_||carriers_==calibrationCarriers_)){
     memcpy(calibrationFeatures_[calibrationCount_],features,sizeof(features));calibrationRssi_[calibrationCount_]=rssi;
     calibrationCarriers_=carriers_;for(size_t i=0;i<carriers_;++i)calibrationStd_[i]+=spread[i];++calibrationCount_;
    }
    if(end-started_>=calibrationMs_)finishCalibration(end);
   } else {
    const float shift=std::fabs(rssi-rssiBase_);size_t broad=0;
    if(carriers_==calibrationCarriers_)for(size_t i=0;i<carriers_;++i)broad+=spread[i]>BroadFactor*calibrationStd_[i]+BroadFloor;
    movedWindows_=shift>=MovedRssiDb||(shift>=BroadRssiDb&&broad>=BroadFraction*carriers_)?movedWindows_+1:0;
    levelWindows_=shift>=LevelRssiDb&&(inMotion_||(hasMotion_&&end-lastMotion_<HoldMs))?levelWindows_+1:0;
    if(movedWindows_>=MovedWindows||levelWindows_>=LevelWindows){detectorMoved(end);return;}
    if(movedWindows_){clearWindow(end);return;}
    float z[4];for(size_t f=0;f<4;++f)z[f]=clamp((features[f]-featureMedian_[f])/featureScale_[f],0,10);
    fused_=median4(z[0],z[1],z[2],z[3]);
    amplitudeScore_=uint8_t(clamp(z[0]*20,0,100)+.5f);correlationScore_=uint8_t(clamp(z[1]*20,0,100)+.5f);phaseScore_=uint8_t(clamp(z[2]*20,0,100)+.5f);
    const float mapped=clamp(fused_/thresholdZ_*MotionThreshold,0,100);windowScore_=uint8_t(mapped+.5f);smoothed_+=(mapped-smoothed_)*.55f;motionScore_=uint8_t(smoothed_+.5f);
    vote(end);
   }
  } else if(calibrated_){
   // A starved window breaks an entry streak; motion that loses its data ends
   // at the last window that showed it.
   motionVotes_=0;if(inMotion_&&end-lastMotion_>=2000)closeInterval(false);
  }
  clearWindow(end);
 }
 // Two windows whose own (unsmoothed) score is over the limit enter MOTION and
 // two under it leave; HOLD then lasts HoldMs after the last motion window.
 void vote(uint32_t end){
  if(windowScore_>=MotionThreshold){
   stillVotes_=0;if(++motionVotes_==1)firstOver_=end-1000;if(motionVotes_>5)motionVotes_=5;
   // Re-entry within HOLD continues the last interval: at 5-7 samples/s one
   // movement can dip under the limit for a few windows.
   if(motionVotes_>=2&&!inMotion_&&hasMotion_&&intervalCount_&&firstOver_-lastMotion_<HoldMs){inMotion_=true;intervals_[(intervalHead_+Intervals-1)%Intervals].open=true;}
   if(motionVotes_>=2&&!inMotion_){inMotion_=true;Interval& i=intervals_[intervalHead_];i={};i.start=firstOver_;i.open=true;intervalHead_=(intervalHead_+1)%Intervals;if(intervalCount_<Intervals)++intervalCount_;}
   if(inMotion_){pendingOver_=false;lastMotion_=end;hasMotion_=true;Interval& i=intervals_[(intervalHead_+Intervals-1)%Intervals];i.end=end;if(windowScore_>i.peak)i.peak=windowScore_;return;}
   // A lone crossing counts as still noise only once the next window stays under.
   pendingOver_=true;pendingScore_=windowScore_;return;
  }
  motionVotes_=0;if(stillVotes_<3)++stillVotes_;if(inMotion_&&stillVotes_>=(windowSamples_<2*MinWindowSamples?3:2))closeInterval(false);if(inMotion_)return;
  if(pendingOver_){addNoise(pendingScore_);pendingOver_=false;}
  addNoise(windowScore_);
 }
 void addNoise(uint8_t v){noise_[noiseHead_]=v;noiseHead_=(noiseHead_+1)%NoiseWindows;if(noiseCount_<NoiseWindows)++noiseCount_;}

 float previousRe_[MaxCarriers]{},previousIm_[MaxCarriers]{},previousNorm_[MaxCarriers]{},magnitudeSum_[MaxCarriers]{},magnitudeSq_[MaxCarriers]{},calibrationStd_[MaxCarriers]{};
 float featureSum_[3]{},featureSq_[3]{},calibrationFeatures_[MaxCalibrationWindows][4]{},calibrationRssi_[MaxCalibrationWindows]{},featureMedian_[4]{},featureScale_[4]{},ring_[HampelWindow]{};
 Interval intervals_[Intervals]{};uint8_t noise_[NoiseWindows]{};
 size_t carriers_=0,previousCount_=0,calibrationCount_=0,calibrationCarriers_=0,ringHead_=0,ringCount_=0,intervalHead_=0,intervalCount_=0,noiseHead_=0,noiseCount_=0;
 uint32_t started_=0,calibrationMs_=30000,windowStarted_=0,lastTick_=0,previousAccepted_=0,previousDropped_=0,rejected_=0,movedCount_=0,movedAt_=0,lastMotion_=0,firstOver_=0;
 float rssiMean_=-100,rssiDeviation_=0,rssiSum_=0,rssiBase_=0,smoothed_=0,fused_=0,thresholdZ_=1;
 uint16_t windowSamples_=0,pairSamples_=0,samplesPerSecond_=0;
 int8_t lastRssi_=-127;uint8_t movedWindows_=0,levelWindows_=0,pendingScore_=0,windowScore_=0,motionScore_=0,amplitudeScore_=0,correlationScore_=0,phaseScore_=0,reliability_=0,motionVotes_=0,stillVotes_=0;
 bool calibrated_=false,skippedLast_=false,pendingOver_=false,inMotion_=false,hasMotion_=false;State state_=State::Calibrating;
};

// Why the selected network is (or is not) feeding the model, from frames heard
// from that BSSID over the last few seconds.
struct Traffic{uint32_t heard=0,dsss=0,ofdm=0,ht=0,usable=0;};
enum class Supply:uint8_t{Ok,NotHeard,NoCsi,Sparse};
inline Supply diagnose(const Traffic& t,uint32_t ms){
 if(!ms||t.usable*1000u/ms>=MotionModel::MinWindowSamples)return Supply::Ok;
 if(!t.heard)return Supply::NotHeard;
 if(!t.ofdm&&!t.ht)return Supply::NoCsi;
 return Supply::Sparse;
}
inline const char* supplyName(Supply s){return s==Supply::Ok?"ok":s==Supply::NotHeard?"not_heard":s==Supply::NoCsi?"dsss_only":"sparse";}
inline const char* supplyMessage(Supply s){return s==Supply::NotHeard?"Selected network not heard here; choose another network":s==Supply::NoCsi?"This network's beacons carry no motion data; choose another network":s==Supply::Sparse?"Too little OFDM traffic from this network; LOW DATA":"";}
}
