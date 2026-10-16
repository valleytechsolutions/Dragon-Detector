"""Opt-in 0.12 Waveshare format checks with a separate laptop BLE receiver.

Starts only the four new benign format simulators and a named custom lab
advertisement, for four seconds each at two updates/second. No Wi-Fi or pairing
spam modes are started. Requires pyserial + Bleak; logs remain private.
"""
import argparse,asyncio,json,re
from pathlib import Path
from bleak import BleakScanner
from test_ble_lab_live import Board
async def run(port,output):
    b=Board(port);observed={};identity=None;active=-1
    def receive(device,advert):
        nonlocal identity
        if active==24:
            data=advert.manufacturer_data.get(0x004c,b'')
            if data.startswith(b'\x02\x15DRAGONLAB'):identity=device.address;observed[24]=True
        if device.address!=identity:return
        service=advert.service_data.get('0000feaa-0000-1000-8000-00805f9b34fb',b'')
        if active==25 and service[:1]==b'\0' and service[2:12]==b'Dragon-Lab':observed[25]=True
        if active==26 and len(service)==14 and service[0]==0x20:observed[26]=True
        home=advert.service_data.get('0000fcd2-0000-1000-8000-00805f9b34fb',b'')
        if active==27 and len(home)==8 and home[0]==0x40:observed[27]=True
        if active==28 and advert.local_name=='DD-Lab':observed[28]=True
    async def status():
        for _ in range(3):
            value=await b.command('tool status',2)
            if 'MODE_STATUS' in value:return value
        raise AssertionError('No status response')
    try:
        s=await status();assert 'mode=0 ' in s and 'active=0 ' in s and 'recording=0 ' in s
        assert 'owned=0 ' in await b.command('lab status',1)
        await b.command('tool seconds 4');await b.command('tool rate 2')
        assert 'RECON_OK' in await b.command('tool payload 020106070944442d4c6162')
        async with BleakScanner(receive):
            await asyncio.sleep(2)
            for mode in range(24,29):
                active=mode;assert 'RECON_OK' in await b.command('tool start '+str(mode),1)
                await asyncio.sleep(4)
                result=await status();assert 'active=0 ' in result and 'errors=0 ' in result,result
                assert int(re.search(r'accepted=(\d+)',result)[1])>0
                assert observed.get(mode),(mode,'No matching payload at receiver')
                print('PASS advertisement mode',mode,'peer-observed, timeout restored radar',flush=True)
        results={'result':'PASS','observed_modes':list(observed)};output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(results,indent=2))
    finally:
        await b.command('tool stop');await b.command('tool seconds 15');await b.command('tool rate 10');await b.command('screen radar');b.port.close()
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,default=Path('.work/advertisement-templates.json'));a=p.parse_args();asyncio.run(run(a.port,a.output))
