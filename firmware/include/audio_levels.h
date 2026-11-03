#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <initializer_list>

namespace dragon {
enum class AudioLevel:uint8_t {Silent,Medium,High};
inline AudioLevel audioLevelFor(bool enabled,unsigned volume){return !enabled||!volume?AudioLevel::Silent:volume<=50?AudioLevel::Medium:AudioLevel::High;}
inline unsigned audioLevelVolume(AudioLevel level){return level==AudioLevel::High?100:level==AudioLevel::Medium?50:0;}
inline const char* audioLevelName(AudioLevel level){return level==AudioLevel::High?"high":level==AudioLevel::Medium?"medium":"silent";}
inline const char* audioLevelLabel(AudioLevel level){return level==AudioLevel::High?"High":level==AudioLevel::Medium?"Medium":"Silent";}
inline AudioLevel nextAudioLevel(AudioLevel level){return level==AudioLevel::Silent?AudioLevel::Medium:level==AudioLevel::Medium?AudioLevel::High:AudioLevel::Silent;}
inline bool parseAudioLevel(const char* text,AudioLevel& out){
 if(!text)return false;
 for(auto level:{AudioLevel::Silent,AudioLevel::Medium,AudioLevel::High})if(!strcmp(text,audioLevelName(level))){out=level;return true;}
 return false;
}
// ledcWriteTone uses 10-bit PWM and duty 511. Shape the fundamental amplitude
// below that existing maximum: 50% gain is about 171/1024 duty, 100% is 511.
inline uint16_t audioPiezoDuty(unsigned volume){
 if(!volume)return 0;if(volume>=100)return 511;
 return uint16_t(lroundf(asinf(volume/100.f)*1024.f/3.14159265359f));
}
}
