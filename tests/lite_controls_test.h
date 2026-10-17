#include "lite_button.h"
#include "clock_sync.h"
static void liteControlsTests(){
 using E=dragon::LitePress;
 dragon::LiteButton b;
 assert(b.update(true,0)==E::None);assert(b.update(true,9000)==E::None);
 assert(b.update(false,9010)==E::None);assert(b.update(false,9045)==E::None);
 assert(b.update(true,9100)==E::None);assert(b.update(false,9110)==E::None); // bounce
 assert(b.update(false,9450)==E::None);
 assert(b.update(true,9500)==E::None);assert(b.update(true,9535)==E::None);
 assert(b.update(false,9600)==E::None);assert(b.update(false,9635)==E::None);
 assert(b.update(false,9934)==E::None);assert(b.update(false,9935)==E::Tap);
 assert(b.update(true,10000)==E::None);assert(b.update(true,10035)==E::None);
 assert(b.update(false,10100)==E::None);assert(b.update(false,10135)==E::None);
 assert(b.update(true,10200)==E::None);assert(b.update(true,10235)==E::None);
 assert(b.update(false,10300)==E::None);assert(b.update(false,10335)==E::Double);
 assert(b.update(false,11000)==E::None); // no stray tap after double
 assert(b.update(true,11100)==E::None);assert(b.update(true,11135)==E::None);
 assert(b.update(true,11934)==E::None);assert(b.update(true,11935)==E::Hold);
 assert(b.update(true,15000)==E::None);assert(b.update(false,15100)==E::None);
 assert(b.update(false,15135)==E::None);assert(b.update(false,15500)==E::None);
 dragon::LiteButton wrap;
 wrap.update(false,0xfffffe00);wrap.update(false,0xfffffe23);
 wrap.update(true,0xffffffc0);wrap.update(true,0xffffffe3);
 wrap.update(false,0xfffffff0);assert(wrap.update(false,19)==E::None);
 assert(wrap.update(false,318)==E::None);assert(wrap.update(false,319)==E::Tap);
 dragon::ClockSyncSchedule schedule;assert(!schedule.due(2999));assert(schedule.due(3000));
 dragon::LiteButton delayed;
 delayed.update(false,0);delayed.update(false,35);
 delayed.update(true,100);delayed.update(true,135);
 delayed.update(false,200);delayed.update(false,235);
 delayed.update(true,450);delayed.update(true,485);
 assert(delayed.update(true,600)==E::None); // second tap began within window
 delayed.update(false,700);assert(delayed.update(false,735)==E::Double);
 assert(delayed.update(false,1100)==E::None);
 schedule.finished(0xffffff00u,false);assert(!schedule.due(0xffffff01u));assert(!schedule.due(899743));assert(schedule.due(899744));
 schedule.finished(100,true);assert(!schedule.due(21600099));assert(schedule.due(21600100));
 puts("PASS: Lite debounce, startup hold, tap/double/hold exclusivity, rollover and clock retry intervals");
}
