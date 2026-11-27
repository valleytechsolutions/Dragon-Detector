"""Compile the production P4 USB drive mode (firmware/src/usb_drive.cpp) against
mock TinyUSB, SDMMC, FatFs and FreeRTOS layers, and check its contract: no medium
until the mode is on, one UNIT ATTENTION, write gathering with the status held
until the card has the data, read-ahead, the four ways out, the remount, and
the serial replies. Other boards must compile the file to nothing."""
from pathlib import Path
import argparse,shutil,subprocess,sys
root=Path(__file__).resolve().parents[1];work=root/'.work/usb-drive';shutil.rmtree(work,ignore_errors=True);(work/'mock').mkdir(parents=True)
p=argparse.ArgumentParser();p.add_argument('--cxx');args=p.parse_args()
compiler=[args.cxx] if args.cxx else [sys.executable,'-m','ziglang','c++']
for name in ['firmware/src/usb_drive.cpp','firmware/include/usb_drive.h','firmware/include/dragon_version.h']:
    shutil.copy(root/name,work/Path(name).name)
mock=r'''
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
uint32_t millis();
struct String:std::string{using std::string::string;String(const std::string& s):std::string(s){}bool startsWith(const char* p)const{return rfind(p,0)==0;}};
struct SerialMock{std::string out;template<class...T>void printf(const char* f,T...v){char b[512];snprintf(b,sizeof(b),f,v...);out+=b;}void println(const char* s){out+=s;out+="\n";}};
extern SerialMock Serial;
// FreeRTOS
typedef void* SemaphoreHandle_t;constexpr int pdTRUE=1;inline uint32_t pdMS_TO_TICKS(uint32_t t){return t;}
int xSemaphoreTake(SemaphoreHandle_t,uint32_t);void xSemaphoreGive(SemaphoreHandle_t);
extern SemaphoreHandle_t sdMutex;extern bool sdReady,loggingEnabled;
// ESP-IDF
typedef int esp_err_t;constexpr esp_err_t ESP_OK=0,ESP_FAIL=-1;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_DMA=4,MALLOC_CAP_INTERNAL=8;
void* heap_caps_aligned_alloc(size_t,size_t,int);
struct sdmmc_host_t{int slot=0;void* dma_aligned_buffer=nullptr;};
struct sdmmc_csd_t{uint32_t capacity=0,sector_size=0;};
struct sdmmc_card_t{sdmmc_host_t host;sdmmc_csd_t csd;};
esp_err_t sdmmc_read_sectors(sdmmc_card_t*,void*,size_t,size_t);esp_err_t sdmmc_write_sectors(sdmmc_card_t*,const void*,size_t,size_t);
esp_err_t sdmmc_get_status(sdmmc_card_t*);esp_err_t sdmmc_card_init(const sdmmc_host_t*,sdmmc_card_t*);
// FatFs and the Arduino SD_MMC object
typedef uint8_t BYTE;enum FRESULT{FR_OK=0,FR_DISK_ERR=1,FR_NO_FILESYSTEM=13};
struct FATFS{int id;};struct FFOBJID{FATFS* fs;};struct FF_DIR{FFOBJID obj;};
FRESULT f_opendir(FF_DIR*,const char*);FRESULT f_closedir(FF_DIR*);FRESULT f_mount(FATFS*,const char*,BYTE);
BYTE ff_diskio_get_pdrv_card(const sdmmc_card_t*);
namespace fs{struct SDMMCFS{void mockCard(sdmmc_card_t* c){_card=c;}protected:sdmmc_card_t* _card=nullptr;};}
extern fs::SDMMCFS SD_MMC;
// TinyUSB
typedef uint16_t (*tinyusb_descriptor_cb_t)(uint8_t*,uint8_t*);
enum tinyusb_interface_t{USB_INTERFACE_MSC,USB_INTERFACE_CDC=4,USB_INTERFACE_CUSTOM=8};
int tinyusb_enable_interface(tinyusb_interface_t,uint16_t,tinyusb_descriptor_cb_t);
uint8_t tinyusb_add_string_descriptor(const char*);uint8_t tinyusb_get_free_duplex_endpoint();
#define TUD_MSC_DESC_LEN 23
#define CFG_TUD_ENDPOINT_SIZE 512
#define TUD_MSC_DESCRIPTOR(itf,str,out,in,size) 9,4,itf,0,2,8,6,0x50,str,7,5,out,2,uint8_t((size)&0xff),uint8_t((size)>>8),0,7,5,in,2,uint8_t((size)&0xff),uint8_t((size)>>8),0
enum{SCSI_SENSE_NOT_READY=2,SCSI_SENSE_MEDIUM_ERROR=3,SCSI_SENSE_ILLEGAL_REQUEST=5,SCSI_SENSE_UNIT_ATTENTION=6};
extern "C" bool tud_msc_set_sense(uint8_t,uint8_t,uint8_t,uint8_t);
bool tud_mounted();bool tud_suspended();
// Dragon
enum class AudioMode{Idle,Recording};struct AppAudioStatus{AudioMode mode=AudioMode::Idle;};AppAudioStatus audioStatus();void audioStop();
bool setCapture(bool);void uiNotify(const char*,const char*,const void*,bool);
bool nodeBusy();bool wardriveBusy();bool wardriveUploadBusy();void bleLabStop();void recipeStop();void missionStop();void reconStop();
bool reconStorageBusy();bool reconRecording();bool bleLabOwnsRadio();void sdResourcesBegin();
bool missionActive();
inline size_t strlcpy(char* dst,const char* src,size_t n){size_t len=strlen(src);if(n){size_t copy=std::min(len,n-1);memcpy(dst,src,copy);dst[copy]=0;}return len;}
'''
(work/'mock/Arduino.h').write_text(mock)
for name in ['app.h','ble_lab.h','recon.h','recipes.h','missions.h','node_mode.h','wardrive.h','wardrive_upload.h','sd_resources.h','voice_recorder.h',
             'esp32-hal-tinyusb.h','tusb.h','ff.h','diskio_sdmmc.h','sdmmc_cmd.h','esp_heap_caps.h']:
    (work/'mock'/name).write_text('#pragma once\n#include <Arduino.h>\n')
test=r'''
#include <Arduino.h>
#include "usb_drive.h"
uint32_t now=1000;uint32_t millis(){return now;}
SerialMock Serial;SemaphoreHandle_t sdMutex=(void*)1;bool sdReady=true,loggingEnabled=true;
int lockDepth=0;bool lockFails=false;
int xSemaphoreTake(SemaphoreHandle_t,uint32_t){if(lockFails)return 0;assert(lockDepth==0);++lockDepth;return pdTRUE;}
void xSemaphoreGive(SemaphoreHandle_t){assert(lockDepth==1);--lockDepth;}
alignas(128) static uint8_t dmaBuffer[64*1024];size_t allocations=0;
void* heap_caps_aligned_alloc(size_t align,size_t n,int){assert(align==128&&n==sizeof(dmaBuffer));++allocations;return dmaBuffer;}
// A 64 MB card in memory, with call counters and error injection.
constexpr uint32_t CardSectors=131072;std::vector<uint8_t> disk(size_t(CardSectors)*512);
unsigned cardReads=0,cardWrites=0,statusCalls=0,inits=0;size_t lastReadCount=0,lastWriteCount=0;bool writeFails=false,cardAnswers=true;
sdmmc_card_t theCard;
esp_err_t sdmmc_read_sectors(sdmmc_card_t* c,void* d,size_t lba,size_t n){assert(c==&theCard&&lockDepth==1&&d==dmaBuffer&&lba+n<=CardSectors&&n<=128);++cardReads;lastReadCount=n;memcpy(d,&disk[lba*512],n*512);return ESP_OK;}
esp_err_t sdmmc_write_sectors(sdmmc_card_t* c,const void* s,size_t lba,size_t n){assert(c==&theCard&&lockDepth==1&&s==dmaBuffer&&lba+n<=CardSectors&&n<=128);++cardWrites;lastWriteCount=n;if(writeFails)return ESP_FAIL;memcpy(&disk[lba*512],s,n*512);return ESP_OK;}
esp_err_t sdmmc_get_status(sdmmc_card_t*){++statusCalls;return cardAnswers?ESP_OK:ESP_FAIL;}
esp_err_t sdmmc_card_init(const sdmmc_host_t* h,sdmmc_card_t* c){++inits;c->host=*h;if(!cardAnswers)return ESP_FAIL;c->csd={CardSectors,512};return ESP_OK;}
FATFS volume{7};bool fatMounted=true;FRESULT remount=FR_OK;std::vector<std::string> fatCalls;
FRESULT f_opendir(FF_DIR* d,const char* path){fatCalls.push_back(std::string("opendir ")+path);if(!fatMounted)return FR_NO_FILESYSTEM;d->obj.fs=&volume;return FR_OK;}
FRESULT f_closedir(FF_DIR*){return FR_OK;}
FRESULT f_mount(FATFS* fs,const char* drive,BYTE now){assert(lockDepth==1&&!strcmp(drive,"0:"));fatCalls.push_back(fs?(std::string("mount ")+char('0'+now)):std::string("unmount"));if(!fs){fatMounted=false;return FR_OK;}assert(fs==&volume);fatMounted=remount==FR_OK;return remount;}
BYTE ff_diskio_get_pdrv_card(const sdmmc_card_t* c){return c==&theCard?0:0xFF;}
fs::SDMMCFS SD_MMC;
tinyusb_descriptor_cb_t loader=nullptr;int registered=-1;
int tinyusb_enable_interface(tinyusb_interface_t i,uint16_t n,tinyusb_descriptor_cb_t cb){assert(n==TUD_MSC_DESC_LEN&&!loader);registered=i;loader=cb;return 0;}
uint8_t tinyusb_add_string_descriptor(const char* s){assert(!strcmp(s,"Dragon Detector Card"));return 5;}
uint8_t tinyusb_get_free_duplex_endpoint(){return 1;}
uint8_t senseKey=0,senseCode=0;
extern "C" bool tud_msc_set_sense(uint8_t,uint8_t k,uint8_t c,uint8_t){senseKey=k;senseCode=c;return true;}
bool mounted=true,suspended=false;bool tud_mounted(){return mounted;}bool tud_suspended(){return suspended;}
int vbus=1;int p4KnobUsbPower(){return vbus;}
bool closedStorage=false,prepareFails=false;unsigned prepares=0,resourceLoads=0;
bool watcherCloseStorage(){closedStorage=true;loggingEnabled=false;return true;}
const char* storagePrepareCard(){assert(lockDepth==1&&fatMounted);++prepares;if(prepareFails)return "the card is not writable";sdReady=true;return nullptr;}
void sdResourcesBegin(){assert(lockDepth==0&&sdReady);++resourceLoads;}
std::string noticeTitle,noticeDetail;bool noticeWatched=false;
void uiNotify(const char* t,const char* d,const void*,bool w){noticeTitle=t;noticeDetail=d;noticeWatched=w;}
bool node=false,saving=false;unsigned stops=0;
bool missionActive(){return false;}
bool nodeBusy(){return node;}bool wardriveBusy(){return false;}bool wardriveUploadBusy(){return false;}
void bleLabStop(){++stops;}void recipeStop(){++stops;}void missionStop(){++stops;}void reconStop(){++stops;}void audioStop(){++stops;}
bool setCapture(bool on){assert(!on);++stops;return false;}
bool reconStorageBusy(){return saving;}bool reconRecording(){return false;}bool bleLabOwnsRadio(){return false;}
AppAudioStatus audioStatus(){return {};}
extern "C" {
uint8_t tud_msc_get_maxlun_cb(void);void tud_msc_inquiry_cb(uint8_t,uint8_t[8],uint8_t[16],uint8_t[4]);bool tud_msc_test_unit_ready_cb(uint8_t);
void tud_msc_capacity_cb(uint8_t,uint32_t*,uint16_t*);bool tud_msc_is_writable_cb(uint8_t);bool tud_msc_prevent_allow_medium_removal_cb(uint8_t,uint8_t,uint8_t);
bool tud_msc_start_stop_cb(uint8_t,uint8_t,bool,bool);int32_t tud_msc_read10_cb(uint8_t,uint32_t,uint32_t,void*,uint32_t);
int32_t tud_msc_write10_cb(uint8_t,uint32_t,uint32_t,uint8_t*,uint32_t);int32_t tud_msc_scsi_cb(uint8_t,uint8_t const[16],void*,uint16_t);
bool __wrap_usbd_edpt_xfer(uint8_t,uint8_t,uint8_t*,uint16_t,bool);
unsigned realCalls=0,writesAtStatus=0;uint8_t lastStatus=0xff;
bool __real_usbd_edpt_xfer(uint8_t,uint8_t ep,uint8_t* d,uint16_t n,bool){++realCalls;writesAtStatus=cardWrites;if(ep==0x81&&n==13)lastStatus=d[12];return true;}
}
// One CSW on the MSC IN endpoint, as TinyUSB sends it after a command.
uint8_t csw(){uint8_t b[13]={'U','S','B','S',1,2,3,4,0,0,0,0,0};lastStatus=0xff;__wrap_usbd_edpt_xfer(0,0x81,b,13,false);return lastStatus;}
void command(const char* c){Serial.out.clear();assert(usbDriveCommand(String(c)));}
uint8_t chunk[4096];
void fill(uint8_t seed){for(unsigned i=0;i<sizeof(chunk);++i)chunk[i]=uint8_t(seed+i*7);}
int main(){
 // Registered at boot after the console (custom slot), interface string and endpoints 0x01/0x81.
 assert(registered==USB_INTERFACE_CUSTOM&&loader);uint8_t d[TUD_MSC_DESC_LEN],itf=2;assert(loader(d,&itf)==TUD_MSC_DESC_LEN&&itf==3&&d[2]==2&&d[8]==5&&d[11]==0x01&&d[18]==0x81);
 uint8_t vendor[8],product[16],revision[4];tud_msc_inquiry_cb(0,vendor,product,revision);
 assert(!memcmp(vendor,"Dragon  ",8)&&!memcmp(product,"Detector Card   ",16)&&tud_msc_get_maxlun_cb()==1&&tud_msc_is_writable_cb(0));
 // Off: no medium, nothing reaches the card.
 uint32_t count;uint16_t size;tud_msc_capacity_cb(0,&count,&size);assert(!count&&!size);
 assert(!tud_msc_test_unit_ready_cb(0)&&senseKey==SCSI_SENSE_NOT_READY&&senseCode==0x3A);
 assert(tud_msc_read10_cb(0,0,0,chunk,4096)==-1&&tud_msc_write10_cb(0,0,0,chunk,4096)==-1&&!cardReads&&!cardWrites);
 command("usb drive status");assert(Serial.out=="USB_DRIVE state=off reads=0 writes=0 sectors=0 why=\"\"\n");
 // No card: a clear refusal and nothing changed.
 char why[112];assert(!usbDriveStart(why,sizeof(why))&&!strcmp(why,"No TF card found. Insert a FAT32 card and restart.")&&sdReady&&!usbDriveOn());
 theCard.csd={CardSectors,512};SD_MMC.mockCard(&theCard);
 node=true;assert(!usbDriveStart(why,sizeof(why))&&!strcmp(why,"Stop the node first.")&&sdReady);node=false;
 saving=true;assert(!usbDriveStart(why,sizeof(why))&&!strcmp(why,"Saving a capture. Try again in a moment.")&&sdReady&&loggingEnabled);saving=false;
 command("node biscuit");assert(false);
}
'''
# The refusal of card commands is only for the mode being on: "node biscuit" falls through when off.
test=test.replace(' command("node biscuit");assert(false);\n}',r''' Serial.out.clear();assert(!usbDriveCommand(String("node biscuit"))&&!usbDriveCommand(String("ls /")));
 // On: card closed and unmounted, FAT never formatted, the card itself stays initialised.
 command("usb drive on");
 assert(Serial.out=="USB_DRIVE_ON sectors=131072\nUSB_DRIVE state=waiting reads=0 writes=0 sectors=131072 why=\"Your PC sees the card as a drive.\"\n");
 assert(usbDriveOn()&&!sdReady&&!loggingEnabled&&closedStorage&&allocations==1&&stops>=6&&fatCalls.back()=="unmount"&&!inits);
 command("ls /");assert(Serial.out.rfind("USB_DRIVE_BUSY",0)==0);command("put /dragon/x 4");assert(Serial.out.rfind("USB_DRIVE_BUSY",0)==0);command("node jcmk");assert(Serial.out.rfind("USB_DRIVE_BUSY",0)==0);
 // The PC: one UNIT ATTENTION, then ready, with the card's capacity.
 assert(!tud_msc_test_unit_ready_cb(0)&&senseKey==SCSI_SENSE_UNIT_ATTENTION&&senseCode==0x28);
 assert(tud_msc_test_unit_ready_cb(0));tud_msc_capacity_cb(0,&count,&size);assert(count==CardSectors&&size==512);
 now+=1000;assert(usbDriveState()==UsbDriveState::Mounted&&!strcmp(usbDriveStateText(),"Connected"));
 // Reads: 4 KB for a random read, then 64 KB ahead once it is sequential.
 for(uint32_t i=0;i<disk.size();++i)disk[i]=uint8_t(i*31+(i>>9));
 assert(tud_msc_read10_cb(0,1000,0,chunk,4096)==4096&&cardReads==1&&lastReadCount==8&&!memcmp(chunk,&disk[1000*512],4096));
 assert(tud_msc_read10_cb(0,1008,0,chunk,4096)==4096&&cardReads==2&&lastReadCount==128&&!memcmp(chunk,&disk[1008*512],4096));
 for(uint32_t lba=1016;lba<1136;lba+=8){assert(tud_msc_read10_cb(0,lba,0,chunk,4096)==4096&&!memcmp(chunk,&disk[lba*512],4096));}
 assert(cardReads==2);assert(tud_msc_read10_cb(0,1136,0,chunk,4096)==4096&&cardReads==3&&lastReadCount==128);
 assert(!strcmp(usbDriveStateText(),"Reading..."));
 // Read-ahead stops at the end of the card; past it is refused.
 assert(tud_msc_read10_cb(0,CardSectors-8,0,chunk,4096)==4096&&lastReadCount==8);
 assert(tud_msc_read10_cb(0,CardSectors-4,0,chunk,4096)==-1&&senseKey==SCSI_SENSE_ILLEGAL_REQUEST);
 // A 96 KB WRITE(10): gathered into 64 KB transfers; its status waits for the card.
 unsigned before=cardWrites;
 for(unsigned i=0;i<24;++i){fill(uint8_t(i));assert(tud_msc_write10_cb(0,2000+i*8,0,chunk,4096)==4096);}
 assert(cardWrites==before+1&&lastWriteCount==128);
 assert(csw()==0&&cardWrites==before+2&&lastWriteCount==64&&writesAtStatus==before+2);
 for(unsigned i=0;i<24;++i){fill(uint8_t(i));assert(!memcmp(&disk[(2000+i*8)*512],chunk,4096));}
 assert(!strcmp(usbDriveStateText(),"Writing..."));
 // A status with nothing held passes straight through; so does every other transfer.
 before=cardWrites;assert(csw()==0&&cardWrites==before);uint8_t other[13]={'U','S','B','S'};__wrap_usbd_edpt_xfer(0,0x84,other,13,false);assert(cardWrites==before);
 // A write the card refuses fails its status, with MEDIUM ERROR sense.
 fill(9);assert(tud_msc_write10_cb(0,5000,0,chunk,4096)==4096);writeFails=true;assert(csw()==1&&senseKey==SCSI_SENSE_MEDIUM_ERROR&&senseCode==0x0C);writeFails=false;
 // Non-contiguous data goes to the card first; a read flushes, and sees what was written.
 fill(3);assert(tud_msc_write10_cb(0,6000,0,chunk,4096)==4096);before=cardWrites;fill(4);assert(tud_msc_write10_cb(0,7000,0,chunk,4096)==4096&&cardWrites==before+1);
 assert(tud_msc_read10_cb(0,7000,0,chunk,4096)==4096&&cardWrites==before+2);fill(4);uint8_t back[4096];assert(tud_msc_read10_cb(0,7000,0,back,4096)==4096&&!memcmp(back,chunk,4096));
 fill(5);assert(tud_msc_write10_cb(0,7000,0,chunk,4096)==4096&&csw()==0);assert(tud_msc_read10_cb(0,7000,0,back,4096)==4096&&!memcmp(back,chunk,4096));
 // SYNCHRONIZE CACHE, VERIFY, MODE SENSE(10) and an unknown command.
 const uint8_t sync[16]={0x35},verify[16]={0x2F},sense10[16]={0x5A},unknown[16]={0x4A};uint8_t reply[64];
 assert(tud_msc_scsi_cb(0,sync,reply,0)==0&&tud_msc_scsi_cb(0,verify,reply,0)==0&&tud_msc_scsi_cb(0,sense10,reply,64)==8&&!(reply[3]&0x80));
 assert(tud_msc_scsi_cb(0,unknown,reply,64)==-1&&senseKey==SCSI_SENSE_ILLEGAL_REQUEST);
 // Done right after a write, or while the PC has the medium locked, asks twice.
 assert(tud_msc_prevent_allow_medium_removal_cb(0,1,0));assert(!usbDriveDone()&&usbDriveConfirming()&&usbDriveOn());
 now+=5000;assert(!usbDriveConfirming()&&!usbDriveDone());assert(tud_msc_prevent_allow_medium_removal_cb(0,0,0));now+=5000;
 // Eject on the PC: the medium goes at once, the card is Dragon's again after the loop.
 assert(tud_msc_start_stop_cb(0,0,false,true));assert(!tud_msc_test_unit_ready_cb(0)&&senseKey==SCSI_SENSE_NOT_READY);
 Serial.out.clear();usbDriveLoop();assert(usbDriveOn()&&!strcmp(usbDriveStateText(),"Finishing..."));now+=250;usbDriveLoop();
 assert(!usbDriveOn()&&sdReady&&loggingEnabled&&prepares==1&&resourceLoads==1&&fatCalls.back()=="mount 1"&&noticeTitle=="USB drive off"&&!noticeWatched&&lockDepth==0);
 assert(Serial.out.rfind("USB_DRIVE_OFF mounted=1 ",0)==0&&Serial.out.find("why=\"Ejected on the PC.\"")!=std::string::npos);
 command("usb drive status");assert(Serial.out.find("USB_DRIVE state=off ")==0&&Serial.out.find(" sectors=131072 why=\"Ejected on the PC.\"")!=std::string::npos);
 // Unplug: no bus and no VBUS for a second, once the PC had been seen.
 assert(usbDriveStart(why,sizeof(why)));usbDriveLoop();now+=60000;usbDriveLoop();assert(usbDriveOn());   // never seen: waits for Done
 assert(!tud_msc_test_unit_ready_cb(0)&&tud_msc_test_unit_ready_cb(0));suspended=true;vbus=0;
 usbDriveLoop();now+=500;usbDriveLoop();assert(usbDriveOn());now+=600;usbDriveLoop();now+=250;usbDriveLoop();
 assert(!usbDriveOn()&&noticeTitle=="USB drive off");command("usb drive status");assert(Serial.out.find("why=\"USB unplugged.\"")!=std::string::npos);
 // Suspended with power (PC asleep): after 30 s.
 suspended=false;vbus=1;assert(usbDriveStart(why,sizeof(why))&&!tud_msc_test_unit_ready_cb(0)&&tud_msc_test_unit_ready_cb(0));suspended=true;
 usbDriveLoop();now+=20000;usbDriveLoop();assert(usbDriveOn());now+=10001;usbDriveLoop();now+=250;usbDriveLoop();assert(!usbDriveOn());suspended=false;
 // Serial off, with the PC's last data still held: it reaches the card first.
 assert(usbDriveStart(why,sizeof(why))&&!tud_msc_test_unit_ready_cb(0));fill(6);assert(tud_msc_write10_cb(0,8000,0,chunk,4096)==4096);before=cardWrites;
 command("usb drive off");assert(cardWrites==before+1&&!memcmp(&disk[8000*512],chunk,4096)&&!usbDriveOn());
 assert(Serial.out.find("USB_DRIVE state=off reads=0 writes=8 sectors=131072 why=\"Turned off over serial.\"")!=std::string::npos);
 // The PC formatted the card exFAT: reported, never formatted, the mode can be used again to fix it.
 remount=FR_NO_FILESYSTEM;assert(usbDriveStart(why,sizeof(why))&&usbDriveDone()==true);now+=250;usbDriveLoop();
 assert(!usbDriveOn()&&!sdReady&&noticeTitle=="Card not mounted"&&noticeWatched&&noticeDetail=="No FAT32 file system. Format the card as FAT32 on the PC.");
 remount=FR_OK;unsigned loads=resourceLoads;assert(usbDriveStart(why,sizeof(why)));command("usb drive off");assert(sdReady&&resourceLoads==loads+1);
 // A card taken out and put back is initialised again on its slot; a missing one is reported.
 cardAnswers=false;assert(usbDriveStart(why,sizeof(why)));command("usb drive off");assert(!sdReady&&inits==1&&noticeTitle=="Card not mounted");
 assert(!usbDriveStart(why,sizeof(why))&&!strcmp(why,"No TF card found. Insert a FAT32 card and try again."));
 cardAnswers=true;assert(usbDriveStart(why,sizeof(why))&&inits==2);command("usb drive off");assert(sdReady);
 // Three card errors in a row: the PC loses the medium and the screen says so.
 assert(usbDriveStart(why,sizeof(why))&&!tud_msc_test_unit_ready_cb(0));writeFails=true;
 for(int i=0;i<3;++i){assert(tud_msc_write10_cb(0,9000,0,chunk,4096)==4096);csw();}
 assert(!tud_msc_test_unit_ready_cb(0)&&!strcmp(usbDriveStateText(),"Card error"));usbDriveLoop();writeFails=false;
 assert(usbDriveDone());now+=250;usbDriveLoop();assert(!usbDriveOn()&&sdReady);
 assert(lockDepth==0&&allocations==1);
 puts("PASS P4 USB drive: medium states, attention, read-ahead, held CSW, write errors, eject, unplug, suspend, serial, remount");
}''')
(work/'test.cpp').write_text(test)
exe=work/('usb_drive.exe' if sys.platform=='win32' else 'usb_drive')
common=['-std=c++17','-O1','-UNDEBUG','-Wall','-Wno-unused-function','-I'+str(work/'mock'),'-I'+str(work)]
subprocess.run(compiler+common+['-DBOARD_WAVESHARE_P4_KNOB',str(work/'usb_drive.cpp'),str(work/'test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
# Every other board: the file is empty and the header gives inline stubs.
pre=subprocess.check_output(compiler+['-std=c++17','-E','-P','-DBOARD_AMOLED_175C','-I'+str(work/'mock'),'-I'+str(work),str(work/'usb_drive.cpp')],text=True)
assert 'tud_msc_read10_cb' not in pre and '__wrap_usbd_edpt_xfer' not in pre and 'registerAtBoot' not in pre and 'inline bool usbDriveOn(){return false;}' in pre
print('PASS other boards compile usb_drive.cpp to stubs')
