// Collect streamed CSV as it arrives; do not depend on a small serial line queue.
export async function readHistory(link, timeout=30000) {
  let unsubscribe, timer;
  const result = new Promise((resolve,reject)=>{
    let started=false, size=0;const lines=[];
    timer=setTimeout(()=>reject(new Error('History export timed out. Reconnect and try again.')),timeout);
    unsubscribe=link.onLine(line=>{
      if(line===null){reject(link.failure??new Error('USB disconnected during export.'));return;}
      if(line==='HISTORY_ERROR'){reject(new Error('Detection history is unavailable on this board.'));return;}
      if(line==='HISTORY_BEGIN'){started=true;return;}
      if(!started)return;
      if(line==='HISTORY_END'){resolve(lines.join('\r\n')+'\r\n');return;}
      size+=line.length;
      if(lines.length>=600 || size>1024*1024){reject(new Error('Invalid history export size.'));return;}
      lines.push(line);
    });
  });
  // Attach rejection handling before the asynchronous write to avoid a race.
  const write=link.write('history export');
  try {const [,csv]=await Promise.all([write,result]);return csv;}
  finally {clearTimeout(timer);unsubscribe?.();}
}
