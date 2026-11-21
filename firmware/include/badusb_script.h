#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// A small, deterministic DuckyScript subset. The caller owns the source buffer
// and each complete keyboard press/release; no heap or Arduino APIs are needed.
namespace dragon::badusb {
constexpr size_t MaxScriptBytes=8192;
constexpr size_t MaxLineBytes=256;
constexpr uint32_t MaxDelayMs=60000;
constexpr uint32_t MaxRunMs=600000;
constexpr uint32_t StrokeIntervalMs=10;
struct Error {unsigned line=0;const char* message="";};
struct Stroke {uint8_t modifiers=0;uint8_t keys[6]{};};
using Send=bool(*)(void*,const Stroke&);

namespace detail {
struct Span {const char* data=nullptr;size_t size=0;};
inline bool equal(Span a,const char* b){return a.size==strlen(b)&&!memcmp(a.data,b,a.size);}
inline bool space(char c){return c==' '||c=='\t';}
inline Span trim(Span s){while(s.size&&space(*s.data)){++s.data;--s.size;}while(s.size&&space(s.data[s.size-1]))--s.size;return s;}
inline Span token(Span& rest){rest=trim(rest);size_t n=0;while(n<rest.size&&!space(rest.data[n]))++n;Span out{rest.data,n};rest.data+=n;rest.size-=n;return out;}
inline bool next(const char* source,size_t size,size_t& offset,Span& out){
  if(offset>=size)return false;
  size_t begin=offset;while(offset<size&&source[offset]!='\n')++offset;
  out={source+begin,offset-begin};if(offset<size)++offset;
  if(out.size&&out.data[out.size-1]=='\r')--out.size;
  return true;
}
inline bool number(Span text,uint32_t& value){
  text=trim(text);if(!text.size)return false;value=0;
  for(size_t i=0;i<text.size;++i){char c=text.data[i];if(c<'0'||c>'9')return false;value=value*10+unsigned(c-'0');if(value>MaxDelayMs)return false;}
  return true;
}
inline bool ascii(char c,Stroke& out){
  out={};uint8_t key=0;bool shift=false;
  if(c>='a'&&c<='z')key=4+c-'a';
  else if(c>='A'&&c<='Z'){key=4+c-'A';shift=true;}
  else if(c>='1'&&c<='9')key=30+c-'1';
  else if(c=='0')key=39;
  else {
    const char* plain=" -=[]\\;'`,./";
    const char* upper=" _+{}|:\"~<>?";
    const uint8_t codes[]={44,45,46,47,48,49,51,52,53,54,55,56};
    for(size_t i=0;i<sizeof(codes);++i){if(c==plain[i]){key=codes[i];break;}if(c==upper[i]){key=codes[i];shift=true;break;}}
    if(!key){const char* digits="!@#$%^&*()";for(unsigned i=0;i<10;++i)if(c==digits[i]){key=i==9?39:30+i;shift=true;break;}}
  }
  if(!key)return false;out.modifiers=shift?2:0;out.keys[0]=key;return true;
}
inline uint8_t modifier(Span s){
  if(equal(s,"CTRL")||equal(s,"CONTROL"))return 1;
  if(equal(s,"SHIFT"))return 2;
  if(equal(s,"ALT"))return 4;
  if(equal(s,"GUI")||equal(s,"WINDOWS")||equal(s,"COMMAND"))return 8;
  return 0;
}
inline uint8_t namedKey(Span s){
  struct Named {const char* name;uint8_t code;};
  static constexpr Named names[]={
    {"ENTER",40},{"RETURN",40},{"ESC",41},{"ESCAPE",41},{"BACKSPACE",42},
    {"TAB",43},{"SPACE",44},{"CAPSLOCK",57},{"PRINTSCREEN",70},{"SCROLLLOCK",71},
    {"PAUSE",72},{"BREAK",72},{"INSERT",73},{"HOME",74},{"PAGEUP",75},
    {"DELETE",76},{"DEL",76},{"END",77},{"PAGEDOWN",78},{"RIGHT",79},
    {"RIGHTARROW",79},{"LEFT",80},{"LEFTARROW",80},{"DOWN",81},
    {"DOWNARROW",81},{"UP",82},{"UPARROW",82},{"NUMLOCK",83},{"MENU",101},{"APP",101}
  };
  for(const auto& key:names)if(equal(s,key.name))return key.code;
  if(s.size>=2&&s.size<=3&&s.data[0]=='F'){
    unsigned n=0;for(size_t i=1;i<s.size;++i){if(s.data[i]<'0'||s.data[i]>'9')return 0;n=n*10+unsigned(s.data[i]-'0');}
    if(n>=1&&n<=12&&s.data[1]!='0')return 57+n;
  }
  return 0;
}
enum class Kind {None,Text,TextLine,Delay,DefaultDelay,Chord};
struct Action {Kind kind=Kind::None;Span text;uint32_t delay=0;Stroke stroke;};
inline bool parse(Span line,Action& action,const char*& error){
  action={};error="";
  if(line.size>MaxLineBytes){error="Line exceeds 256 bytes";return false;}
  for(size_t i=0;i<line.size;++i){unsigned char c=line.data[i];if((c<32&&c!='\t')||c>126){error="Use printable ASCII text";return false;}}
  while(line.size&&space(*line.data)){++line.data;--line.size;}
  if(!line.size)return true;
  size_t commandSize=0;while(commandSize<line.size&&!space(line.data[commandSize]))++commandSize;
  Span command{line.data,commandSize},rest{line.data+commandSize,line.size-commandSize};
  if(equal(command,"REM"))return true;
  if(equal(command,"STRING")||equal(command,"STRINGLN")){
    // Exactly one separator belongs to the command. Further spaces are text.
    if(rest.size){++rest.data;--rest.size;}
    for(size_t i=0;i<rest.size;++i)if(rest.data[i]=='\t'){error="STRING accepts printable ASCII only";return false;}
    action.kind=equal(command,"STRING")?Kind::Text:Kind::TextLine;action.text=rest;return true;
  }
  if(equal(command,"DELAY")||equal(command,"DEFAULT_DELAY")||equal(command,"DEFAULTDELAY")){
    if(!number(rest,action.delay)){error="Delay must be 0..60000 milliseconds";return false;}
    action.kind=equal(command,"DELAY")?Kind::Delay:Kind::DefaultDelay;return true;
  }
  action.kind=Kind::Chord;rest=line;unsigned count=0;
  while((rest=trim(rest)).size){
    Span key=token(rest);uint8_t mod=modifier(key);
    if(mod){if(action.stroke.modifiers&mod){error="Duplicate modifier";return false;}action.stroke.modifiers|=mod;continue;}
    uint8_t usage=namedKey(key);Stroke character;
    if(!usage&&key.size==1&&ascii(key.data[0],character)){usage=character.keys[0];action.stroke.modifiers|=character.modifiers;}
    if(!usage){error="Unsupported command or key";return false;}
    if(count==6){error="A chord supports at most six keys";return false;}
    for(unsigned i=0;i<count;++i)if(action.stroke.keys[i]==usage){error="Duplicate key";return false;}
    action.stroke.keys[count++]=usage;
  }
  return true;
}
}

inline bool validate(const char* source,size_t size,Error& error){
  error={};if(!source||!size){error.message="Script is empty";return false;}
  if(size>MaxScriptBytes){error.message="Script exceeds 8192 bytes";return false;}
  size_t offset=0;unsigned lineNumber=0;uint32_t defaultDelay=0;uint64_t duration=0;bool commands=false;detail::Span line;
  while(detail::next(source,size,offset,line)){
    ++lineNumber;detail::Action action;const char* message;
    if(!detail::parse(line,action,message)){error={lineNumber,message};return false;}
    using detail::Kind;
    if(action.kind==Kind::None)continue;
    if(action.kind==Kind::DefaultDelay){defaultDelay=action.delay;continue;}
    commands=true;
    if(action.kind==Kind::Delay)duration+=action.delay+defaultDelay;
    else {
      size_t strokes=action.kind==Kind::Chord?1:action.text.size+(action.kind==Kind::TextLine?1:0);
      duration+=(strokes?strokes-1:0)*StrokeIntervalMs+(defaultDelay>StrokeIntervalMs?defaultDelay:StrokeIntervalMs);
    }
    if(duration>MaxRunMs){error={lineNumber,"Script exceeds the ten-minute run limit"};return false;}
  }
  if(!commands){error.message="Script has no commands";return false;}
  return true;
}

class Runner {
 public:
  bool begin(const char* source,size_t size,uint32_t now,Error& error){
    stop();error_={};complete_=false;failed_=false;line_=0;
    if(!validate(source,size,error)){error_=error;failed_=true;return false;}
    source_=source;size_=size;offset_=0;started_=ready_=now;defaultDelay_=0;activeText_=false;running_=true;return true;
  }
  // The sink sends a complete stroke, including release. False aborts the run.
  // At most one stroke or source line is handled per call, so Stop remains live.
  void tick(uint32_t now,Send send,void* context=nullptr){
    if(!running_)return;
    if(uint32_t(now-started_)>MaxRunMs){fail("Ten-minute run limit reached");return;}
    if(int32_t(now-ready_)<0)return;
    if(activeText_){sendText(now,send,context);return;}
    detail::Span line;if(!detail::next(source_,size_,offset_,line)){running_=false;complete_=true;return;}
    ++line_;detail::Action action;const char* message;
    if(!detail::parse(line,action,message)){fail(message);return;}
    using detail::Kind;
    if(action.kind==Kind::None)return;
    if(action.kind==Kind::DefaultDelay){defaultDelay_=action.delay;return;}
    if(action.kind==Kind::Delay){ready_=now+action.delay+defaultDelay_;return;}
    if(action.kind==Kind::Chord){if(!send||!send(context,action.stroke)){fail("USB keyboard send failed");return;}finishCommand(now);return;}
    text_=action.text;newline_=action.kind==Kind::TextLine;activeText_=true;sendText(now,send,context);
  }
  void stop(){running_=false;activeText_=false;complete_=false;}
  bool running()const{return running_;}
  bool complete()const{return complete_;}
  bool failed()const{return failed_;}
  unsigned line()const{return line_;}
  const Error& error()const{return error_;}
 private:
  const char* source_=nullptr;size_t size_=0,offset_=0;
  uint32_t started_=0,ready_=0,defaultDelay_=0;unsigned line_=0;
  bool running_=false,complete_=false,failed_=false,activeText_=false,newline_=false;
  detail::Span text_;Error error_;
  void fail(const char* message){running_=false;activeText_=false;failed_=true;error_={line_,message};}
  void finishCommand(uint32_t now){activeText_=false;ready_=now+(defaultDelay_>StrokeIntervalMs?defaultDelay_:StrokeIntervalMs);}
  void sendText(uint32_t now,Send send,void* context){
    Stroke stroke;
    if(text_.size){
      if(!detail::ascii(*text_.data,stroke)){fail("Use printable ASCII text");return;}
      ++text_.data;--text_.size;
    }else if(newline_){stroke.keys[0]=40;newline_=false;}
    else {finishCommand(now);return;}
    if(!send||!send(context,stroke)){fail("USB keyboard send failed");return;}
    if(!text_.size&&!newline_)finishCommand(now);else ready_=now+StrokeIntervalMs;
  }
};
}
