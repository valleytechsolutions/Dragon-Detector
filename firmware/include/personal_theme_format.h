#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dragon {
inline constexpr unsigned PersonalThemeId=34,PersonalWatchId=37;
inline constexpr size_t ThemeLogoSide=160,ThemeHeaderBytes=64;
inline constexpr size_t ThemePixelsBytes=ThemeLogoSide*ThemeLogoSide*4;
inline constexpr size_t ThemeFileBytes=ThemeHeaderBytes+ThemePixelsBytes+4;
struct PersonalTheme {
  uint8_t layout=1,hands=0,font=0,markers=0,flags=7,logoSize=1,sweep=0;
  uint32_t bg=0x080d16,ink=0xf3f6fb,accent=0x68e2c1,secondary=0x72aaff;
  char name[24]="My theme";
};
inline uint32_t themeU32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint32_t themeRgb(const uint8_t* p){return uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];}
inline bool themeHeader(const uint8_t* h,size_t size,PersonalTheme& out){
  if(size<ThemeHeaderBytes||memcmp(h,"DDT1",4)||h[4]!=1||h[5]>2||h[6]>3||h[7]>2||h[8]>3||(h[9]&0xf0)||h[10]>2||h[11]>2)return false;
  if(h[48]!=160||h[49]||h[50]!=160||h[51]||themeU32(h+52)!=ThemePixelsBytes)return false;
  for(unsigned i=56;i<64;++i)if(h[i])return false;
  bool end=false,visible=false;for(unsigned i=24;i<48;++i){if(h[i]==0)end=true;else if(end||h[i]<32||h[i]>126)return false;else if(h[i]!=' ')visible=true;}
  if(!end||!visible)return false;
  out.layout=h[5];out.hands=h[6];out.font=h[7];out.markers=h[8];out.flags=h[9];out.logoSize=h[10];out.sweep=h[11];
  out.bg=themeRgb(h+12);out.ink=themeRgb(h+15);out.accent=themeRgb(h+18);out.secondary=themeRgb(h+21);memcpy(out.name,h+24,24);return true;
}
}
