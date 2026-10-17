#include "dino_runner.h"
static void dinoRunnerTests(){
  for(uint32_t t=0;t<6000;++t){auto a=dragon::dinoRunner(t);assert(a.lift>=0&&a.lift<=56);assert(a.cactusX>=0&&a.cactusX<=340);assert(a.leg<2);
    // Full horizontal overlap, including tail and both feet, clears the tallest cactus.
    if(a.cactusX+18>=99&&a.cactusX<=147)assert(a.lift>25);
  }
  assert(dragon::dinoRunner(3900).lift==56);assert(dragon::dinoRunner(5000).lift==0);
  for(uint32_t t:{0u,5999u,6000u,0xfffffff0u,0xffffffffu}){auto a=dragon::dinoRunner(t);assert(a.lift>=0&&a.lift<=56);}
}
