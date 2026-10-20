// Only protocol versions with a known compatible USB contract are accepted.
export function parseBadgeInfo(line){
  const match=/^BADGE_INFO (0\.(?:[3456789]|10|11|12|13)\.\d+) (240|360|466) (READY|RADAR_ONLY|LITE) (.+)$/.exec(line);
  if(match&&match[2]==='240'&&!/^0\.(?:9\.([2-9]|\d{2,})|(?:10|11|12|13)\.\d+)$/.test(match[1]))return null;
  if(match){
    const lite=match[4]==='Waveshare ESP32-S3-LCD-1.28 Lite';
    if((match[3]!=='READY')!==lite)return null;
    if(match[3]==='LITE'&&!hasRetainedClock(match[1]))return null;
    if(lite&&(match[2]!=='240'||!/^0\.(?:9\.([4-9]|\d{2,})|(?:10|11|12|13)\.\d+)$/.test(match[1])))return null;
  }
  return match?{version:match[1],size:Number(match[2]),board:match[4]}:null;
}
export function hasDetectorTools(version){return /^0\.(?:[456789]|10|11|12|13)\.\d+$/.test(version);}
export function hasActivityTools(version){return /^0\.(?:[6789]|10|11|12|13)\.\d+$/.test(version);}
export function hasRadarTools(version){return /^0\.(?:[789]|10|11|12|13)\.\d+$/.test(version);}

export function hasWearableTools(version){return /^0\.(?:9|10|11|12|13)\.\d+$/.test(version);}
export function hasRetainedClock(version){return /^0\.13\.(?:9|[1-9]\d+)$/.test(version);}
export function hasHardwareClockCheck(version){return /^0\.13\.[1-9]\d+$/.test(version);}
export function hasClockTools(board,version){return board==='waveshare_lite'?hasRetainedClock(version):hasWearableTools(version);}
export function hasWifiClock(board,version){return ['waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear','xiao_round'].includes(board)&&hasRetainedClock(version);}

// Branded radar assets and indices 3..5 first shipped in 0.13.1.
export function hasBrandedThemes(version){return /^0\.13\.[1-9]\d*$/.test(version);}
export function hasCommunityThemes(version){return /^0\.13\.(?:[6-9]|[1-9]\d+)$/.test(version);}

// The CI / LockPickingLawyer theme first shipped in 0.13.8.
export function hasLplTheme(version){return /^0\.13\.(?:[89]|[1-9]\d+)$/.test(version);}

// Appended persistent theme IDs 29 and 30.
export function hasRunnerThemes(version){const m=/^0\.13\.(0|[1-9]\d*)$/.exec(version);return !!m&&Number(m[1])>=12;}

export function hasGreenArtThemes(version){const m=/^0\.13\.(0|[1-9]\d*)$/.exec(version);return !!m&&Number(m[1])>=23;}

export function hasCanCommanderTheme(version){const m=/^0\.13\.(0|[1-9]\d*)$/.exec(version);return !!m&&Number(m[1])>=18;}

export function hasCapsuleSparrowsThemes(version){const m=/^0\.13\.(0|[1-9]\d*)$/.exec(version);return !!m&&Number(m[1])>=15;}
