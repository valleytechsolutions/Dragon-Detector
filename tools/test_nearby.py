"""Source guard for the "BLE radios near" count: it only counts and shows.

The estimator must stay free of alerts, sound, logging and people or distance
claims, be fed from one place, reach both UI flavours, and keep its settings
out of the size-checked DetectorSettings blob.
"""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1];src=root/'firmware/src'
header=(root/'firmware/include/nearby.h').read_text(encoding='utf-8')
allowed='// BLE radio addresses heard at a close level; never people.'
assert header.count(allowed)==1
body=header.replace(allowed,'').lower()
for word in ['uinotify','addalert','audio','emitdetection','historyobserve','logcontact','people','person','metre','meter','distance','follow','serial','#include "']:
    assert word not in body,word
assert re.findall(r'#include <([^>]+)>',header)==['stdint.h','stddef.h','stdio.h','stdlib.h']
feeds={p.name:p.read_text(encoding='utf-8').count('nearby.observe(') for p in sorted(src.iterdir()) if p.suffix in ('.cpp','.inc','.c')}
assert {k:v for k,v in feeds.items() if v}=={'radio.cpp':1},feeds
radio=(src/'radio.cpp').read_text(encoding='utf-8')
line=next(x for x in radio.splitlines() if 'nearby.observe(' in x)
assert line.strip().startswith('if(c.rssi>=nearby.exitDbm()&&!(deviceRules(contactKey(c))&RuleIgnore))'),line
assert radio.count('nearby.listen(elapsed,now)')==2 and radio.count('nearby.tick()')==2
assert 'nearby.listen(ble.listen_ms,ble.captured_ms)' in radio
assert 'if(ble.listen_ms)observeNearby(*dot)' in radio and 'observeNearby(c);' in radio
for name in ['ui.cpp','ui_lite.cpp']:
    assert 'dragon::nearbyText(' in (src/name).read_text(encoding='utf-8'),name
# Nothing that touches the count may notify, sound, log or record history.
for name in ['radio.cpp','detector.cpp','ui.cpp','ui_lite.cpp','main.cpp']:
    for x in (src/name).read_text(encoding='utf-8').splitlines():
        if 'nearby' in x and 'nearbyCommand' not in x:
            assert not re.search(r'uiNotify|addAlert|audioAlert|emitDetection|historyObserve|logContact',x),(name,x)
monitor=(root/'firmware/include/monitor.h').read_text(encoding='utf-8')
settings=monitor[monitor.index('struct DetectorSettings {'):];settings=settings[:settings.index('};')]
fields=[x.strip() for x in settings.splitlines()[1:] if '(' not in x.split('=')[0]]
assert fields==['uint32_t families=((1u<<unsigned(Family::Count))-1u)&~(1u<<unsigned(Family::Adult));','Confidence minimum=Confidence::Medium;','uint8_t autoQuiet=5;','bool privacy=false,history=true,wakeAlert=true,quiet=false;','uint16_t dimSeconds=0,offSeconds=0;','uint8_t idleBrightness=8;','uint16_t smoothingMs=1800;'],fields
detector=(src/'detector.cpp').read_text(encoding='utf-8')
assert 'prefs.getBool("near",true)' in detector and 'prefs.getChar("near-dbm",-70)' in detector
command=detector[detector.index('bool nearbyCommand('):detector.index('bool detectorCommand(')]
assert 'mac' not in command.lower() and 'unit=addresses' in command and 'radio=BLE' in command
main=(src/'main.cpp').read_text(encoding='utf-8')
assert main.index('"radar near"')<main.index('#ifdef BOARD_RADAR_LITE',main.index('"radar signal"')) # before the Lite gate
print('PASS near count: no alert, sound, log or address output; one feed; both UIs; settings blob unchanged')
