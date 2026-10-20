"""Check the 1.75 H0175Y003AM hardware contract and matched partition layout."""
from pathlib import Path
import argparse,csv,subprocess,sys
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--cxx');args=parser.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
work=root/'.work/lilygo-tests';work.mkdir(parents=True,exist_ok=True)
source=work/'pins.cpp';exe=work/('pins.exe' if sys.platform=='win32' else 'pins')
source.write_text('''#define BOARD_LILYGO_175
#include "board_pins.h"
static_assert(SCREEN_RES==466 && BOARD_HAS_SD==1 && BOARD_SD_SPI==1 && BOARD_HAS_AUDIO==0);
static_assert(LCD_CS==10 && LCD_SCLK==12 && LCD_RESET==17 && LCD_ENABLE==16);
static_assert(LCD_SDIO0==11 && LCD_SDIO1==13 && LCD_SDIO2==14 && LCD_SDIO3==15);
static_assert(LCD_COL_OFFSET==6 && IIC_SDA==7 && IIC_SCL==6);
static_assert(TP_RESET==-1 && TP_INT==9 && TP_I2C_ADDR==0x5a);
static_assert(LILY_SD_CS==38 && LILY_SD_MOSI==39 && LILY_SD_MISO==40 && LILY_SD_SCLK==41);
int main(){}
''',encoding='utf-8')
subprocess.run(compiler+['-std=c++17','-I'+str(root/'firmware/include'),str(source),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
rows={row[0].strip():(int(row[3],0),int(row[4],0)) for row in csv.reader((root/'hardware/lilygo-amoled-175/partitions.csv').read_text().splitlines()) if row and not row[0].startswith('#')}
# Extracted from the submitted image's actual ESP partition table, not its name.
assert rows=={'nvs':(0x9000,0x5000),'otadata':(0xe000,0x2000),'app0':(0x10000,0x400000),'app1':(0x410000,0x400000),'spiffs':(0x810000,0x7f0000)}
assert max(offset+size for offset,size in rows.values())==16*1024*1024
print('PASS LILYGO 1.75 verified display/touch/SD pins and reference-BIN flash layout')
