// Only protocol versions with a known compatible USB contract are accepted.
export function parseBadgeInfo(line){
  const match=/^BADGE_INFO (0\.[3456]\.\d+) (360|466) READY (.+)$/.exec(line);
  return match?{version:match[1],size:Number(match[2]),board:match[3]}:null;
}
export function hasDetectorTools(version){return /^0\.[456]\.\d+$/.test(version);}
export function hasActivityTools(version){return /^0\.6\.\d+$/.test(version);}
