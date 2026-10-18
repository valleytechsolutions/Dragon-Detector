#include "app.h"
#include "personal_theme.h"
#include "background_radio.h"
#include "recon.h"
#include "ble_lab.h"
#include "flash_budget.h"
#include <LittleFS.h>

namespace {
constexpr const char* Active="/personal/active.ddtheme";
constexpr const char* Previous="/personal/previous.ddtheme";
constexpr const char* Staging="/personal/upload.part";
bool storage=false,loaded=false;
dragon::PersonalTheme config;
dragon::UiPalette palette=dragon::UiPalettes[1];
lv_img_dsc_t logos[2]{};
uint32_t fileCrc=0;
uint32_t blend(uint32_t a,uint32_t b,unsigned weight){uint32_t v=0;for(int s:{16,8,0})v|=((((a>>s)&255)*(100-weight)+((b>>s)&255)*weight)/100)<<s;return v;}
uint32_t contrast(uint32_t c){return (((c>>16)&255)*299+((c>>8)&255)*587+(c&255)*114)>150000?0x000000:0xffffff;}
struct Prepared {
  dragon::PersonalTheme theme;
  lv_img_dsc_t images[2]{};
  uint32_t crc=0;
  ~Prepared(){for(auto& image:images)free(const_cast<uint8_t*>(image.data));}
};
bool prepare(const char* path,Prepared& next){
  File f=LittleFS.open(path);if(!f||f.size()!=dragon::ThemeFileBytes)return false;
  auto* bytes=static_cast<uint8_t*>(ps_malloc(dragon::ThemeFileBytes));if(!bytes)return false;
  bool ok=f.read(bytes,dragon::ThemeFileBytes)==dragon::ThemeFileBytes;f.close();
  if(ok)ok=dragon::themeHeader(bytes,dragon::ThemeHeaderBytes,next.theme)&&
    (dragon::crc32(bytes,dragon::ThemeFileBytes-4)^0xffffffffu)==dragon::themeU32(bytes+dragon::ThemeFileBytes-4);
  if(ok){
    next.crc=dragon::themeU32(bytes+dragon::ThemeFileBytes-4);
    for(unsigned index=0;index<2;++index){
      const unsigned design=index?72+next.theme.logoSize*18:42;
      const unsigned side=(design*SCREEN_RES+180)/360;
      auto& image=next.images[index];auto* pixels=static_cast<uint8_t*>(ps_malloc(side*side*3));
      if(!pixels){ok=false;break;}
      image.header.cf=LV_IMG_CF_TRUE_COLOR_ALPHA;image.header.w=side;image.header.h=side;image.data_size=side*side*3;image.data=pixels;
      for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){
        const auto* src=bytes+dragon::ThemeHeaderBytes+((y*160/side)*160+x*160/side)*4;
        const lv_color_t color=lv_color_hex(dragon::themeRgb(src));auto* dst=pixels+(y*side+x)*3;
        memcpy(dst,&color,2);dst[2]=src[3];
      }
    }
  }
  free(bytes);return ok;
}
void install(Prepared& next,bool invalidate){
  config=next.theme;fileCrc=next.crc;
  palette={config.bg,blend(config.bg,config.ink,8),blend(config.bg,config.ink,16),config.ink,blend(config.bg,config.ink,65),config.accent,blend(config.bg,config.ink,35),contrast(config.accent),contrast(config.bg)==0};
  for(unsigned i=0;i<2;++i){if(invalidate)lv_img_cache_invalidate_src(&logos[i]);free(const_cast<uint8_t*>(logos[i].data));logos[i]=next.images[i];next.images[i].data=nullptr;}
  loaded=true;
}
}
bool personalThemeReady(){return loaded;}
const dragon::PersonalTheme& personalTheme(){return config;}
const dragon::UiPalette& themeUiColors(unsigned i){return i==dragon::PersonalThemeId&&loaded?palette:dragon::UiPalettes[i<dragon::UiPaletteCount?i:1];}
const lv_img_dsc_t* personalThemeLogo(bool clock){return loaded?&logos[clock?1:0]:nullptr;}
void personalThemeBegin(){
  storage=LittleFS.begin(false,"/littlefs",8,"spiffs");if(!storage)return;
  LittleFS.mkdir("/personal");Prepared next;
  if(prepare(Active,next))install(next,false);
  else {Prepared recovery;if(prepare(Previous,recovery)){LittleFS.remove(Active);if(LittleFS.rename(Previous,Active))install(recovery,false);}}
  LittleFS.remove(Staging);
}
bool personalThemeCommand(const String& c){
  if(c=="theme remove"){
    if(stopwatch.running||reconOwnsRadio()||bleLabOwnsRadio()||reconStorageBusy()||audioStatus().mode==AudioMode::Recording||audioStatus().mode==AudioMode::Playing){Serial.println("THEME_ERROR stop timing, audio and radio sessions first");return true;}
    bool ok=storage;
    for(const char* path:{Previous,Staging,Active})if(ok&&LittleFS.exists(path))ok=LittleFS.remove(path);
    if(ok){uiPersonalThemeRemoved();for(auto& image:logos){lv_img_cache_invalidate_src(&image);free(const_cast<uint8_t*>(image.data));image={};}loaded=false;fileCrc=0;Serial.println("THEME_REMOVED");}
    else Serial.println("THEME_ERROR storage");return true;
  }
  if(c=="theme info"){Serial.printf("THEME_INFO format=1 saved=%u bytes=%u crc=%08lx storage=%u\n",loaded,loaded?unsigned(dragon::ThemeFileBytes):0,fileCrc,storage);return true;}
  if(c=="theme apply"){
    if(!loaded)Serial.println("THEME_ERROR upload a theme first");
    else{uiPersonalThemeChanged();Serial.println("THEME_APPLIED");}return true;
  }
  if(c.startsWith("theme read ")){
    unsigned offset=0,count=0;char extra;
    if(!loaded||sscanf(c.c_str(),"theme read %u %u %c",&offset,&count,&extra)!=2||!count||count>512||offset>dragon::ThemeFileBytes||count>dragon::ThemeFileBytes-offset){Serial.println("THEME_ERROR range");return true;}
    File f=LittleFS.open(Active);uint8_t bytes[512];bool ok=f&&f.seek(offset)&&f.read(bytes,count)==count;f.close();
    if(!ok){Serial.println("THEME_ERROR read");return true;}
    char reply[1060];const size_t head=snprintf(reply,sizeof(reply),"THEME_DATA %u ",offset);constexpr char hex[]="0123456789abcdef";
    for(unsigned i=0;i<count;++i){reply[head+i*2]=hex[bytes[i]>>4];reply[head+i*2+1]=hex[bytes[i]&15];}
    reply[head+count*2]='\n';reply[head+count*2+1]='\n';Serial.write(reinterpret_cast<const uint8_t*>(reply),head+count*2+2);return true;
  }
  if(!c.startsWith("theme upload "))return false;
  unsigned long length=0,checksum=0;char extra;
  if(sscanf(c.c_str(),"theme upload %lu %lx %c",&length,&checksum,&extra)!=2||length!=dragon::ThemeFileBytes){Serial.println("THEME_ERROR format or length");return true;}
  if(stopwatch.running||reconOwnsRadio()||bleLabOwnsRadio()||reconStorageBusy()||audioStatus().mode==AudioMode::Recording||audioStatus().mode==AudioMode::Playing){Serial.println("THEME_ERROR stop timing, audio and radio sessions first");return true;}
  if(!storage||!dragon::storageFits(LittleFS.totalBytes(),LittleFS.usedBytes(),length+16384)){Serial.println("THEME_ERROR storage unavailable or full");return true;}
  LittleFS.remove(Staging);File f=LittleFS.open(Staging,FILE_WRITE);if(!f){Serial.println("THEME_ERROR open");return true;}
  RadioTransfer transfer;Serial.println("THEME_READY");
  uint8_t buffer[1024];size_t received=0;uint32_t crc=0xffffffffu,last=millis(),ack=last;bool ok=true;
  while(received<length&&uint32_t(millis()-last)<5000){
    const size_t available=Serial.available();if(!available){
      if(uint32_t(millis()-ack)>=250){if(received)Serial.printf("\nTHEME_ACK %u\n\n",unsigned(received));else Serial.println("\nTHEME_READY\n");ack=millis();}
      radioTransferTick();delay(1);continue;
    }
    size_t n=Serial.readBytes(buffer,std::min(sizeof(buffer),std::min(available,size_t(length-received))));
    crc=dragon::crc32(buffer,n,crc);if(f.write(buffer,n)!=n){ok=false;break;}
    received+=n;last=ack=millis();Serial.printf("THEME_ACK %u\n\n",unsigned(received));radioTransferTick();delay(1);
  }
  f.flush();f.close();Prepared next;
  ok=ok&&received==length&&(crc^0xffffffffu)==checksum&&prepare(Staging,next);
  if(ok){
    const bool had=LittleFS.exists(Active);
    if(LittleFS.exists(Previous))ok=LittleFS.remove(Previous);
    if(ok&&had)ok=LittleFS.rename(Active,Previous);
    if(ok)ok=LittleFS.rename(Staging,Active);
    if(!ok&&had&&!LittleFS.exists(Active))LittleFS.rename(Previous,Active);
  }
  if(ok){install(next,true);uiPersonalThemeChanged();}
  else LittleFS.remove(Staging);
  Serial.println(ok?"\nTHEME_OK\n":"\nTHEME_ERROR transfer, format or checksum\n");return true;
}
