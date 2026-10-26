import {deviceQuery} from './device-query.js';
const radarThemes=['dragon','green','gold','valleytech','biscuit','marauder','toool','church','toool-dark','covert','awok','baph0mau5','wdgwars','ghoststrats','n3rdsec','halehound','waveshare','m5stack','lab5','sasquach','nyan','dmac','seeed','dagnazty','octo-pig','deflock','lpl','ghostbusters','lpu','hackazillarex','m33k-t3ch','capsule-corp','sparrows','dzaz','custom','can-commander','ogg','xombi','muldersrevenge','mitm','hypnotic','vectorpotential','argyle','lawlocktools','moki'];
export async function readRadarSettings(link){
  const line=await deviceQuery(link,'radar status',s=>s.startsWith('RADAR_SETTINGS '));
  // 0.9.6 removed paging and added the selected primary target. Accept both
  // known schemas so reading radar preferences cannot block badge connection.
  const m=/^RADAR_SETTINGS theme=(0|[1-9]\d?) labels=([01]) recent=([01]) (?:page=\d+ pages=\d+ plotted=\d+|pages=1 plotted=\d+ primary=-?\d+)$/.exec(line);
  if(!m||Number(m[1])>=radarThemes.length)throw new Error('Invalid radar settings response.');
  return {theme:radarThemes[Number(m[1])],labels:m[2]==='1',recent:m[3]==='1'};
}
export async function saveRadarSettings(link,{theme,labels,recent}){
  const index=radarThemes.indexOf(theme);
  if(index<0||typeof labels!=='boolean'||typeof recent!=='boolean')throw new Error('Invalid radar preferences.');
  for(const [command,key,expected] of [[`radar theme ${theme}`,'theme',theme],[`radar labels ${labels?'on':'off'}`,'labels',labels],[`radar recent ${recent?'on':'off'}`,'recent',recent]]){
    // Read back each setting before the next write. The harmless status query
    // also releases short acknowledgments held by some native USB devices.
    await link.write(command);let saved=await readRadarSettings(link);
    if(saved[key]!==expected)saved=await readRadarSettings(link);
    if(saved[key]!==expected)throw new Error('Radar setting was not saved. Reconnect to reload saved preferences.');
  }
}
