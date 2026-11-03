"""Run production theme selection, persisted-setting migration and retired-ID guards."""
from pathlib import Path
import argparse
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
work = root / '.work/theme-retirement-tests'
work.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
ui = (root / 'firmware/src/ui.cpp').read_text()
lite = (root / 'firmware/src/ui_lite.cpp').read_text()
watch = (root / 'firmware/src/watch_faces.inc').read_text()
catalog = (root / 'firmware/include/radar_theme_catalog.h').read_text()
dial = (root / 'firmware/src/dial_menu.inc').read_text()
dial_ui = (root / 'firmware/src/dial_ui.inc').read_text()
personal = (root / 'firmware/src/personal_theme.cpp').read_text()


def block(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def boot_prefix(source, end):
    start = source.index('{', source.index('void uiBegin()')) + 1
    return source[start:source.index(end, start)]


(work / 'Arduino.h').write_text('#pragma once\nclass String;\n')
(work / 'lvgl.h').write_text('#pragma once\nstruct lv_img_dsc_t {int id=0;};\nstruct lv_draw_ctx_t {};\n')
logos = '\n'.join(line.removeprefix('extern ') for line in catalog.splitlines()
                  if line.startswith('extern ') and 'lv_img_dsc_t' in line)
logos = logos.replace('const lv_img_dsc_t radar_logo;', 'const lv_img_dsc_t radar_logo{};')
watch_data = watch[watch.index('struct WatchPalette'):watch.index('static constexpr float WatchTau')]
menu_loop = block(ui, 'for(unsigned i=0;i<RadarThemeCount;++i){if(!radarThemeAllowed(i))continue;')
full_usb = block(ui, 'if(cmd.startsWith("radar theme "))')
dial_usb = block(dial_ui, 'if(c.startsWith("radar theme "))')
palette_functions = re.findall(r'const dragon::UiPalette& themeUiColors\(unsigned i\)\{[^\n]+', personal)
assert len(palette_functions) == 2
for path in ['radar_renderer.inc', 'watch_faces.inc', 'lite_pages.inc']:
    assert 'hypnotic' not in (root / 'firmware/src' / path).read_text().lower()
assert not (root / 'firmware/src/hypnotic_theme.inc').exists()

source = r'''
#include <cassert>
#include <climits>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "radar_theme_catalog.h"
class String:public std::string {public:using std::string::string;String(const std::string& s):std::string(s){}String substring(size_t n)const{return substr(n);}bool startsWith(const char* s)const{return rfind(s,0)==0;}};
LOGOS
bool loaded=false;
dragon::PersonalTheme config;
dragon::UiPalette palette{1,2,3,4,5,6,7,8,false};
bool personalThemeReady(){return loaded;}
const dragon::PersonalTheme& personalTheme(){return config;}
lv_img_dsc_t personalLogo;
const lv_img_dsc_t* personalThemeLogo(bool){return &personalLogo;}
FULL_PALETTE
namespace dialPalette {DIAL_PALETTE}
bool prepareThemeLogos(){return true;}
struct Preferences {
 std::map<std::string,unsigned> values;unsigned writes=0;bool opened=true;
 bool begin(const char*,bool){return opened;}
 unsigned getUChar(const char* key,unsigned fallback){auto it=values.find(key);return it==values.end()?fallback:it->second;}
 bool getBool(const char* key,bool fallback){return getUChar(key,fallback);}
 void putUChar(const char* key,unsigned value){values[key]=value;++writes;}
 void putBool(const char* key,bool value){putUChar(key,value);}
};
Preferences settings,litePrefs;
struct SerialStub {std::string last;void println(const char* message){last=message;}template<class... A>void printf(const char* fmt,A... args){char out[128];snprintf(out,sizeof(out),fmt,args...);last=out;}} Serial;
unsigned radarTheme=1,brightness=100,themeChanges=0,applied=1;
uint8_t watchFace=0;
bool candidatesOnly=false,radarLabels=true,radarRecent=true,prefsReady=false;
void* root=reinterpret_cast<void*>(1);
enum Page{Boot,Radar,RadarStyle,Clock};Page page=Radar,pending=Radar;
void go(Page value){pending=value;}
void dismissNotice(){}
void uiApplyTheme(unsigned id){applied=dragon::uiThemeIndex(id);}
void refreshLiteTheme(){++themeChanges;}
void dialMenuRefresh(){}
bool locksport(){return false;}
bool delta44(){return radarTheme==45;}bool lawLockTools(){return radarTheme==43;}bool moki(){return radarTheme==44;}bool argyle(){return radarTheme==42;}bool vectorPotential(){return radarTheme==41;}
bool sparrows(){return false;}bool lpu(){return false;}bool lpl(){return false;}bool dmac(){return false;}bool covert(){return false;}bool locksportDark(){return false;}
constexpr unsigned Delta44Watch=48,LawLockToolsWatch=46,MokiWatch=47,ArgyleWatch=45,VectorPotentialWatch=44;
WATCH_DATA
SELECT
DIAL_ALLOWED
DIAL_AT
DIAL_COUNT
DIAL_LABEL
LITE_CYCLE
void fullBoot(){FULL_BOOT}
void liteBoot(){LITE_BOOT}
bool fullCommand(const String& cmd){FULL_USB return false;}
bool dialCommand(const String& c){DIAL_USB return false;}
#define LV_SYMBOL_OK "+"
std::vector<unsigned> shown;
void row(void*,const char* name,const char* detail,unsigned,unsigned action){assert(name&&name[0]&&detail&&detail[0]);shown.push_back(action-9000);}
void gallery(){shown.clear();void* body=nullptr;unsigned n=0;MENU_LOOP}
void seed(Preferences& prefs,unsigned radar,unsigned face=0){prefs.values={{"panel-v04",1},{"radar-v07",1},{"radar-theme",radar},{"theme",radar},{"watch-face",face},{"brightness",75},{"keep-user-data",123}};prefs.writes=0;}
int main(){
 assert(RadarThemeCount==46&&dragon::UiPaletteCount==46);
 assert(!radarThemeNames[40][0]&&!radarThemeCommands[40][0]&&!radarThemeDescriptions[40][0]);
 assert(watches[43].name==nullptr);
 const char* later[]={"vectorpotential","argyle","lawlocktools","moki","44delta"};
 for(unsigned n=0;n<5;++n){assert(std::string(radarThemeCommands[41+n])==later[n]);assert(watches[44+n].name);}
 for(unsigned id=0;id<256;++id){
  loaded=true;const bool valid=id<46&&id!=40;
  assert(radarThemeActive(id)==valid&&radarThemeAllowed(id)==valid);
  assert(savedRadarTheme(id)==(valid?id:1));
  seed(settings,id,43);fullBoot();assert(radarTheme==(valid?id:1)&&watchFace==0);
  assert(settings.values["watch-face"]==0&&settings.values["radar-theme"]==radarTheme&&settings.values["keep-user-data"]==123);
  assert(settings.writes==(valid?1u:2u));
  seed(litePrefs,id);liteBoot();assert(radarTheme==(valid?id:1)&&litePrefs.values["theme"]==radarTheme&&litePrefs.values["keep-user-data"]==123);
  assert(litePrefs.writes==(valid?0u:1u));
 }
 loaded=true;for(auto id:WatchFaces){seed(settings,41,id);fullBoot();assert(watchFace==id&&radarTheme==41&&settings.writes==0&&watches[id].name);}
 for(unsigned face:{3u,4u,5u,6u,7u,43u,49u,255u,UINT_MAX})assert(!watchFaceAllowed(face));
 for(unsigned id:{40u,46u,255u,256u,UINT_MAX}){
  radarTheme=41;watchFace=44;settings.writes=0;selectRadarTheme(id);
  assert(radarTheme==41&&watchFace==44&&settings.writes==0);
  assert(&themeUiColors(id)==&dragon::UiPalettes[1]&&&dialPalette::themeUiColors(id)==&dragon::UiPalettes[1]);
  assert(radarPalette(id).top==radarPalette(1).top&&radarLogo(id)==radarLogo(1));
 }
 for(unsigned id=41;id<46;++id){selectRadarTheme(id);assert(radarTheme==id&&watchFace==id+3&&applied==id);}
 for(bool personal:{false,true}){
  loaded=personal;gallery();assert(shown.size()==(personal?45:44));
  for(auto id:shown)assert(id!=40&&radarThemeAllowed(id));
  radarTheme=0;std::vector<unsigned> cycled;
  do{uiKey(false);cycled.push_back(radarTheme);assert(radarTheme!=40&&radarThemeAllowed(radarTheme));}while(radarTheme!=0&&cycled.size()<50);
  assert(cycled.size()==shown.size());
  radarTheme=39;uiKey(false);assert(radarTheme==41);radarTheme=45;uiKey(false);assert(radarTheme==0);
  unsigned before=themeChanges;uiKey(true);assert(radarTheme==0&&themeChanges==before);
 }
 loaded=false;seed(settings,34,37);fullBoot();assert(radarTheme==1&&watchFace==0);seed(litePrefs,34);liteBoot();assert(radarTheme==1);
 loaded=true;assert(&themeUiColors(34)==&palette);
 std::vector<unsigned> dialIds;for(unsigned id=0;id<46;++id)if(id!=34&&id!=40)dialIds.push_back(id);
 assert(dialThemeCount()==44);for(int n=0;n<44;++n)assert(dialThemeAt(n)==dialIds[n]&&dialThemeLabel(dialThemeAt(n))[0]);
 assert(dialThemeAt(-1)==0&&dialThemeAt(44)==0&&!dialThemeLabel(40)[0]&&!dialThemeLabel(UINT_MAX)[0]);
 for(const char* command:{"radar theme hypnotic","radar theme ","radar theme 40"}){
  radarTheme=41;watchFace=44;settings.writes=0;themeChanges=0;
  assert(fullCommand(command)&&Serial.last.find("ERROR")!=std::string::npos);assert(radarTheme==41&&watchFace==44&&settings.writes==0);
  assert(dialCommand(command)&&Serial.last.find("ERROR")!=std::string::npos);assert(radarTheme==41&&themeChanges==0);
 }
 for(unsigned id=41;id<46;++id){String command="radar theme "+std::string(radarThemeCommands[id]);assert(fullCommand(command)&&radarTheme==id&&watchFace==id+3);radarTheme=0;assert(dialCommand(command)&&radarTheme==id);}
 loaded=false;radarTheme=1;assert(fullCommand("radar theme custom")&&radarTheme==1);assert(dialCommand("radar theme custom")&&radarTheme==1);
 puts("PASS retired themes: stable IDs, persisted full/Lite migration, unchanged valid saves, selection/gallery/cycling, Dial enumeration, USB rejection and palette bounds");
}
'''
replacements = {
    'LOGOS': logos, 'FULL_PALETTE': palette_functions[1], 'DIAL_PALETTE': palette_functions[0],
    'WATCH_DATA': watch_data, 'SELECT': block(ui, 'static void selectRadarTheme('),
    'DIAL_ALLOWED': block(dial, 'static bool dialThemeAllowed('),
    'DIAL_AT': block(dial, 'static unsigned dialThemeAt('),
    'DIAL_COUNT': block(dial, 'static int dialThemeCount('),
    'DIAL_LABEL': block(dial, 'static const char* dialThemeLabel('),
    'LITE_CYCLE': block(lite, 'void uiKey('),
    'FULL_BOOT': boot_prefix(ui, 'scr_lvgl_init();'),
    'LITE_BOOT': boot_prefix(lite, 'candidatesOnly=true;'),
    'FULL_USB': full_usb, 'DIAL_USB': dial_usb, 'MENU_LOOP': menu_loop,
}
for key, value in replacements.items():
    source = source.replace(key, value)
cpp = work / 'themes.cpp'
cpp.write_text(source)
exe = work / 'themes.exe'
subprocess.run(compiler + ['-std=c++17', '-O1', '-g', '-UNDEBUG', '-I'+str(work),
                          '-I'+str(root/'firmware/include'), str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
