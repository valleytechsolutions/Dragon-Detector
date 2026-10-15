// Only protocol versions with a known compatible USB contract are accepted.
export function parseBadgeInfo(line){
  const match=/^BADGE_INFO (0\.[3456789]\.\d+) (240|360|466) (READY|RADAR_ONLY) (.+)$/.exec(line);
  if(match&&match[2]==='240'&&!/^0\.9\.([2-9]|\d{2,})$/.test(match[1]))return null;
  if(match){
    const lite=match[4]==='Waveshare ESP32-S3-LCD-1.28 Lite';
    if((match[3]==='RADAR_ONLY')!==lite)return null;
    if(lite&&(match[2]!=='240'||!/^0\.9\.([4-9]|\d{2,})$/.test(match[1])))return null;
  }
  return match?{version:match[1],size:Number(match[2]),board:match[4]}:null;
}
export function hasDetectorTools(version){return /^0\.[456789]\.\d+$/.test(version);}
export function hasActivityTools(version){return /^0\.[6789]\.\d+$/.test(version);}
export function hasRadarTools(version){return /^0\.[789]\.\d+$/.test(version);}

export function hasWearableTools(version){return /^0\.9\.\d+$/.test(version);}
