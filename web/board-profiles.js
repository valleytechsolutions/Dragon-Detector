export const boardProfiles=Object.freeze({
  spotpear:{name:'Spotpear / Guition JC3636W518',firmwareName:'Spotpear JC3636W518',size:360,display:'Round LCD',filesystemOffset:0xc10000,storage:'16 MB flash · 3.94 MiB shared user storage · optional microSD CSV/PCAP and file browser.',features:'Full radar, 27 themes, badge, stopwatch, radio tools and BLE Lab. Touch controls and speaker connector; software clock.'},
  waveshare:{name:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',firmwareName:'Waveshare ESP32-S3-Touch-AMOLED-1.75C',size:466,display:'Round AMOLED · 32 MB · BLE Lab',filesystemOffset:0xc10000,reconOffset:0x1000000,storage:'32 MB internal flash · 3.94 MiB badge/history storage + 16 MiB recon storage. No SD slot or SD controls. Save captures internally and download over USB.',features:'Full radar, 27 themes, badge, stopwatch, radio tools and BLE Lab. Touch, two timing buttons, RTC and speaker connector.'},
  stopwatch:{name:'M5Stack StopWatch C152',firmwareName:'M5Stack StopWatch C152',size:466,display:'Round AMOLED',filesystemOffset:0x810000,storage:'16 MB internal flash · 7.94 MiB shared user storage. No SD slot or SD controls. Save captures internally and download over USB.',features:'Full radar, 27 themes, badge, stopwatch, radio tools and BLE Lab. Touch, yellow/blue buttons, RTC, built-in speaker and haptics.'},
  waveshare_lite:{name:'Waveshare ESP32-S3-LCD-1.28 Lite',firmwareName:'Waveshare ESP32-S3-LCD-1.28 Lite',size:240,display:'Radar only · no touch · no speaker',filesystemOffset:0xc10000,lite:true,storage:'16 MB flash · radar-only profile. No user file browser, saved captures or SD controls.',features:'Always-on radar with all detection families. BOOT cycles 27 palettes and logos; the choice is saved. No touch, menus, badge, stopwatch, audio or interactive radio/BLE labs.'},
  xiao_round:{name:'Seeed XIAO ESP32-S3 + Round Display',firmwareName:'Seeed XIAO ESP32-S3 + Round Display',size:240,display:'Round LCD · touch · microSD · RTC · no speaker',filesystemOffset:0x610000,storage:'8 MB flash · 1.94 MiB shared user storage · optional microSD CSV/PCAP and file browser.',features:'Full radar, 27 themes, badge, stopwatch, radio tools and BLE Lab. Touch, BOOT button and RTC. Visual alerts; no speaker.'}
});
export function supportsBadge(board){return !!boardProfiles[board]&&!boardProfiles[board].lite;}
export function connectedProfile(info){
  return Object.entries(boardProfiles).find(([,p])=>p.firmwareName===info?.board&&p.size===info?.size)?.[0]||null;
}
export function validateManifest(data,board,mode){
  const profile=boardProfiles[board];
  if(!profile||!['factory','update'].includes(mode))throw new Error('Choose a supported board and installation type.');
  const expected=[['bootloader.bin',0],['partitions.bin',0x8000],['boot_app0.bin',0xe000],['firmware.bin',0x10000]];
  if(mode==='factory')expected.push(['littlefs.bin',profile.filesystemOffset]);
  if(mode==='factory'&&board==='waveshare'&&/^0\.(?:9\.(?:[8-9]|\d{2,})|(?:10|11|12|13)\.\d+)$/.test(data?.version))expected.push(['reconfs.bin',profile.reconOffset]);
  const build=data?.builds?.[0];
  if(board==='waveshare_lite'&&!/^0\.(?:9\.([4-9]|\d{2,})|(?:10|11|12|13)\.\d+)$/.test(data?.version))throw new Error('Waveshare Lite requires a 0.9.4 or newer matching manifest.');
  if(board==='xiao_round'&&!/^0\.(?:9\.([2-9]|\d{2,})|(?:10|11|12|13)\.\d+)$/.test(data?.version))throw new Error('XIAO Round Display requires a 0.9.2 or newer matching manifest.');
  if(data?.name!==`Dragon Detector — ${profile.firmwareName} (${mode})`||!/^0\.(?:[3-9]|10|11|12|13)\.\d+$/.test(data.version)||data.new_install_prompt_erase!==(mode==='factory')||data.builds?.length!==1||build?.chipFamily!=='ESP32-S3'||build.parts?.length!==expected.length||expected.some(([path,offset],i)=>build.parts[i]?.path!==path||build.parts[i]?.offset!==offset))throw new Error('Firmware manifest does not match this exact board and installation type.');
  return data;
}
