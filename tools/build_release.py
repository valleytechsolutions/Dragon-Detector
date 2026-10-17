"""Build matching board images and ESP Web Tools manifests; never use device dumps."""
from pathlib import Path
import argparse, csv, hashlib, json, os, re, shutil, subprocess, sys
from flash_budget import report as flash_report
ROOT=Path(__file__).resolve().parents[1]
BOARDS={'spotpear':('Spotpear JC3636W518','spotpear'),'waveshare':('Waveshare ESP32-S3-Touch-AMOLED-1.75C','waveshare-amoled'),'stopwatch':('M5Stack StopWatch C152','m5stack-stopwatch'),'xiao_round':('Seeed XIAO ESP32-S3 + Round Display','xiao-round'),'waveshare_lite':('Waveshare ESP32-S3-LCD-1.28 Lite','waveshare-lite')}
VERSION=re.search(r'#define DRAGON_VERSION "([0-9.]+)"',(ROOT/'firmware/include/version.h').read_text()).group(1)

def run(*args):subprocess.run([sys.executable,*args],cwd=ROOT,check=True)
def build(board,skip):
    if not skip:run('-m','platformio','run','-e',board)
    build=ROOT/'.pio/build'/board;out=ROOT/'web/firmware'/board;out.mkdir(parents=True,exist_ok=True)
    core=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))
    partition=ROOT/'hardware'/BOARDS[board][1]/'partitions.csv'
    entries={row[0].strip():row for row in csv.reader(partition.read_text().splitlines()) if row and not row[0].startswith('#')}
    app_offset=int(entries['app0'][3],0);fs_offset=int(entries['spiffs'][3],0);fs_size=int(entries['spiffs'][4],0)
    if board=='waveshare':
        # PlatformIO chooses the last filesystem partition, which is recon.
        # Build the legacy badge image explicitly at its unchanged size.
        mk=core/'packages/tool-mklittlefs'/('mklittlefs.exe' if os.name=='nt' else 'mklittlefs')
        subprocess.run([str(mk),'-c',str(ROOT/'firmware/data'),'-b','4096','-p','256','-s',str(fs_size),str(build/'littlefs.bin')],check=True)
    else:run('-m','platformio','run','-e',board,'-t','buildfs')
    assert (build/'firmware.bin').stat().st_size<=int(entries['app0'][4],0),'Firmware exceeds partition'
    assert (build/'littlefs.bin').stat().st_size==fs_size,'Filesystem size mismatch'
    sources={'bootloader.bin':build/'bootloader.bin','partitions.bin':build/'partitions.bin','firmware.bin':build/'firmware.bin','littlefs.bin':build/'littlefs.bin','boot_app0.bin':core/'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'}
    for name,path in sources.items():
        assert path.is_file(),path
        shutil.copyfile(path,out/name)
    # Each standalone board ZIP also carries the embedded fonts' license.
    shutil.copyfile(ROOT/'docs/licenses/Montserrat-OFL.txt',out/'FONT-LICENSE.txt')
    shutil.copyfile(ROOT/'docs/licenses/Unscii-NOTICE.txt',out/'ASCII-FONT-NOTICE.txt')
    filesystem_segments=[('littlefs.bin',fs_offset)]
    if board=='waveshare':
        # A second independent LittleFS. Updates NEVER write either filesystem.
        recon_offset=int(entries['recon'][3],0);recon_size=int(entries['recon'][4],0)
        assert (recon_offset,recon_size)==(0x1000000,0x1000000)
        empty=ROOT/'.work/recon-factory';empty.mkdir(parents=True,exist_ok=True)
        assert not list(empty.iterdir()),'Recon factory source must be empty'
        mk=core/'packages/tool-mklittlefs'/('mklittlefs.exe' if os.name=='nt' else 'mklittlefs')
        subprocess.run([str(mk),'-c',str(empty),'-b','4096','-p','256','-s',str(recon_size),str(out/'reconfs.bin')],check=True)
        filesystem_segments.append(('reconfs.bin',recon_offset))
    segments=[('bootloader.bin',0),('partitions.bin',0x8000),('boot_app0.bin',0xe000),('firmware.bin',app_offset)]
    for mode in ('factory','update'):
        selected=segments+(filesystem_segments if mode=='factory' else [])
        manifest={'name':f'Dragon Detector — {BOARDS[board][0]} ({mode})','version':VERSION,'new_install_prompt_erase':mode=='factory','builds':[{'chipFamily':'ESP32-S3','parts':[{'path':name,'offset':offset} for name,offset in selected]}]}
        (out/f'{mode}.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    # A merged factory image is also provided for esptool/M5Burner-style tools.
    args=['-m','esptool','--chip','esp32s3','merge-bin','-o',str(out/'factory-0x0.bin')]
    for name,offset in segments+filesystem_segments:args += [hex(offset),str(out/name)]
    run(*args)
    merged=(out/'factory-0x0.bin').read_bytes()
    for name,offset in segments+filesystem_segments:
        expected=(out/name).read_bytes();assert merged[offset:offset+len(expected)]==expected,(board,name)
    checks=[]
    for file in sorted(out.glob('*.bin')):
        data=file.read_bytes();assert b'C:/Users/' not in data and b'C:\\Users\\' not in data, 'Private workstation path in image'
        if file.name=='firmware.bin' and board in ('waveshare','stopwatch','waveshare_lite'):
            for prompt in (b'SD CARD',b'SD files',b'SD event logging:',b'Requires a mounted SD card',b'Insert FAT32 card',b'SD: this board has no card slot'):
                assert prompt not in data,(board,'Unsupported card UI in firmware',prompt)
        checks.append(f'{hashlib.sha256(data).hexdigest()}  {file.name}\n')
    (out/'SHA256SUMS.txt').write_text(''.join(checks),encoding='ascii')
    metadata={'version':VERSION,'board':board,'name':BOARDS[board][0],'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain','--untracked-files=normal'],cwd=ROOT,text=True).strip()),'filesystem_offset':fs_offset,'filesystem_size':fs_size,'factory_resets_badges':True,'update_preserves_nvs_and_filesystem':True}
    metadata['flash_size']=0x2000000 if board=='waveshare' else 0x800000 if board=='xiao_round' else 0x1000000
    metadata['memory']=flash_report(entries,(build/'firmware.bin').stat().st_size,metadata['flash_size'])
    (out/'memory-budget.json').write_text(json.dumps(metadata['memory'],indent=2)+'\n',encoding='utf-8')
    if board=='waveshare':metadata.update(recon_offset=recon_offset,recon_size=recon_size,update_preserves_recon=True)
    (out/'build-info.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    print(f'PACKAGED {board}: {len(merged)} bytes, verified offsets and SHA-256')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--board',choices=BOARDS,action='append');parser.add_argument('--skip-build',action='store_true');args=parser.parse_args()
    for board in args.board or BOARDS:build(board,args.skip_build)
