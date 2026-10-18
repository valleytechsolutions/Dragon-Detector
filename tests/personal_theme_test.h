#include "personal_theme_format.h"
static void personalThemeTests(){
  uint8_t header[64]={'D','D','T','1',1,1,0,0,0,7,1,0};
  memcpy(header+24,"My theme",9);header[48]=header[50]=160;header[53]=0x90;header[54]=1;
  dragon::PersonalTheme out;assert(dragon::themeHeader(header,64,out));assert(!strcmp(out.name,"My theme"));
  for(unsigned n=0;n<64;++n)assert(!dragon::themeHeader(header,n,out));
  {uint8_t blank[64];memcpy(blank,header,64);memset(blank+24,0,24);memset(blank+24,' ',8);assert(!dragon::themeHeader(blank,64,out));}
  const unsigned offsets[]={4,5,6,7,8,9,10,11,24,47,48,49,50,51,52,56,63};
  for(auto offset:offsets){uint8_t bad[64];memcpy(bad,header,64);bad[offset]=255;assert(!dragon::themeHeader(bad,64,out));}
  for(unsigned layout=0;layout<3;++layout)for(unsigned hands=0;hands<4;++hands)for(unsigned font=0;font<3;++font)for(unsigned flags=0;flags<16;++flags){header[5]=layout;header[6]=hands;header[7]=font;header[9]=flags;assert(dragon::themeHeader(header,64,out));assert(out.layout==layout&&out.hands==hands&&out.font==font&&out.flags==flags);}
  uint32_t seed=0x551a;for(unsigned n=0;n<10000;++n){uint8_t fuzz[64];for(auto& byte:fuzz){seed=seed*1664525+1013904223;byte=seed>>24;}dragon::themeHeader(fuzz,n%65,out);}
  puts("PASS: personal theme headers, style bounds, truncated fields and 10000 fuzz inputs");
}
