#pragma once
#include "rotary_input.h"
static void rotaryInputTests(){
  RotaryDecoder encoder;encoder.reset(0);
  for(int cycle=0;cycle<20;++cycle){
    assert(encoder.update(2)==0);assert(encoder.update(3)==0);
    assert(encoder.update(1)==0);assert(encoder.update(0)==1);
  }
  for(int cycle=0;cycle<20;++cycle){
    assert(encoder.update(1)==0);assert(encoder.update(3)==0);
    assert(encoder.update(2)==0);assert(encoder.update(0)==-1);
  }
  encoder.reset(0);
  for(int bounce=0;bounce<10;++bounce){assert(encoder.update(2)==0);assert(encoder.update(0)==0);}
  assert(encoder.update(2)==0);assert(encoder.update(3)==0);
  assert(encoder.update(1)==0);assert(encoder.update(0)==1);
  encoder.reset(0);assert(encoder.update(2)==0);assert(encoder.update(1)==0); // missed edge
  assert(encoder.update(0)==0);assert(encoder.update(2)==0);
  assert(encoder.update(3)==0);assert(encoder.update(1)==1);
  RotaryButton button;using Event=RotaryPress;
  assert(button.update(false,0)==Event::None);
  assert(button.update(true,10)==Event::None);
  assert(button.update(false,20)==Event::None); // bounce does not click
  assert(button.update(true,30)==Event::None);
  assert(button.update(true,70)==Event::None);
  assert(button.update(false,100)==Event::None);
  assert(button.update(false,140)==Event::Click);
  assert(button.update(false,200)==Event::None);
  assert(button.update(true,250)==Event::None);
  assert(button.update(true,290)==Event::None);
  assert(button.update(true,1089)==Event::None);
  assert(button.update(true,1090)==Event::Back);
  assert(button.update(true,2000)==Event::None);
  assert(button.update(false,2010)==Event::None);
  assert(button.update(false,2050)==Event::None); // held release never selects
  RotaryButton wrap;wrap.update(false,0xfffffff0u);wrap.update(true,0xfffffff5u);
  assert(wrap.update(true,40)==Event::None);wrap.update(false,50);
  assert(wrap.update(false,90)==Event::Click);
  RotaryButton bootHeld;bootHeld.update(true,0);bootHeld.update(true,40);
  assert(bootHeld.update(true,1000)==Event::None);bootHeld.update(false,1100);
  assert(bootHeld.update(false,1140)==Event::None);
  RotaryNavigation navigation;
  navigation.push(12,0);assert(navigation.take(0)==1);
  for(uint32_t t=10;t<180;t+=10){
    navigation.push(48,t);assert(navigation.take(t)==0);
  }
  assert(navigation.take(180)==1);assert(navigation.take(400)==0);
  navigation.push(48,410);navigation.push(-48,420);
  assert(navigation.take(420)==-1); // most recent direction wins, no backlog
  navigation.push(1,450);navigation.clear();assert(navigation.take(700)==0);
  navigation.push(1,710);assert(navigation.take(911)==0); // stale input expires
  RotaryNavigation watcherNavigation(800,80);
  watcherNavigation.push(1,100);assert(watcherNavigation.take(350)==1); // survives a slow display frame
  watcherNavigation.push(-1,360);assert(watcherNavigation.take(400)==0);
  assert(watcherNavigation.take(430)==-1); // Watcher focus repeat interval
  watcherNavigation.push(1,500);assert(watcherNavigation.take(1301)==0); // stale USB backlog discarded
  RotaryNavigation navWrap;navWrap.push(1,0xfffffff0u);
  assert(navWrap.take(0xfffffff0u)==1);navWrap.push(-1,40);
  assert(navWrap.take(40)==0);assert(navWrap.take(164)==-1);
  puts("PASS: rotary detents, reversal, bounce, missed edges, short/held presses, rollover and boot-held button");
  puts("PASS: rotary navigation burst limiting, direction changes, cancel, expiry and rollover");
}
