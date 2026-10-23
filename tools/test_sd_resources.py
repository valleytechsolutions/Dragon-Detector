"""Exercise the actual C++ resource reader, corrupt packs and every board gate."""
from pathlib import Path
import csv,io,json,struct,subprocess,sys,time,zipfile,zlib,hashlib
root=Path(__file__).resolve().parents[1];work=root/'.work/resource-tests';work.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'tools'))
from build_sd_resources import build,archive
def fixture(bits,rows):
    p=work/f'{bits}.csv';s=io.StringIO();w=csv.writer(s);w.writerow(['Registry','Assignment','Organization Name'])
    for prefix,name in rows:w.writerow([{24:'MA-L',28:'MA-M',36:'MA-S'}[bits],prefix,name])
    p.write_text(s.getvalue());return p
data,meta=build({24:fixture(24,[('001122','Broad')]),28:fixture(28,[('0011223','Medium')]),36:fixture(36,[('001122334','Specific')])},20261023)
(work/'valid.ddr').write_bytes(data)
archive(work/'first.zip',data,meta);archive(work/'second.zip',data,meta)
assert (work/'first.zip').read_bytes()==(work/'second.zip').read_bytes()
cases={'truncated':data[:-1],'future-schema':data[:4]+struct.pack('<I',2)+data[8:],'crc':data[:-1]+bytes([data[-1]^1]),'old':data[:20]+struct.pack('<I',20200101)+data[24:],'header-bitflip':data[:20]+struct.pack('<I',20261024)+data[24:],'huge-count':data[:8]+struct.pack('<I',0xffffffff)+data[12:]}
def corrupt_record(offset,value):
    p=bytearray(data);p[offset]=value;p[16:20]=bytes(4);p[16:20]=struct.pack('<I',zlib.crc32(p));return p
cases.update({'bad-width':corrupt_record(32,25),'name-offset':corrupt_record(38,255),'unsorted':corrupt_record(42,24),'bad-label':corrupt_record(len(data)-2,10)})
for name,value in cases.items():(work/f'{name}.ddr').write_bytes(value)
source=r'''
#include "resource_index.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <iterator>
#include <chrono>
#include <cstdio>
std::vector<uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 dragon::ResourceIndex index;auto valid=read(argv[1]);assert(index.load(valid.data(),valid.size()));
 uint8_t mac[]={0x00,0x11,0x22,0x33,0x44,0x55};assert(!strcmp(index.lookup(mac),"Specific"));
 mac[4]=0x50;assert(!strcmp(index.lookup(mac),"Medium"));mac[3]=0x40;assert(!strcmp(index.lookup(mac),"Broad"));
 assert(!index.lookup(mac,true,1));assert(!index.lookup(mac,true,3));assert(!index.lookup(mac,true,4));assert(index.lookup(mac,true,2));
 mac[0]=2;assert(!index.lookup(mac));mac[0]=1;assert(!index.lookup(mac));assert(!index.lookup(nullptr));
 for(int i=2;i<argc-1;++i){auto bad=read(argv[i]);assert(!index.load(bad.data(),bad.size()));assert(!index.lookup(mac));}
 auto full=read(argv[argc-1]);auto start=std::chrono::steady_clock::now();assert(index.load(full.data(),full.size()));
 auto loaded=std::chrono::steady_clock::now();unsigned matches=0;
 for(unsigned n=0;n<100000;++n){uint8_t m[]={uint8_t((n>>16)&0xfc),uint8_t(n>>8),uint8_t(n),0x33,0x44,0x55};if(index.lookup(m))++matches;}
 auto end=std::chrono::steady_clock::now();
 printf("entries=%u bytes=%zu host_validate_us=%lld host_100k_lookup_us=%lld matches=%u\n",index.count(),full.size(),(long long)std::chrono::duration_cast<std::chrono::microseconds>(loaded-start).count(),(long long)std::chrono::duration_cast<std::chrono::microseconds>(end-loaded).count(),matches);
}
'''
(work/'reader.cpp').write_text(source)
compiler=[sys.executable,'-m','ziglang','c++']
subprocess.run(compiler+['-std=c++17','-O2','-UNDEBUG','-I'+str(root/'firmware/include'),str(work/'reader.cpp'),'-o',str(work/'reader.exe')],check=True)
with zipfile.ZipFile(root/'web/resources/dragon-resources-v1-20261023.zip') as z:
    raw=z.read('dragon/resources/vendors.ddr');manifest=json.loads(z.read('dragon/resources/manifest.json'))
    assert hashlib.sha256(raw).hexdigest()==manifest['sha256']
    assert len(raw)==manifest['bytes']
    (work/'full.ddr').write_bytes(raw)
    assert all('config.txt' not in n and 'wardrive/' not in n for n in z.namelist())
subprocess.run([str(work/'reader.exe'),str(work/'valid.ddr')]+[str(work/f'{name}.ddr') for name in cases]+[str(work/'full.ddr')],check=True)
boards={'spotpear':('',1,0),'waveshare':('BOARD_AMOLED_175C',0,0),'waveshare_175':('BOARD_AMOLED_175',1,0),'waveshare_175_gps':('BOARD_AMOLED_175',1,1),'stopwatch':('BOARD_STOPWATCH',0,0),'xiao_round':('BOARD_XIAO_ROUND',1,0),'waveshare_lite':('BOARD_RADAR_LITE',0,0),'lilygo_175':('BOARD_LILYGO_175',1,0),'elecrow_21':('BOARD_CROWPANEL_21',0,0),'m5dial':('BOARD_M5DIAL',0,0)}
for board,(define,sd,gps) in boards.items():
    p=work/'gate.cpp';p.write_text(f'#include "board_pins.h"\nstatic_assert(BOARD_HAS_SD=={sd});\nstatic_assert(BOARD_HAS_GPS=={gps});\nint main(){{}}')
    flags=(['-D'+define] if define else [])+(['-DBOARD_GPS_LC76G'] if gps else [])
    subprocess.run(compiler+['-std=c++17','-c','-o',str(work/'gate.o'),'-I'+str(root/'firmware/include')]+flags+[str(p)],check=True)
    print('PASS capability',board)
print('PASS resource corruption, longest prefix, address privacy, all board gates')
