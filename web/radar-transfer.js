import {deviceQuery} from './device-query.js';
export async function readRadarSettings(link){
  const line=await deviceQuery(link,'radar status',s=>s.startsWith('RADAR_SETTINGS '));
  // 0.9.6 removed paging and added the selected primary target. Accept both
  // known schemas so reading radar preferences cannot block badge connection.
  const m=/^RADAR_SETTINGS theme=([0-5]) labels=([01]) recent=([01]) (?:page=\d+ pages=\d+ plotted=\d+|pages=1 plotted=\d+ primary=-?\d+)$/.exec(line);
  if(!m)throw new Error('Invalid radar settings response.');
  return {theme:['dragon','green','gold','valleytech','biscuit','marauder'][Number(m[1])],labels:m[2]==='1',recent:m[3]==='1'};
}
export async function saveRadarSettings(link,{theme,labels,recent}){
  const index=['dragon','green','gold','valleytech','biscuit','marauder'].indexOf(theme);
  if(index<0||typeof labels!=='boolean'||typeof recent!=='boolean')throw new Error('Invalid radar preferences.');
  for(const [command,expected] of [[`radar theme ${theme}`,`RADAR_THEME ${index}`],[`radar labels ${labels?'on':'off'}`,`RADAR_LABELS ${Number(labels)}`],[`radar recent ${recent?'on':'off'}`,`RADAR_RECENT ${Number(recent)}`]]){
    await link.write(command);const reply=await link.wait(s=>s.startsWith(expected.split(' ')[0]+' '));
    if(reply!==expected)throw new Error('Radar setting was not acknowledged. Reconnect to reload saved preferences.');
  }
}
