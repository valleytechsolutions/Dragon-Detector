// Exercise the staged site in Edge with a simulated device on its Web Serial port.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');

async function run() {
  const base = process.argv[2];
  if (!base) throw new Error('Pass the staging site URL.');
  const browser = await chromium.launch({ channel: process.env.STUDIO_TEST_BROWSER || 'msedge' });
  try {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => {
      const device = window.testDevice = { board:'Elecrow CrowPanel 2.1 Rotary', size:480, commands:[], storage:'READY' };
      Object.defineProperty(navigator,'serial',{configurable:true,value:{async requestPort(){
        let reader, received, expected, payload;
        return {
          async open(){
            this.readable=new ReadableStream({start(c){reader=c;}});
            const reply=line=>reader.enqueue(new TextEncoder().encode(line+'\n'));
            this.writable=new WritableStream({write(data){
              if(expected){
                payload.set(data,received);received+=data.length;reply('BADGE_ACK '+received);
                if(received===expected){device.badge=payload;expected=0;reply('BADGE_OK');}
                return;
              }
              const cmd=new TextDecoder().decode(data).trim();device.commands.push(cmd);
              if(cmd==='badge info')reply('BADGE_INFO 0.14.9 '+device.size+' '+device.storage+' '+device.board);
              else if(cmd.startsWith('badge upload ')){expected=Number(cmd.split(' ')[2]);received=0;payload=new Uint8Array(expected);reply('BADGE_READY');}
              else if(cmd.startsWith('time '))reply('TIME_OK');
              else if(cmd.startsWith('clock offset ')){device.offset=Number(cmd.split(' ')[2]);reply('CLOCK_OK');}
              else if(cmd==='clock status')reply('CLOCK_STATUS rtc=1 valid=1 epoch='+Math.floor(Date.now()/1000)+' offset='+device.offset);
              else if(cmd==='orientation status')reply('ORIENTATION_STATUS manual=1 degrees=0');
              else if(cmd==='clock hardware')reply('CLOCK_HARDWARE detected=1 valid=1 epoch='+Math.floor(Date.now()/1000));
              else throw new Error('Unexpected command: '+cmd);
            }});
          },
          async close(){this.readable=null;this.writable=null;}
        };
      }}});
    });
    await page.goto(base);
    const {boardProfiles,supportsBadge}=await import('../web/board-profiles.js');
    const {decodeTheme}=await import('../web/theme-codec.js');
    const {encodeBadge}=await import('../web/badge-codec.js');
    await page.waitForFunction(()=>document.querySelector('#image-info').textContent.includes('source pixels'));
    const profiles=Object.keys(boardProfiles).filter(supportsBadge);
    assert.deepEqual((await page.locator('#badge-board option').evaluateAll(a=>a.map(x=>x.value))).sort(),profiles.slice().sort());
    for(const id of profiles){
      await page.selectOption('#badge-board',id);
      await Promise.all([page.waitForResponse(r=>r.url().endsWith('/assets/dragon-detector.png')&&r.ok()),page.click('#stock-bwm')]);
      assert.equal(await page.locator('#preview').evaluate(c=>c.width),boardProfiles[id].size);
      assert.equal(await page.locator('#preview').evaluate(c=>c.height),boardProfiles[id].height||boardProfiles[id].size);
    }
    console.log('PASS stock artwork on all '+profiles.length+' badge profiles');
    await page.locator('#image').setInputFiles('web/assets/dragon-detector-icon.png');
    await page.waitForFunction(()=>document.querySelector('#image-info').textContent.includes('icon.png'));
    await page.locator('#rotate-right').click();await page.locator('#mirror').click();
    await page.locator('.badge-text-controls summary').click();
    await page.fill('#badge-text','VT TEST');
    const pngPromise=page.waitForEvent('download');
    await page.click('#download-badge');
    const png=await fs.readFile(await (await pngPromise).path());
    assert.equal(png.readUInt32BE(16),await page.locator('#preview').evaluate(c=>c.width));
    await page.click('#connect');
    await page.waitForFunction(()=>!document.querySelector('#upload').disabled);
    assert.deepEqual(await page.evaluate(()=>testDevice.commands),['badge info']);
    const pixels=await page.locator('#preview').evaluate(c=>Array.from(c.getContext('2d').getImageData(0,0,c.width,c.height).data));
    await page.click('#upload');
    await page.waitForFunction(()=>document.querySelector('#studio-status').textContent.includes('Saved and displayed'));
    assert.deepEqual(new Uint8Array(await page.evaluate(()=>Array.from(testDevice.badge))),encodeBadge(pixels,480));
    await page.click('#disconnect');
    await page.evaluate(board=>Object.assign(testDevice,{board,size:410}),boardProfiles.waveshare_206.firmwareName);
    await page.click('#connect');await page.waitForFunction(()=>!document.querySelector('#upload').disabled);
    assert.deepEqual(await page.locator('#preview').evaluate(c=>[c.width,c.height,c.style.borderRadius]),[410,502,'12px']);
    const rectangle=await page.locator('#preview').evaluate(c=>Array.from(c.getContext('2d').getImageData(0,0,410,502).data));
    await page.click('#upload');await page.waitForFunction(()=>document.querySelector('#studio-status').textContent.includes('Saved and displayed'));
    assert.deepEqual(new Uint8Array(await page.evaluate(()=>Array.from(testDevice.badge))),encodeBadge(rectangle,410,502));
    const rectangleDownload=page.waitForEvent('download');await page.click('#download-badge');
    const rectanglePng=await fs.readFile(await (await rectangleDownload).path());
    assert.deepEqual([rectanglePng.readUInt32BE(16),rectanglePng.readUInt32BE(20)],[410,502]);
    await page.locator('.studio').screenshot({path:'.work/waveshare206-badge-studio.png'});
    await page.click('#theme-tab');
    assert.equal(await page.locator('#theme-read').isDisabled(),true);
    assert.equal(await page.locator('#theme-upload').isDisabled(),true);
    assert.match(await page.locator('#theme-upload').textContent(),/rectangular displays/);
    await page.click('#firmware-tab');
    await page.click('#disconnect');
    console.log('PASS 410 x 502 preview, PNG and exact rectangular badge bytes over simulated USB');
    await page.evaluate(()=>testDevice.storage='NO_STORAGE');await page.click('#connect');
    await page.waitForFunction(()=>document.querySelector('#studio-status').textContent.includes('mount its badge storage'));
    assert.equal(await page.locator('#download-badge').isEnabled(),true);
    console.log('PASS custom art, transforms, text, PNG download, exact badge bytes over mock USB and missing storage');
    await page.click('#theme-tab');
    await page.click('#theme-use-mark');
    await page.waitForFunction(()=>!document.querySelector('#theme-export').disabled);
    await page.selectOption('#theme-palette','Valleytech');await page.selectOption('#theme-face-style','Instrument dial');
    for(const view of ['clock','radar','counter'])await page.click('[data-theme-view='+view+']');
    const filePromise=page.waitForEvent('download');await page.click('#theme-export');
    const file=await (await filePromise).path(),data=decodeTheme(new Uint8Array(await fs.readFile(file)));
    assert.equal(data.theme.accent,'#9ff4ff');assert.equal(data.theme.secondary,'#d6b46e');assert.equal(data.theme.sweep,1);
    await page.selectOption('#theme-palette','Ultraviolet');
    await page.locator('#theme-import').setInputFiles(file);
    await page.waitForFunction(()=>document.querySelector('#theme-palette').value==='Valleytech');
    assert.equal(await page.inputValue('#theme-face-style'),'Instrument dial');
    await page.check('#custom-enabled');await page.click('#firmware-tab');
    assert.equal(await page.locator('#install-custom-summary').isVisible(),true);
    console.log('PASS clock/radar/counter preview and theme preset export/import');
    await page.evaluate(board=>Object.assign(testDevice,{board,storage:'READY',size:466}),boardProfiles.stopwatch.firmwareName);
    await page.click('[data-board=stopwatch]');await page.click('#finish-clock');
    await page.waitForFunction(()=>document.querySelector('#clock-setup-status').textContent.includes('ticking verified'));
    await page.click('[data-board=watcher]');await page.click('#finish-clock');
    await page.waitForFunction(()=>document.querySelector('#clock-setup-status').textContent.includes('identity does not match'));
    for(const section of ['detector','radio-tools','ble-lab','notebook','sound'])assert.equal(await page.locator('#'+section).count(),0);
    await page.setViewportSize({width:390,height:844});
    for(const tab of ['#theme-tab','#firmware-tab']){
      await page.click(tab);
      assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
    }
    assert.deepEqual(errors,[]);
    console.log('PASS standalone RTC sync, wrong-board rejection, mobile layouts and no page errors');
  } finally { await browser.close(); }
}
run().catch(e=>{console.error(e);process.exitCode=1;});
