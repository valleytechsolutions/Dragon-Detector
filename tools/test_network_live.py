"""Opt-in Waveshare check against a temporary Windows 11 24H2 mobile hotspot session.

Requires pyserial, winrt-Windows.Networking.NetworkOperators and
winrt-Windows.Networking.Connectivity. Uses an empty profile slot only. Stops the
temporary AP and removes its detector profile on exit; never reads Wi-Fi keys.
Run: python tools/test_network_live.py --port COM51
"""
import argparse, asyncio, json, re, time, secrets
from pathlib import Path
from test_ble_lab_live import Board
from winrt.windows.networking.connectivity import NetworkInformation
from winrt.windows.networking.networkoperators import NetworkOperatorTetheringManager,NetworkOperatorTetheringSessionAccessPointConfiguration,TetheringWiFiBand,TetheringWiFiAuthenticationKind,TetheringOperationalState

async def run(port,output):
 b=Board(port);manager=None;hotspot=False;created=False;slot=None;saved=[];log={}
 async def cmd(c,delay=.5):
  r=await b.command(c,delay)
  assert 'Guru Meditation' not in r and 'Rebooting' not in r,(c,r)
  if not c.startswith('net profile put '):log.setdefault(c,[]).append(r)
  return r
 async def wait_state(test,seconds=28):
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   s=await cmd('net status')
   if test(s):return s
   await asyncio.sleep(.3)
  raise AssertionError(s)
 try:
  initial=''
  for _ in range(15):
   initial=await cmd('tool status',1)
   if 'MODE_STATUS ' in initial:break
  assert 'active=0' in initial and 'recording=0' in initial,initial
  assert 'active=0' in await cmd('net status')
  original_badge=await cmd('badge checksum',1.5)
  profiles=await cmd('net profiles');used={int(x) for x in re.findall(r'RECON_NETWORK_PROFILE (\d+) ',profiles)}
  slot=next(i for i in range(7,-1,-1) if i not in used)
  # Fresh identity avoids Windows reusing a prior per-session hotspot credential.
  ssid='Dragon-Network-'+secrets.token_hex(3);password=secrets.token_hex(16)
  manager=NetworkOperatorTetheringManager.create_from_connection_profile(NetworkInformation.get_internet_connection_profile())
  assert manager.tethering_operational_state==TetheringOperationalState.OFF,'Existing hotspot is active; leave it alone'
  configuration=NetworkOperatorTetheringSessionAccessPointConfiguration();configuration.ssid=ssid;configuration.passphrase=password;configuration.band=TetheringWiFiBand.TWO_POINT_FOUR_GIGAHERTZ;configuration.authentication_kind=TetheringWiFiAuthenticationKind.WPA2
  result=await manager.start_tethering_async(configuration);assert int(result.status)==0,result.status;hotspot=True
  await asyncio.sleep(3)
  assert 'RECON_OK' in await cmd(f'net profile put {slot} {ssid.encode().hex()} {password.encode().hex()}');created=True
  assert password not in await cmd('net profiles')
  assert 'RECON_ERROR' in await cmd('net profile put 8 61 -')
  assert 'RECON_ERROR' in await cmd('net scan 8.8.8.8 8.8.8.8')
  assert 'RECON_OK' in await cmd(f'net connect {slot}')
  s=await wait_state(lambda s:'connected=1' in s);print('PASS WPA2 join and DHCP on temporary fixture',flush=True)
  assert 'RECON_ERROR' in await cmd('portal start')
  assert 'RECON_ERROR' in await cmd('recon capture wifi')
  assert 'RECON_ERROR' in await cmd('net scan 8.8.8.8 8.8.8.8')
  await cmd('screen network');assert 'RECON_OK' in await cmd('net ping gateway')
  s=await wait_state(lambda s:'scanning=0' in s,8);assert 'sent=1 ' in s,s
  log['live_echo_reply']='received=1 ' in s
  print('Gateway ping reply:',log['live_echo_reply'],flush=True)
  assert 'RECON_ERROR' in await cmd('net ping gateway') # unsaved-results guard
  assert 'RECON_OK' in await cmd('net save')
  files=await cmd('recon files');saved=re.findall(r'RECON_SAVED (net-[a-f0-9]{8}\.csv) ',files)
  assert saved,'No network CSV saved'
  assert 'RECON_OK' in await cmd('net scan local');await asyncio.sleep(1)
  assert 'RECON_OK' in await cmd('net scan stop');assert 'scanning=0' in await cmd('net status')
  await cmd('net clear');await cmd('screen radar');assert 'active=0 connected=0' in await cmd('net status')
  print('PASS session exclusion, subnet restriction, CSV, scan stop and Home restoration',flush=True)
  # A second connection catches leaked netif/event handlers.
  assert 'RECON_OK' in await cmd(f'net connect {slot}');await wait_state(lambda s:'connected=1' in s)
  await manager.stop_tethering_async();hotspot=False;await wait_state(lambda s:'active=0 connected=0' in s,35)
  print('PASS repeated connection and network-loss cleanup',flush=True)
  assert 'RECON_OK' in await cmd(f'net connect {slot}')
  s=await wait_state(lambda s:'active=0 connected=0' in s,30);assert 'timed out' in s,s
  print('PASS absent-network deadline restores radar',flush=True)
  after=await cmd('tool status');assert re.search(r'mode=(\d+)',initial)[1]==re.search(r'mode=(\d+)',after)[1]
  badge=await cmd('badge checksum',1.5);assert re.search(r'crc=([a-f0-9]+) boot=(\d)',original_badge).groups()==re.search(r'crc=([a-f0-9]+) boot=(\d)',badge).groups()
  log['result']='PASS';print('PASS original badge and listening mode preserved',flush=True)
 finally:
  await cmd('net disconnect')
  if hotspot:await manager.stop_tethering_async()
  if created:await cmd(f'net profile remove {slot}')
  await cmd('net clear');await cmd('screen radar');b.port.close()
  if output:Path(output).write_text(json.dumps(log,indent=2))

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--port',required=True);parser.add_argument('--output');a=parser.parse_args();asyncio.run(run(a.port,a.output))
