"""Opt-in Windows integration test for the 0.11–0.13 full-board BLE Lab.

Requires pyserial, Bleak and PyWinRT. Connects only to synthetic services created
by this script. Exercises central writes, subscriptions, MTU, profile deletion,
advertisement replay and a tracker-sound opcode on a controlled fixture.
Private diagnostic output stays under ignored .work; never publish those logs.
"""
import asyncio,sys,time,uuid,json,re,argparse
from pathlib import Path
from test_ble_lab_live import Board,attributes
from winrt.windows.devices.bluetooth.genericattributeprofile import GattServiceProvider,GattLocalCharacteristicParameters,GattCharacteristicProperties as P,GattProtectionLevel,GattServiceProviderAdvertisingParameters,GattWriteOption
from winrt.windows.storage.streams import DataWriter,DataReader
from bleak import BleakScanner,BleakClient

def buffer(v):
 w=DataWriter();w.write_bytes(v);return w.detach_buffer()
async def main():
 b=Board(args.port);log={'transcript':[]};providers=[];writes=[];loop=asyncio.get_running_loop()
 async def cmd(c,seconds=2):
  print('TEST',c.split()[0:2],flush=True)
  pending=b.port.read(b.port.in_waiting).decode(errors='replace');log['transcript'].append({'pending':pending});b.port.write((c+'\n').encode());r='';end=time.monotonic()+45
  marker='LAB_END' if c in ['lab status','lab events','lab adverts','lab security','lab subscriptions','lab list'] else 'BADGE_INFO' if c=='badge info' else 'BADGE_CHECKSUM' if c=='badge checksum' else 'LAB_OK'
  if c.startswith('screen '):await asyncio.sleep(1);return ''
  while time.monotonic()<end:
   if b.port.in_waiting:r+=b.port.read(b.port.in_waiting).decode(errors='replace')
   if marker in r or 'LAB_ERROR' in r:break
   await asyncio.sleep(.02)
  await asyncio.sleep(.1);log[c]=r;log['transcript'].append({'command':c,'response':r})
  assert marker in r or 'LAB_ERROR' in r,(c,r)
  assert 'Guru Meditation' not in r and 'Rebooting' not in r,(c,r)
  return r
 async def wait_idle():
  for _ in range(30):
   r=await cmd('lab status')
   if 'state=IDLE owned=0' in r:return r
   await asyncio.sleep(1)
  raise AssertionError(r)
 async def wait_connected():
  for _ in range(40):
   r=await cmd('lab status',1)
   if 'state=CONNECTED' in r:return r
   if 'state=IDLE' in r:raise AssertionError(r)
   await asyncio.sleep(1)
  raise AssertionError(r)
 async def provider(svc,ch,props,static):
  result=await GattServiceProvider.create_async(svc);assert result.error==0
  p=result.service_provider;providers.append(p);params=GattLocalCharacteristicParameters();params.characteristic_properties=props;params.read_protection_level=GattProtectionLevel.PLAIN;params.write_protection_level=GattProtectionLevel.PLAIN;params.user_description='Dragon 011 controlled fixture'
  result=await p.service.create_characteristic_async(ch,params);assert result.error==0;c=result.characteristic
  async def handle(args,defer):
   try:
    request=await args.get_request_async()
    if request:
     reader=DataReader.from_buffer(request.value);data=bytearray(reader.unconsumed_buffer_length);reader.read_bytes(data);writes.append(bytes(data))
     if request.option==GattWriteOption.WRITE_WITH_RESPONSE:request.respond()
   finally:defer.complete()
  def callback(sender,args):asyncio.run_coroutine_threadsafe(handle(args,args.get_deferral()),loop)
  async def read_handle(args,defer):
   try:
    request=await args.get_request_async()
    if request:request.respond_with_value(buffer(static[request.offset:]))
   finally:defer.complete()
  def read_callback(sender,args):asyncio.run_coroutine_threadsafe(read_handle(args,args.get_deferral()),loop)
  rt=c.add_read_requested(read_callback)
  token=c.add_write_requested(callback) if props&(P.WRITE|P.WRITE_WITHOUT_RESPONSE) else None
  ad=GattServiceProviderAdvertisingParameters();ad.is_connectable=True;ad.is_discoverable=True;p.start_advertising_with_parameters(ad)
  return p,c,(token,rt),(callback,read_callback)
 async def select(svc,connect=True):
  for _ in range(15):
   adverts=await cmd('lab adverts',2)
   for line in adverts.splitlines():
    if line.startswith('LAB_AD '):
     f=line.split()
     if svc.bytes[::-1].hex() in ''.join(f[5:]):
      target=f[1]+' '+f[2];r=await cmd(('lab inspect ' if connect else 'lab select ')+target)
      assert 'LAB_OK' in r,r
      return target
   await asyncio.sleep(2)
  raise AssertionError('Own synthetic UUID not seen')
 try:
  assert re.search(r'BADGE_INFO 0\.(?:11|12|13)\.\d+ 466 READY Waveshare',await cmd('badge info'))
  original_badge=re.search(r'BADGE_CHECKSUM [^\r\n]+',await cmd('badge checksum'))[0]
  await cmd('screen radar');await cmd('lab stop');await wait_idle();await cmd('lab pairing 0 1 0')
  svc=uuid.uuid4();ch=uuid.uuid4();p,c,token,cb=await provider(svc,ch,P.READ|P.WRITE|P.WRITE_WITHOUT_RESPONSE|P.NOTIFY,b'DD011 fixture')
  await asyncio.sleep(8);target=await select(svc);await wait_connected()
  attrs=attributes(await b.export());a=next(x for x in attrs if x['uuid']==str(ch));i=a['index'];assert a['value']==b'DD011 fixture';print('Fixture properties',a['props'],flush=True);assert a['props']&12==12
  print('PASS own fixture discovery/read',flush=True)
  for op,data in [('write',b'WITH-RESPONSE'),('writenr',b'NO-RESPONSE')]:
   assert 'LAB_OK pending' in await cmd(f'lab {op} {i} '+data.hex());s=await wait_connected();assert 'Write' in s,s;await asyncio.sleep(1);assert data in writes,writes
   print('PASS central '+op+' received by Windows fixture',flush=True)
  assert 'LAB_OK pending' in await cmd('lab subscribe '+str(i));await wait_connected();assert len(c.subscribed_clients)==1
  await c.notify_value_async(buffer(b'NOTIFY-011'));await asyncio.sleep(1);assert b'NOTIFY-011'.hex() in (await cmd('lab events')).replace(' ','')
  assert f'LAB_SUB {i}' in await cmd('lab subscriptions')
  assert 'LAB_OK pending' in await cmd('lab unsubscribe '+str(i));await wait_connected();assert len(c.subscribed_clients)==0;assert 'LAB_SUB ' not in await cmd('lab subscriptions')
  print('PASS notify delivery and CCCD unsubscribe',flush=True)
  assert 'LAB_OK pending' in await cmd('lab mtu 247');await wait_connected();security=await cmd('lab security');assert int(re.search(r'mtu=(\d+)',security)[1])>=23
  print('PASS MTU command/status',flush=True)
  if args.bridge:
   assert 'LAB_OK bridge starting' in await cmd('lab bridge')
   for _ in range(30):
    status=await cmd('lab status')
    if 'state=BRIDGING' in status:break
    await asyncio.sleep(1)
   assert 'state=BRIDGING' in status,status
   dev=await BleakScanner.find_device_by_filter(lambda d,a:str(svc) in a.service_uuids and d.address.lower()!=target.split()[0].lower(),timeout=20)
   assert dev,'Own bridge service was not advertised'
   async with BleakClient(dev,timeout=20) as receiver:
    assert await receiver.read_gatt_char(str(ch))==b'NO-RESPONSE'
    received=[];await receiver.start_notify(str(ch),lambda _,v:received.append(bytes(v)))
    await receiver.write_gatt_char(str(ch),b'BRIDGE-WRITE',response=True)
    for _ in range(20):
     if b'BRIDGE-WRITE' in writes:break
     await asyncio.sleep(.25)
    assert b'BRIDGE-WRITE' in writes,writes
    await c.notify_value_async(buffer(b'BRIDGE-NOTIFICATION'))
    for _ in range(20):
     if b'BRIDGE-NOTIFICATION' in received:break
     await asyncio.sleep(.25)
    assert b'BRIDGE-NOTIFICATION' in received,received
    assert await receiver.read_gatt_char(str(ch))==b'BRIDGE-NOTIFICATION'
    status=await b.command('lab bridge status',2);log['bridge']=status;assert 'writes=1 errors=0' in status,status
   # Windows is both peripheral and client here. It can retain the shared
   # physical link after its client closes; this fixture verifies explicit Stop.
   await cmd('lab stop');await wait_idle();print('PASS bridge cached read, forwarded write, forwarded notification and explicit Stop restoring radar',flush=True)
  name='fixture-011-'+str(int(time.time()));assert 'LAB_OK saved' in await cmd('lab save '+name)
  await cmd('lab stop');await wait_idle();assert 'LAB_OK deleted' in await cmd('lab delete '+name);assert name not in await cmd('lab list')
  await asyncio.sleep(5);target=await select(svc,False);p.stop_advertising()
  assert 'LAB_OK starting' in await cmd('lab replay',3);assert 'state=EMULATING' in await cmd('lab status')
  d=await BleakScanner.find_device_by_filter(lambda d,a:str(svc) in a.service_uuids and d.address.lower()!=target.split()[0].lower(),timeout=12);assert d,'Replay not observed over air';print('PASS captured service advertisement replay at detector identity',flush=True)
  await cmd('lab stop');await wait_idle();print('PASS replay stop / radar restored',flush=True)
  # Known sound characteristic on our own provider; verifies protocol bytes only.
  sound_svc=uuid.UUID('7dfc9000-7d1c-4951-86aa-8d9728f8d66c');sound_ch=uuid.UUID('7dfc9001-7d1c-4951-86aa-8d9728f8d66c')
  p2,c2,t2,cb2=await provider(sound_svc,sound_ch,P.READ|P.WRITE,b'fixture');await asyncio.sleep(8);await select(sound_svc);await wait_connected();assert 'LAB_OK sound request' in await cmd('lab sound');await wait_connected();assert b'\xaf' in writes
  print('PASS tracker sound opcode delivered to controlled fixture',flush=True)
  await cmd('lab stop');await wait_idle();assert original_badge in await cmd('badge checksum');log['result']='PASS'
 finally:
  await cmd('lab stop');await wait_idle();await cmd('lab pairing 0 1 0');await cmd('screen radar');b.port.close()
  for p in providers:
   try:p.stop_advertising()
   except OSError:pass
  Path('.work').mkdir(exist_ok=True)
  Path('.work/ble-lab-extended-results.json').write_text(json.dumps(log,indent=2))
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--port',required=True);parser.add_argument('--bridge',action='store_true');args=parser.parse_args();asyncio.run(main())
