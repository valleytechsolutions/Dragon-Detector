// USB drive mode for the Waveshare ESP32-P4 Knob (see usb_drive.h).
//
// The mass storage interface is registered at boot after the serial console,
// so the console keeps interfaces 0 and 1 (and its COM port), and it reports
// "medium not present" until the mode starts. TinyUSB (MIT, prebuilt in
// arduino_tinyusb) runs the bulk-only transport in its own task; the
// tud_msc_* callbacks below only move whole sectors between its 4 KB buffer
// and one 64 KB DMA buffer, with sdMutex held, and re-check the mode inside it.
//
// Writes: the data of one WRITE(10) is gathered and written as one
// multi-sector transfer when the buffer is full or the data stops being
// contiguous, and always before that command's status (CSW) goes to the PC.
// usbd_edpt_xfer is wrapped (-Wl,--wrap in CMakeLists.txt) so the 13-byte CSW
// on this interface's IN endpoint waits until the card has the data; when the
// write fails the CSW says "failed" with MEDIUM ERROR sense. Nothing is
// acknowledged before it is on the card, so there is no write-back cache to
// lose. Reads: sequential reads fill the buffer ahead, random reads take 4 KB.
//
// The card stays initialised on SDMMC slot 0 throughout. Only the FAT volume
// is unmounted (f_mount(NULL)) and mounted again (f_mount(fs,drive,1)), the
// same pair ESP-IDF's esp_vfs_fat_sdcard_format_cfg() uses around a format.
// The host, slot 1 (the radio chip's SDIO link) and LDO4 are never touched.
#include "usb_drive.h"
#ifdef DRAGON_USB_DRIVE
#include "app.h"
#include "ble_lab.h"
#include "recon.h"
#include "recipes.h"
#include "missions.h"
#include "node_mode.h"
#include "wardrive.h"
#include "wardrive_upload.h"
#include "sd_resources.h"
#include "dragon_version.h"
#include "voice_recorder.h"   // voiceBusy/voiceShutdown (declared only when BOARD_HAS_MIC)
#include <esp32-hal-tinyusb.h>
#include <tusb.h>
#include <ff.h>
#include <diskio_sdmmc.h>
#include <sdmmc_cmd.h>
#include <esp_heap_caps.h>

extern bool watcherCloseStorage();
extern const char* storagePrepareCard();
extern int p4KnobUsbPower();

namespace {
// SD_MMC keeps its card handle protected; this reads it without a copy.
struct CardAccess:fs::SDMMCFS{static sdmmc_card_t* of(fs::SDMMCFS& f){return f.*(&CardAccess::_card);}};
constexpr uint32_t BufferBytes=64*1024;
constexpr uint32_t RandomSectors=8;   // 4 KB for a read that does not follow the last one
enum class Mode:uint8_t{Off,On,Stopping};
enum class Held:uint8_t{None,Read,Write};
// Shared with the USB task. present: the PC may use the card. attention: the
// next TEST UNIT READY reports the new medium once (UNIT ATTENTION).
volatile Mode mode=Mode::Off;
volatile bool present=false,attention=false,hostSeen=false,ejected=false,pcLocked=false,lost=false,pending=false;
volatile uint32_t readAt=0,writeAt=0,reads=0,writes=0;
uint8_t mscIn=0;
// Card and buffer: the USB task uses them only with sdMutex held.
sdmmc_card_t* card=nullptr;FATFS* volume=nullptr;char drive[6]="0:";
uint32_t sectors=0,sectorBytes=512;
uint8_t* buffer=nullptr;
Held held=Held::None;uint32_t heldLba=0,heldBytes=0,nextLba=UINT32_MAX;uint8_t failures=0;
// Main loop only.
bool wasLogging=false,lostShown=false;uint32_t stopAt=0,awayAt=0,powerAt=0,confirmUntil=0;int vbus=-1;
const char* stopReason="";char why[112]="";

void setWhy(const char* text){strlcpy(why,text?text:"",sizeof(why));}
void sense(uint8_t key,uint8_t code,uint8_t qualifier=0){tud_msc_set_sense(0,key,code,qualifier);}
void notPresent(){sense(SCSI_SENSE_NOT_READY,0x3A);}
bool lock(){return xSemaphoreTake(sdMutex,pdMS_TO_TICKS(1000))==pdTRUE;}
void unlock(){xSemaphoreGive(sdMutex);}
// sdMutex held. Three card errors in a row: the card is gone for the PC.
bool failed(esp_err_t e){if(e==ESP_OK){failures=0;return false;}if(++failures>=3){lost=true;present=false;}return true;}
// sdMutex held. Writes what a WRITE(10) left in the buffer.
bool flush(){
  if(held!=Held::Write){if(held==Held::None)pending=false;return true;}
  const uint32_t lba=heldLba,count=heldBytes/sectorBytes;held=Held::None;heldBytes=0;pending=false;nextLba=UINT32_MAX;
  if(!count)return true;
  if(failed(sdmmc_write_sectors(card,buffer,lba,count)))return false;
  writes+=count;return true;
}
// sdMutex held. Null when the card answers, after initialising it again on
// its own slot if it was taken out and put back. Never formats.
const char* wakeCard(){
  if(card->csd.capacity&&sdmmc_get_status(card)==ESP_OK)return nullptr;
  sdmmc_host_t host=card->host;void* old=host.dma_aligned_buffer;
  if(sdmmc_card_init(&host,card)!=ESP_OK)return "No TF card found. Insert a FAT32 card and try again.";
  if(old&&card->host.dma_aligned_buffer!=old)free(old);
  return nullptr;
}
bool range(uint32_t lba,uint32_t offset,uint32_t bytes){
  if(offset||!bytes||bytes%sectorBytes){sense(SCSI_SENSE_ILLEGAL_REQUEST,0x24);return false;}
  if(lba>=sectors||bytes/sectorBytes>sectors-lba){sense(SCSI_SENSE_ILLEGAL_REQUEST,0x21);return false;}
  return true;
}

uint16_t descriptor(uint8_t* dst,uint8_t* itf){
  const uint8_t name=tinyusb_add_string_descriptor("Dragon Detector Card");
  const uint8_t ep=tinyusb_get_free_duplex_endpoint();if(!ep)return 0;
  const uint8_t d[TUD_MSC_DESC_LEN]={TUD_MSC_DESCRIPTOR(*itf,name,ep,uint8_t(0x80|ep),CFG_TUD_ENDPOINT_SIZE)};
  mscIn=uint8_t(0x80|ep);*itf+=1;memcpy(dst,d,sizeof(d));return sizeof(d);
}
// Before USB.begin() (app_main), like USBCDC's own registration.
struct Register{Register(){tinyusb_enable_interface(USB_INTERFACE_CUSTOM,TUD_MSC_DESC_LEN,descriptor);}} registerAtBoot;

// Main loop: the card is Dragon's again. Waits at most `wait` ms for a USB
// transfer still holding the card; false (try again) when it is still busy.
bool finish(uint32_t wait){
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(wait))!=pdTRUE)return false;
  present=false;const bool flushed=flush();held=Held::None;
  // A failed final flush means the PC's last write never reached the card: say
  // so, never report a clean "off", so the loss is not hidden.
  const char* error=flushed?nullptr:"Card write failed while finishing; the last copy may be incomplete.";
  if(!error)error=wakeCard();
  if(!error){const FRESULT r=f_mount(volume,drive,1);
    error=r==FR_OK?storagePrepareCard():r==FR_NO_FILESYSTEM?"No FAT32 file system. Format the card as FAT32 on the PC.":"The card could not be mounted. Check it on a PC.";}
  mode=Mode::Off;unlock();
  if(!error){loggingEnabled=wasLogging;sdResourcesBegin();}
  setWhy(error?error:stopReason);
  Serial.printf("USB_DRIVE_OFF mounted=%u reads=%lu writes=%lu why=\"%s\"\n",!error,(unsigned long)reads,(unsigned long)writes,why);
  uiNotify(error?"Card not mounted":"USB drive off",error?error:"Dragon is using the card again.",nullptr,error!=nullptr);
  return true;
}
const char* stateName(UsbDriveState s){return s==UsbDriveState::Waiting?"waiting":s==UsbDriveState::Mounted?"mounted":s==UsbDriveState::Busy?"busy":"off";}
void reply(){
  const sdmmc_card_t* c=CardAccess::of(SD_MMC);
  Serial.printf("USB_DRIVE state=%s reads=%lu writes=%lu sectors=%lu why=\"%s\"\n",stateName(usbDriveState()),(unsigned long)reads,(unsigned long)writes,(unsigned long)(mode!=Mode::Off?sectors:c?c->csd.capacity:0),why);
}
}

// ---- TinyUSB mass storage callbacks (USB task) ----
extern "C" {
uint8_t tud_msc_get_maxlun_cb(void){return 1;}
void tud_msc_inquiry_cb(uint8_t,uint8_t vendor[8],uint8_t product[16],uint8_t revision[4]){
  auto put=[](uint8_t* to,size_t n,const char* text){memset(to,' ',n);memcpy(to,text,std::min(n,strlen(text)));};
  put(vendor,8,"Dragon");put(product,16,"Detector Card");put(revision,4,DRAGON_VERSION);
}
bool tud_msc_test_unit_ready_cb(uint8_t){
  if(!present){notPresent();return false;}
  if(attention){attention=false;sense(SCSI_SENSE_UNIT_ATTENTION,0x28);return false;}
  hostSeen=true;return true;
}
void tud_msc_capacity_cb(uint8_t,uint32_t* count,uint16_t* size){*count=present?sectors:0;*size=present?uint16_t(sectorBytes):0;}
bool tud_msc_is_writable_cb(uint8_t){return true;}
bool tud_msc_prevent_allow_medium_removal_cb(uint8_t,uint8_t prohibit,uint8_t){pcLocked=prohibit!=0;return true;}
bool tud_msc_start_stop_cb(uint8_t,uint8_t,bool start,bool eject){
  if(!eject)return true;                                   // power state only
  if(start){if(present)return true;notPresent();return false;}
  if(present){if(lock()){flush();unlock();}present=false;ejected=true;}   // the PC ejected the card
  return true;
}
int32_t tud_msc_read10_cb(uint8_t,uint32_t lba,uint32_t offset,void* out,uint32_t bytes){
  if(!present){notPresent();return -1;}
  if(!range(lba,offset,bytes)||!lock())return -1;
  int32_t result=-1;const uint32_t count=bytes/sectorBytes;
  if(present&&flush()){
    if(held!=Held::Read||lba<heldLba||lba+count>heldLba+heldBytes/sectorBytes){
      const uint32_t window=std::min(std::max(lba==nextLba?BufferBytes/sectorBytes:RandomSectors,count),sectors-lba);
      held=Held::None;
      if(!failed(sdmmc_read_sectors(card,buffer,lba,window))){held=Held::Read;heldLba=lba;heldBytes=window*sectorBytes;}
    }
    if(held==Held::Read&&lba>=heldLba&&lba+count<=heldLba+heldBytes/sectorBytes){
      memcpy(out,buffer+(lba-heldLba)*sectorBytes,bytes);nextLba=lba+count;reads+=count;readAt=millis();hostSeen=true;result=int32_t(bytes);
    }
  }
  unlock();return result;
}
int32_t tud_msc_write10_cb(uint8_t,uint32_t lba,uint32_t offset,uint8_t* in,uint32_t bytes){
  if(!present){notPresent();return -1;}
  if(!range(lba,offset,bytes)||!lock())return -1;
  int32_t result=-1;
  if(present){
    if(held==Held::Read)held=Held::None;
    const bool joins=held==Held::Write&&lba==heldLba+heldBytes/sectorBytes&&heldBytes+bytes<=BufferBytes;
    if(joins||flush()){
      if(held!=Held::Write){held=Held::Write;heldLba=lba;heldBytes=0;}
      memcpy(buffer+heldBytes,in,bytes);heldBytes+=bytes;pending=true;writeAt=millis();hostSeen=true;result=int32_t(bytes);
    }
  }
  unlock();return result;
}
int32_t tud_msc_scsi_cb(uint8_t,uint8_t const command[16],void* out,uint16_t bytes){
  switch(command[0]){
    case 0x35:case 0x91:{   // SYNCHRONIZE CACHE (10/16): nothing waits unwritten, but make sure
      if(!present){notPresent();return -1;}
      bool ok=lock();if(ok){ok=flush();unlock();}
      if(!ok){sense(SCSI_SENSE_MEDIUM_ERROR,0x0C);return -1;}
      return 0;}
    case 0x2F:              // VERIFY (10): the card checks its own data
      if(!present){notPresent();return -1;}
      return 0;
    case 0x5A:{             // MODE SENSE (10): header only, not write protected
      const uint8_t header[8]={0,6,0,0,0,0,0,0};const uint16_t n=std::min<uint16_t>(bytes,sizeof(header));
      memcpy(out,header,n);return n;}
    default:sense(SCSI_SENSE_ILLEGAL_REQUEST,0x20);return -1;
  }
}
// Every TinyUSB transfer passes here. Only the status of a command on this
// interface that left write data in the buffer waits for the card.
bool __real_usbd_edpt_xfer(uint8_t,uint8_t,uint8_t*,uint16_t,bool);
bool __wrap_usbd_edpt_xfer(uint8_t port,uint8_t ep,uint8_t* data,uint16_t bytes,bool isr){
  if(bytes==13&&ep==mscIn&&mscIn&&pending&&!isr&&data&&!memcmp(data,"USBS",4)){
    bool ok=lock();if(ok){ok=flush();unlock();}
    if(!ok&&!data[12]){data[12]=1;sense(SCSI_SENSE_MEDIUM_ERROR,0x0C);}   // bCSWStatus: failed, WRITE ERROR
  }
  return __real_usbd_edpt_xfer(port,ep,data,bytes,isr);
}
}

// ---- Main loop ----
bool usbDriveOn(){return mode!=Mode::Off;}
UsbDriveState usbDriveState(){
  if(mode==Mode::Off)return UsbDriveState::Off;
  if(mode==Mode::Stopping)return UsbDriveState::Busy;
  const uint32_t now=millis();
  if(present&&((readAt&&now-readAt<600)||(writeAt&&now-writeAt<600)))return UsbDriveState::Busy;
  if(present&&hostSeen&&tud_mounted()&&!tud_suspended())return UsbDriveState::Mounted;
  return UsbDriveState::Waiting;
}
const char* usbDriveStateText(){
  if(mode==Mode::Stopping)return "Finishing...";
  if(lost)return "Card error";
  switch(usbDriveState()){
    case UsbDriveState::Busy:return int32_t(writeAt-readAt)>=0?"Writing...":"Reading...";
    case UsbDriveState::Mounted:return "Connected";
    case UsbDriveState::Waiting:return "Waiting for PC";
    default:return "Off";
  }
}
void usbDriveDetail(char* out,size_t length){
  const double mb=double(sectorBytes)/1048576.0;
  snprintf(out,length,"Read %.1f MB   Written %.1f MB\n%.1f GB card",reads*mb,writes*mb,sectors*mb/1024.0);
}
bool usbDriveStart(char* out,size_t length){
  auto no=[&](const char* text){if(out&&length)strlcpy(out,text,length);setWhy(text);return false;};
  if(mode!=Mode::Off){if(out&&length)strlcpy(out,"The PC already has the card.",length);return true;}
  if(!mscIn)return no("USB drive is not available on this USB port.");
  card=CardAccess::of(SD_MMC);
  if(!card)return no("No TF card found. Insert a FAT32 card and restart.");
  if(nodeBusy())return no("Stop the node first.");
  if(wardriveBusy()||wardriveUploadBusy())return no("Finish the wardrive first.");
  // Everything that writes the card stops here; what is still saving is waited for.
  bleLabStop();recipeStop();missionStop();reconStop();setCapture(false);audioStop();
#ifdef BOARD_WAVESHARE_P4_KNOB
  if(missionActive())return no("A session is waiting to save. Reinsert its card and retry.");
#endif
  if(reconStorageBusy()||reconRecording())return no("Saving a capture. Try again in a moment.");
  if(bleLabOwnsRadio())return no("BLE Lab is closing. Try again.");
  if(audioStatus().mode==AudioMode::Recording)return no("Stop the recording first.");
#if BOARD_HAS_MIC
  // The Voice Recorder holds the card while it records or plays; close it like
  // the sleep and power-off paths do before the PC takes the card.
  if(voiceBusy()&&!voiceShutdown(1000))return no("Stop the Voice Recorder first.");
#endif
  if(!buffer)buffer=static_cast<uint8_t*>(heap_caps_aligned_alloc(128,BufferBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));   // 128: the PSRAM cache line, for DMA
  if(!buffer)buffer=static_cast<uint8_t*>(heap_caps_aligned_alloc(128,BufferBytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));
  if(!buffer)return no("Not enough memory for USB drive mode.");
  const bool logging=loggingEnabled;
  if(!watcherCloseStorage())return no("The card is busy. Try again.");   // flushes and closes the event log and capture
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(1000))!=pdTRUE){loggingEnabled=logging;return no("The card is busy. Try again.");}
  const bool ready=sdReady;sdReady=false;
  const char* error=ready?nullptr:wakeCard();
  const BYTE pdrv=ff_diskio_get_pdrv_card(card);
  if(!error&&pdrv==0xFF)error="The card cannot be read. Restart Dragon.";
  if(!error){snprintf(drive,sizeof(drive),"%u:",unsigned(pdrv));
    if(!volume){FF_DIR d;char root[8];snprintf(root,sizeof(root),"%s/",drive);if(f_opendir(&d,root)==FR_OK){volume=d.obj.fs;f_closedir(&d);}}
    if(!volume)error="The card cannot be read. Check it on a PC.";}
  if(error){sdReady=ready;loggingEnabled=logging;unlock();return no(error);}
  f_mount(nullptr,drive,0);   // FAT is Dragon's no more; open files were closed above
  sectors=card->csd.capacity;sectorBytes=card->csd.sector_size?card->csd.sector_size:512;
  held=Held::None;heldBytes=0;pending=false;nextLba=UINT32_MAX;failures=0;
  reads=0;writes=0;readAt=0;writeAt=0;hostSeen=false;ejected=false;pcLocked=false;lost=false;lostShown=false;
  wasLogging=logging;stopReason="";awayAt=powerAt=confirmUntil=0;vbus=-1;
  attention=true;mode=Mode::On;present=true;
  unlock();
  setWhy("Your PC sees the card as a drive.");if(out&&length)*out=0;
  Serial.printf("USB_DRIVE_ON sectors=%lu\n",(unsigned long)sectors);
  return true;
}
void usbDriveStop(const char* reason){
  if(mode!=Mode::On)return;
  present=false;stopReason=reason?reason:"";mode=Mode::Stopping;stopAt=millis();
}
bool usbDriveConfirming(){return mode==Mode::On&&confirmUntil&&int32_t(confirmUntil-millis())>0;}
bool usbDriveDone(){
  if(mode!=Mode::On)return true;
  // The PC still has it open (it locked the medium) or wrote a moment ago: ask once more.
  const bool inUse=present&&(pcLocked||(writeAt&&millis()-writeAt<2000));
  if(inUse&&!usbDriveConfirming()){confirmUntil=millis()+4000;if(!confirmUntil)confirmUntil=1;return false;}
  usbDriveStop("Done on Dragon.");return true;
}
void usbDriveLoop(){
  if(mode==Mode::Off)return;
  const uint32_t now=millis();
  if(mode==Mode::On){
    if(ejected)usbDriveStop("Ejected on the PC.");
    else if(lost){if(!lostShown){lostShown=true;setWhy("The card stopped answering. Tap Done.");Serial.println("USB_DRIVE_CARD_ERROR");}}
    else if(hostSeen){
      // Unplugged: no bus activity with no power on USB-C for 1 s. Without that
      // power reading, or with power but no bus (PC asleep), after 30 s.
      if(!powerAt||now-powerAt>=250){powerAt=now|1;vbus=p4KnobUsbPower();}
      const bool away=!tud_mounted()||tud_suspended();
      if(!away)awayAt=0;else if(!awayAt)awayAt=now|1;
      else if(now-awayAt>=(vbus==0?1000u:30000u))usbDriveStop(vbus==0?"USB unplugged.":"The PC stopped using the USB port.");
    }
  }
  // A moment for the PC to see the card go and the screen to say so, then mount it.
  if(mode==Mode::Stopping&&now-stopAt>=200)finish(50);
}
bool usbDriveCommand(const String& c){
  if(c=="usb drive status"){reply();return true;}
  if(c=="usb drive on"){char text[112];usbDriveStart(text,sizeof(text));reply();return true;}
  if(c=="usb drive off"){
    if(mode==Mode::On)usbDriveStop("Turned off over serial.");
    for(uint32_t at=millis();mode==Mode::Stopping&&millis()-at<3000;)finish(500);
    reply();return true;
  }
  if(mode!=Mode::Off&&(c.startsWith("ls ")||c.startsWith("put ")||c.startsWith("get ")||c=="node biscuit"||c=="node jcmk")){
    Serial.println("USB_DRIVE_BUSY the PC has the card; send usb drive off first");return true;
  }
  return false;
}
#endif
