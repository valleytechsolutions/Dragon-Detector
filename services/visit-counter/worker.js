// The public counter stores one integer, not visitor identities or IP addresses.
const ORIGINS = new Set(['https://valleytechsolutions.github.io','https://valleytech-dragon-detector.pages.dev']);
const headers = {'Access-Control-Allow-Origin':'*','Vary':'Origin','Cache-Control':'no-store','Content-Type':'application/json'};
const reply = (value,status=200) => Response.json(value,{status,headers});

export class VisitCounter {
  constructor(ctx) {
    this.sql=ctx.storage.sql;
    this.sql.exec('CREATE TABLE IF NOT EXISTS visits (id INTEGER PRIMARY KEY CHECK(id=1), count INTEGER NOT NULL)');
    this.sql.exec('INSERT OR IGNORE INTO visits (id,count) VALUES (1,0)');
  }
  fetch(request) {
    // A single atomic statement prevents lost increments under concurrent visits.
    const row=this.sql.exec(request.method==='POST'
      ? 'UPDATE visits SET count=count+1 WHERE id=1 RETURNING count'
      : 'SELECT count FROM visits WHERE id=1').one();
    return reply({count:row.count,since:'2026-10-16'});
  }
}

export default {
  async fetch(request,env) {
    if(new URL(request.url).pathname!=='/visits')return reply({error:'Not found'},404);
    if(request.method==='OPTIONS')return new Response(null,{status:204,headers:{...headers,'Access-Control-Allow-Methods':'GET, POST, OPTIONS','Access-Control-Max-Age':'86400'}});
    if(request.method!=='GET'&&request.method!=='POST')return reply({error:'Method not allowed'},405);
    if(request.method==='POST'&&!ORIGINS.has(request.headers.get('Origin')))return reply({error:'Origin not allowed'},403);
    const counter=env.VISITS.get(env.VISITS.idFromName('dragon-detector-flasher'));
    return counter.fetch(new Request('https://counter/visits',{method:request.method}));
  }
};
