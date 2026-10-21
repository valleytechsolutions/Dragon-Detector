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
static bool playing=false;
static uint32_t noteUntil=0,huntAt=0;
void audioSave(){soundPrefs.putBool("alerts",alertSound);soundPrefs.putBool("hunt",huntSound);}
void audioBegin(){soundPrefs.begin("dial-sound",false);alertSound=soundPrefs.getBool("alerts",true);huntSound=soundPrefs.getBool("hunt",true);soundReady=ledcAttach(DIAL_BUZZER,4000,8);}
void audioStop(){toneQueue.clear();playing=false;noteIndex=melody.count=0;if(soundReady)ledcWriteTone(DIAL_BUZZER,0);}
bool audioReady(){return soundReady;}
void audioAlert(Family family){if(alertSound&&!detector.quiet&&soundReady)toneQueue.request(unsigned(family),millis());}
void audioBeep(){audioAlert(Family::Axon);}
void audioLoop(){
  if(!soundReady)return;const uint32_t now=millis();
  if(playing&&int32_t(now-noteUntil)<0)return;
  if(playing&&noteIndex<melody.count){auto n=melody.notes[noteIndex++];ledcWriteTone(DIAL_BUZZER,n.hz);noteUntil=now+n.ms;return;}
  if(playing){playing=false;ledcWriteTone(DIAL_BUZZER,0);}
  if(alertSound){int family=toneQueue.take(now);if(family>=0&&dragon::parseTone(dragon::defaultTones[family],melody)){playing=true;noteIndex=0;noteUntil=now;return;}}
  else toneQueue.clear();
  if(huntSound&&trackedContact>=0&&size_t(trackedContact)<MAX_CONTACTS){
    const auto& c=contacts[trackedContact];if(c.used&&uint32_t(now-c.seen)<3000&&uint32_t(now-huntAt)>=dragon::huntInterval(c.rssi)){
      huntAt=now;melody.count=0;noteIndex=0;playing=true;noteUntil=now+55;ledcWriteTone(DIAL_BUZZER,dragon::huntPitch(c.rssi));
    }
  }
}
AppAudioStatus audioStatus(){AppAudioStatus s;s.volume=alertSound?50:0;s.mode=playing?AudioMode::Playing:AudioMode::Idle;strlcpy(s.message,soundReady?(alertSound?"Dial buzzer / alerts on":"Dial buzzer / alerts muted"):"Dial buzzer unavailable",sizeof(s.message));return s;}
void audioVolume(int value){alertSound=value>0;audioSave();if(!alertSound)audioStop();}
void audioPlay(const String&){} void audioRecord(){}
bool audioCommand(const String& c){
  if(c=="dial sound on"||c=="dial sound off"){alertSound=c.endsWith(" on");audioSave();if(!alertSound)audioStop();Serial.println("DIAL_OK");return true;}
  if(c=="dial sound status"){Serial.printf("DIAL_SOUND ready=%u alerts=%u hunt=%u\n",soundReady,alertSound,huntSound);return true;}
  return false;
}
#endif
