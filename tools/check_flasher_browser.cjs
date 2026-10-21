// Run with Playwright available on NODE_PATH. Pass a deployed HTTPS URL, or a
// local site directory. USB calls are mocked: this never flashes hardware.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const http=require('node:http');

(async()=>{
  let server,browser;
  try{
    let base=process.argv[2];
    if(!/^https?:/.test(base||'')){
      const root=path.resolve(base||'.work/flasher-site');
      server=http.createServer((req,res)=>{
        const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
        const file=path.resolve(root,'.'+(name==='/'?'/index.html':name));
        if(!file.startsWith(root+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404);res.end();return;}
        const types={'.html':'text/html','.js':'text/javascript','.json':'application/json','.css':'text/css','.png':'image/png','.jpg':'image/jpeg','.bin':'application/octet-stream'};
        res.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream','Content-Length':fs.statSync(file).size});
        if(req.method==='HEAD')res.end();else fs.createReadStream(file).pipe(res);
      });
      await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
      base=`http://127.0.0.1:${server.address().port}/`;
    }
    browser=await chromium.launch({channel:'msedge',headless:true});
    const context=await browser.newContext({viewport:{width:1440,height:1000}});
    const page=await context.newPage(),errors=[],externalScripts=[];
    page.on('pageerror',e=>errors.push(e.message));
    await context.route('**/*',route=>{
      const req=route.request();
      if(new URL(req.url()).origin!==new URL(base).origin){
        if(req.resourceType()==='script')externalScripts.push(req.url());
        return route.abort();
      }
      return route.continue();
    });
    await page.goto(base);
    assert.equal(await page.locator('script[src^="app.js"]').getAttribute('src'),'app.js?v=2026-10-21-flasher','site packaging must preserve cache-busting versions');
    await page.waitForFunction(()=>customElements.get('dragon-clock-install-dialog'));
    const boards=await page.locator('[data-board]').evaluateAll(items=>items.map(e=>e.dataset.board));
    assert.equal(boards.length,9);
    async function select(board,mode='factory'){
      await page.locator(`[data-board="${board}"]`).click();
      await page.locator('#install-mode').selectOption(mode);
      await page.waitForFunction(()=>!document.getElementById('download').hidden);
      assert.equal(await page.locator('#hardware-confirm').isChecked(),false);
      assert.equal(await page.locator('#install-button').isDisabled(),true);
      await page.locator('#hardware-confirm').check();
      await page.waitForFunction(()=>!document.getElementById('install-button').disabled);
    }
    for(const board of boards)for(const mode of ['factory','update'])await select(board,mode);
    console.log('PASS all 18 board/mode combinations, with external hosts blocked');
    const broken='**/firmware/waveshare_lite/firmware.bin';
    await page.route(broken,r=>r.fulfill({status:404,body:''}));
    await page.locator('[data-board="waveshare_lite"]').click();
    await page.waitForFunction(()=>!document.getElementById('retry-release').hidden);
    assert.match(await page.locator('#board-status').innerText(),/unavailable/);
    assert.equal(await page.locator('#install-button').isDisabled(),true);
    await page.unroute(broken);await page.locator('#retry-release').click();
    await page.waitForFunction(()=>!document.getElementById('download').hidden);
    await page.locator('#hardware-confirm').check();
    console.log('PASS missing binary blocks install; retry recovers');
    await page.evaluate(()=>Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>{throw new DOMException('Cancelled','NotFoundError');}}}));
    await page.locator('#install-button').click();
    await page.waitForFunction(()=>document.getElementById('clock-setup-status').textContent.includes('Nothing was installed'));
    assert.equal(await page.locator('#finish-clock').isHidden(),true);
    await page.evaluate(()=>Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>{
      window.mockPort={readable:{},writable:{},opens:0,closes:0,async open(){this.opens++;},async close(){this.closes++;this.readable=null;this.writable=null;},getInfo(){return {};}};return window.mockPort;
    }}}));
    await page.locator('#install-button').click();
    await page.waitForFunction(()=>document.querySelector('dragon-clock-install-dialog')?._manifest);
    await page.evaluate(()=>document.querySelector('dragon-clock-install-dialog')._closeDialog());
    await page.waitForFunction(()=>document.getElementById('clock-setup-status').textContent.includes('setup were skipped'));
    assert.deepEqual(await page.evaluate(()=>[mockPort.opens,mockPort.closes]),[1,1]);
    assert.equal(await page.locator('#finish-clock').isHidden(),true);
    console.log('PASS port chooser cancellation and real dialog close skip clock setup');
    await page.locator('#install-button').click();
    await page.waitForFunction(()=>document.querySelector('dragon-clock-install-dialog')?._manifest);
    await page.evaluate(()=>{const d=document.querySelector('dragon-clock-install-dialog');d._installState={state:'error',message:'Test write failed'};d._closeDialog();});
    await page.waitForFunction(()=>document.getElementById('clock-setup-status').textContent.includes('Test write failed'));
    assert.equal(await page.locator('#finish-clock').isHidden(),true);
    assert.deepEqual(await page.evaluate(()=>[mockPort.opens,mockPort.closes]),[1,1]);
    console.log('PASS failed writes do not start clock setup');
    await page.locator('#auto-clock').uncheck();
    await page.locator('#install-button').click();
    await page.waitForFunction(()=>document.querySelector('dragon-clock-install-dialog')?._manifest);
    await page.evaluate(()=>{const d=document.querySelector('dragon-clock-install-dialog');d._installState={state:'finished'};d._closeDialog();});
    await page.waitForFunction(()=>document.getElementById('clock-setup-status').textContent.includes('Firmware installed. Automatic clock setup was turned off'));
    console.log('PASS completed install reports success with clock opt-out');
    await page.locator('#copy-install-details').click();
    assert.match(await page.locator('#install-details').inputValue(),/Waveshare ESP32-S3-LCD-1.28 Lite/);
    for(const href of ['https://espterminator.com/downloads/CP210x_Universal_Windows_Driver.zip','http://www.wch-ic.com/downloads/CH341SER_EXE.html','https://www.wch-ic.com/downloads/CH343SER_ZIP.html'])
      assert.equal(await page.locator(`#esp32-drivers a[href="${href}"]`).count(),1);
    assert.deepEqual(errors,[]);assert.deepEqual(externalScripts,[]);
    console.log('PASS driver links and troubleshooting report; no JavaScript errors');
    await page.locator('#radar-motion').click();
    await page.locator('#esp32-drivers').screenshot({path:'.work/flasher-drivers.png'});
    const mobile=await context.newPage();
    await mobile.setViewportSize({width:390,height:844});
    await mobile.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined,configurable:true}));
    await mobile.goto(base);await mobile.waitForFunction(()=>document.getElementById('install-message').textContent.includes('desktop Chrome'));
    assert.equal(await mobile.locator('#install-button').isDisabled(),true);
    const widths=await mobile.evaluate(()=>[document.documentElement.scrollWidth,innerWidth]);
    assert.ok(widths[0]<=widths[1]+1,`Mobile overflow: ${widths}`);
    await mobile.locator('#esp32-drivers').screenshot({path:'.work/flasher-drivers-mobile.png'});
    console.log('PASS unsupported mobile browser guidance and 390px layout');
  }finally{await browser?.close();if(server)await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
