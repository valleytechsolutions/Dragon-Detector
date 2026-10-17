"""Check every panel-size logo through the actual bounded firmware decoder."""
from pathlib import Path
import argparse,re,subprocess,sys,struct
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
source=(root/'firmware/src/radar_logo.cpp').read_text()
pattern=r'// Raw pixel CRC32: ([0-9a-f]+); bytes: (\d+)\s+static const uint8_t (\w+)_pixels\[\]=\{([^}]+)\};'
data=bytearray();count=0;raw=packed=0
for crc,size,name,array in re.findall(pattern,source):
    if name=='radar_logo':continue
    payload=bytes(map(int,re.findall(r'\d+',array)))
    data+=struct.pack('<III',int(size),len(payload),int(crc,16))+payload
    count+=1;raw+=int(size);packed+=len(payload)
assert count>=135,'Missing generated assets'
work=root/'.work';work.mkdir(exist_ok=True)
(work/'theme-assets.bin').write_bytes(data)
cpp=work/'theme-assets.cpp';exe=work/('theme-assets.exe' if sys.platform=='win32' else 'theme-assets')
cpp.write_text('''#include "lz_block.h"
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
int main(){std::ifstream f(".work/theme-assets.bin",std::ios::binary);unsigned count=0;uint32_t h[3];
 while(f.read((char*)h,12)){std::vector<uint8_t> src(h[1]),dst(h[0]);assert(f.read((char*)src.data(),src.size()));
 assert(dragon::unpackBlock(src.data(),src.size(),dst.data(),dst.size()));
 uint32_t crc=0xffffffff;for(auto b:dst){crc^=b;for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
 assert((crc^0xffffffff)==h[2]);++count;}
 assert(f.eof()&&count>=135);printf("PASS: %u native logo decodes match original pixel CRC32\\n",count);}
''')
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
subprocess.run(compiler+['-O2','-UNDEBUG','-std=c++17','-I'+str(root/'firmware/include'),str(cpp),'-o',str(exe)],check=True,cwd=root)
subprocess.run([str(exe)],check=True,cwd=root)
print(f'All three resolutions combined: {raw:,} original bytes, {packed:,} lossless packed bytes')
