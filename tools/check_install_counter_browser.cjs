// Real bundled installer UI with mocked USB and counter. Never writes hardware
// or the production counter. Serve the staged files under the production origin.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
  const root=path.resolve(process.argv[2]||'.work/site-install-counter');
  const base='https://valleytech-dragon-detector.pages.dev/';
  const browser=await chromium.launch({channel:'msedge',headless:true});
  try{
    const context=await browser.newContext({viewport:{width:1440,height:1000}});
    let count=0,reads=0,posts=0;const ids=new Set(),errors=[];
    await context.route('**/*',async route=>{
      const req=route.request(),url=new URL(req.url());
      if(url.hostname==='dragon-detector-visits.valleytechsolutions-dd.workers.dev'){
        assert.equal(url.pathname,'/installs');
        if(req.method()==='POST'){posts++;const {id}=req.postDataJSON();if(!ids.has(id)){ids.add(id);count++;}}
        else if(req.method()==='GET')reads++;
        return route.fulfill({status:req.method()==='OPTIONS'?204:200,headers:{'Access-Control-Allow-Origin':'*','Access-Control-Allow-Methods':'GET, POST, OPTIONS','Access-Control-Allow-Headers':'Content-Type'},contentType:'application/json',body:req.method()==='OPTIONS'?'':JSON.stringify({count})});
      }
      if(url.origin!==new URL(base).origin)return route.abort();
      const file=path.resolve(root,'.'+(url.pathname==='/'?'/index.html':decodeURIComponent(url.pathname)));
      if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile())return route.fulfill({status:404,body:''});
      const types={'.html':'text/html','.js':'text/javascript','.json':'application/json','.css':'text/css','.png':'image/png','.jpg':'image/jpeg','.bin':'application/octet-stream'};
      return route.fulfill({status:200,contentType:types[path.extname(file)]||'application/octet-stream',headers:{'Content-Length':String(fs.statSync(file).size)},body:req.method()==='HEAD'?'':fs.readFileSync(file)});
    });
    const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));
    await page.goto(base);await page.waitForFunction(()=>document.querySelector('#install-count').textContent==='0');
    await page.reload();await page.waitForFunction(()=>document.querySelector('#install-count').textContent==='0');assert.equal(posts,0);assert.equal(reads,2);
    await page.waitForFunction(()=>customElements.get('dragon-clock-install-dialog'));
    assert.equal(await page.locator('[data-install-counter] span').textContent(),'INSTALLS');
    const boards=await page.locator('[data-board]').evaluateAll(items=>items.map(e=>e.dataset.board));assert.equal(boards.length,11);
    for(const board of boards){
      await page.locator(`[data-board="${board}"]`).click();
      await page.waitForFunction(()=>!document.getElementById('download').hidden||!document.getElementById('retry-release').hidden);
      assert.equal(await page.locator('#retry-release').isHidden(),true,board);
    }
    await page.locator('[data-board="stopwatch"]').click();await page.waitForFunction(()=>!document.getElementById('download').hidden);
    await page.locator('#hardware-confirm').check();await page.locator('#auto-clock').uncheck();
    await page.evaluate(()=>Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>{throw new DOMException('Cancelled','NotFoundError');}}}));
    await page.locator('#install-button').click();await page.waitForFunction(()=>document.getElementById('clock-setup-status').textContent.includes('Nothing was installed'));assert.equal(posts,0);
    await page.evaluate(()=>Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>({readable:{},writable:{},async open(){},async close(){this.readable=null;this.writable=null;},getInfo(){return {};}})}}));
    for(const state of ['writing','error','finished','finished']){
      await page.locator('#install-button').click();await page.waitForFunction(()=>document.querySelector('dragon-clock-install-dialog')?._manifest);
      await page.evaluate(state=>{const d=document.querySelector('dragon-clock-install-dialog');d._installState={state,message:'Mock error'};d._reportInstall();d._reportInstall();d._closeDialog();},state);
      await page.waitForFunction(()=>!document.querySelector('dragon-clock-install-dialog'));
      await page.waitForFunction(()=>!document.getElementById('install-button').disabled);
      if(state==='finished')await page.waitForFunction(()=>document.getElementById('install-count').textContent!=='0');
      else assert.equal(posts,0);
    }
    await page.waitForFunction(()=>document.getElementById('install-count').textContent==='2');assert.equal(posts,2);assert.deepEqual(errors,[]);
    await page.locator('header').screenshot({path:'.work/install-counter-header-test.png'});
    await page.setViewportSize({width:390,height:844});
    const dimensions=await page.evaluate(()=>[document.documentElement.scrollWidth,innerWidth]);assert.ok(dimensions[0]<=dimensions[1]+1);
    await page.locator('header').screenshot({path:'.work/install-counter-header-mobile-test.png'});
    console.log('PASS 11 board selectors, read-only reloads, cancelled/failed writes excluded, completed/repeated flashes counted once, desktop/mobile layout; all USB and counter writes mocked');
  }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
