"""Opt-in hardware checks using only our synthetic peripheral. Never scans/connects to arbitrary devices.

Windows: pip install bleak pyserial (Bleak installs the WinRT bindings).
python tools/test_ble_lab_live.py --port PORT --mode clone
python tools/test_ble_lab_live.py --port PORT --mode discover
"""
import argparse, asyncio, json, struct, time, uuid, zlib
from pathlib import Path
import serial
SERVICE='dd980001-7114-452b-8e26-5f44c65a4b90'
CHARACTERISTIC='dd980002-7114-452b-8e26-5f44c65a4b90'
TEST_SERVICE='dd980003-7114-452b-8e26-5f44c65a4b90'
TEST_CHARACTERISTIC='dd980004-7114-452b-8e26-5f44c65a4b90'
class Board:
    def __init__(self,port):
        self.port=serial.Serial(port=None,baudrate=115200,timeout=.05)
        self.port.dtr=False;self.port.rts=False;self.port.port=port;self.port.open();self.pending=b''
    async def command(self,command,seconds=.6):
        self.port.reset_input_buffer();self.port.write((command+'\n').encode());data=b'';end=time.monotonic()+seconds
        while time.monotonic()<end:
            if self.port.in_waiting:data+=self.port.read(self.port.in_waiting)
            await asyncio.sleep(.02)
        return data.decode(errors='replace')
    async def idle(self):
        for _ in range(20):
            s=await self.command('lab status')
            if 'state=IDLE owned=0' in s:return s
        raise AssertionError('Lab did not release radio')
    async def export(self):
        log=await self.command('lab export',5);lines=log.splitlines();header=next(x for x in lines if x.startswith('LAB_FILE ')).split();n=int(header[1]);data=bytearray()
        for line in lines:
            if line.startswith('LAB_DATA '):
                _,offset,hexdata=line.split();assert int(offset)==len(data);data.extend(bytes.fromhex(hexdata))
        assert len(data)==n and zlib.crc32(data)==int(header[2],16) and 'LAB_FILE_END' in lines
        return bytes(data)
def attributes(data):
    assert data[:4]==b'DDG1';count=struct.unpack_from('<H',data,6)[0];assert len(data)==580+559*count
    return [dict(index=i,kind=data[o],props=data[o+1],flags=data[o+2],uuid=data[o+10:o+47].split(b'\0')[0].decode(),value=data[o+47:o+47+struct.unpack_from('<H',data,o+8)[0]]) for i in range(count) for o in [580+559*i]]
async def clone(board):
    from bleak import BleakScanner,BleakClient
    assert 'LAB_OK demo' in await board.command('lab demo')
    assert 'LAB_OK starting' in await board.command('lab clone',2)
    dev=await BleakScanner.find_device_by_filter(lambda d,a:a.local_name=='Dragon Lab Demo',timeout=15)
    assert dev,'Our demo advertisement not found';print('PASS demo advertisement',flush=True)
    async with BleakClient(dev,timeout=15) as c:
        assert await c.read_gatt_char('2a19')==b'W';print('PASS battery read',flush=True)
        assert await c.read_gatt_char(CHARACTERISTIC)==b'Dragon Lab'
        notifications=[];await c.start_notify(CHARACTERISTIC,lambda _,v:notifications.append(bytes(v)))
        await c.write_gatt_char(CHARACTERISTIC,b'CLONE-VERIFIED',response=True)
        assert await c.read_gatt_char(CHARACTERISTIC)==b'CLONE-VERIFIED';print('PASS writable characteristic',flush=True)
        await board.command('lab notify 3',2);assert b'CLONE-VERIFIED' in notifications;print('PASS notification delivery',flush=True)
        ch=c.services.get_characteristic(CHARACTERISTIC);d=next(d for d in ch.descriptors if d.uuid.startswith('00002901'))
        assert (await c.read_gatt_descriptor(d.handle)).startswith(b'Lab loopback');print('PASS descriptor',flush=True)
    events=await board.command('lab events');assert '434c4f4e452d5645524946494544' in events.replace(' ','');print('PASS callback event queue',flush=True)
    await board.command('lab stop',2);await board.idle();print('PASS return to radar',flush=True)
async def discover(board):
    from winrt.windows.devices.bluetooth import BluetoothAdapter
    from winrt.windows.devices.bluetooth.genericattributeprofile import GattServiceProvider,GattLocalCharacteristicParameters,GattCharacteristicProperties,GattProtectionLevel,GattServiceProviderAdvertisingParameters
    from winrt.windows.storage.streams import DataWriter
    def buffer(data):
        w=DataWriter();w.write_bytes(data);return w.detach_buffer()
    adapter=await BluetoothAdapter.get_default_async();assert adapter and adapter.is_peripheral_role_supported
    test_service=uuid.uuid4() # Never select an older cached Windows advertisement.
    result=await GattServiceProvider.create_async(test_service);assert result.error==0,result.error;provider=result.service_provider
    params=GattLocalCharacteristicParameters();params.characteristic_properties=GattCharacteristicProperties.READ|GattCharacteristicProperties.NOTIFY;params.read_protection_level=GattProtectionLevel.PLAIN;params.static_value=buffer(b'DD live GATT fixture');params.user_description='Dragon detector integration test'
    result=await provider.service.create_characteristic_async(uuid.UUID(TEST_CHARACTERISTIC),params);assert result.error==0,result.error;characteristic=result.characteristic
    ad=GattServiceProviderAdvertisingParameters();ad.is_connectable=True;ad.is_discoverable=True;provider.start_advertising_with_parameters(ad)
    try:
        await asyncio.sleep(8);print('Test provider advertisement status:',provider.advertisement_status,flush=True)
        adverts=await board.command('lab adverts',3);needle=test_service.bytes[::-1].hex();target=None
        for line in adverts.splitlines():
            if line.startswith('LAB_AD '):
                fields=line.split();payload=''.join(fields[5:])
                if needle in payload:target=fields[1]+' '+fields[2];break
        assert target,'Our test service UUID was not received by the detector'
        reply=await board.command('lab inspect '+target,1);assert 'LAB_OK connecting' in reply,reply
        for _ in range(30):
            status=await board.command('lab status',2)
            if 'state=CONNECTED' in status:break
            if 'state=IDLE' in status:raise AssertionError(status)
        assert 'state=CONNECTED' in status,status
        profile=await board.export();attrs=attributes(profile);print('Discovered',len(attrs),'attributes; partial:',profile[63],flush=True);found=next(a for a in attrs if a['uuid']==TEST_CHARACTERISTIC)
        assert found['value']==b'DD live GATT fixture' and found['flags']==0,found;print('PASS full discovery and readable value',flush=True)
        assert any(a['kind']==2 and a['uuid']=='2901' for a in attrs);print('PASS descriptor discovery',flush=True)
        assert 'LAB_OK pending' in await board.command('lab subscribe '+str(found['index']),1)
        for _ in range(15):
            status=await board.command('lab status',1)
            if 'state=CONNECTED' in status:break
        assert 'Subscribed;' in status,status
        print('PASS CCCD subscription; Windows clients:',len(characteristic.subscribed_clients),flush=True)
        results=await characteristic.notify_value_async(buffer(b'DD live notification'));await asyncio.sleep(1)
        print('Notification results:',[str(r.status) for r in results],flush=True)
        events=await board.command('lab events');assert b'DD live notification'.hex() in events.replace(' ',''),events;print('PASS live remote notifications',flush=True)
        name='test-'+str(int(time.time()));assert 'LAB_OK saved' in await board.command('lab save '+name)
        await board.command('lab stop',2);await board.idle()
        assert 'added=0 removed=0 changed=0' in await board.command('lab compare '+name);print('PASS saved profile CRC and comparison',flush=True)
    finally:provider.stop_advertising()
async def main(args):
    board=Board(args.port)
    try:
        await asyncio.sleep(1);info=await board.command('badge info');assert '466 READY Waveshare ESP32-S3-Touch-AMOLED-1.75C' in info,'Wrong board; no test commands sent'
        if args.mode=='clone':await clone(board)
        else:await discover(board)
    finally:
        await board.command('lab stop',2);board.port.close()
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--mode',choices=['clone','discover'],required=True);asyncio.run(main(p.parse_args()))
