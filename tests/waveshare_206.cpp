#include "board_pins.h"
#include "wearable.h"
#include "waveshare_206_touch.h"
#include <cassert>
#include <vector>
#include "rectangular_layout.h"

int main(){
  static_assert(SCREEN_WIDTH==410&&SCREEN_HEIGHT==502);
  static_assert(LCD_SCLK==11&&LCD_RESET==8&&LCD_COL_OFFSET==22);
  static_assert(TP_I2C_ADDR==0x38&&TP_RESET==9&&TP_INT==38);
  static_assert(BOARD_HAS_AUDIO&&BOARD_HAS_SD&&BOARD_SD_SPI);
  static_assert(AUDIO_I2S_MCK_IO==16&&AUDIO_I2S_BCK_IO==41&&AUDIO_I2S_DO_IO==40);
  // Every logical pixel must reach a distinct native pixel in each orientation.
  for(unsigned rotation=0;rotation<4;++rotation){
    const int width=(rotation&1)?502:410,height=(rotation&1)?410:502;
    std::vector<bool> visited(410*502);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
      auto p=dragon::rotateRectPoint(x,y,410,502,rotation);
      assert(p.x>=0&&p.x<410&&p.y>=0&&p.y<502);
      assert(!visited[p.y*410+p.x]);visited[p.y*410+p.x]=true;
    }
    for(bool pixel:visited)assert(pixel);
  }
  for(auto dims:{dragon::PanelPoint{410,502},dragon::PanelPoint{502,410}}){
    auto field=dragon::rectangularRadarField(dims.x,dims.y);
    for(int radius=0;radius<=180;++radius)for(int angle=0;angle<360;++angle){
      const float a=angle*0.0174532925f;
      auto p=dragon::rectangularRadarPoint(radius,std::cos(a),std::sin(a),dims.x,dims.y);
      assert(p.x>=field.x&&p.x<field.x+field.w&&p.y>=field.y&&p.y<field.y+field.h);
    }
    for(unsigned slot=0;slot<6;++slot){auto b=dragon::rectangularRadarLabel(slot,dims.x,dims.y);
      assert(b.x>=field.x&&b.y>=field.y&&b.x+b.w<=field.x+field.w&&b.y+b.h<=field.y+field.h);
      for(unsigned other=slot+1;other<6;++other){auto c=dragon::rectangularRadarLabel(other,dims.x,dims.y);assert(b.x+b.w<=c.x||c.x+c.w<=b.x||b.y+b.h<=c.y||c.y+c.h<=b.y);}
    }
    for(int minute=0;minute<60;++minute){auto p=dragon::rectangularRay(minute/60.f,dims.x/2,dims.y/2,dims.x/2-24,dims.y/2-80);assert(p.x>=24&&p.x<=dims.x-24&&p.y>=80&&p.y<=dims.y-80);}
  }
  int x=0,y=0;
  uint8_t packet[]={1,0x81,0x99,1,0xF5}; // contact at bottom-right
  assert(dragon::ft3168Point(packet,5,x,y)&&x==409&&y==501);
  packet[1]=0x41;assert(!dragon::ft3168Point(packet,5,x,y)); // release
  packet[1]=0xC1;assert(!dragon::ft3168Point(packet,5,x,y)); // reserved event
  packet[1]=0x81;packet[2]=0x9A;assert(!dragon::ft3168Point(packet,5,x,y));
  packet[2]=0x99;packet[4]=0xF6;assert(!dragon::ft3168Point(packet,5,x,y));
  packet[4]=0xF5;packet[0]=0;assert(!dragon::ft3168Point(packet,5,x,y));
  packet[0]=3;assert(!dragon::ft3168Point(packet,5,x,y));
  assert(!dragon::ft3168Point(packet,4,x,y)&&!dragon::ft3168Point(nullptr,5,x,y));
}
