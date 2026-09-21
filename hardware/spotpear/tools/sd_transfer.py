"""Transfer local SD files over Dragon Detector's USB CLI (no radio networking)."""
import argparse, time
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('action',choices=['put','get']);p.add_argument('local');p.add_argument('remote');p.add_argument('--port',required=True);a=p.parse_args()
s=serial.Serial(None,115200,timeout=.5);s.dtr=False;s.rts=False;s.port=a.port;s.open()
def line_until(prefix,seconds=8):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline()
        if line.startswith(prefix):return line
        if b'ERROR' in line:raise RuntimeError(line.decode(errors='replace'))
    raise TimeoutError(f'No {prefix!r} reply')
with s:
    time.sleep(.2);s.reset_input_buffer();path=Path(a.local)
    if a.action=='put':
        data=path.read_bytes();s.write(f'put {a.remote} {len(data)}\n'.encode());line_until(b'PUT_READY')
        for i in range(0,len(data),128):
            block=data[i:i+128];s.write(block);s.flush();target=i+len(block)
            while int(line_until(b'PUT_ACK').split()[1])<target:pass
        line_until(b'PUT_OK',12);print(f'Uploaded {len(data)} bytes to {a.remote}')
    else:
        s.write(f'get {a.remote}\n'.encode());header=line_until(b'FILE_BYTES');size=int(header.split()[1]);data=bytearray();last=time.monotonic()
        while len(data)<size:
            chunk=s.read(min(8192,size-len(data)))
            if chunk:data.extend(chunk);last=time.monotonic()
            elif time.monotonic()-last>8:raise TimeoutError(f'Incomplete transfer: {len(data)}/{size}')
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);print(f'Downloaded {len(data)} bytes to {path.resolve()}')
