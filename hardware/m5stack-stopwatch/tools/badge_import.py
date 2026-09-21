"""Import PNG/JPEG/WebP/BMP images as native 466x466 StopWatch badges via USB.
Examples: python tools/badge_import.py portrait.png --port COM4
          python tools/badge_import.py logo.png --output data/badges/logo.rgb565
"""
import argparse, struct, time, re, hashlib
from pathlib import Path
from PIL import Image, ImageOps
import serial

def convert(path, fit='contain'):
    with Image.open(path) as im:
        im=ImageOps.exif_transpose(im).convert('RGBA')
        if fit=='cover': im=ImageOps.fit(im,(466,466),Image.Resampling.LANCZOS)
        else:
            im.thumbnail((466,466),Image.Resampling.LANCZOS)
            canvas=Image.new('RGBA',(466,466),(0,0,0,255));canvas.alpha_composite(im,((466-im.width)//2,(466-im.height)//2));im=canvas
        bg=Image.new('RGBA',(466,466),(0,0,0,255));bg.alpha_composite(im)
        data=bytearray(b'DDB1'+struct.pack('<HH',466,466))
        for r,g,b in bg.convert('RGB').getdata():data.extend(struct.pack('>H',((r>>3)<<11)|((g>>2)<<5)|(b>>3)))
        return bytes(data)

def wait_line(s,prefix,timeout=15):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        line=s.readline().decode(errors='replace').strip()
        if 'ERROR' in line:raise RuntimeError(line)
        if line.startswith(prefix):return line
    raise TimeoutError('No '+prefix+' from device')

def upload(port,path,data):
    s=serial.Serial(None,115200,timeout=.4);s.dtr=False;s.rts=False;s.port=port;s.open()
    with s:
        time.sleep(.15);s.reset_input_buffer();s.write(f'put {path} {len(data)}\n'.encode());wait_line(s,'PUT_READY')
        at=0
        while at<len(data):
            block=data[at:at+128];s.write(block);at+=len(block)
            acknowledged=0
            while acknowledged<at:
                ack=wait_line(s,'PUT_ACK');acknowledged=int(ack.split()[1])
                if acknowledged>at:raise RuntimeError('Transfer acknowledgement mismatch')
        wait_line(s,'PUT_OK');s.write(f'badge {path}\n'.encode())
    print('Imported',path,'bytes',len(data),'SHA256',hashlib.sha256(data).hexdigest())

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path);p.add_argument('--port');p.add_argument('--name');p.add_argument('--fit',choices=['contain','cover'],default='contain');p.add_argument('--output',type=Path);a=p.parse_args()
    payload=convert(a.image,a.fit)
    if a.output:a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(payload);print(a.output)
    if a.port:
        name=re.sub('[^A-Za-z0-9_-]','_',a.name or a.image.stem)[:40]
        upload(a.port,'/badges/'+name+'.rgb565',payload)
    if not a.port and not a.output:p.error('Specify --port or --output')
