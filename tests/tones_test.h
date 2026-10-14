#include "tones.h"
#include <string>
static void toneTests(){
  Melody m;
  for(unsigned i=0;i<unsigned(Family::Count);++i){assert(parseTone(defaultTones[i],m)&&m.count>0&&m.duration<2000);for(unsigned j=0;j<i;++j)assert(strcmp(defaultTones[i],defaultTones[j]));}
  assert(parseTone("",m)&&m.count==0);
  assert(parseTone("Test:d=4,o=5,b=120:8c#,p,16a.6,b6.",m));assert(m.count==4&&m.notes[0].hz==554&&m.notes[0].ms==250&&m.notes[1].hz==0&&m.notes[2].ms==187);
  for(const char* bad:{"", "a", "a:", "a:d", "a:d=", "a:d=4", "a::", "a:d=0:c", "a:o=3:c", "a:b=0:c", "a:b=901:c", "a:d=4,d=8:c", "a::c#8", "a::x", "a::c,", "a::c..", "a::p#", "a::b#", "a::4", "a::0c", "a::c\n", "a:b=25:1c,1c"}){if(*bad)assert(!parseTone(bad,m));}
  std::string tooLong(513,'a');assert(!parseTone(tooLong.c_str(),m));std::string tooMany="a:d=32,b=900:";for(int i=0;i<97;++i)tooMany+=(i?",c":"c");assert(!parseTone(tooMany.c_str(),m));
  ToneQueue q;q.request(3,100);q.request(3,200);q.request(4,200);assert(q.take(300)==3&&q.take(300)==4&&q.take(300)==-1);q.request(3,500);assert(q.take(600)==-1);q.request(3,4400);assert(q.take(15000)==-1);q.request(26,16000);assert(q.take(16000)==-1);
  ToneQueue wrap;wrap.request(2,0xffffff00);assert(wrap.take(0xffffff01)==2);wrap.request(2,100);assert(wrap.take(101)==-1);wrap.request(2,5000);assert(wrap.take(5001)==2);
  assert(huntInterval(-95)==2000&&huntInterval(-35)==250&&huntPitch(-95)==440&&huntPitch(-35)==1760);for(int r=-126;r<0;++r){assert(huntInterval(r)>=huntInterval(r+1));assert(huntPitch(r)<=huntPitch(r+1));}
  uint32_t rng=123;for(int n=0;n<50000;++n){std::string text;for(unsigned j=0;j<unsigned(n)%515;++j){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;text+=char(1+rng%127);}parseTone(text.c_str(),m);}
  puts("PASS: 26 RTTTL presets, validation/bounds, tone queue cooldown/expiry/rollover, hunt mapping, 50000 tone fuzz inputs");
}
