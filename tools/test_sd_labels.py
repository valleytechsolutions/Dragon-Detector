"""Compile the production SD loader, typed label reader and registry wrappers."""
from pathlib import Path
import argparse,hashlib,json,re,struct,subprocess,sys,zipfile,zlib
from build_sd_resources import build,build_labels,extend_pack

root=Path(__file__).resolve().parents[1];work=root/'.work/sd-label-tests';work.mkdir(parents=True,exist_ok=True)
p=argparse.ArgumentParser();p.add_argument('--cxx');p.add_argument('--sanitize',action='store_true');args=p.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
labels_path=root/'docs/ble-labels.json';document=json.loads(labels_path.read_text());labels,label_meta=build_labels(document)
(work/'labels.ddb').write_bytes(labels)
# Confirm that offloading preserves every existing label on other profiles.
insights=(root/'firmware/include/ble_insights.h').read_text();expected={}
for kind,start,end in [('uuid16','inline const char* uuidName(uint16_t u){switch(u){','default:return nullptr;}}'),('company16','inline const char* companyName(uint16_t c){switch(c){','default:return "Unresolved company ID";}}')]:
    block=insights.split(start,1)[1].split(end,1)[0]
    for cases,label in re.findall(r'((?:case 0x[0-9a-f]+:\s*)+)return "([^"]+)";',block):
        for value in re.findall(r'case 0x([0-9a-f]+):',cases):expected[(kind,int(value,16))]=label
assert {(x['kind'],int(x['id'],16)):x['name'] for x in document['labels']}==expected
assert len(expected)==44
source=root/'web/resources/dragon-resources-v1-20261023.zip';pack=root/'web/resources/dragon-resources-v1-20261103.zip'
extend_pack(source,labels_path,work/'first.zip');extend_pack(source,labels_path,work/'second.zip')
assert (work/'first.zip').read_bytes()==(work/'second.zip').read_bytes()==pack.read_bytes()
with zipfile.ZipFile(source) as old,zipfile.ZipFile(pack) as new:
    assert old.read('dragon/resources/vendors.ddr')==new.read('dragon/resources/vendors.ddr')
    meta=json.loads(new.read('dragon/resources/manifest.json'))
    assert new.read('dragon/resources/ble-labels.ddb')==labels
    assert meta['ble_labels']==label_meta and meta['sd_only_profiles']==['waveshare_185b']
    assert len(labels)==meta['ble_labels']['bytes'] and hashlib.sha256(labels).hexdigest()==meta['ble_labels']['sha256']
    assert all('config.txt' not in n and 'wardrive/' not in n for n in new.namelist())
    (work/'full.ddr').write_bytes(new.read('dragon/resources/vendors.ddr'))
csv=work/'fixture.csv';csv.write_text('Registry,Assignment,Organization Name\nMA-L,001122,Fixture vendor\n')
vendors,_=build({24:csv},20261023);(work/'vendors.ddr').write_bytes(vendors)
collision,_=build_labels(dict(schema=1,snapshot=20261103,labels=[dict(kind='uuid16',id='004c',name='UUID namespace'),dict(kind='company16',id='004c',name='Company namespace')]))
(work/'collision.ddb').write_bytes(collision)
cases={}
def corrupt(name,offset,value,checksum=True):
    b=bytearray(labels);b[offset:offset+len(value)]=value
    if checksum:b[16:20]=bytes(4);b[16:20]=struct.pack('<I',zlib.crc32(b))
    cases[name]=bytes(b)
cases.update(truncated=labels[:-1],empty=b'',oversized=bytes(65537))
corrupt('schema',4,struct.pack('<I',2));corrupt('count',8,struct.pack('<I',0xffffffff))
corrupt('crc',len(labels)-2,b'X',False);corrupt('reserved',28,b'\x01')
corrupt('old',20,struct.pack('<I',20200101));corrupt('record-size',24,struct.pack('<I',7))
corrupt('kind',32,b'\x03');corrupt('record-reserved',33,b'\x01')
corrupt('duplicate',40,labels[32:36]);corrupt('unsorted',34,b'\xff\xff')
corrupt('offset',36,struct.pack('<I',0xffffffff));corrupt('middle-string',36,struct.pack('<I',1))
pool=32+label_meta['entries']*8
corrupt('empty-string',pool,b'\0');corrupt('control',pool,b'\x1b');corrupt('non-ascii',pool,b'\xff');corrupt('unterminated',len(labels)-1,b'X')
for name,data in cases.items():(work/(name+'.ddb')).write_bytes(data)
(work/'esp_heap_caps.h').write_text('#pragma once\n#include <stddef.h>\n#define MALLOC_CAP_SPIRAM 1\nextern size_t availableMemory;\ninline size_t heap_caps_get_free_size(int){return availableMemory;}\n')
(work/'esp32-hal-psram.h').write_text('#pragma once\n#include "app.h"\n')
(work/'app.h').write_text(r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using SemaphoreHandle_t=void*;
inline bool sdReady=true,mutexAvailable=true,shortVendor=false,shortLabels=false;
inline SemaphoreHandle_t sdMutex=(void*)1;
inline size_t availableMemory=8*1024*1024,allocations=0;
inline unsigned lockDepth=0,reads=0;
inline std::map<std::string,std::vector<uint8_t>> files;
constexpr int FILE_READ=0,pdTRUE=1;
inline int pdMS_TO_TICKS(int n){return n;}
inline int xSemaphoreTake(void*,int){if(!mutexAvailable)return 0;++lockDepth;return pdTRUE;}
inline void xSemaphoreGive(void*){if(lockDepth)--lockDepth;}
inline uint32_t micros(){static uint32_t t=0;return ++t;}
inline void delay(int){}
inline void* ps_malloc(size_t n){++allocations;return malloc(n);}
struct File {
 std::shared_ptr<std::vector<uint8_t>> data;size_t pos=0;bool shortRead=false;
 explicit operator bool()const{return bool(data);}
 size_t size()const{return data?data->size():0;}
 size_t read(uint8_t* p,size_t n){++reads;if(shortRead)return 0;n=std::min(n,size()-pos);memcpy(p,data->data()+pos,n);pos+=n;return n;}
 void close(){}
};
struct MockFS {File open(const char* name,int){auto p=files.find(name);if(p==files.end())return {};return {std::make_shared<std::vector<uint8_t>>(p->second),0,strstr(name,"vendors")?shortVendor:shortLabels};}};
inline MockFS CardFS;
struct MockSerial {template<class...T> void printf(const char* fmt,T...values){std::printf(fmt,values...);}};
inline MockSerial Serial;
''')
test=r'''
#include "app.h"
#include "sd_resources.h"
#include "vendor_registry.h"
#include "resource_index.h"
#include "ble_label_index.h"
#include "ble_insights.h"
#include <cassert>
#include <fstream>
#include <iterator>
std::vector<uint8_t> read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 const std::string folder=argv[1],mode=argv[2];
 auto vendors=read(folder+"/vendors.ddr"),labels=read(folder+"/labels.ddb");
 dragon::BleLabelIndex index;assert(index.load(labels.data(),labels.size()));assert(index.count()==44);
 assert(!strcmp(index.lookup(1,0x2a4b),"HID Report Map"));assert(!strcmp(index.lookup(2,0x004c),"Apple"));
 assert(!index.lookup(1,0x004c));assert(!index.lookup(0,0x004c));assert(!index.lookup(3,0x004c));assert(!index.lookup(1,0xffff));
 for(int i=3;i<argc;++i){auto bad=read(argv[i]);assert(!index.load(bad.data(),bad.size()));assert(!index.count());assert(!index.lookup(1,0x2a4b));}
 auto coll=read(folder+"/collision.ddb");assert(index.load(coll.data(),coll.size()));
 assert(!strcmp(index.lookup(1,0x004c),"UUID namespace"));assert(!strcmp(index.lookup(2,0x004c),"Company namespace"));
 auto full=read(folder+"/full.ddr");dragon::ResourceIndex fullIndex;assert(fullIndex.load(full.data(),full.size()));assert(fullIndex.count()==54282);
 files["/dragon/resources/vendors.ddr"]=vendors;files["/dragon/resources/ble-labels.ddb"]=labels;
 if(mode=="missing-card")sdReady=false;
 if(mode=="missing-vendor")files.erase("/dragon/resources/vendors.ddr");
 if(mode=="missing-labels")files.erase("/dragon/resources/ble-labels.ddb");
 if(mode=="corrupt-vendor")files["/dragon/resources/vendors.ddr"].back()^=1;
 if(mode=="corrupt-labels")files["/dragon/resources/ble-labels.ddb"].back()^=1;
 if(mode=="low-memory")availableMemory=2*1024*1024;
 if(mode=="mutex-failure")mutexAvailable=false;
 if(mode=="short-vendor")shortVendor=true;
 if(mode=="short-labels")shortLabels=true;
 sdResourcesBegin();assert(!lockDepth);
 const bool neither=mode=="missing-card"||mode=="low-memory"||mode=="mutex-failure";
 const bool vendorReady=!neither&&mode!="missing-vendor"&&mode!="corrupt-vendor"&&mode!="short-vendor";
 const bool labelReady=!neither&&mode!="missing-labels"&&mode!="corrupt-labels"&&mode!="short-labels";
 assert(sdResourcesReady()==vendorReady);assert(sdResourceVendorCount()==unsigned(vendorReady));
 uint8_t mac[]={0,0x11,0x22,0x33,0x44,0x55};
 assert(bool(sdResourceVendor(mac,false,0))==vendorReady);assert(!sdResourceVendor(mac,true,1));assert(!sdResourceVendor(mac,true,3));assert(!sdResourceVendor(nullptr,false,0));
#ifdef BOARD_WAVESHARE_185B
 assert(dragon::vendorRegistryBegin()==vendorReady);assert(dragon::vendorRegistryCount()==unsigned(vendorReady));
 assert(sdResourceBleLabelCount()==(labelReady?44:0));
 assert(bool(dragon::lab::uuidName(uint16_t(0x2a4b)))==labelReady);
 assert(bool(dragon::lab::uuidName("00002a4b-0000-1000-8000-00805f9b34fb"))==labelReady);
 assert(!dragon::lab::uuidName("00002a4b-0000-2000-8000-00805f9b34fb"));
 assert(!strcmp(dragon::lab::companyName(0x004c),labelReady?"Apple":"Unresolved company ID"));
 assert(!strcmp(dragon::lab::companyName(0xffff),"Unresolved company ID"));
#else
 assert(dragon::vendorRegistryBegin());assert(dragon::vendorRegistryCount()==40333);
 assert(!strcmp(dragon::lab::uuidName(uint16_t(0x2a4b)),"HID Report Map"));
 assert(!strcmp(dragon::lab::companyName(0x004c),"Apple"));
#endif
 const auto readCount=reads;const auto allocCount=allocations;
 // Classification is independent of the removable display-label resources.
 const uint8_t motorola[]={0x00,0x04,0x7d,0,0,0};auto match=dragon::addressHint(motorola,true,{dragon::Family::Wifi,dragon::Confidence::None,""});
 assert(match.family==dragon::Family::Alpr&&match.confidence==dragon::Confidence::Low);
 uint8_t swift[]={4,0xff,6,0,3};auto insight=dragon::lab::inspectAdvertisement(swift,sizeof(swift),sizeof(swift));assert(insight.valid);assert(!strcmp(insight.protocol,"Microsoft Swift Pair"));
 const char* before=dragon::registeredVendor(mac);if(vendorReady)assert(!strcmp(before,"Fixture vendor"));
 for(unsigned i=0;i<100000;++i){dragon::registeredVendor(mac);dragon::lab::uuidName(uint16_t(0x2a4b));dragon::lab::companyName(0x004c);}
 assert(reads==readCount&&allocations==allocCount); // No card I/O or allocation per lookup.
 files.clear();sdReady=false;sdResourcesBegin(); // Removal/format preserves live pointers.
 assert(dragon::registeredVendor(mac)==before);
 puts("PASS production SD registry/label loader, display mappings and detector independence");
}
'''
(work/'test.cpp').write_text(test)
includes=['-I'+str(work),'-I'+str(root/'firmware/include'),'-I'+str(root/'lib/opendroneid/src')]
for board in ['BOARD_WAVESHARE_185B','BOARD_AMOLED_175']:
    out=work/(board+('.exe' if sys.platform=='win32' else ''))
    sources=[str(work/'test.cpp'),str(root/'firmware/src/sd_resources.cpp'),str(root/'firmware/src/vendor_registry.cpp')]
    subprocess.run(compiler+flags+['-std=c++17','-UNDEBUG','-DARDUINO','-D'+board]+includes+sources+['-o',str(out)],check=True)
    preprocessed=subprocess.check_output(compiler+['-std=c++17','-E','-P','-D'+board]+includes+[sources[-1]],text=True)
    assert ('vendorPacked' in preprocessed)==(board!='BOARD_WAVESHARE_185B')
    for mode in ['ready','missing-card','missing-vendor','missing-labels','corrupt-vendor','corrupt-labels','low-memory','mutex-failure','short-vendor','short-labels']:
        subprocess.run([str(out),str(work),mode]+[str(work/(name+'.ddb')) for name in cases],check=True)
    print('PASS compiled-table boundary',board)
print('PASS deterministic pack, unchanged IEEE data, 44 mappings, corruption and live-pointer tests')
