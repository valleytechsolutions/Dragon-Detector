import argparse,time,serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--seconds',type=float,default=15);p.add_argument('--reset',action='store_true');a=p.parse_args()
s=serial.Serial(None,115200,timeout=.1);s.dtr=False;s.rts=False;s.port=a.port;s.open()
with s:
    if a.reset:
        s.dtr=False;s.rts=True;time.sleep(.15);s.rts=False
    s.dtr=True;s.write(b'status\n');end=time.monotonic()+a.seconds
    while time.monotonic()<end:
        data=s.read(8192)
        if data:print(data.decode(errors='replace'),end='',flush=True)
