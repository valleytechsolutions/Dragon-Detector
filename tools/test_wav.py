"""Host test of the Voice Recorder's WAV writer/reader and sample helpers
(firmware/include/wav_format.h), and of which boards build the recorder.
Install ziglang, or pass --cxx g++."""
from pathlib import Path
import argparse
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work/wav-tests'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']

source = r'''
#include "wav_format.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
using namespace dragon;
using Bytes=std::vector<uint8_t>;
static void put(Bytes& b,const char* tag){b.insert(b.end(),tag,tag+4);}
static void put32(Bytes& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(uint8_t(v>>(8*i)));}
static void put16(Bytes& b,uint16_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));}
static void fmt(Bytes& b,uint16_t format,uint16_t channels,uint32_t rate,uint16_t bits,uint32_t size=16,uint16_t sub=0){
  put(b,"fmt ");put32(b,size);put16(b,format);put16(b,channels);put32(b,rate);put32(b,rate*channels*bits/8);put16(b,uint16_t(channels*bits/8));put16(b,bits);
  for(uint32_t i=16;i<size;++i)b.push_back(0);
  if(sub){b[b.size()-size+24]=uint8_t(sub);b[b.size()-size+25]=uint8_t(sub>>8);}
}
static void chunk(Bytes& b,const char* tag,uint32_t size,uint8_t fill=0x55){put(b,tag);put32(b,size);for(uint32_t i=0;i<size;++i)b.push_back(fill);if(size&1)b.push_back(0);}
static Bytes riff(){Bytes b;put(b,"RIFF");put32(b,0);put(b,"WAVE");return b;}
static void close(Bytes& b){uint32_t n=uint32_t(b.size()-8);b[4]=uint8_t(n);b[5]=uint8_t(n>>8);b[6]=uint8_t(n>>16);b[7]=uint8_t(n>>24);}
struct Reader {const Bytes& b;bool fail=false;bool operator()(uint32_t at,uint8_t* out,uint32_t n)const{if(fail||uint64_t(at)+n>b.size())return false;memcpy(out,b.data()+at,n);return true;}};
static wav::Status parse(const Bytes& b,wav::Format& f,bool fail=false){Reader r{b,fail};return wav::parse(r,uint32_t(b.size()),f);}

int main(){
  // ---- the recorder's own header: streaming placeholder first, sizes patched later
  uint8_t h[wav::HeaderBytes];wav::header(h,16000,1,wav::Unknown);
  assert(!memcmp(h,"RIFF",4)&&!memcmp(h+8,"WAVEfmt ",8)&&!memcmp(h+36,"data",4));
  assert(wav::get32(h+4)==wav::Unknown&&wav::get32(h+40)==wav::Unknown&&wav::get32(h+16)==16);
  assert(wav::get16(h+20)==1&&wav::get16(h+22)==1&&wav::get32(h+24)==16000&&wav::get32(h+28)==32000&&wav::get16(h+32)==2&&wav::get16(h+34)==16);
  Bytes file(h,h+44);for(int i=0;i<32000;++i)file.push_back(uint8_t(i)); // 1 s of samples, header never updated
  wav::Format f;assert(parse(file,f)==wav::Status::Ok);
  assert(f.format==1&&f.channels==1&&f.rate==16000&&f.bits==16&&f.blockAlign==2&&f.dataOffset==44&&f.dataBytes==32000&&f.sizeGuessed);
  assert(!wav::unsupported(f)&&wav::durationMs(f)==1000);
  // patched as recStep/recFinish do: RIFF size at 4, data size at 40
  wav::put32(file.data()+4,wav::riffSize(32000));wav::put32(file.data()+40,32000);
  assert(parse(file,f)==wav::Status::Ok&&f.dataBytes==32000&&!f.sizeGuessed&&wav::get32(file.data()+4)==32036);
  // a power cut after a patch: the bytes after it are not counted, the rest plays
  for(int i=0;i<16000;++i)file.push_back(0);
  assert(parse(file,f)==wav::Status::Ok&&f.dataBytes==32000);
  // a canonical header with a known size
  wav::header(h,44100,2,4000);assert(wav::get32(h+4)==4036&&wav::get32(h+28)==176400&&wav::get16(h+32)==4&&wav::get32(h+40)==4000);
  assert(wav::riffSize(wav::Unknown-36)==wav::Unknown&&wav::riffSize(wav::FileLimit)==wav::FileLimit+36);
  assert(wav::FileLimit<0xFFFFFFFFu&&wav::FileLimit%2==0);

  // ---- other writers: LIST/fact/odd chunks before and after, data before fmt
  {Bytes b=riff();chunk(b,"LIST",5);chunk(b,"JUNK",28);fmt(b,1,2,22050,16,18);chunk(b,"fact",4);chunk(b,"data",4410*4,0x11);chunk(b,"id3 ",7);close(b);
   assert(parse(b,f)==wav::Status::Ok&&f.channels==2&&f.rate==22050&&f.dataBytes==17640&&!f.sizeGuessed);
   assert(b[f.dataOffset]==0x11&&b[f.dataOffset-8]=='d'&&wav::durationMs(f)==200&&!wav::unsupported(f));}
  {Bytes b=riff();chunk(b,"data",1600,0x22);fmt(b,1,1,8000,16);close(b);
   assert(parse(b,f)==wav::Status::Ok&&f.rate==8000&&f.dataBytes==1600&&b[f.dataOffset]==0x22);}
  // WAVE_FORMAT_EXTENSIBLE with PCM and with float sub-formats
  {Bytes b=riff();fmt(b,0xFFFE,2,48000,16,40,1);chunk(b,"data",192);close(b);assert(parse(b,f)==wav::Status::Ok&&f.format==1&&!wav::unsupported(f));}
  {Bytes b=riff();fmt(b,0xFFFE,2,48000,32,40,3);chunk(b,"data",192);close(b);assert(parse(b,f)==wav::Status::Ok&&f.format==3&&wav::unsupported(f));}
  // cut off: the data size runs past the end, an odd tail is not half a frame
  {Bytes b=riff();fmt(b,1,2,16000,16);put(b,"data");put32(b,100000);for(int i=0;i<1003;++i)b.push_back(1);close(b);
   assert(parse(b,f)==wav::Status::Ok&&f.dataBytes==1000&&f.sizeGuessed);}
  {Bytes b=riff();fmt(b,1,1,16000,16);put(b,"data");put32(b,0);for(int i=0;i<640;++i)b.push_back(1);close(b);
   assert(parse(b,f)==wav::Status::Ok&&f.dataBytes==640&&f.sizeGuessed);}

  // ---- what is not a playable WAV
  {Bytes b={'R','I','F'};assert(parse(b,f)==wav::Status::NotRiff);}
  {Bytes b=riff();b[0]='X';fmt(b,1,1,16000,16);chunk(b,"data",2);assert(parse(b,f)==wav::Status::NotRiff);}
  {Bytes b=riff();b[8]='A';b[9]='V';b[10]='I';b[11]=' ';fmt(b,1,1,16000,16);chunk(b,"data",2);assert(parse(b,f)==wav::Status::NotWave);}
  {Bytes b=riff();chunk(b,"data",64);close(b);assert(parse(b,f)==wav::Status::NoFormat);}
  {Bytes b=riff();fmt(b,1,1,16000,16);chunk(b,"LIST",10);close(b);assert(parse(b,f)==wav::Status::NoData);}
  {Bytes b=riff();put(b,"fmt ");put32(b,12);for(int i=0;i<12;++i)b.push_back(0);chunk(b,"data",8);assert(parse(b,f)==wav::Status::NoFormat);}
  {Bytes b=riff();fmt(b,1,1,16000,16);chunk(b,"data",8);assert(parse(b,f,true)==wav::Status::ReadError);}
  {Bytes b=riff();for(int i=0;i<300;++i)chunk(b,"JUNK",0);fmt(b,1,1,16000,16);chunk(b,"data",8);assert(parse(b,f)==wav::Status::NoFormat);} // bounded walk
  for(auto s:{wav::Status::ReadError,wav::Status::NotRiff,wav::Status::NotWave,wav::Status::NoFormat,wav::Status::NoData})assert(strlen(wav::statusText(s))>8);
  auto check=[&](uint16_t format,uint16_t channels,uint32_t rate,uint16_t bits,bool ok){Bytes b=riff();fmt(b,format,channels,rate,bits);chunk(b,"data",channels*bits/8*4);
    assert(parse(b,f)==wav::Status::Ok);const char* why=wav::unsupported(f);if(ok!=(why==nullptr)){printf("format %u ch %u rate %u bits %u: %s\n",format,channels,rate,bits,why?why:"ok");assert(false);}};
  for(uint32_t rate:{8000u,11025u,12000u,16000u,22050u,24000u,32000u,44100u,48000u})for(uint16_t ch:{1,2})check(1,ch,rate,16,true);
  check(1,1,7999,16,false);check(1,2,48001,16,false);check(1,2,96000,16,false);check(1,3,16000,16,false);check(1,1,16000,8,false);check(1,1,16000,24,false);
  check(3,1,16000,32,false);check(2,1,16000,4,false);check(0x55,2,44100,0,false);
  {Bytes b=riff();fmt(b,1,2,16000,16);b[32]=2;chunk(b,"data",8);assert(parse(b,f)==wav::Status::Ok&&wav::unsupported(f));} // blockAlign lies
  {wav::Format g;g.format=3;g.bits=32;g.channels=1;g.rate=16000;g.blockAlign=4;assert(strstr(wav::unsupported(g),"Float"));}

  // ---- file names
  char name[64];
  assert(wav::recordingName(name,sizeof(name),true,2026,11,20,15,33,7,754,1)&&!strcmp(name,"/recordings/REC_2026-11-20_15-33-07.wav"));
  assert(wav::recordingName(name,sizeof(name),true,2026,11,20,15,33,7,754,2)&&!strcmp(name,"/recordings/REC_2026-11-20_15-33-07_2.wav"));
  assert(wav::recordingName(name,sizeof(name),false,0,0,0,0,0,0,754,1)&&!strcmp(name,"/recordings/REC_UPTIME_000754s.wav"));
  assert(wav::recordingName(name,sizeof(name),false,0,0,0,0,0,0,4000000000u,99)&&!strcmp(name,"/recordings/REC_UPTIME_4000000000s_99.wav"));
  assert(wav::recordingName(name,sizeof(name),true,2026,1,2,3,4,5,0,99)&&strlen(name)<48+12); // the listing keeps 47-character names
  assert(!wav::recordingName(name,20,true,2026,11,20,15,33,7,754,1));
  assert(wav::isWavName("a.wav")&&wav::isWavName("REC.WAV")&&wav::isWavName("x.Wav")&&!wav::isWavName(".wav")&&!wav::isWavName("a.wave")&&!wav::isWavName("a.mp3")&&!wav::isWavName("wav"));
  // newest first by name is newest first by time
  char a[64],b2[64];wav::recordingName(a,64,true,2026,11,20,9,59,59,0,1);wav::recordingName(b2,64,true,2026,11,20,10,0,0,0,1);assert(strcmp(b2,a)>0);

  // ---- level meter and sample helpers
  assert(wav::meterPercent(0)==0&&wav::meterPercent(32)==0&&wav::meterPercent(32767)==100&&wav::meterPercent(-32768)==100);
  assert(wav::meterPercent(3277)==67&&wav::meterPercent(328)==33&&wav::meterPercent(-3277)==67);
  for(int p=0,last=0;p<=32767;p+=97){int m=wav::meterPercent(p);assert(m>=last&&m<=100);last=m;}
  assert(wav::mix(32767,32767)==32767&&wav::mix(-32768,-32768)==-32768&&wav::mix(1000,-1000)==0&&wav::mix(3,4)==3);
  assert(wav::scale(1000,50)==500&&wav::scale(-32768,100)==-32768&&wav::scale(1234,0)==0&&wav::scale(-1000,25)==-250&&wav::scale(77,150)==77);

  // ---- rate conversion to the speaker's 16 kHz: exact length, no drift, level and pitch kept
  for(uint32_t rate:{8000u,11025u,12000u,16000u,22050u,24000u,32000u,44100u,48000u}){
    wav::Resampler r;r.begin(rate,16000);
    const uint64_t inputs=uint64_t(rate)*600; // ten minutes
    uint64_t outputs=0;bool flat=true;
    for(uint64_t i=0;i<inputs;++i)r.push(1000,[&](int16_t v){++outputs;if(v!=1000)flat=false;});
    const double want=double(inputs)*16000/rate;
    if(std::fabs(double(outputs)-want)>2.5){printf("rate %u: %llu outputs for %.0f\n",rate,(unsigned long long)outputs,want);assert(false);}
    assert(flat);
    // one second of a 1 kHz tone keeps 2000 zero crossings and most of its level
    r.begin(rate,16000);std::vector<int16_t> out;
    const double radiansPerSample=2*std::acos(-1.0)*1000/rate;
    for(uint32_t i=0;i<rate;++i)r.push(int16_t(lround(10000*sin(radiansPerSample*i))),[&](int16_t v){out.push_back(v);});
    unsigned crossings=0;double energy=0;for(size_t i=1;i<out.size();++i){if((out[i-1]<0)!=(out[i]<0))++crossings;energy+=double(out[i])*out[i];}
    const double rms=std::sqrt(energy/(out.size()-1));
    if(crossings<1996||crossings>2004||rms<0.9*10000/std::sqrt(2.0)){printf("rate %u: %u crossings rms %.0f\n",rate,crossings,rms);assert(false);}
    // one 4096-byte card read never makes more samples than the player's buffer (4104)
    for(uint16_t channels:{1,2}){
      r.begin(rate,16000);const unsigned frames=4096/(2*channels);
      for(int block=0;block<8;++block){unsigned made=0;for(unsigned i=0;i<frames;++i)r.push(int16_t(i),[&](int16_t){++made;});assert(made<=4096+8);}
    }
  }
  {wav::Resampler r;r.begin(8000,16000);std::vector<int16_t> out;for(int16_t x:{0,100,200,300})r.push(x,[&](int16_t v){out.push_back(v);});
   assert((out==std::vector<int16_t>{0,50,100,150,200,250}));}
  {wav::Resampler r;r.begin(48000,16000);std::vector<int16_t> out;for(int16_t x:{3,6,9,30,60,90})r.push(x,[&](int16_t v){out.push_back(v);});
   assert((out==std::vector<int16_t>{6,60}));}
  {wav::Resampler r;r.begin(32767,32768);unsigned n=0;r.push(int16_t(-32768),[&](int16_t){++n;});for(int i=0;i<1000;++i)r.push(int16_t(32767),[&](int16_t v){assert(v>=-32768&&v<=32767);++n;});assert(n>=999&&n<=1001);}

  // ---- the one-producer / one-consumer ring, including the 32-bit counter wrap
  {std::vector<int16_t> store(8);wav::Ring<int16_t> ring;assert(ring.space()==0);ring.attach(store.data(),8);
   int16_t in[10]={1,2,3,4,5,6,7,8,9,10},out[10]{};
   assert(ring.write(in,5)==5&&ring.used()==5&&ring.space()==3);
   assert(ring.read(out,3)==3&&out[0]==1&&out[2]==3);
   assert(ring.write(in+5,5)==5&&ring.used()==7&&ring.write(in,3)==1&&ring.space()==0&&!ring.put(1));
   assert(ring.read(out,10)==8);int16_t want[8]={4,5,6,7,8,9,10,1};assert(!memcmp(out,want,sizeof(want)));
   assert(ring.read(out,1)==0&&ring.put(42)&&ring.used()==1);ring.reset();assert(ring.used()==0&&ring.space()==8);
   ring.head=0xFFFFFFFCu;ring.tail=0xFFFFFFFCu;assert(ring.write(in,8)==8&&ring.used()==8&&ring.head.load()==4);
   assert(ring.read(out,8)==8&&!memcmp(out,in,16)&&ring.used()==0);}
  puts("PASS wav header, parser, names, meter, resampler and ring");
}
'''
cpp = work / 'wav_test.cpp'
cpp.write_text(source, encoding='utf-8')
exe = work / ('wav_test.exe' if sys.platform == 'win32' else 'wav_test')
subprocess.run(compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-I' + str(root / 'firmware/include'), str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)

# The recorder is built only where a microphone driver exists (BOARD_HAS_MIC),
# and every other profile compiles it out.
boards = {
    'waveshare_p4_knob': (['BOARD_WAVESHARE_P4_KNOB'], True),
    'spotpear': ([], False), 'guition_185_beta': (['BOARD_GUITION_185_BETA'], False),
    'waveshare': (['BOARD_AMOLED_175C'], False), 'waveshare_175': (['BOARD_AMOLED_175'], False),
    'waveshare_175_gps': (['BOARD_AMOLED_175', 'BOARD_GPS_LC76G'], False), 'waveshare_185b': (['BOARD_WAVESHARE_185B'], False),
    'waveshare_143c': (['BOARD_WAVESHARE_143C'], False), 'xiao_round': (['BOARD_XIAO_ROUND'], False),
    'lilygo_175': (['BOARD_LILYGO_175'], False), 'stopwatch': (['BOARD_STOPWATCH'], False),
    'elecrow_21': (['BOARD_CROWPANEL_21'], False), 'waveshare_lite': (['BOARD_RADAR_LITE'], False),
    'm5dial': (['BOARD_M5DIAL', 'BOARD_RADAR_LITE'], False), 'watcher': (['BOARD_WATCHER'], False),
}
probe = work / 'mic_probe.cpp'
probe.write_text('#include "board_pins.h"\nint mic=BOARD_HAS_MIC;\nint sd=BOARD_HAS_SD;\n', encoding='utf-8')
for board, (flags, mic) in boards.items():
    out = subprocess.check_output(compiler + ['-E', '-P', '-I' + str(root / 'firmware/include')] + ['-D' + f for f in flags] + [str(probe)], text=True)
    values = dict(re.findall(r'int (\w+)=(\d+);', out))
    assert values['mic'] == ('1' if mic else '0'), (board, values)
    assert not mic or values['sd'] == '1', (board, 'the recorder saves to the card')
    print('PASS microphone gate', board, 'mic' if mic else 'none')

# The card task patches the sizes where wav::header puts them, and the whole
# recorder sits behind BOARD_HAS_MIC.
recorder = (root / 'firmware/src/voice_recorder.cpp').read_text(encoding='utf-8')
assert 'recFile.seek(4)' in recorder and 'recFile.seek(40)' in recorder
assert recorder.index('#if BOARD_HAS_MIC') < recorder.index('namespace {') and recorder.rstrip().endswith('#endif')
assert 'sdMutex' in recorder and 'cardLock(' in recorder
ui = (root / 'firmware/src/voice_ui.inc').read_text(encoding='utf-8')
assert ui.index('#if BOARD_HAS_MIC') < ui.index('static ') and ui.rstrip().endswith('#endif')
print('PASS recorder wiring')
