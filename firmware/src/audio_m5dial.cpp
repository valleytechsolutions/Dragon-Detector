// Nonblocking piezo playback. No I2S, files, microphone or PSRAM required.
#ifdef BOARD_M5DIAL
#include "app.h"
#include "tones.h"
#include <Preferences.h>
bool alertSound=true,alertVibration=false,huntSound=true;
int audioFocus=-1;
static Preferences soundPrefs;
static bool soundReady=false;
static dragon::ToneQueue toneQueue;
static dragon::Melody melody;
static unsigned noteIndex=0;
static bool playing=false,preview=false;
static uint8_t soundVolume=100; // Old Dial output was fixed at full duty.
static uint32_t noteUntil=0,huntAt=0;
static void output(uint16_t hz){
 if(!soundReady)return;
 if(!hz||!alertSound||!soundVolume){ledcWriteTone(DIAL_BUZZER,0);return;}
 // Tone resets duty to 511. Apply the chosen gain afterward, on its own timer.
 if(ledcWriteTone(DIAL_BUZZER,hz))ledcWrite(DIAL_BUZZER,dragon::audioPiezoDuty(soundVolume));
}
void audioSave(){if(!alertSound||!soundVolume)audioStop();soundPrefs.putBool("alerts",alertSound);soundPrefs.putUChar("volume",soundVolume);soundPrefs.putBool("hunt",huntSound);soundPrefs.putInt("focus",audioFocus);}
void audioBegin(){
 soundPrefs.begin("dial-sound",false);alertSound=soundPrefs.getBool("alerts",true);huntSound=soundPrefs.getBool("hunt",true);
 soundVolume=constrain(soundPrefs.getUChar("volume",100),0,100);audioFocus=soundPrefs.getInt("focus",-1);if(audioFocus<-1||audioFocus>=int(Family::Count))audioFocus=-1;
 // Backlight uses channel 0/timer 0. Channel 2/timer 1 prevents tone frequency
 // and 10-bit resolution changes from changing the screen's brightness.
 soundReady=ledcAttachChannel(DIAL_BUZZER,4000,10,2);audioStop();
}
void audioStop(){toneQueue.clear();playing=preview=false;noteIndex=melody.count=0;output(0);}
bool audioReady(){return soundReady;}
dragon::AudioLevel audioLevel(){return dragon::audioLevelFor(soundReady&&alertSound,soundVolume);}
void audioSetLevel(dragon::AudioLevel level){unsigned volume=dragon::audioLevelVolume(level);if(volume&&!soundReady)return;audioStop();soundVolume=volume;alertSound=volume!=0;audioSave();}
static bool start(const char* text,bool isPreview){
 dragon::Melody next;if(!soundReady||!alertSound||!soundVolume||!dragon::parseTone(text,next))return false;
 output(0);melody=next;noteIndex=0;noteUntil=millis();playing=next.count>0;preview=isPreview;return true;
}
void audioAlert(Family family){if(alertSound&&soundVolume&&!detector.quiet&&soundReady)toneQueue.request(unsigned(family),millis());}
void audioBeep(){start("Test:d=16,o=6,b=180:c,e,g",true);}
void audioLoop(){
  if(!soundReady||!alertSound||!soundVolume){audioStop();return;}
  const uint32_t now=millis();bool automatic=!detector.quiet&&radioEnabled;
  if(!automatic){toneQueue.clear();if(!preview){audioStop();return;}}
  if(playing&&int32_t(now-noteUntil)<0)return;
  if(playing&&noteIndex<melody.count){auto n=melody.notes[noteIndex++];output(n.hz);noteUntil=now+n.ms;return;}
  if(playing){playing=preview=false;output(0);}
  if(!automatic)return;
  int family=toneQueue.take(now);if(family>=0&&start(dragon::defaultTones[family],false))return;
  if(huntSound&&trackedContact>=0&&size_t(trackedContact)<MAX_CONTACTS){
    const auto& c=contacts[trackedContact];if(c.used&&uint32_t(now-c.seen)<3000&&uint32_t(now-huntAt)>=dragon::huntInterval(c.rssi)){
      huntAt=now;melody.count=0;noteIndex=0;playing=true;preview=false;noteUntil=now+55;output(dragon::huntPitch(c.rssi));
    }
  }
}
AppAudioStatus audioStatus(){AppAudioStatus s;s.volume=soundVolume;s.mode=playing?AudioMode::Playing:AudioMode::Idle;snprintf(s.message,sizeof(s.message),soundReady?"Dial buzzer / %s":"Dial buzzer unavailable",dragon::audioLevelLabel(audioLevel()));return s;}
void audioVolume(int value){soundVolume=constrain(value,0,100);alertSound=soundVolume!=0;audioSave();}
void audioPlay(const String&){} void audioRecord(){}
bool audioCommand(const String& c){
  if(c=="dial sound on"||c=="dial sound off"){alertSound=c.endsWith(" on");if(alertSound&&!soundVolume)soundVolume=50;audioSave();Serial.println("DIAL_OK");return true;}
  if(c=="dial sound status"){Serial.printf("DIAL_SOUND ready=%u alerts=%u hunt=%u volume=%u level=%s\n",soundReady,alertSound,huntSound,soundVolume,dragon::audioLevelName(audioLevel()));return true;}
  if(c=="tone settings"){Serial.printf("TONE_SETTINGS ready=%u enabled=%u volume=%u hunt=%u focus=%d quiet=%u level=%s\n",soundReady,alertSound,soundVolume,huntSound,audioFocus,detector.quiet,dragon::audioLevelName(audioLevel()));return true;}
  if(c=="tone stop"){audioStop();Serial.println("TONE_OK");return true;}
  if(c.startsWith("tone level ")){dragon::AudioLevel level;if(!dragon::parseAudioLevel(c.c_str()+11,level)){Serial.println("TONE_ERROR level");return true;}if(level!=dragon::AudioLevel::Silent&&!soundReady){Serial.println("TONE_ERROR output");return true;}audioSetLevel(level);Serial.println("TONE_OK");return true;}
  if(c.startsWith("tone config ")){unsigned enabled,v,hunt;int focus;char extra;if(sscanf(c.c_str(),"tone config %u %u %u %d %c",&enabled,&v,&hunt,&focus,&extra)==4&&enabled<=1&&v<=100&&hunt<=1&&focus>=-1&&focus<int(Family::Count)){alertSound=enabled;soundVolume=v;huntSound=hunt;audioFocus=focus;audioStop();audioSave();Serial.println("TONE_OK");}else Serial.println("TONE_ERROR settings");return true;}
  if(c.startsWith("tone play ")){unsigned family;char extra;if(sscanf(c.c_str(),"tone play %u %c",&family,&extra)!=1||family>=unsigned(Family::Count))Serial.println("TONE_ERROR family");else Serial.println(start(dragon::defaultTones[family],true)?"TONE_OK":"TONE_ERROR output");return true;}
  return false;
}
#endif
