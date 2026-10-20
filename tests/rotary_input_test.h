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
  puts("PASS: rotary detents, reversal, bounce, missed edges, short/held presses, rollover and boot-held button");
}
