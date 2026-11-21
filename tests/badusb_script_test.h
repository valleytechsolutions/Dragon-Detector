#pragma once
#include "badusb_script.h"
#include <string>
#include <vector>

static void badusbScriptTests(){
  using namespace dragon::badusb;
  Error error;Runner runner;std::vector<Stroke> strokes;
  auto send=[](void* context,const Stroke& stroke){static_cast<std::vector<Stroke>*>(context)->push_back(stroke);return true;};
  auto begin=[&](const std::string& source,uint32_t now=0){return runner.begin(source.data(),source.size(),now,error);};
  auto finish=[&](uint32_t start=0){for(uint32_t elapsed=0;runner.running()&&elapsed<20000;elapsed+=StrokeIntervalMs)runner.tick(start+elapsed,send,&strokes);assert(runner.complete());};

  const std::string hello="REM Plain text demonstration\r\nDEFAULT_DELAY 20\r\nSTRING  Hello! \r\nSTRINGLN\r\nTAB\r\nCTRL ALT DELETE\r\nF12\r\n";
  assert(begin(hello));finish();assert(strokes.size()==12);
  assert(strokes[0].keys[0]==44&&strokes[0].modifiers==0); // Extra separator preserved.
  assert(strokes[1].keys[0]==11&&strokes[1].modifiers==2); // Uppercase H.
  assert(strokes[6].keys[0]==30&&strokes[6].modifiers==2); // !
  assert(strokes[7].keys[0]==44&&strokes[8].keys[0]==40); // Trailing space, STRINGLN.
  assert(strokes[9].keys[0]==43);
  assert(strokes[10].keys[0]==76&&strokes[10].modifiers==5);
  assert(strokes[11].keys[0]==69);

  // Every supported printable US ASCII character maps to a nonzero HID key.
  std::string ascii="STRING ";for(int c=32;c<=126;++c)ascii+=char(c);
  strokes.clear();assert(begin(ascii));finish();assert(strokes.size()==95);
  for(const auto& stroke:strokes){assert(stroke.keys[0]>=4&&stroke.keys[0]<=56);for(int k=1;k<6;++k)assert(!stroke.keys[k]);}
  assert(strokes['@'-32].keys[0]==31&&strokes['@'-32].modifiers==2);
  assert(strokes['\\'-32].keys[0]==49&&!strokes['\\'-32].modifiers);
  assert(strokes['|'-32].keys[0]==49&&strokes['|'-32].modifiers==2);

  // Full validation happens before any earlier valid line can send a key.
  for(const char* bad:{"DELAY -1","DELAY +1","DELAY 60001","DELAY 1 extra","DELAY 999999999999999999999999999999","DEFAULT_DELAY","DEFAULTDELAY 3.5","REPEAT 4","STRING\tbad\ttext","CTRL UNKNOWN","CTRL CTRL a","a a","a b c d e f g","F13","F01","STRING \x7f","STRING \xc3\xa9"}){
    std::string source=std::string("STRING valid\n")+bad;
    strokes.clear();assert(!begin(source));assert(error.line==2&&*error.message);runner.tick(100,send,&strokes);assert(strokes.empty()&&runner.failed());
  }
  assert(!validate(nullptr,0,error)&&!error.line);
  assert(!validate("REM only\n",9,error)&&!error.line);
  std::string nul="STRING first\nSTRING a";nul.push_back('\0');nul+='b';assert(!validate(nul.data(),nul.size(),error)&&error.line==2);
  std::string tooLong="STRING "+std::string(MaxLineBytes-6,'a');assert(!validate(tooLong.data(),tooLong.size(),error)&&error.line==1);
  std::string fullLine="STRING "+std::string(MaxLineBytes-7,'a');assert(validate(fullLine.data(),fullLine.size(),error));
  std::string tooLarge(MaxScriptBytes+1,'\n');assert(!validate(tooLarge.data(),tooLarge.size(),error)&&!error.line);
  std::string duration;for(int i=0;i<11;++i)duration+="DELAY 60000\n";assert(!validate(duration.data(),duration.size(),error)&&error.line==11);

  // DELAY and default inter-command pauses are cooperative, including rollover.
  const std::string timed="DEFAULTDELAY 25\nSTRING ab\nDELAY 40\nENTER";
  strokes.clear();assert(begin(timed,0xfffffff0u));
  runner.tick(0xfffffff0u,send,&strokes); // default
  runner.tick(0xfffffff0u,send,&strokes);assert(strokes.size()==1);
  runner.tick(0xfffffff9u,send,&strokes);assert(strokes.size()==1);
  runner.tick(0xfffffffau,send,&strokes);assert(strokes.size()==2);
  runner.tick(18,send,&strokes);assert(runner.line()==2);
  runner.tick(19,send,&strokes);assert(runner.line()==3); // 25 ms after last character.
  runner.tick(83,send,&strokes);assert(strokes.size()==2);
  runner.tick(84,send,&strokes);assert(strokes.size()==3&&strokes.back().keys[0]==40);
  runner.tick(109,send,&strokes);assert(runner.complete());

  const std::string chord="CTRL ALT SHIFT GUI a b c d e f";
  strokes.clear();assert(begin(chord));finish();assert(strokes.size()==1&&strokes[0].modifiers==15);
  for(unsigned i=0;i<6;++i)assert(strokes[0].keys[i]==i+4);
  const std::string textLine="STRINGLN ok";strokes.clear();assert(begin(textLine));finish();assert(strokes.size()==3&&strokes.back().keys[0]==40);

  // Stop, transport loss, missing sink, and a stalled host cannot resume typing.
  const std::string stopped="DELAY 60000\nSTRING stop";
  strokes.clear();assert(begin(stopped));runner.tick(0,send,&strokes);runner.stop();runner.tick(60001,send,&strokes);assert(!runner.running()&&!runner.complete()&&strokes.empty());
  const std::string one="ENTER";assert(begin(one));runner.tick(0,[](void*,const Stroke&){return false;});assert(runner.failed()&&!runner.running()&&runner.error().line==1);
  assert(begin(one));runner.tick(0,nullptr);assert(runner.failed());
  assert(begin(stopped,0xffffff00u));runner.tick(0xffffff00u+MaxRunMs+1,send,&strokes);assert(runner.failed()&&strstr(runner.error().message,"limit"));
  assert(begin(one));finish();assert(!runner.failed());
  puts("PASS BadUSB ASCII/chord parsing, whole-script validation, limits, cooperative timing, rollover, stop and transport failure");
}
