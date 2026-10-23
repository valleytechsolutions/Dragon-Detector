"""Deterministic DDR1 resource pack from explicitly downloaded IEEE CSV files."""
from pathlib import Path
import argparse,csv,hashlib,io,json,struct,zipfile,zlib

SOURCES={24:('MA-L','https://standards-oui.ieee.org/oui/oui.csv'),28:('MA-M','https://standards-oui.ieee.org/oui28/mam.csv'),36:('MA-S','https://standards-oui.ieee.org/oui36/oui36.csv')}
def build(inputs,snapshot):
    rows={};provenance=[]
    for bits,path in inputs.items():
        raw=path.read_bytes();kind,url=SOURCES[bits]
        provenance.append(dict(source=url,sha256=hashlib.sha256(raw).hexdigest(),bits=bits))
        for r in csv.DictReader(io.StringIO(raw.decode('utf-8-sig'))):
            if r['Registry']!=kind:continue
            assignment=r['Assignment'];name=r['Organization Name'].strip()
            if len(assignment)*4!=bits:raise ValueError('wrong assignment width')
            key=int(assignment,16)<<(36-bits)
            if not name or any(ord(c)<32 or ord(c)==127 for c in name):raise ValueError('invalid name')
            rows.setdefault((bits,key),set()).add(name)
    rows={k:' / '.join(sorted(v)) for k,v in rows.items()}
    pool=bytearray();offset={}
    for name in sorted(set(rows.values())):
        encoded=name.encode('utf-8')
        if len(encoded)>255:raise ValueError('name exceeds reader limit')
        offset[name]=len(pool);pool+=encoded+b'\0'
    records=b''.join(bytes([bits])+key.to_bytes(5,'big')+struct.pack('<I',offset[name]) for (bits,key),name in sorted(rows.items()))
    payload=records+pool
    data=b'DDR1'+struct.pack('<7I',1,len(rows),len(pool),zlib.crc32(payload),snapshot,10,0)+payload
    if len(data)>3*1024*1024:raise ValueError('pack exceeds firmware bound')
    metadata=dict(schema=1,minimum_firmware='0.13.28',snapshot=snapshot,entries=len(rows),bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),sources=provenance)
    return data,metadata

def archive(output,data,metadata):
    files={'dragon/resources/vendors.ddr':data,'dragon/resources/manifest.json':(json.dumps(metadata,indent=2)+'\n').encode(),
      'READ-ME.txt':b'Dragon Detector optional SD resource pack v1\nRequires SD-resource beta firmware 0.13.28 or newer.\nExtract dragon/ to the FAT32 SD root: /dragon/resources/vendors.ddr\nDo not copy the ZIP itself or add a containing directory. Restart to load.\nExisting captures and private config.txt must be preserved.\nNo card or invalid pack: internal MA-L lookup remains available.\nResource labels are registry ownership facts, not product identity.\nNo credentials or user captures are included.\nSee manifest.json for IEEE source hashes.\n'}
    files['dragon/resources/research-notes.txt']=b'Dragon Detector: interpreting radio evidence\n\nRegistry assignments identify address registrants, not finished products or operators.\nMA-S (36 bits) takes precedence over MA-M (28), then MA-L (24).\nRandom/private BLE addresses and locally administered Wi-Fi addresses have no OUI claim.\nBluetooth Company Identifiers and IEEE address registrants are different namespaces.\nAdvertising a service UUID is not proof that every associated GATT characteristic exists.\nPreserve raw bytes and address type with an observation; record unknowns rather than guess.\nUse existing BLE Lab/DDG1 profiles for owned-device service inspection and comparison.\nNotification payloads require a known format; unknown values remain hexadecimal.\nObserved advertisements can be spoofed; a label alone is not detection confidence.\nNo executable rules or scripts are loaded from this pack.\nOriginal Dragon Detector reference notes; MIT application license.\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for name,content in sorted(files.items()):
            info=zipfile.ZipInfo(name,(2026,10,23,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,content)
    return files
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--mal',type=Path,required=True);p.add_argument('--mam',type=Path,required=True);p.add_argument('--mas',type=Path,required=True);p.add_argument('--snapshot',type=int,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    data,meta=build({24:a.mal,28:a.mam,36:a.mas},a.snapshot);archive(a.output,data,meta);print(json.dumps(meta,indent=2))
