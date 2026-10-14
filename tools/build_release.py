"""Build matching board images and ESP Web Tools manifests; never use device dumps."""
from pathlib import Path
import argparse, csv, hashlib, json, os, shutil, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
BOARDS={'spotpear':('Spotpear JC3636W518','spotpear'),'waveshare':('Waveshare ESP32-S3-Touch-AMOLED-1.75C','spotpear'),'stopwatch':('M5Stack StopWatch C152','m5stack-stopwatch')}
VERSION='0.5.0'

def run(*args):subprocess.run([sys.executable,*args],cwd=ROOT,check=True)
def build(board,skip):
    if not skip:run('-m','platformio','run','-e',board)
    run('-m','platformio','run','-e',board,'-t','buildfs')
    build=ROOT/'.pio/build'/board;out=ROOT/'web/firmware'/board;out.mkdir(parents=True,exist_ok=True)
    core=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))
    partition=ROOT/'hardware'/BOARDS[board][1]/'partitions.csv'
    entries={row[0].strip():row for row in csv.reader(partition.read_text().splitlines()) if row and not row[0].startswith('#')}
    app_offset=int(entries['app0'][3],0);fs_offset=int(entries['spiffs'][3],0);fs_size=int(entries['spiffs'][4],0)
    assert (build/'firmware.bin').stat().st_size<=int(entries['app0'][4],0),'Firmware exceeds partition'
    assert (build/'littlefs.bin').stat().st_size==fs_size,'Filesystem size mismatch'
    sources={'bootloader.bin':build/'bootloader.bin','partitions.bin':build/'partitions.bin','firmware.bin':build/'firmware.bin','littlefs.bin':build/'littlefs.bin','boot_app0.bin':core/'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'}
    for name,path in sources.items():
        assert path.is_file(),path
        shutil.copyfile(path,out/name)
    segments=[('bootloader.bin',0),('partitions.bin',0x8000),('boot_app0.bin',0xe000),('firmware.bin',app_offset)]
    for mode in ('factory','update'):
        selected=segments+([('littlefs.bin',fs_offset)] if mode=='factory' else [])
        manifest={'name':f'Dragon Detector — {BOARDS[board][0]} ({mode})','version':VERSION,'new_install_prompt_erase':mode=='factory','builds':[{'chipFamily':'ESP32-S3','parts':[{'path':name,'offset':offset} for name,offset in selected]}]}
        (out/f'{mode}.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    # A merged factory image is also provided for esptool/M5Burner-style tools.
    args=['-m','esptool','--chip','esp32s3','merge-bin','-o',str(out/'factory-0x0.bin')]
    for name,offset in segments+[('littlefs.bin',fs_offset)]:args += [hex(offset),str(out/name)]
    run(*args)
    merged=(out/'factory-0x0.bin').read_bytes()
    for name,offset in segments+[('littlefs.bin',fs_offset)]:
        expected=(out/name).read_bytes();assert merged[offset:offset+len(expected)]==expected,(board,name)
    checks=[]
    for file in sorted(out.glob('*.bin')):
        data=file.read_bytes();assert b'C:/Users/' not in data and b'C:\\Users\\' not in data, 'Private workstation path in image'
        checks.append(f'{hashlib.sha256(data).hexdigest()}  {file.name}\n')
    (out/'SHA256SUMS.txt').write_text(''.join(checks),encoding='ascii')
    metadata={'version':VERSION,'board':board,'name':BOARDS[board][0],'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=normal'],cwd=ROOT,text=True).strip()),'filesystem_offset':fs_offset,'filesystem_size':fs_size,'factory_resets_badges':True,'update_preserves_nvs_and_filesystem':True}
    (out/'build-info.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    print(f'PACKAGED {board}: {len(merged)} bytes, verified offsets and SHA-256')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--board',choices=BOARDS,action='append');parser.add_argument('--skip-build',action='store_true');args=parser.parse_args()
    for board in args.board or BOARDS:build(board,args.skip_build)
