"""Opt-in serial test of ordinary screens and synthetic passive Hunt targets.

No RF transmission is started. The test requires idle labs and Dragon radar
mode, preserves badge/settings, and leaves the detector on Radar without Hunt.
Output may contain nearby identifiers: keep logs private.
"""
import argparse,asyncio,json,re
from pathlib import Path
from test_ble_lab_live import Board
def scan(text):
    line=next(x for x in text.splitlines() if x.startswith('SCAN_STATUS '))
    return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
async def run(port,output):
    b=Board(port);results={}
    async def query(command,marker):
        for _ in range(3):
            reply=await b.command(command,2)
            if marker in reply:return reply
        raise AssertionError((command,'Missing response marker',reply))
    try:
        status=await query('tool status','MODE_STATUS')
        if 'MODE_STATUS' in status:assert 'mode=0 ' in status and 'active=0 ' in status and 'recording=0 ' in status
        lab=await query('lab status','LAB_STATUS')
        if 'LAB_STATUS' in lab:assert 'owned=0 ' in lab
        before=await query('badge checksum','BADGE_CHECKSUM');results['badge_before']=before
        cases=['screen radar','screen badge','screen settings','screen notebook','screen notebook-ble','hunt BLE 1 c2:dd:12:00:00:01 0','hunt WiFi 0 02:dd:12:00:00:02 6']
        for command in cases:
            await b.command(command,1);a=scan(await query('scan status','SCAN_STATUS'))
            await asyncio.sleep(8);z=scan(await query('scan status','SCAN_STATUS'))
            assert z['enabled']==1 and z['lab']==0,(command,z)
            assert z['ble_ms']>a['ble_ms'] and z['wifi_ms']>a['wifi_ms'],(command,a,z)
            assert all(z['ch'+str(i)]>a['ch'+str(i)] for i in range(1,12)),(command,a,z)
            results[command]={'before':a,'after':z};print('PASS',command,'both radios + channels 1–11',flush=True)
        await b.command('hunt stop');assert 'RADIO_ALWAYS_ON' in await b.command('pause')
        results['health']=await query('status','HEALTH');results['badge_after']=await query('badge checksum','BADGE_CHECKSUM')
        checksum=lambda s:re.search(r'bytes=\d+ crc=[0-9a-f]+ boot=\d',s).group()
        assert checksum(before)==checksum(results['badge_after'])
        results['selftest']=await query('selftest','SELFTEST');assert 'failed=0' in results['selftest']
        results['result']='PASS';output.parent.mkdir(exist_ok=True,parents=True);output.write_text(json.dumps(results,indent=2));print('PASS badge preserved, pause rejected, self-tests passed')
    finally:
        await b.command('hunt stop');await b.command('screen radar');b.port.close()
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,default=Path('.work/scan-continuity.json'));a=p.parse_args();asyncio.run(run(a.port,a.output))
