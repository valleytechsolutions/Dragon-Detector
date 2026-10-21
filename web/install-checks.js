import {validateManifest} from './board-profiles.js?v=2026-10-21-flasher';

// Check availability before asking for USB access. The installer downloads the
// binaries itself before erasing; these HEAD requests do not verify their hash.
export async function checkRelease(board,mode,{fetchImpl=fetch,signal,timeoutMs=15000}={}){
  const controller=new AbortController();
  const cancel=()=>controller.abort();
  if(signal?.aborted)cancel();
  signal?.addEventListener('abort',cancel,{once:true});
  const timeout=setTimeout(cancel,timeoutMs);
  const options={cache:'no-cache',signal:controller.signal};
  try{
    const base=`firmware/${board}/`;
    const response=await fetchImpl(`${base}${mode}.json`,options);
    if(!response.ok)throw new Error(`Firmware list unavailable (HTTP ${response.status}). Retry the release check.`);
    const manifest=validateManifest(await response.json(),board,mode);
    await Promise.all(manifest.builds[0].parts.map(async part=>{
      const file=await fetchImpl(base+part.path,{...options,method:'HEAD'});
      if(!file.ok)throw new Error(`${part.path} is unavailable (HTTP ${file.status}). No installation was started.`);
      const size=Number(file.headers.get('content-length'));
      if(!Number.isSafeInteger(size)||size<=0||/text\/html/i.test(file.headers.get('content-type')||''))
        throw new Error(`${part.path} did not return a valid firmware file. Retry or report this release.`);
      const capacity=(board==='waveshare'?32:board==='xiao_round'?8:16)*1024*1024;
      if(part.offset+size>capacity)throw new Error(`${part.path} exceeds this board's flash capacity.`);
    }));
    return manifest;
  }catch(error){
    if(controller.signal.aborted)throw new Error(signal?.aborted?'Release check cancelled.':'Release check timed out. Check your internet connection and retry.');
    if(error instanceof TypeError)throw new Error('Could not reach the firmware files. Check your internet connection and retry.');
    throw error;
  }finally{
    clearTimeout(timeout);signal?.removeEventListener('abort',cancel);
    controller.abort(); // Cancel remaining HEAD requests if one already failed.
  }
}

export function usbErrorText(error){
  switch(error?.name){
    case 'NotFoundError': return 'No USB port selected. Nothing was installed. If the list is empty, check the data cable, ESP32 drivers and BOOT/RESET instructions below.';
    case 'SecurityError': case 'NotAllowedError': return 'USB access was blocked. Open the direct flasher in a desktop browser, allow serial access, and try again.';
    case 'NetworkError': case 'InvalidStateError': return 'Could not open the USB port. Close other flashers, Detector tools and serial monitors, reconnect the board, then select its port again.';
    default: return `Unable to start the installer: ${error?.message||'USB connection failed'}. Check the connection help below.`;
  }
}

export function installDiagnostics({board,mode,version,status,installerReady,serialSupported,userAgent}){
  return ['Dragon Detector flasher: 2026-10-21',`Board selected: ${board||'none'}`,`Firmware: ${version||'not loaded'}`,
    `Mode: ${mode}`,`Installer loaded: ${!!installerReady}`,`Web Serial available: ${!!serialSupported}`,
    `Browser: ${userAgent}`,`Status: ${String(status).slice(0,1500)}`,
    'Please add the exact model printed on the board and the error shown inside the installer.'].join('\n');
}
