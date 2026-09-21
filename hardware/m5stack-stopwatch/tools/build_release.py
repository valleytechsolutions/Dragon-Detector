"""Create clean complete images from build inputs, never from a user's flash dump."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
root=Path(__file__).resolve().parents[1];build=root/'.pio/build/stopwatch';out=root/'release';out.mkdir(exist_ok=True)
core=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))
bootapp=core/'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
parts=[(0,build/'bootloader.bin'),(0x8000,build/'partitions.bin'),(0xe000,bootapp),(0x10000,build/'firmware.bin')]
factory=parts+[(0x810000,build/'littlefs.bin')]
files={}
for name,segments in [('Dragon-Detector-StopWatch-v0.2.0-update-0x0.bin',parts),('Dragon-Detector-StopWatch-v0.2.0-factory-0x0.bin',factory)]:
    cmd=[sys.executable,'-m','esptool','--chip','esp32s3','merge-bin','-o',str(out/name)]
    for offset,path in segments:cmd += [hex(offset),str(path)]
    subprocess.run(cmd,check=True)
    payload=(out/name).read_bytes()
    for offset,path in segments:
        p=path.read_bytes();assert payload[offset:offset+len(p)]==p,(path,'mismatched segment')
    assert b'C:/Users/' not in payload and b'C:\\Users\\' not in payload,'Private build path in binary'
    files[name]={'sha256':hashlib.sha256(payload).hexdigest(),'bytes':len(payload),'flash_offset':'0x0'}
(out/'SHA256SUMS.txt').write_text(''.join(f'{v["sha256"]}  {n}\n' for n,v in files.items()))
(out/'manifest.json').write_text(json.dumps({'version':'0.2.0','device':'M5Stack StopWatch C152','chip':'ESP32-S3','flash':'16MB','files':files},indent=2))
print(json.dumps(files,indent=2))
