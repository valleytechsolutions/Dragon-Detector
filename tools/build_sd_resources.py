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
    data=bytearray(b'DDR1'+struct.pack('<7I',1,len(rows),len(pool),0,snapshot,10,0)+payload)
    data[16:20]=struct.pack('<I',zlib.crc32(data))
    data=bytes(data)
    if len(data)>3*1024*1024:raise ValueError('pack exceeds firmware bound')
    metadata=dict(schema=1,minimum_firmware='0.13.28',snapshot=snapshot,entries=len(rows),bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),sources=provenance)
    return data,metadata

def build_labels(document):
    if document['schema']!=1:raise ValueError('unsupported label schema')
    rows={};kinds={'uuid16':1,'company16':2}
    for entry in document['labels']:
        kind=kinds[entry['kind']];value=int(entry['id'],16);name=entry['name']
        if not 0<=value<=65535 or not name or len(name)>96 or any(not 32<=ord(c)<=126 for c in name):raise ValueError('invalid BLE label')
        key=(kind,value)
        if key in rows:raise ValueError('duplicate BLE label')
        rows[key]=name
    if not 0<len(rows)<=4096:raise ValueError('invalid BLE label count')
    pool=bytearray();offsets={}
    for name in sorted(set(rows.values())):offsets[name]=len(pool);pool+=name.encode('ascii')+b'\0'
    records=b''.join(struct.pack('<BBHI',kind,0,value,offsets[name]) for (kind,value),name in sorted(rows.items()))
    snapshot=document['snapshot']
    if not 20261103<=snapshot<=20991231:raise ValueError('invalid label snapshot')
    data=bytearray(b'DDB1'+struct.pack('<7I',1,len(rows),len(pool),0,snapshot,8,0)+records+pool)
    data[16:20]=struct.pack('<I',zlib.crc32(data));data=bytes(data)
    if len(data)>65536:raise ValueError('BLE labels exceed firmware bound')
    return data,dict(schema=1,path='/dragon/resources/ble-labels.ddb',snapshot=snapshot,entries=len(rows),bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),namespaces=['uuid16','company16'])

def archive(output,data,metadata,labels=None):
    files={'dragon/resources/vendors.ddr':data,'dragon/resources/manifest.json':(json.dumps(metadata,indent=2)+'\n').encode(),
      'READ-ME.txt':b'Dragon Detector optional SD resource pack v1\nRequires SD-resource beta firmware 0.13.28 or newer.\nExtract dragon/ to the FAT32 SD root: /dragon/resources/vendors.ddr\nDo not copy the ZIP itself or add a containing directory. Restart to load.\nExisting captures and private config.txt must be preserved.\nNo card or invalid pack: internal MA-L lookup remains available.\nResource labels are registry ownership facts, not product identity.\nNo credentials or user captures are included.\nSee manifest.json for IEEE source hashes.\n'}
    files['dragon/resources/research-notes.txt']=b'Dragon Detector: interpreting radio evidence\n\nRegistry assignments identify address registrants, not finished products or operators.\nMA-S (36 bits) takes precedence over MA-M (28), then MA-L (24).\nRandom/private BLE addresses and locally administered Wi-Fi addresses have no OUI claim.\nBluetooth Company Identifiers and IEEE address registrants are different namespaces.\nAdvertising a service UUID is not proof that every associated GATT characteristic exists.\nPreserve raw bytes and address type with an observation; record unknowns rather than guess.\nUse existing BLE Lab/DDG1 profiles for owned-device service inspection and comparison.\nNotification payloads require a known format; unknown values remain hexadecimal.\nObserved advertisements can be spoofed; a label alone is not detection confidence.\nNo executable rules or scripts are loaded from this pack.\nOriginal Dragon Detector reference notes; MIT application license.\n'
    if labels is not None:
        files['dragon/resources/ble-labels.ddb']=labels
        files['READ-ME.txt']=b'Dragon Detector SD lookup names for Waveshare LCD 1.85B\nRequires 1.85B firmware 0.14.1 or newer.\nExtract dragon/ to the FAT32 SD root. Both /dragon/resources/vendors.ddr and /dragon/resources/ble-labels.ddb are needed for complete lookup names.\nDo not copy the ZIP itself or add a containing directory. Restart to load immutable names into PSRAM.\nPreserve existing captures and private configuration. Formatting erases the card and requires reinstalling this pack.\nThe 1.85B omits the compiled general IEEE registry and BLE display label tables to free firmware space. Missing or invalid files leave their labels unresolved; radar detection rules remain compiled and continue operating.\nOther profiles retain their internal registry and do not require this pack.\nLabels are metadata, not product identity or detection confidence. No credentials, captures, executable scripts or detector rules are included.\nSee manifest.json for file and IEEE source hashes.\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for name,content in sorted(files.items()):
            info=zipfile.ZipInfo(name,(2026,11,3,0,0,0) if labels is not None else (2026,10,23,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,content)
    return files

def extend_pack(base,labels_path,output):
    # Reuse the reviewed IEEE snapshot byte-for-byte; no network refresh or
    # silently changed MAC assignments when adding a board's display names.
    with zipfile.ZipFile(base) as z:
        data=z.read('dragon/resources/vendors.ddr');metadata=json.loads(z.read('dragon/resources/manifest.json'))
    if len(data)!=metadata['bytes'] or hashlib.sha256(data).hexdigest()!=metadata['sha256']:raise ValueError('base registry integrity failed')
    labels,label_metadata=build_labels(json.loads(labels_path.read_text(encoding='utf-8')))
    metadata.update(minimum_firmware='0.14.1',pack_date=20261103,sd_only_profiles=['waveshare_185b'],ble_labels=label_metadata)
    archive(output,data,metadata,labels);return metadata

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--mal',type=Path);p.add_argument('--mam',type=Path);p.add_argument('--mas',type=Path);p.add_argument('--snapshot',type=int);p.add_argument('--output',type=Path,required=True);p.add_argument('--base-pack',type=Path);p.add_argument('--ble-labels',type=Path,default=Path(__file__).resolve().parents[1]/'docs/ble-labels.json');a=p.parse_args()
    if a.base_pack:meta=extend_pack(a.base_pack,a.ble_labels,a.output)
    else:
        if not all((a.mal,a.mam,a.mas,a.snapshot)):p.error('provide --base-pack or all three IEEE inputs and --snapshot')
        data,meta=build({24:a.mal,28:a.mam,36:a.mas},a.snapshot);archive(a.output,data,meta)
    print(json.dumps(meta,indent=2))
