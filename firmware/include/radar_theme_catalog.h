#pragma once
#include <lvgl.h>
#include "ui_palette.h"
#include "personal_theme.h"
extern const lv_img_dsc_t radar_logo;
extern lv_img_dsc_t radar_logo_awok;
extern lv_img_dsc_t radar_logo_baph;
extern lv_img_dsc_t radar_logo_biscuit;
extern lv_img_dsc_t radar_logo_church;
extern lv_img_dsc_t radar_logo_ci;
extern lv_img_dsc_t radar_logo_mcnally;
extern lv_img_dsc_t radar_logo_lpl;
extern lv_img_dsc_t radar_logo_dagnazty;
extern lv_img_dsc_t radar_logo_deflock;
extern lv_img_dsc_t radar_logo_dmac;
extern lv_img_dsc_t radar_logo_halehound;
extern lv_img_dsc_t radar_logo_lab5;
extern lv_img_dsc_t radar_logo_m5stack;
extern lv_img_dsc_t radar_logo_marauder;
extern lv_img_dsc_t radar_logo_n3rdsec;
extern lv_img_dsc_t radar_logo_nyan;
extern lv_img_dsc_t radar_logo_sasquach;
extern lv_img_dsc_t radar_logo_seeed;
extern lv_img_dsc_t radar_logo_toool;
extern lv_img_dsc_t radar_logo_valleytech;
extern lv_img_dsc_t radar_logo_waveshare;
extern lv_img_dsc_t radar_logo_wdgwars;
extern lv_img_dsc_t radar_logo_ghostbusters,radar_logo_lpu;
extern lv_img_dsc_t radar_logo_hackazillarex,radar_logo_m33k;
extern lv_img_dsc_t radar_logo_capsule,radar_logo_sparrows;
extern lv_img_dsc_t radar_logo_dzaz,radar_logo_can_commander;
extern lv_img_dsc_t radar_logo_ogg,radar_logo_xombi,radar_logo_muldersrevenge;
bool prepareThemeLogos();
struct RadarPalette {uint32_t top,bottom,grid,ring,dot,sweep,text,panel;};
static const RadarPalette radarPalettes[]={
  {0x20b970,0x0c9553,0x095134,0xe0e5cd,0xffec50,0xb9ff72,0xfffae2,0x064a2e},
  {0x09291c,0x020c08,0x153d28,0x286c42,0xc0ff65,0x52dc84,0xdbf9c3,0x0b2419},
  {0x19130b,0x070707,0x3e301b,0xa88742,0xffd166,0xe54138,0xffe4a3,0x21190f},
  {0x091820,0x03090e,0x183440,0x71919d,0xdcebf0,0x63d5ef,0xe5f3f9,0x10242e},
  {0x22180e,0x0c0805,0x49331e,0xaf7c40,0xffdf9e,0xffc56b,0xffecd0,0x2e2014},
  {0x000000,0x100405,0x36161b,0xa34550,0xf0eeee,0xff354e,0xffe9ec,0x240c11},
  {0xffffff,0xfaf3f4,0xead8dc,0xc8102e,0xc8102e,0xdb1737,0x601323,0xffffff},
  {0x111111,0x000000,0x282828,0x9b9b9b,0xffffff,0xededed,0xf5f5f5,0x111111},
  {0x160d12,0x050507,0x41202b,0xb52a48,0xffe8ee,0xf33c60,0xf4e9ed,0x180e14},
  {0x090b0d,0x000000,0x252b30,0x858e95,0xf6f6f6,0xd6dde2,0xf2f4f6,0x111315},
  {0x0f1736,0x05091b,0x30265b,0xab74d9,0xffff79,0xeb5df4,0xfff7fd,0x111731},
  {0x170c24,0x040207,0x392347,0x965ad9,0xffd568,0xca88f9,0xfff1da,0x1b1025},
  {0x03151e,0x000308,0x123844,0x33a6bb,0xf8dbff,0x43e9f4,0xe5fbff,0x071b25},
  {0x17101f,0x030205,0x30243d,0x86629b,0xf4eaff,0xcf91ff,0xf3eef8,0x17141e},
  {0x17140c,0x020202,0x322b1a,0xb49451,0xffffff,0xe7c775,0xf7f4ec,0x191714},
  {0x24091e,0x050207,0x482040,0xae489e,0xffdec8,0xff76c5,0xffeff8,0x210d20},
  {0x10210a,0x030802,0x293d16,0x6ea72e,0xf6ffe5,0xafe653,0xf1f8ed,0x132017},
  {0x081d2a,0x02070c,0x183748,0x4594bd,0xe9f8ff,0x61caff,0xf1f8ff,0x10212e},
  {0x0e112e,0x040616,0x292754,0x9b60d6,0xffb55a,0x3fe5ff,0xf6f6ff,0x141a35},
  {0x11261e,0x020907,0x244638,0x4f9d85,0xffd692,0x78d5c1,0xf9ead3,0x102621},
  {0x19112e,0x04040e,0x36234f,0xaf66c6,0xffe58f,0xff75cf,0xfff4fc,0x1a1530},
  {0x073346,0x030a12,0x164759,0x328ca3,0xffe6f4,0xf16ab8,0xf5f8fc,0x102531},
  {0x07343f,0x021116,0x1b4e51,0x6b9b51,0xf7fbef,0xa7d52c,0xf7fbef,0x07303a},
  {0xffffff,0xf7f7f5,0xe1e1dd,0x767671,0x101010,0x181818,0x161616,0xffffff},
  {0x080808,0x080808,0x301723,0x6b304c,0xffdbe9,0xff89b6,0xffdbe9,0x1d1017},
  {0x152131,0x152131,0x29445b,0x678ba5,0xe5f4ff,0xabcde2,0xf0f6fb,0x1c2d40},
  {0x000000,0x07182d,0x152b45,0xa48943,0xfff3d4,0xecc75b,0xfff3d4,0x0b1a30},
  {0x000000,0x000000,0x261619,0x9f232c,0xffffff,0xff4b48,0xf5f5f5,0x161113},
  {0x000000,0x000000,0x222629,0x82898d,0xffffff,0xe5e9ec,0xffffff,0x111315},
  {0x000000,0x070204,0x31121b,0x9f3946,0xffedcf,0xff7073,0xffedcf,0x191113},
  {0x060818,0x030410,0x24203f,0x65487e,0xffffff,0x4ae9ff,0xfff2fc,0x14172e},
  {0x22a953,0x1e944b,0x146c3b,0xc9d6dd,0xffeb47,0xeaff91,0xf1ffcf,0x0b4028},
  {0x000000,0x000000,0x36151d,0x8d2332,0xfff1ed,0xd9384c,0xf3f2ef,0x141114},
  {0x101923,0x05080e,0x243543,0x647b8e,0xdce6ef,0xb6cbdc,0xe5edf4,0x121c27},
  {0x080d16,0x080d16,0x243542,0x68e2c1,0x72aaff,0x68e2c1,0xf3f6fb,0x151e28},
  {0x101d29,0x02040c,0x182935,0x597284,0xe8edf2,0x72c5e8,0xe8edf2,0x101d29},
  {0x000000,0x000000,0x0c2812,0x146823,0x3ee052,0x22bb39,0xe0f5e0,0x041206},
  {0x090f0b,0x090f0b,0x233826,0x416a45,0xa7e76b,0x7bc854,0xdceccc,0x142019},
  {0x000000,0x000000,0x332311,0x98602a,0xf2b95f,0xe6a241,0xffe8bc,0x181109},
  {0x09051c,0x02040c,0x26204b,0xaa3dff,0xa7ff16,0x00eaff,0xf5f7ff,0x110b28}
};
// Persistent indices 0..2 remain unchanged for existing settings.
static const char* radarThemeNames[]={"Dragon Ball","Green radar","Black / gold / red","Valleytech","Biscuit Shop","Marauder","TOOOL","Church of Malware","TOOOL Dark","Covert Instruments / McNally","AWOK","Baph0Mau5","WDGWARS","GhostStrats","N3rdsec","HaleHound","Waveshare","M5Stack","LAB5","Talking Sasquach","Nyan Devices","DMAC","Seeed Studio","DagNazty","0ct0's Pig","DeFlock","Covert Instruments / LockPickingLawyer","Ghostbusters","Lockpickers United","Hackazillarex","m33k_t3ch","Capsule Corp","Sparrows","DZ_AZ","Personal theme","Can Commander","Ogg","XomBi","MuldersreVenge","MITM"};
static const char* radarThemeCommands[]={"dragon","green","gold","valleytech","biscuit","marauder","toool","church","toool-dark","covert","awok","baph0mau5","wdgwars","ghoststrats","n3rdsec","halehound","waveshare","m5stack","lab5","sasquach","nyan","dmac","seeed","dagnazty","octo-pig","deflock","lpl","ghostbusters","lpu","hackazillarex","m33k-t3ch","capsule-corp","sparrows","dzaz","custom","can-commander","ogg","xombi","muldersrevenge","mitm"};
static const char* radarThemeDescriptions[]={"Emerald grid / colored device families","Green phosphor / rings and live sweep","Dark field / gold rings / red sweep","Silver rings / cyan sweep / VTS logo","Warm amber / cream / Biscuit Shop logo","Black field / red sweep / JCMK logo","White / red / locksport timer + watch","Monochrome circuits / binary watch","Near-black / red / locksport timer + watch","Black / silver / pick hands + locksport timer","Midnight ink / neon paint / mushroom dial","Violet ritual dial / sigils / ember gold","Cyan / magenta / world map / signal dial","Black / grey / purple / glowing slit eyes","Black / white / gold / original skull","Black / hot pink / violet / living hellfire","Green / silver / precision waveform dial","Blue / white / modular hybrid dial","Neon laboratory / large 12-hour digital","Forest / teal / character-arm watch","Pink / rainbow / flying Nyan Cat","Teal / pink / acrylic picks / locksport counter","Lime / deep teal / leaf-circuit maker dial","White / black / skull and talon dial","Pink ASCII octopus / interactive wandering pig","Navy / steel blue / camera-raven viewfinder","Navy / gold / balance scales / locksport timer","No-ghost / proton stream / containment clock","Monochrome / silver picks / moving pin seconds","Red / ivory / bone sweep / jumping T-rex","Cyan / magenta / cyber circuits / neon dial","Silver / blue / emerald grid / Capsule chronometer","Black / red / steel picks / locksport timer","Charcoal crow / rotating feather / feather hands","Your logo, colors and watch from Theme Studio","MK / silver and blue / CAN-bus instrument clock","Black / green / original eye emblem / simple digital clock","Zombie hacker artwork / moss green / phosphor accents","Black / brass / supplied emblem / simple digital clock","Neon photo dial / magenta, cyan and lime color cycle"};
static const lv_img_dsc_t* radarLogos[]={&radar_logo,&radar_logo,&radar_logo,&radar_logo_valleytech,&radar_logo_biscuit,&radar_logo_marauder,&radar_logo_toool,&radar_logo_church,&radar_logo_toool,&radar_logo_ci,&radar_logo_awok,&radar_logo_baph,&radar_logo_wdgwars,nullptr,&radar_logo_n3rdsec,&radar_logo_halehound,&radar_logo_waveshare,&radar_logo_m5stack,&radar_logo_lab5,&radar_logo_sasquach,&radar_logo_nyan,&radar_logo_dmac,&radar_logo_seeed,&radar_logo_dagnazty,nullptr,&radar_logo_deflock,&radar_logo_ci,&radar_logo_ghostbusters,&radar_logo_lpu,&radar_logo_hackazillarex,&radar_logo_m33k,&radar_logo_capsule,&radar_logo_sparrows,&radar_logo_dzaz,nullptr,&radar_logo_can_commander,&radar_logo_ogg,&radar_logo_xombi,&radar_logo_muldersrevenge,nullptr};
static constexpr unsigned RadarThemeCount=sizeof(radarThemeNames)/sizeof(radarThemeNames[0]);
static RadarPalette radarPalette(unsigned i){
  if(i!=dragon::PersonalThemeId)return radarPalettes[i];
  const auto& c=personalTheme();const auto& p=themeUiColors(i);
  return {c.bg,c.bg,p.raised,p.border,c.secondary,c.accent,c.ink,p.surface};
}
static const lv_img_dsc_t* radarLogo(unsigned i){return i==dragon::PersonalThemeId?personalThemeLogo():radarLogos[i];}
static_assert(dragon::UiPaletteCount==RadarThemeCount,"All themes need UI palettes");
static_assert(sizeof(radarPalettes)/sizeof(radarPalettes[0])==RadarThemeCount && sizeof(radarLogos)/sizeof(radarLogos[0])==RadarThemeCount && sizeof(radarThemeCommands)/sizeof(radarThemeCommands[0])==RadarThemeCount && sizeof(radarThemeDescriptions)/sizeof(radarThemeDescriptions[0])==RadarThemeCount,"Every radar theme needs a palette, logo and menu/USB name");
