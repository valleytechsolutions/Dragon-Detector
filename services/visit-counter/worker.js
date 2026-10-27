// Keep the existing binding/class/instance so the legacy visits remain intact.
// Install IDs identify one flash attempt, never a visitor or device.
const ORIGINS = new Set(['https://valleytechsolutions.github.io','https://valleytech-dragon-detector.pages.dev']);
const headers = {'Access-Control-Allow-Origin':'*','Vary':'Origin','Cache-Control':'no-store','Content-Type':'application/json'};
const reply = (value,status=200) => Response.json(value,{status,headers});

export class VisitCounter {
  constructor(ctx) {
    this.sql=ctx.storage.sql;
    this.sql.exec('CREATE TABLE IF NOT EXISTS visits (id INTEGER PRIMARY KEY CHECK(id=1), count INTEGER NOT NULL)');
    this.sql.exec('INSERT OR IGNORE INTO visits (id,count) VALUES (1,0)');
    this.sql.exec('CREATE TABLE IF NOT EXISTS installs (id INTEGER PRIMARY KEY CHECK(id=1), count INTEGER NOT NULL)');
    this.sql.exec('INSERT OR IGNORE INTO installs (id,count) VALUES (1,0)');
    this.sql.exec('CREATE TABLE IF NOT EXISTS install_events (id TEXT PRIMARY KEY, created_at INTEGER NOT NULL)');
    this.sql.exec('CREATE TRIGGER IF NOT EXISTS count_install AFTER INSERT ON install_events BEGIN UPDATE installs SET count=count+1 WHERE id=1; END');
  }
  async fetch(request) {
    if(new URL(request.url).pathname==='/visits'){
      // Cached old pages may still POST here. Freeze visits instead of
      // relabeling them as installs or continuing to count page loads.
      return reply({count:this.sql.exec('SELECT count FROM visits WHERE id=1').one().count,since:'2026-10-16',frozen:true});
    }
    if(request.method==='POST'){
      let data;
      // Bounded body even when Content-Length is absent or untrusted.
      if(!request.body)return reply({error:'Install ID required'},400);
      const reader=request.body.getReader();let body='';let size=0;
      try{while(true){const {done,value}=await reader.read();if(done)break;size+=value.byteLength;if(size>128){await reader.cancel();return reply({error:'Body too large'},413);}body+=new TextDecoder().decode(value);}
        data=JSON.parse(body);
      }catch{return reply({error:'Invalid JSON'},400);}
      if(!data||typeof data.id!=='string'||!/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(data.id))return reply({error:'Invalid install ID'},400);
      const now=Date.now();
      // Retain random retry IDs for seven days, without retaining identities.
      this.sql.exec('DELETE FROM install_events WHERE created_at < ?',now-7*86400000);
      // INSERT + trigger is one atomic statement. Repeated delivery of the same
      // completion cannot increment twice, even after a response is lost.
      this.sql.exec('INSERT OR IGNORE INTO install_events (id,created_at) VALUES (?,?)',data.id.toLowerCase(),now);
    }
    return reply({count:this.sql.exec('SELECT count FROM installs WHERE id=1').one().count,since:'2026-10-26'});
  }
}

export default {
  async fetch(request,env) {
    const path=new URL(request.url).pathname;
    if(path!=='/visits'&&path!=='/installs')return reply({error:'Not found'},404);
    if(request.method==='OPTIONS')return new Response(null,{status:204,headers:{...headers,'Access-Control-Allow-Methods':'GET, POST, OPTIONS','Access-Control-Allow-Headers':'Content-Type','Access-Control-Max-Age':'86400'}});
    if(request.method!=='GET'&&request.method!=='POST')return reply({error:'Method not allowed'},405);
    if(request.method==='POST'&&!ORIGINS.has(request.headers.get('Origin')))return reply({error:'Origin not allowed'},403);
    const counter=env.VISITS.get(env.VISITS.idFromName('dragon-detector-flasher'));
    return counter.fetch(new Request('https://counter'+path,request));
  }
};
