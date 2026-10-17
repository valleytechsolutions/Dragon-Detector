#pragma once
#include <stdint.h>
namespace dragon {
// Pure monotonic animation. Never reads or changes the wall clock or RF state.
struct DinoRunner {int cactusX,lift;unsigned leg;};
inline DinoRunner dinoRunner(uint32_t now){
  unsigned t=now%6000,p=t>=2800&&t<5000?t-2800:0;
  return {340-int(t*340/6000),int(224*p*(2200-p)/4840000),unsigned(now/130%2)};
}
}
