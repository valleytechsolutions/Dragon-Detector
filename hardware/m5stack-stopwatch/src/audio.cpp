#include "app.h"
#include <M5Unified.h>
// Speaker feedback only; no media decoder or recording task.
void audioBegin(){M5.Mic.end();M5.Speaker.setVolume(90);}
void audioBeep(){if(M5.Speaker.begin())M5.Speaker.tone(1200,70);}
