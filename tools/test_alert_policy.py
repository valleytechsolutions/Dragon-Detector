"""Source guard for the alert policy: one notification per device per visit.

The host behaviour is tested in tests/alert_policy_test.h. This checks that the
firmware keeps using it: no forced alerts, no per-packet alert decisions, a
silent repeat path, the read-only USB commands on every board, and the
Settings row describing the visit rule.
"""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1];src=root/'firmware/src';inc=root/'firmware/include'
radio=(src/'radio.cpp').read_text(encoding='utf-8')
detector=(src/'detector.cpp').read_text(encoding='utf-8')
app=(inc/'app.h').read_text(encoding='utf-8')
# No caller may skip the ledgers.
for p in sorted(src.iterdir()):
    if p.suffix in ('.cpp','.inc'):
        for call in re.findall(r'emitDetection\(([^;]*)\);',p.read_text(encoding='utf-8')):
            assert not re.search(r',\s*true\s*\)?$',call),(p.name,call)
assert 'AlertGate' not in (inc/'monitor.h').read_text(encoding='utf-8') and 'AlertGate' not in (inc/'contacts.h').read_text(encoding='utf-8')
assert 'const dragon::Incident* incident=nullptr' in app
# One decision per evidence run; the old per-packet watched firing and dead guard are gone.
update=radio[radio.index('static void update('):radio.index('static void checkBleBurst')]
assert 'dragon::alertDecisionDue(c.signatureAlerted,c.alertRevision,alertRevision())' in update
assert '(watched&&c.packets>=2)) && uint32_t(o.at-c.seen)<30000' not in update and 'o.at-c.seen)<30000' not in update
assert update.count('visitTouch(keyOf(c),o.at)')==1
# Every incident path names its kind and quiet period.
for kind,quiet in [('Deauth','DeauthQuietMs'),('EvilTwin','EvilTwinQuietMs'),('BleBurst','BleBurstQuietMs')]:
    assert re.search(r'dragon::IncidentKind::%s,[^;]*dragon::%s\}'%(kind,quiet),radio),kind
# The repeat path is silent: it returns before the sound, the card and the USB line.
emit=detector[detector.index('bool emitDetection('):detector.index('bool alertCommand(')]
repeat=emit.index('dragon::Verdict::Repeat')
line=emit[repeat:emit.index('\n',repeat)]
assert 'alertRepeat(' in line and 'return false;' in line,line
assert emit.index('incidents.claim(')<repeat and emit.index('visits.claim(')<repeat
for call in ['addAlert(','audioAlert(','uiNotify(']:
    assert emit.index(call)>repeat,call
storage=(src/'storage.cpp').read_text(encoding='utf-8')
fn=storage[storage.index('void alertRepeat('):storage.index('void addAlert(')]
assert 'Serial' not in fn and 'append(' not in fn
assert '<60000' in storage[storage.index('void addAlert('):]
# Detections always get their own row; incident repeats fold by incident tag.
assert 'addAlert(title,detail,c.mac,false,tag)' in emit and 'alertRepeat(title,incident?nullptr:c.mac,tag)' in emit
# An incident never takes the watch presentation (LOCKED ON, sticky, past quiet).
assert 'bool watch=!incident&&(flags&RuleWatch)' in emit
# Incident quiet follows the visit setting; a participant visit gets each kind once.
assert 'dragon::incidentQuietMs(in.quietMs,detector.autoQuiet)' in emit and 'visits.claimIncident(' in emit
assert 'dragon::deauthId(f.bssid,o.channel)' in radio
assert 'dragon::ssidConflictId(ssid,length,secured?peer:c.mac)' in radio
assert 'reportSsidConflict(c,f.ssid,f.ssidLength,f.security!=0,peer,o.at)' in radio
# Only listening time counts as away, with the same gates as process().
loop=radio[radio.index('void radioLoop()'):]
visits=radio[radio.index('static void maintainVisits('):radio.index('void radioLoop()')]
assert 'maintainVisits(on&&reconRadio()!=2,on&&reconRadio()!=1,now)' in loop
assert visits.index('visitListen(wifi,ble,now)')<visits.index('visitExpire(now)')
assert '!surveyListen&&!wardriveBusy()&&!reconOwnsRadio()&&!bleLabOwnsRadio()' in loop
# Visits survive deep sleep: saved periodically and right before sleeping.
assert 'RTC_NOINIT_ATTR static VisitKeep visitKeep;' in detector and 'visitsPersist();' in visits
for board in ['board_amoled175c.cpp','board_watcher.cpp']:
    b=(src/board).read_text(encoding='utf-8');assert b.index('visitsPersist();')<b.index('esp_deep_sleep_start();'),board
# No static fallback table in internal RAM on PSRAM boards.
assert 'static dragon::Visit fallback' not in detector and 'VISIT_LIMIT=256' in detector
recon_ui=(src/'recon_ui.cpp').read_text(encoding='utf-8')
assert 'const bool locked=radioEnabled&&channelLock&&reconRadio()==1;' in recon_ui
assert 'if(!listening&&radioEnabled){setRadio(false);surveyListenOwned=radioSurveyListen(true);}' in recon_ui
# Read-only diagnostics, on Lite and Dial too.
assert 'if(alertCommand(cmd))return true;' in detector
main=(src/'main.cpp').read_text(encoding='utf-8')
assert 'c=="alerts status"||c=="visits status"' in main
# The Settings row explains the visit rule; the saved byte keeps its size.
ui=(src/'ui.cpp').read_text(encoding='utf-8')
assert '"One alert per device visit; repeats are counted"' in ui and 'Re-alert at +7 dB' not in ui
assert 'prefs.getBool("visit-v1",false)' in detector
print('PASS alert policy: ledgers on every path, one decision per evidence run, silent repeats, status commands')
