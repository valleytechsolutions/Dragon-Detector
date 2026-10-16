#include "hid_decoder.h"
static void hidDecoderTests(){
 using namespace dragon::hid;Map m;char out[256];
 const uint8_t keyboard[]={5,1,9,6,0xa1,1,5,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,6,0x75,8,0x15,0,0x25,0x65,5,7,0x19,0,0x29,0x65,0x81,0,0xc0};
 assert(parse(m,keyboard,sizeof(keyboard)));uint8_t keys[]={2,0,4,0x28,0,0,0,0};assert(decode(m,0,0,keys,8,out,sizeof(out))&&strstr(out,"LeftShift=1")&&strstr(out,"KeyA=1")&&strstr(out,"Enter=1"));assert(!decode(m,0,0,keys,7,out,sizeof(out)));assert(!decode(m,1,0,keys,8,out,sizeof(out)));assert(!decode(m,0,1,keys,8,out,sizeof(out)));
 const uint8_t mouse[]={5,1,9,2,0xa1,1,0x85,3,9,0x30,9,0x31,0x15,0x81,0x25,0x7f,0x75,8,0x95,2,0x81,6,0xc0};assert(parse(m,mouse,sizeof(mouse)));uint8_t axes[]={0xfd,12};assert(decode(m,3,0,axes,2,out,sizeof(out))&&strstr(out,"X=-3(rel)")&&strstr(out,"Y=12(rel)"));
 const uint8_t consumer[]={5,0xc,9,1,0xa1,1,0x85,5,0x15,1,0x25,2,0x75,8,0x95,1,9,0xe9,9,0xea,0x81,0,0xc0};assert(parse(m,consumer,sizeof(consumer)));uint8_t v=2;assert(decode(m,5,0,&v,1,out,sizeof(out))&&strstr(out,"VolumeDown=1"));v=0;assert(decode(m,5,0,&v,1,out,sizeof(out))&&strstr(out,"released"));
 const uint8_t stacked[]={5,1,9,2,0xa1,1,0x15,0,0x75,1,0x95,1,9,0x30,0xa4,5,9,9,1,0x81,2,0xb4,9,0x31,0x81,2,0xc0};assert(parse(m,stacked,sizeof(stacked)));v=3;assert(decode(m,0,0,&v,1,out,sizeof(out))&&strstr(out,"Y=1"));
 for(size_t i=0;i<sizeof(keyboard);++i)assert(!parse(m,keyboard,i));
 uint32_t seed=22;uint8_t fuzz[512],report[1024]{};for(unsigned trial=0;trial<40000;++trial){for(auto& b:fuzz){seed=seed*1664525+1013904223;b=seed>>24;}if(parse(m,fuzz,seed%sizeof(fuzz)))for(unsigned i=0;i<m.reportCount;++i)for(unsigned k=0;k<3;++k)decode(m,m.reports[i].id,k,report,(m.reports[i].bits[k]+7)/8,out,sizeof(out));}
 puts("PASS HID semantic keyboard, report-ID mouse, signed axes, consumer arrays, global push/pop, truncation and 40000 malformed maps");
}
