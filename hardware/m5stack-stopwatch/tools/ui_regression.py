"""Exercise real LVGL pointer input on the flashed device; no RF transmission."""
import argparse,time,re,json
import serial
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--out',default='docs/ui-validation.json');a=p.parse_args()
s=serial.Serial(None,115200,timeout=.2);s.dtr=False;s.rts=False;s.port=a.port;s.open()
results=[]
def command(c,pause=.12):s.write((c+'\n').encode());time.sleep(pause)
def state():
    s.reset_input_buffer();command('ui status');end=time.monotonic()+8;retry=time.monotonic()+2
    while time.monotonic()<end:
        if time.monotonic()>retry:command('ui status');retry=time.monotonic()+2
        line=s.readline().decode(errors='replace')
        m=re.search(r'UI page=(\d+) pending=(\d+) scroll=(-?\d+)(?: tab=(\d+))?',line)
        if m:return dict(zip(['page','pending','scroll','tab'],[int(v or 0) for v in m.groups()]))
    raise RuntimeError('No UI status')
def swipe(y0,y1):
    command(f'touch 233 {y0} 1')
    for i in range(1,13):command(f'touch 233 {y0+(y1-y0)*i//12} 1',.045)
    command(f'touch 233 {y1} 0',.35)
with s:
    for name,page in [('channels',6),('settings',9),('about',10)]:
        command('screen '+name,.35);before=state();swipe(345,155);up=state();assert up['page']==page and up['scroll']>0,(name,up)
        swipe(155,345);down=state();assert down['page']==page and down['scroll']<up['scroll'],(name,down)
        results.append({'screen':name,'before':before,'after_up':up,'after_down':down})
    command('key a',.5);home=state();assert home['page']==1,home
    command('key b',.5);first=state();assert first['page']==2,first
    command('key b',.5);second=state();assert second['page']==2 and second['tab']!=(first['tab']),second
    command('screen badge',.35);assert state()['page']==11
    command('key a',.5);assert state()['page']==1
    results.append({'buttons':'A returns to radar; B opens menu and cycles tabs; A exits badge','status':'passed'})
Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
