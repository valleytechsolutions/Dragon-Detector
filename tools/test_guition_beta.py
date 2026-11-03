"""Verify isolated Guition beta table, stable profile and production SPI ownership."""
from pathlib import Path
import argparse
import configparser
import hashlib
import re
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
work=root/'.work/guition-beta-tests'
work.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--cxx');args=parser.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
stable_hashes={
 'firmware/include/vendor_display.h':'fcf471df80bca8ab51a9470e8268fc6cb037a08dd1acb6f9b7368ccb59868759',
 'hardware/spotpear/lib/ESP32_Display_Panel/src/lcd/base/esp_lcd_st77916.c':'0390f2e4cafe648c72ac6aeeffa4a3bdc14fb0e7d9dd1efc5ad61bf3b515e9d0',
 'hardware/spotpear/partitions.csv':'085f192002303b212b3d0b764993c645bdbc354562f34657f60baf49ce8e7afa',
}
for name,digest in stable_hashes.items():
 assert hashlib.sha256((root/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
upstream=(root/'third_party/guition-jc3636w518/lcd_init_commands.upstream.h').read_bytes()
assert hashlib.sha256(upstream).hexdigest()=='a320cc874a5c53967a1fb6d56fe9891e42908fb8032559f589b4bed0f07e3691'
pattern=r'\{(0x[0-9a-f]+),\s*\(const uint8_t\[\]\)\{([^}]+)\},\s*(\d+),\s*(\d+)\}'
def commands(text):
 return [(int(cmd,16),tuple(int(b.strip(),16) for b in data.split(',')),int(count),int(delay)) for cmd,data,count,delay in re.findall(pattern,text,re.I)]
expected=commands(upstream.decode())
actual=commands((root/'firmware/include/guition_lcd_commands.h').read_text(encoding='utf-8'))
assert len(actual)==184 and actual==expected
assert [(cmd,delay) for cmd,data,n,delay in actual if delay]==[(0x11,120)]
assert actual[:4]==[(0xf0,(0x28,),1,0),(0xf2,(0x28,),1,0),(0x73,(0xf0,),1,0),(0x7c,(0xd1,),1,0)]
assert actual[-1]==(0x29,(0,),1,0)
ini=configparser.ConfigParser(interpolation=None);ini.read(root/'platformio.ini',encoding='utf-8')
assert ini['env:guition_185_beta']['extends']=='env:spotpear'
assert ini['env:guition_185_beta']['build_flags']=='${env:spotpear.build_flags} -DBOARD_GUITION_185_BETA'
assert 'GUITION' not in ini['env:spotpear']['build_flags']
assert ini['env']['board_upload.flash_size']=='16MB'
assert ini['env']['board_build.arduino.memory_type']=='qio_opi'
assert ini['env']['board_build.partitions']=='hardware/spotpear/partitions.csv'
display=(root/'firmware/src/display.cpp').read_text(encoding='utf-8')
assert '#ifdef BOARD_GUITION_185_BETA\n#include "guition_display.h"\n#else\n#include "vendor_display.h"\n#endif' in display

source=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include "board_pins.h"
static_assert(SCREEN_RES==360&&BOARD_HAS_SD==1&&BOARD_HAS_AUDIO==0&&BOARD_HAS_GPS==0);
static_assert(TFT_CS==10&&TFT_SCK==9&&TFT_SDA0==11&&TFT_SDA1==12&&TFT_SDA2==13&&TFT_SDA3==14&&TFT_RST==47&&TFT_BLK==15);
static_assert(TOUCH_PIN_NUM_I2C_SDA==7&&TOUCH_PIN_NUM_I2C_SCL==8&&TOUCH_PIN_NUM_INT==41&&TOUCH_PIN_NUM_RST==40);
static_assert(SD_MMC_D0_PIN==2&&SD_MMC_D1_PIN==1&&SD_MMC_D2_PIN==6&&SD_MMC_D3_PIN==5&&SD_MMC_CLK_PIN==3&&SD_MMC_CMD_PIN==4);
static_assert(AUDIO_I2S_MCK_IO==-1&&AUDIO_I2S_BCK_IO==18&&AUDIO_I2S_WS_IO==16&&AUDIO_I2S_DO_IO==17&&AUDIO_MUTE_PIN==48);
using esp_err_t=int;using spi_host_device_t=int;using spi_device_handle_t=void*;
constexpr int SPI2_HOST=2,SPI_DMA_CH_AUTO=3,ESP_OK=0,ESP_ERR_INVALID_STATE=1,ESP_ERR_TIMEOUT=2,OUTPUT=1,LOW=0,HIGH=1;
constexpr unsigned SPI_DEVICE_HALFDUPLEX=1,SPI_TRANS_USE_RXDATA=2;
constexpr unsigned pdMS_TO_TICKS(unsigned n){return n;}
struct spi_bus_config_t {int clk,d0,d1,d2,d3;};
#define ESP_PANEL_HOST_QSPI_CONFIG_DEFAULT(clk,d0,d1,d2,d3) {clk,d0,d1,d2,d3}
struct esp_lcd_panel_io_spi_config_t {int cs;};
#define ESP_PANEL_IO_QSPI_CONFIG_DEFAULT(cs) {cs}
struct spi_device_interface_config_t {unsigned command_bits=0,address_bits=0,dummy_bits=0,clock_speed_hz=0,mode=0,spics_io_num=0,queue_size=0,flags=0,cs_ena_pretrans=0,cs_ena_posttrans=0;};
struct spi_transaction_t {uint16_t cmd=0;uint64_t addr=0;unsigned rxlength=0,flags=0;uint8_t rx_data[4]{};};
struct esp_lcd_panel_vendor_init_cmd_t {int cmd;const void* data;unsigned data_bytes,delay_ms;};
std::string fail;
std::vector<std::string> calls;
bool hostOwned=false,deviceOwned=false,pending=false,panelIo=false;
unsigned hostStarts=0,probeCount=0,panelCount=0,delayTotal=0;
spi_transaction_t* queued=nullptr;
void pinMode(int pin,int mode){assert(pin==47&&mode==OUTPUT);calls.push_back("reset_pin");}
void digitalWrite(int pin,int value){assert(pin==47);calls.push_back(value?"reset_high":"reset_low");}
void delay(unsigned ms){assert(ms==10||ms==120);delayTotal+=ms;}
struct SerialStub {template<class... A>void printf(const char*,A...){}} Serial;
int spi_bus_initialize(int id,const spi_bus_config_t* config,int dma){
 assert(id==SPI2_HOST&&dma==SPI_DMA_CH_AUTO);assert(config->clk==9&&config->d0==11&&config->d1==12&&config->d2==13&&config->d3==14);
 calls.push_back("host_init");++hostStarts;
 if(fail=="probe_host"||(fail=="panel_host"&&hostStarts==2)||hostOwned)return ESP_ERR_INVALID_STATE;
 hostOwned=true;return ESP_OK;
}
int spi_bus_add_device(int id,const spi_device_interface_config_t* config,spi_device_handle_t* out){
 assert(hostOwned&&!deviceOwned&&id==SPI2_HOST);assert(config->clock_speed_hz==3000000&&config->command_bits==8&&config->address_bits==24&&config->dummy_bits==0);
 assert(config->spics_io_num==10&&config->queue_size==1&&config->mode==0&&config->flags==SPI_DEVICE_HALFDUPLEX&&config->cs_ena_pretrans==1&&config->cs_ena_posttrans==1);
 calls.push_back("probe_add");if(fail=="probe_add")return ESP_ERR_INVALID_STATE;
 deviceOwned=true;*out=reinterpret_cast<void*>(1);return ESP_OK;
}
int spi_device_queue_trans(spi_device_handle_t device,spi_transaction_t* transaction,unsigned timeout){
 assert(device&&deviceOwned&&!pending&&timeout==25);assert(transaction->cmd==0x0b&&transaction->addr==0x400&&transaction->rxlength==32&&transaction->flags==SPI_TRANS_USE_RXDATA);
 calls.push_back("probe_queue");++probeCount;if(fail=="probe_queue")return ESP_ERR_TIMEOUT;
 pending=true;queued=transaction;return ESP_OK;
}
int spi_device_get_trans_result(spi_device_handle_t device,spi_transaction_t** result,unsigned timeout){
 assert(device&&deviceOwned&&pending&&timeout==25);calls.push_back("probe_result");if(fail=="probe_result")return ESP_ERR_TIMEOUT;
 pending=false;*result=queued;queued->rx_data[0]=0;queued->rx_data[1]=0x77;queued->rx_data[2]=0x91;queued->rx_data[3]=0x16;return ESP_OK;
}
int spi_bus_remove_device(spi_device_handle_t device){assert(device&&deviceOwned&&!pending);calls.push_back("probe_remove");if(fail=="probe_remove")return ESP_ERR_INVALID_STATE;deviceOwned=false;return ESP_OK;}
int spi_bus_free(int id){assert(id==SPI2_HOST&&hostOwned&&!deviceOwned&&!panelIo);calls.push_back("host_free");if(fail=="probe_free"&&hostStarts==1)return ESP_ERR_INVALID_STATE;hostOwned=false;return ESP_OK;}
struct ESP_PanelBus_QSPI {
 unsigned hz=0;
 static void* operator new(size_t size,const std::nothrow_t&)noexcept{return fail=="qspi_allocate"?nullptr:malloc(size);}
 static void operator delete(void* ptr)noexcept{free(ptr);}
 ESP_PanelBus_QSPI(const esp_lcd_panel_io_spi_config_t& io,int host){assert(io.cs==10&&host==SPI2_HOST&&hostOwned&&!deviceOwned);calls.push_back("panel_bus_new");}
 ~ESP_PanelBus_QSPI(){assert(!panelCount);calls.push_back("panel_io_delete");panelIo=false;}
 void configQspiFreqHz(unsigned value){assert(!panelIo&&value==20000000);hz=value;calls.push_back("panel_frequency");}
 bool begin(){assert(hz==20000000&&hostOwned);calls.push_back("panel_bus_begin");if(fail=="qspi_begin")return false;panelIo=true;return true;}
};
struct ESP_PanelLcd {
 virtual ~ESP_PanelLcd(){assert(panelIo&&panelCount==1);calls.push_back("panel_delete");--panelCount;}
 bool step(const char* name){calls.push_back(name);return fail!=name;}
 bool configVendorCommands(const esp_lcd_panel_vendor_init_cmd_t* table,unsigned count){assert(count==184&&table[0].cmd==0xf0&&static_cast<const uint8_t*>(table[0].data)[0]==0x28);return step("vendor_commands");}
 bool init(){return step("panel_init");}bool reset(){return step("panel_reset");}bool begin(){return step("panel_begin");}
 bool invertColor(bool value){assert(value);return step("panel_invert");}bool displayOn(){return step("panel_on");}
};
struct ESP_PanelLcd_ST77916:ESP_PanelLcd {
 static void* operator new(size_t size,const std::nothrow_t&)noexcept{return fail=="panel_allocate"?nullptr:malloc(size);}
 static void operator delete(void* ptr)noexcept{free(ptr);}
 ESP_PanelLcd_ST77916(ESP_PanelBus_QSPI* bus,int bits,int reset){assert(bus&&panelIo&&bits==16&&reset==47);++panelCount;calls.push_back("panel_new");}
};
static ESP_PanelLcd* lcd=nullptr;
#include "guition_panel_init.h"
void clean(){
 fail.clear();pending=false;guitionPanelCleanup();assert(guitionProbeCleanup());
 calls.clear();hostStarts=probeCount=delayTotal=0;queued=nullptr;guitionProbeError=ESP_ERR_INVALID_STATE;memset(guitionId,0,4);
 assert(!lcd&&!hostOwned&&!deviceOwned&&!panelIo&&!panelCount);
}
int main(){
#ifdef BOARD_GUITION_185_BETA
 assert(std::string(BOARD_NAME)=="Guition / Surenoo JC3636W518 Beta 1");
#else
 assert(std::string(BOARD_NAME)=="Spotpear JC3636W518");
#endif
 assert(guitionPanelBegin()&&delayTotal==130&&hostStarts==2&&probeCount==1&&lcd&&panelIo&&hostOwned&&!deviceOwned&&!guitionProbeOwned);
 assert(guitionProbeError==ESP_OK&&guitionId[1]==0x77&&guitionId[2]==0x91&&guitionId[3]==0x16);
 const std::vector<std::string> expected={"reset_pin","reset_low","reset_high","host_init","probe_add","probe_queue","probe_result","probe_remove","host_free","host_init","panel_bus_new","panel_frequency","panel_bus_begin","panel_new","vendor_commands","panel_init","panel_reset","panel_begin","panel_invert","panel_on"};
 assert(calls==expected);unsigned attempts=probeCount;assert(!guitionPanelBegin()&&probeCount==attempts);clean();
 for(const char* name:{"probe_host","panel_host","qspi_allocate","qspi_begin","panel_allocate","vendor_commands","panel_init","panel_reset","panel_begin","panel_invert","panel_on"}){
  fail=name;assert(!guitionPanelBegin());assert(!lcd&&!panelIo&&!panelCount&&!hostOwned&&!deviceOwned);assert(probeCount<=1);clean();
 }
 for(const char* name:{"probe_add","probe_queue"}){fail=name;assert(guitionPanelBegin());assert(guitionProbeError!=ESP_OK&&!guitionProbeOwned&&!deviceOwned);clean();}
 for(const char* name:{"probe_result","probe_remove","probe_free"}){
  fail=name;assert(!guitionPanelBegin());assert(guitionProbeOwned&&!lcd&&!panelIo&&hostStarts==1);assert(guitionProbeError!=ESP_OK);
  auto count=calls.size();assert(!guitionPanelBegin()&&calls.size()==count);clean();
 }
 // An existing SPI owner is never removed or freed on a failed claim.
 hostOwned=true;assert(!guitionPanelBegin());assert(hostOwned&&calls.back()=="host_init"&&calls.size()==4);hostOwned=false;clean();
 puts("PASS Guition: exact table, fixed beta identity/pins, bounded raw ID, no revision guessing,20MHz beforebegin,commands beforeinit, failure ownership and stable isolation");
}
'''
for profile,flags in [('guition',['-DBOARD_GUITION_185_BETA']),('spotpear',[])]:
 cpp=work/(profile+'.cpp');cpp.write_text(source,encoding='utf-8');exe=work/(profile+'.exe')
 subprocess.run(compiler+['-std=gnu++17','-O1','-UNDEBUG','-I'+str(root/'firmware/include')]+flags+[str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS stable Spotpear driver/table/partition SHA256, inherited beta environment and pinned184-command byte/delay match')
