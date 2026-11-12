import {firmwareAtLeast,knownFirmware} from './firmware-version.js?v=0.14.7-143c';
// Only protocol versions with a known compatible USB contract are accepted.
export function parseBadgeInfo(line){
  const match=/^BADGE_INFO (0\.(?:[3456789]|10|11|12|13|14)\.\d+) (240|360|412|466|480) (READY|RADAR_ONLY|LITE) (.+)$/.exec(line);
  if(match&&match[2]==='412'&&(match[4]!=='Seeed SenseCAP Watcher'||!firmwareAtLeast(match[1],'0.13.29')))return null;
  if(match&&match[2]==='480'&&(match[4]!=='Elecrow CrowPanel 2.1 Rotary'||!firmwareAtLeast(match[1],'0.13.20')))return null;
  if(match&&match[2]==='240'&&!firmwareAtLeast(match[1],'0.9.2'))return null;
  if(match&&match[4]==='Waveshare ESP32-S3-Touch-AMOLED-1.43C'&&(match[2]!=='466'||match[3]!=='READY'||!firmwareAtLeast(match[1],'0.14.7')))return null;
  if(match&&match[4]==='Waveshare ESP32-S3-Touch-LCD-1.85B'&&(match[2]!=='360'||!firmwareAtLeast(match[1],'0.14.1')))return null;
  if(match&&match[4]==='Guition / Surenoo JC3636W518 Beta 1'&&(match[2]!=='360'||match[3]!=='READY'||!firmwareAtLeast(match[1],'0.14.2')))return null;
  if(match&&!knownFirmware(match[1]))return null;
  if(match){
    const lite=['Waveshare ESP32-S3-LCD-1.28 Lite','M5Stack Dial Beta','M5Stack Dial'].includes(match[4]);
    if(match[4]==='M5Stack Dial Beta'&&(match[3]!=='LITE'||!firmwareAtLeast(match[1],'0.13.26')))return null;
    if(match[4]==='M5Stack Dial'&&(match[3]!=='LITE'||!firmwareAtLeast(match[1],'0.13.27')))return null;
    if((match[3]!=='READY')!==lite)return null;
    if(match[3]==='LITE'&&!hasRetainedClock(match[1]))return null;
    if(lite&&(match[2]!=='240'||!firmwareAtLeast(match[1],'0.9.4')))return null;
  }
  return match?{version:match[1],size:Number(match[2]),board:match[4]}:null;
}
export function hasDetectorTools(version){return firmwareAtLeast(version,'0.4.0');}
export function hasActivityTools(version){return firmwareAtLeast(version,'0.6.0');}
export function hasRadarTools(version){return firmwareAtLeast(version,'0.7.0');}

export function hasWearableTools(version){return firmwareAtLeast(version,'0.9.0');}
export function hasRetainedClock(version){return firmwareAtLeast(version,'0.13.9');}
export function hasHardwareClockCheck(version){return firmwareAtLeast(version,'0.13.10');}
export function hasClockTools(board,version){return board==='waveshare_lite'?hasRetainedClock(version):hasWearableTools(version);}
export function hasWifiClock(board,version){return ['waveshare_143c','guition_185_beta','waveshare_185b','watcher','lilygo_175','waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear','xiao_round','elecrow_21'].includes(board)&&(board==='waveshare_143c'?firmwareAtLeast(version,'0.14.7'):board==='guition_185_beta'?firmwareAtLeast(version,'0.14.2'):board==='waveshare_185b'?firmwareAtLeast(version,'0.14.1'):hasRetainedClock(version));}

// Branded radar assets and indices 3..5 first shipped in 0.13.1.
export function hasBrandedThemes(version){return firmwareAtLeast(version,'0.13.1');}
export function hasCommunityThemes(version){return firmwareAtLeast(version,'0.13.6');}

// The CI / LockPickingLawyer theme first shipped in 0.13.8.
export function hasLplTheme(version){return firmwareAtLeast(version,'0.13.8');}

// Appended persistent theme IDs 29 and 30.
export function hasRunnerThemes(version){return firmwareAtLeast(version,'0.13.12');}

export function hasGreenArtThemes(version){return firmwareAtLeast(version,'0.13.23');}

export function hasCanCommanderTheme(version){return firmwareAtLeast(version,'0.13.18');}

export function hasCapsuleSparrowsThemes(version){return firmwareAtLeast(version,'0.13.15');}

export function hasMuldersRevengeTheme(version){return firmwareAtLeast(version,'0.13.24');}

export function hasMitmTheme(version){return firmwareAtLeast(version,'0.13.26');}

// Persistent ID 45: 44Delta, including its analog-only clock.
export function hasDelta44Theme(version){return firmwareAtLeast(version,'0.13.41');}

// Persistent IDs 41..44: VectorPotential, Argyle, Law Lock Tools and Moki.
export function hasLocksportCollection(version){return firmwareAtLeast(version,'0.13.38');}
