"""Bundle only newly built SD profiles; leave stable no-SD/Tuya releases alone."""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,zipfile
root=Path(__file__).resolve().parents[1]
boards=['spotpear','waveshare_175','waveshare_175_gps','xiao_round','lilygo_175']
p=argparse.ArgumentParser();p.add_argument('output',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
pack=root/'web/resources/dragon-resources-v1-20261023.zip';shutil.copy2(pack,a.output/pack.name)
for board in boards:
    folder=root/'web/firmware'/board
    meta=json.loads((folder/'build-info.json').read_text())
    assert meta['version']=='0.13.28' and meta['board']==board
    for mode in ['factory','update']:
        m=json.loads((folder/f'{mode}.json').read_text());assert m['sdResources']['schema']==1 and not m['sdResources']['required']
    # Every individual binary was offset-checked by build_release; verify again
    # against the written checksum list before including it in a download.
    for line in (folder/'SHA256SUMS.txt').read_text().splitlines():
        digest,name=line.split('  ');assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==digest
    out=a.output/f'Dragon-Detector-{board}-0.13.28-SD-Beta.zip'
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
        for f in sorted(folder.iterdir()):
            if f.suffix in ('.bin','.json','.txt'):z.write(f,f.name)
        z.write(root/'docs/SD-RESOURCES.md','SD-RESOURCES.md');z.write(root/'LICENSE','LICENSE.txt')
        z.writestr('START-HERE.txt',f'Dragon Detector 0.13.28 SD RESOURCE BETA\nExact board: {board}\nHardware validation pending.\nfactory-0x0.bin: fresh installation at offset 0x0, replaces saved files.\nfirmware.bin: matching partition-layout update at offset 0x10000.\nPrefer the prepared web flasher, which checks the exact board manifest.\nOptional companion pack: {pack.name}\nExtract its dragon/ directory to the FAT32 SD root and restart.\nNo card or invalid pack: internal MA-L registry remains available.\nAutomatic wardrive uploading is NOT enabled in this beta.\n')
    shutil.copy2(out,folder/out.name)
for name in ['SD-RESOURCES.md','WARDRIVE-UPLOAD-DESIGN.md','sd-resource-manifest.json']:
    shutil.copy2(root/'docs'/name,a.output/name)
subprocess.run(['git','archive','--format=zip','--prefix=Dragon-Detector/','-o',str(a.output/'Dragon-Detector-0.13.28-SD-Beta-source.zip'),'HEAD'],cwd=root,check=True)
(a.output/'START-HERE.txt').write_text('Dragon Detector 0.13.28 — OPTIONAL SD RESOURCE BETA\n\nHardware validation is pending. Choose ONLY the ZIP matching your exact board.\nFive supported SD profiles: spotpear, waveshare_175, waveshare_175_gps, xiao_round, lilygo_175.\nThe non-GPS 1.75, GPS 1.75-G and no-SD 1.75C are different profiles.\n\nExtract the companion resource ZIP so the FAT32 SD card contains /dragon/resources/vendors.ddr.\nInsert before boot; restart after changing the pack. Firmware remains usable without this optional pack.\nVerify downloads against SHA256SUMS.txt. Each firmware ZIP includes flashing offsets and its own checksums.\nRead SD-RESOURCES.md for capability limits, provenance and required hardware checks.\n\nAutomatic WiGLE / WDG uploads are NOT enabled. WARDRIVE-UPLOAD-DESIGN.md contains the investigation and proposed opt-in policy.\nProduction hosting and existing non-SD/T5-E1 releases were not changed.\n',encoding='utf-8')
checks=[]
for f in sorted(a.output.iterdir()):
    if f.is_file() and f.name!='SHA256SUMS.txt':checks.append(f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n')
(a.output/'SHA256SUMS.txt').write_text(''.join(checks),encoding='ascii')
print('Packaged five exact SD board variants, optional resource pack, source and checksums:',a.output)
