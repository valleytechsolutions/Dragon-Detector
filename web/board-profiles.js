export const boardProfiles=Object.freeze({
  spotpear:{name:'Spotpear / Guition JC3636W518',firmwareName:'Spotpear JC3636W518',size:360,display:'Round LCD',filesystemOffset:0xc10000},
  waveshare:{name:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',firmwareName:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',size:466,display:'Round AMOLED',filesystemOffset:0xc10000},
  stopwatch:{name:'M5Stack StopWatch C152',firmwareName:'M5Stack StopWatch C152',size:466,display:'Round AMOLED',filesystemOffset:0x810000}
});
export function connectedProfile(info){
  return Object.entries(boardProfiles).find(([,p])=>p.firmwareName===info?.board&&p.size===info?.size)?.[0]||null;
}
export function validateManifest(data,board,mode){
  const profile=boardProfiles[board];
  if(!profile||!['factory','update'].includes(mode))throw new Error('Choose a supported board and installation type.');
  const expected=[['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]];
  if(mode==='factory')expected.push(['littlefs.bin',profile.filesystemOffset]);
  const build=data?.builds?.[0];
  if(data?.name!==`Dragon Detector — ${profile.firmwareName} (${mode})`||!/^0\.[3-8]\.\d+$/.test(data.version)||data.new_install_prompt_erase!==(mode==='factory')||data.builds?.length!==1||build?.chipFamily!=='ESP32-S3'||build.parts?.length!==expected.length||expected.some(([path,offset],i)=>build.parts[i]?.path!==path||build.parts[i]?.offset!==offset))throw new Error('Firmware manifest does not match this exact board and installation type.');
  return data;
}
