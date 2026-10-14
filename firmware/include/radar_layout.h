#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>

namespace dragon {
constexpr unsigned RadarPageSize=6;
struct RadarPoint {int x,y;};
// Keep markers clear of the logo/status and bottom controls. Only the
// decorative angle changes; radius still represents exactly the same RSSI.
inline RadarPoint radarPlotPoint(float radius,float unitX,float unitY){
  float y=radius*unitY;if(y<-76)y=-76;if(y>62)y=62;
  float square=radius*radius-y*y;float x=sqrtf(square>0?square:0);if(unitX<0)x=-x;
  return {180+int(lroundf(x)),180+int(lroundf(y))};
}
struct RadarLabelBox {int x,y,w=94,h=32;};
inline RadarLabelBox radarLabelSlot(unsigned slot){return {30+int(slot%3)*103,102+int(slot/3)*40,94,32};}
inline unsigned radarPages(unsigned count){return count?(count+RadarPageSize-1)/RadarPageSize:1;}
// Fixed, non-overlapping slots keep every plotted target labeled on a small
// round display. Prefer short leaders and avoid covering any target marker.
// Coordinates are logical 360px units; the renderer scales both resolutions.
inline void placeRadarLabels(const RadarPoint* points,unsigned count,uint8_t* slots,const uint8_t* preferred=nullptr){
  uint16_t used=0;
  for(unsigned i=0;i<count&&i<RadarPageSize;++i){int best=0;int64_t bestCost=INT64_MAX;
    for(unsigned slot=0;slot<12;++slot){if(used&(1u<<slot))continue;auto b=radarLabelSlot(slot);
      int edgeX=points[i].x<b.x?b.x:points[i].x>b.x+b.w?b.x+b.w:points[i].x;
      int edgeY=points[i].y<b.y?b.y:points[i].y>b.y+b.h?b.y+b.h:points[i].y;
      int dx=points[i].x-edgeX,dy=points[i].y-edgeY;int64_t cost=dx*dx+dy*dy;
      if(preferred&&preferred[i]==slot)cost-=800; // resist small RSSI-driven label jitter
      for(unsigned j=0;j<count&&j<RadarPageSize;++j)if(points[j].x>=b.x-9&&points[j].x<=b.x+b.w+9&&points[j].y>=b.y-9&&points[j].y<=b.y+b.h+9)cost+=1000000;
      // Keep the center cursor visible whenever another slot is available.
      if(180>=b.x&&180<=b.x+b.w&&180>=b.y-7&&180<=b.y+b.h+7)cost+=500000;
      if(cost<bestCost){bestCost=cost;best=slot;}
    }
    slots[i]=best;used|=1u<<best;
  }
}
}
