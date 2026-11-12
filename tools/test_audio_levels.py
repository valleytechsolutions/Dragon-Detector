"""Exercise production PCM/Dial audio, saved legacy gains and true Silent behavior."""
from pathlib import Path
import argparse
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work/audio-level-tests'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']

(work / 'Arduino.h').write_text(r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <cassert>
class String:public std::string {public:using std::string::string;String(const std::string& s):std::string(s){}String substring(size_t n)const{return substr(n);}bool startsWith(const char* s)const{return rfind(s,0)==0;}bool endsWith(const char* s)const{size_t n=strlen(s);return size()>=n&&compare(size()-n,n,s)==0;}};
template<class T,class A,class B>T constrain(T value,A a,B b){return std::clamp(value,T(a),T(b));}
inline size_t testStrlcpy(char* out,const char* in,size_t size){size_t n=strlen(in);if(size){memcpy(out,in,std::min(n,size-1));out[std::min(n,size-1)]=0;}return n;}
#define strlcpy testStrlcpy
inline uint32_t now=10000;
inline uint32_t millis(){return now;}
struct SerialStub {std::string last;void println(const char* s){last=s;}template<class... A>void printf(const char* f,A... a){char s[512];snprintf(s,sizeof(s),f,a...);last=s;}};
inline SerialStub Serial;
inline bool hardwareReady=true;
inline unsigned outputHz=0,outputGain=0,audibleWrites=0,attachedChannel=99,attachedBits=0;
inline bool ledcAttachChannel(unsigned,unsigned,unsigned bits,unsigned channel){attachedBits=bits;attachedChannel=channel;return hardwareReady;}
inline uint32_t ledcWriteTone(unsigned,uint32_t hz){outputHz=hz;outputGain=hz?511:0;if(hz)++audibleWrites;return hz;}
inline bool ledcWrite(unsigned,unsigned duty){assert(outputHz&&duty&&duty<=511);outputGain=duty;return true;}
''')
(work / 'Preferences.h').write_text(r'''
#pragma once
#include "Arduino.h"
inline std::map<std::string,std::map<std::string,long long>> nvs;
inline std::map<std::string,std::map<std::string,String>> texts;
struct Preferences {
 std::string ns;
 bool begin(const char* name,bool){ns=name;return true;}
 long long read(const char* key,long long fallback){auto it=nvs[ns].find(key);return it==nvs[ns].end()?fallback:it->second;}
 uint8_t getUChar(const char* key,uint8_t fallback){return read(key,fallback);}
 bool getBool(const char* key,bool fallback){return read(key,fallback);}
 int getInt(const char* key,int fallback){return read(key,fallback);}
 void putUChar(const char* key,uint8_t value){nvs[ns][key]=value;}void putBool(const char* key,bool value){nvs[ns][key]=value;}void putInt(const char* key,int value){nvs[ns][key]=value;}
 bool isKey(const char* key){return nvs[ns].count(key)||texts[ns].count(key);}
 String getString(const char* key,const char* fallback){auto it=texts[ns].find(key);return it==texts[ns].end()?String(fallback):it->second;}
 bool remove(const char* key){return nvs[ns].erase(key)+texts[ns].erase(key)>0;}
 size_t putString(const char* key,const String& value){texts[ns][key]=value;return value.length();}
};
''')
(work / 'recon.h').write_text('#pragma once\nnamespace dragon {enum class ReconMode{Off,Axon,Alpr,Sky};}\ninline dragon::ReconMode reconMode(){return dragon::ReconMode::Off;}\n')
(work / 'app.h').write_text(r'''
#pragma once
#include "Arduino.h"
#include "Preferences.h"
#include "contacts.h"
#include "audio_levels.h"
using dragon::Family;
inline Contact entries[MAX_CONTACTS];inline ContactStore contacts{entries};
inline dragon::DetectorSettings detector;
inline bool axonPriority=true,radioEnabled=true;
inline int trackedContact=-1;
extern bool alertSound,alertVibration,huntSound;extern int audioFocus;
inline dragon::DeviceKey contactKey(const Contact& c){return keyOf(c);}
enum {RuleIgnore=1,RuleWatch=2,RuleSnooze=4};
inline uint8_t deviceRules(const dragon::DeviceKey&){return 0;}
inline void detectorSave(){nvs["dd-detect"]["sound"]=alertSound;}
enum class AudioMode:uint8_t {Idle,Playing,Recording,Error};
struct AppAudioStatus {AudioMode mode=AudioMode::Idle;char message[100]{};uint32_t seconds=0;uint16_t peak=0;uint8_t volume=50;};
void audioStop();void audioSave();dragon::AudioLevel audioLevel();void audioSetLevel(dragon::AudioLevel);
#define DIAL_BUZZER 3
''')

common = r'''
using dragon::AudioLevel;
void tick(unsigned step=10){now+=step;audioLoop();}
void boot(){audioBegin();
#ifndef BOARD_M5DIAL
 alertSound=nvs["dd-detect"].count("sound")?bool(nvs["dd-detect"]["sound"]):true;
#endif
}
void freshTarget(){trackedContact=0;auto& c=contacts[0];c={};c.used=true;c.seen=now;c.rssi=-45;c.signal.count=1;c.signal.smooth=-45;c.match.family=Family::Axon;}
void previewTone(){assert(audioCommand("tone play 3"));tick();tick();}
int main(){
 for(unsigned gain=0;gain<=100;++gain){auto level=dragon::audioLevelFor(true,gain);assert(level==(gain==0?AudioLevel::Silent:gain<=50?AudioLevel::Medium:AudioLevel::High));assert(dragon::audioLevelFor(false,gain)==AudioLevel::Silent);assert(dragon::audioPiezoDuty(gain)<=511);if(gain)assert(dragon::audioPiezoDuty(gain)>=dragon::audioPiezoDuty(gain-1));}
 assert(dragon::audioPiezoDuty(0)==0&&dragon::audioPiezoDuty(50)==171&&dragon::audioPiezoDuty(100)==511);
 assert(dragon::nextAudioLevel(AudioLevel::Silent)==AudioLevel::Medium&&dragon::nextAudioLevel(AudioLevel::Medium)==AudioLevel::High&&dragon::nextAudioLevel(AudioLevel::High)==AudioLevel::Silent);
#ifdef BOARD_M5DIAL
 const char* ns="dial-sound";const char* enabledKey="alerts";
#else
 const char* ns="dd-tones";const char* enabledKey="sound";
#endif
 nvs[ns]["volume"]=17;nvs[ns]["hunt"]=0;nvs[ns]["focus"]=3;nvs[ns]["keep"]=73;
#ifdef BOARD_M5DIAL
 nvs[ns][enabledKey]=1;
#else
 nvs["dd-detect"][enabledKey]=1;
#endif
 boot();assert(audioStatus().volume==17&&!huntSound&&audioFocus==3&&nvs[ns]["keep"]==73&&nvs[ns]["volume"]==17);
#ifdef NO_SPEAKER
 assert(!audioReady()&&audioLevel()==AudioLevel::Silent);
 assert(audioCommand("tone level high")&&Serial.last=="TONE_ERROR output");assert(audioStatus().volume==17);
 audioBeep();audioAlert(Family::Axon);freshTarget();huntSound=true;for(int i=0;i<5;++i)tick(500);
 assert(!audibleWrites);assert(audioCommand("tone play 3")&&Serial.last=="TONE_ERROR output");
 audioCommand("tone settings");assert(Serial.last.find("ready=0")!=std::string::npos&&Serial.last.find("level=silent")!=std::string::npos);
 puts("PASS no-speaker: no false output/readiness, preserved gain, effective Silent");return 0;
#endif
 assert(audioReady()&&audioLevel()==AudioLevel::Medium);
#ifdef BOARD_M5DIAL
 assert(attachedChannel==2&&attachedBits==10);
#endif
 assert(audioCommand("tone settings")&&Serial.last.find("volume=17 hunt=0 focus=3 quiet=0 level=medium")!=std::string::npos);
 previewTone();assert(outputHz&&audibleWrites);
 for(const char* bad:{"tone level medium ","tone level Medium","tone level", "tone level loud"}){unsigned volume=audioStatus().volume;assert(audioCommand(bad)||std::string(bad)=="tone level");assert(audioStatus().volume==volume);}
 audioSetLevel(AudioLevel::Silent);assert(!outputHz&&!alertSound&&audioStatus().volume==0&&!huntSound&&audioFocus==3&&audioLevel()==AudioLevel::Silent);
 unsigned writes=audibleWrites;audioBeep();audioAlert(Family::Axon);previewTone();freshTarget();huntSound=true;for(int i=0;i<6;++i)tick(500);assert(audibleWrites==writes&&!outputHz);
 boot();assert(audioLevel()==AudioLevel::Silent&&audioStatus().volume==0&&!huntSound&&audioFocus==3); // silent persisted the original preferences
 assert(audioCommand("tone level medium")&&Serial.last=="TONE_OK");assert(audioStatus().volume==50&&alertSound&&audioLevel()==AudioLevel::Medium&&!huntSound&&audioFocus==3);
 previewTone();assert(outputHz&&outputGain==MEDIUM_GAIN);audioStop();
 assert(audioCommand("tone level high")&&Serial.last=="TONE_OK");previewTone();assert(outputHz&&outputGain==HIGH_GAIN);
 boot();assert(audioStatus().volume==100&&alertSound&&audioLevel()==AudioLevel::High);
 audioStop();detector.quiet=true;huntSound=true;freshTarget();writes=audibleWrites;audioAlert(Family::Axon);for(int i=0;i<3;++i)tick(500);assert(audibleWrites==writes&&!outputHz);
 previewTone();assert(outputHz); // quiet preserves deliberate previews, unlike Silent
 audioSetLevel(AudioLevel::Silent);assert(!outputHz&&detector.quiet&&huntSound&&audioFocus==3);
 detector.quiet=false;audioSetLevel(AudioLevel::Medium);freshTarget();tick(500);tick();assert(outputHz);audioSetLevel(AudioLevel::Silent);writes=audibleWrites;tick(500);assert(!outputHz&&audibleWrites==writes&&huntSound);
 // Legacy settings stay exact, including an enabled flag with volume zero and a disabled flag with nonzero gain.
 assert(audioCommand("tone config 0 25 1 4")&&Serial.last=="TONE_OK");assert(audioStatus().volume==25&&!alertSound&&huntSound&&audioFocus==4&&audioLevel()==AudioLevel::Silent);
 writes=audibleWrites;previewTone();assert(!outputHz&&audibleWrites==writes);boot();assert(audioStatus().volume==25&&!alertSound&&audioFocus==4);
 assert(audioCommand("tone config 1 0 1 4")&&audioLevel()==AudioLevel::Silent);previewTone();assert(!outputHz&&audibleWrites==writes);
 assert(audioCommand("tone config 1 25 0 2")&&audioStatus().volume==25&&audioLevel()==AudioLevel::Medium);boot();assert(audioStatus().volume==25&&audioFocus==2&&!huntSound);
 assert(audioCommand("tone config 1 75 0 2")&&audioLevel()==AudioLevel::High&&audioStatus().volume==75);boot();assert(audioStatus().volume==75);
 audioVolume(0);assert(!outputHz&&audioLevel()==AudioLevel::Silent);
#ifdef BOARD_M5DIAL
 assert(audioCommand("dial sound on")&&alertSound&&audioStatus().volume==50);previewTone();assert(outputHz);
 assert(audioCommand("dial sound off")&&!outputHz&&!alertSound);writes=audibleWrites;huntSound=true;freshTarget();tick(500);assert(audibleWrites==writes);
#endif
 assert(nvs[ns]["keep"]==73);
 hardwareReady=false;boot();assert(!audioReady()&&audioLevel()==AudioLevel::Silent);writes=audibleWrites;audioBeep();tick();assert(audibleWrites==writes);
 puts("PASS audio: presets, legacy gain persistence, preview/alert/hunt Silent, quiet semantics, USB compatibility, hardware failure and backend output bounds");
}
'''

profiles = [
    ('waveshare185b', ['BOARD_WAVESHARE_185B'], 'audio.cpp'),
    ('waveshare143c', ['BOARD_WAVESHARE_143C'], 'audio.cpp'),
    ('watcher', ['BOARD_WATCHER'], 'audio.cpp'),
    ('amoled175', ['BOARD_AMOLED_175'], 'audio.cpp'),
    ('stopwatch', ['BOARD_STOPWATCH'], 'audio.cpp'),
    ('spotpear', [], 'audio.cpp'),
    ('lilygo_no_speaker', ['BOARD_LILYGO_175', 'NO_SPEAKER'], 'audio.cpp'),
    ('xiao_no_speaker', ['BOARD_XIAO_ROUND', 'NO_SPEAKER'], 'audio.cpp'),
    ('crowpanel_no_speaker', ['BOARD_CROWPANEL_21', 'NO_SPEAKER'], 'audio.cpp'),
    ('dial', ['BOARD_M5DIAL'], 'audio_m5dial.cpp'),
]
for name, defines, filename in profiles:
    cpp = work / (name+'.cpp')
    preamble = '#include "app.h"\n'
    if 'BOARD_STOPWATCH' in defines or 'BOARD_CROWPANEL_21' in defines:
        preamble += 'bool alertSound=true,alertVibration=false;\n'
    if filename == 'audio.cpp':
        preamble += 'bool toneOutputBegin(){return hardwareReady;}void toneOutput(uint16_t hz,uint16_t,uint8_t gain){outputHz=hz;outputGain=gain;if(hz&&gain)++audibleWrites;}\n'
    cpp.write_text(preamble+'#include "'+(root/'firmware/src'/filename).as_posix()+'"\n'+
                   common.replace('MEDIUM_GAIN', '171' if name=='dial' else '50').replace('HIGH_GAIN', '511' if name=='dial' else '100'))
    exe = work / (name+'.exe')
    subprocess.run(compiler+['-std=c++17','-O1','-UNDEBUG','-I'+str(work),'-I'+str(root/'firmware/include'),'-I'+str(root/'lib/opendroneid/src')]+['-D'+d for d in defines]+[str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

# Compile the no-audio Lite production stubs without linking unrelated clock support.
lite = (root/'firmware/src/lite_support.cpp').read_text()
start = lite.index('bool alertSound=false')
end = lite.index('#endif',start)
cpp = work/'lite.cpp'
cpp.write_text('#include "app.h"\n'+lite[start:end]+r'''
int main(){audioBegin();audioSetLevel(dragon::AudioLevel::High);audioVolume(100);audioBeep();audioLoop();assert(!audioReady()&&audioLevel()==dragon::AudioLevel::Silent&&audioStatus().volume==0&&!audibleWrites);puts("PASS Lite no-audio stubs");}
''')
exe = work/'lite.exe'
subprocess.run(compiler+['-std=c++17','-O1','-UNDEBUG','-I'+str(work),'-I'+str(root/'firmware/include'),'-I'+str(root/'lib/opendroneid/src'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)

# Preprocess the actual codec branch for each affected ES8311 board.
output = (root/'firmware/src/tone_output.cpp').read_text()
start = output.index('#ifdef BOARD_WAVESHARE_185B\n // This board needs')
end = output.index('\n bool ok=es8311_init',start)
codec = (root/'lib/es8311/src/es8311.c').read_text()
function_start = codec.index('esp_err_t es8311_voice_volume_set(')
function_end = codec.index('\nesp_err_t ',function_start+1)
set_volume = codec[function_start:function_end]
for board, value, register in [('BOARD_WAVESHARE_185B',75,0xbf),('BOARD_WATCHER',65,0xa5),('BOARD_AMOLED_175',65,0xa5),('BOARD_WAVESHARE_143C',65,0xa5),('BOARD_AMOLED_175C',100,0xff)]:
    cpp = work/'codec.cpp'
    cpp.write_text('#include <cassert>\n#include <cstddef>\nusing esp_err_t=int;using es8311_handle_t=void*;constexpr int ES8311_DAC_REG32=0x32;int written=0;int es8311_write_reg(void*,int address,int value){assert(address==ES8311_DAC_REG32);written=value;return 0;}\n'+
                   set_volume+output[start:end]+f'\nstatic_assert(codecVolume=={value});int main(){{int saved=0;assert(es8311_voice_volume_set(nullptr,codecVolume,&saved)==0);assert(written=={register}&&saved=={value});}}\n')
    exe = work/'codec.exe'
    subprocess.run(compiler+['-std=c++17','-O1','-UNDEBUG','-D'+board,str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS codec branch isolation: 1.85B 75/0xBF, Watcher + AMOLED-1.75 + AMOLED-1.43C 65/0xA5, AMOLED-1.75C unchanged')
