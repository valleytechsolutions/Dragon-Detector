"""USB diagnostics and exact firmware-render screenshot. Does not transmit RF."""
import argparse, time, re
from pathlib import Path
import serial
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('command',nargs='?',default='status');p.add_argument('--port',required=True);p.add_argument('--seconds',type=float,default=8);p.add_argument('--out',default='docs/radar-hardware.png');args=p.parse_args()
s=serial.Serial(None,115200,timeout=0.5);s.dtr=False;s.rts=False;s.port=args.port;s.open()
with s:
    time.sleep(0.2);s.reset_input_buffer();s.write((args.command+'\n').encode());end=time.monotonic()+args.seconds
    while time.monotonic()<end:
        line=s.readline()
        if line.startswith(b'SCREENSHOT '):
            _,w,h,length=line.split();w,h,length=int(w),int(h),int(length);print('Snapshot:',w,h,length);data=bytearray()
            last=time.monotonic()
            while len(data)<length:
                chunk=s.read(length-len(data))
                if not chunk and time.monotonic()-last>8:raise RuntimeError(f'Incomplete screenshot {len(data)}/{length}')
                if chunk:last=time.monotonic()
                data.extend(chunk)
            rgb=bytearray()
            for i in range(0,len(data),2):
                v=(data[i]<<8)|data[i+1];rgb.extend(((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
            target=Path(args.out);target.parent.mkdir(parents=True,exist_ok=True);Image.frombytes('RGB',(w,h),bytes(rgb)).save(target);print(f'Saved {target.resolve()}');break
        if line:print(line.decode(errors='replace').rstrip())
