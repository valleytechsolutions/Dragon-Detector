#ifdef BOARD_WAVESHARE_185B
#include "app.h"
#include "recon.h"
#include "ble_lab.h"
#include "wardrive.h"
#include "wardrive_upload.h"
#include <Preferences.h>
#include <vfs_api.h>
#include <esp_vfs_fat.h>
#include <ff.h>
#include <diskio_sdmmc.h>
#include <esp_heap_caps.h>
#include <inttypes.h>

DragonSdMmc::DragonSdMmc() : SDMMCFS(fs::FSImplPtr(new VFSImpl())) {}
DragonSdMmc dragonSdMmc;

uint64_t DragonSdMmc::identity() const {
  if (!_card) return 0;
  // Stable decoded CID fields, excluding any struct padding.
  uint64_t hash=14695981039346656037ULL;
  auto byte=[&](uint8_t v){hash=(hash^v)*1099511628211ULL;};
  const uint32_t fields[]={uint32_t(_card->cid.mfg_id),uint32_t(_card->cid.oem_id),
    uint32_t(_card->cid.serial),uint32_t(_card->cid.revision),uint32_t(_card->cid.date)};
  for(auto field:fields)for(unsigned i=0;i<4;++i)byte(uint8_t(field>>(i*8)));
  for(unsigned i=0;i<5;++i)byte(uint8_t(_card->cid.name[i]));
  return hash;
}
unsigned DragonSdMmc::fatType() const {
  if (!_card || _pdrv==0xff) return 0;
  char volume[8];snprintf(volume,sizeof(volume),"%u:",_pdrv);
  DWORD freeClusters=0;FATFS* fs=nullptr;
  if(f_getfree(volume,&freeClusters,&fs)!=FR_OK || !fs)return 0;
  return fs->fs_type==FS_FAT32?32:fs->fs_type==FS_FAT16?16:fs->fs_type==FS_FAT12?12:0;
}
unsigned DragonSdMmc::clusterBytes() const {
  if(!_card || _pdrv>=FF_VOLUMES)return 0;
  char volume[8];snprintf(volume,sizeof(volume),"%u:",_pdrv);
  DWORD freeClusters=0;FATFS* fs=nullptr;
  if(f_getfree(volume,&freeClusters,&fs)!=FR_OK || !fs)return 0;
#if FF_MAX_SS != FF_MIN_SS
  return unsigned(fs->csize)*fs->ssize;
#else
  return unsigned(fs->csize)*FF_MIN_SS;
#endif
}
int DragonSdMmc::formatMountedCard(){
  if(!_card || _pdrv>=FF_VOLUMES || ff_diskio_get_pdrv_card(_card)!=_pdrv)return ESP_ERR_INVALID_STATE;
#if FF_MULTI_PARTITION
  // IDF 5.1 maps logical drive N to physical drive N, partition auto-detect.
  // Refuse any different mapping rather than formatting an unintended drive.
  if(VolToPart[_pdrv].pd!=_pdrv || VolToPart[_pdrv].pt!=0)return ESP_ERR_INVALID_STATE;
#endif
  constexpr unsigned workBytes=4096;
  void* work=heap_caps_malloc(workBytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT);
  if(!work)return ESP_ERR_NO_MEM; // Leave the mounted filesystem intact on OOM.
  char volume[8];snprintf(volume,sizeof(volume),"%u:",_pdrv);
  FRESULT result=f_mount(nullptr,volume,0);
  if(result==FR_OK){
    // Explicit FAT32, two FATs and 32 KiB clusters suit sequential SD logging.
    // Without FM_SFD, FatFs creates a partitioned volume on the matched card.
    const MKFS_PARM options={FM_FAT32,2,0,0,32768};
    result=f_mkfs(volume,&options,work,workBytes);
  }
  heap_caps_free(work);
  // The filesystem stays unmounted. The caller MUST reboot without card/FS
  // access (including end()); only a fresh mount can validate the new volume.
  return result==FR_OK?ESP_OK:ESP_FAIL;
}

namespace {
constexpr uint32_t FormatMagic=0x185bf001;
struct FormatRequest {uint64_t identity=0,bytes=0;uint32_t magic=FormatMagic,reserved=0;};
struct FormatVerification {FormatRequest request{};uint32_t stage=1;int32_t result=-1;};
const char* formatResult="not_requested";
}

bool sdMaintenanceBoot(bool mounted){
  Preferences prefs;
  if(!prefs.begin("dd185-sd",false)){Serial.println("SD_FORMAT_ERROR request storage unavailable");return false;}
  if(prefs.isKey("verify")){
    FormatVerification verification{};
    const bool decoded=prefs.getBytesLength("verify")==sizeof(verification)&&
      prefs.getBytes("verify",&verification,sizeof(verification))==sizeof(verification);
    const bool consumed=prefs.remove("verify");
    if(!consumed){prefs.end();formatResult="verification_clear_failed";return false;}
    const auto& expected=verification.request;
    const bool matched=decoded&&expected.magic==FormatMagic&&expected.reserved==0&&expected.identity&&mounted&&
      expected.identity==dragonSdMmc.identity()&&expected.bytes==dragonSdMmc.cardSize();
    formatResult=!matched?"verification_card_missing_or_changed":
      verification.stage!=2?"interrupted_result_unknown":
      verification.result!=ESP_OK?"format_failed":
      dragonSdMmc.fatType()!=32?"fresh_mount_not_fat32":
      dragonSdMmc.clusterBytes()!=32768?"fresh_mount_cluster_mismatch":"complete";
    Serial.printf("SD_FORMAT_VERIFY %s\n",formatResult);
  }
  if(!prefs.isKey("format")){prefs.end();return mounted;}
  FormatRequest request{};
  const size_t stored=prefs.getBytesLength("format");
  const bool decoded=stored==sizeof(request)&&prefs.getBytes("format",&request,sizeof(request))==sizeof(request);
  // NVS commits remove() before any filesystem change. Never repeat a format
  // after a reset, missing/swapped card, failed mount or interrupted format.
  const bool consumed=prefs.remove("format");
  prefs.end();
  if(!consumed){formatResult="request_clear_failed";return false;}
  if(!decoded||request.magic!=FormatMagic||request.reserved!=0){formatResult="invalid_request";return mounted;}
  if(!mounted){formatResult="card_not_mounted";return false;}
  if(!request.identity||request.identity!=dragonSdMmc.identity()||request.bytes!=dragonSdMmc.cardSize()){
    formatResult="card_changed";Serial.println("SD_FORMAT_REFUSED card identity changed");return mounted;
  }
  FormatVerification verification{};verification.request=request;
  Preferences resultPrefs;
  if(!resultPrefs.begin("dd185-sd",false)){formatResult="verification_storage_failed";return false;}
  if(resultPrefs.putBytes("verify",&verification,sizeof(verification))!=sizeof(verification)){
    resultPrefs.end();formatResult="verification_storage_failed";return false;
  }
  Serial.printf("SD_FORMAT_BEGIN id=%016" PRIx64 " bytes=%" PRIu64 "\n",request.identity,request.bytes);
  Serial.flush();
  verification.result=dragonSdMmc.formatMountedCard();verification.stage=2;
  // No SD calls after the API, including end(), cardSize() or fatType().
  resultPrefs.putBytes("verify",&verification,sizeof(verification));resultPrefs.end();
  Serial.printf("SD_FORMAT_API result=%ld; restarting for fresh mount\n",long(verification.result));Serial.flush();
  delay(50);ESP.restart();return false;
}

bool sdMaintenanceCommand(const String& command){
  if(command=="sd status"){
    Serial.printf("SD_STATUS mounted=%u id=%016" PRIx64 " card_bytes=%" PRIu64 " total_bytes=%" PRIu64 " used_bytes=%" PRIu64 " fat=%u cluster_bytes=%u format=%s\n",
      sdReady,dragonSdMmc.identity(),dragonSdMmc.cardSize(),sdReady?dragonSdMmc.totalBytes():0,
      sdReady?dragonSdMmc.usedBytes():0,dragonSdMmc.fatType(),dragonSdMmc.clusterBytes(),formatResult);
    return true;
  }
  if(!command.startsWith("sd format"))return false;
  if(!sdReady){Serial.println("SD_FORMAT_ERROR no mounted card");return true;}
  if(stopwatch.running||reconOwnsRadio()||bleLabOwnsRadio()||reconStorageBusy()||wardriveBusy()||wardriveUploadBusy()||
     loggingEnabled||captureEnabled||audioStatus().mode==AudioMode::Playing||audioStatus().mode==AudioMode::Recording){
    Serial.println("SD_FORMAT_ERROR stop timing, recording, logging, audio and radio tools first");return true;
  }
  uint64_t id=0,bytes=0;char confirm[8]{},extra=0;
  const int fields=sscanf(command.c_str(),"sd format %16" SCNx64 " %20" SCNu64 " %7s %c",&id,&bytes,confirm,&extra);
  if(fields!=3||strcmp(confirm,"ERASE")||!id||id!=dragonSdMmc.identity()||bytes!=dragonSdMmc.cardSize()){
    Serial.println("SD_FORMAT_ERROR use sd format <id from sd status> <card_bytes> ERASE");return true;
  }
  // FAT32 maintenance is limited to cards whose mounted filesystem is FAT32;
  // ordinary mounts never format a missing, corrupt or unsupported card.
  if(dragonSdMmc.fatType()!=32){Serial.println("SD_FORMAT_ERROR FAT32 card required");return true;}
  Preferences prefs;
  if(!prefs.begin("dd185-sd",false)){Serial.println("SD_FORMAT_ERROR request storage unavailable");return true;}
  FormatRequest request{};request.identity=id;request.bytes=bytes;
  const bool saved=prefs.putBytes("format",&request,sizeof(request))==sizeof(request);
  prefs.end();
  if(!saved){Serial.println("SD_FORMAT_ERROR request not saved");return true;}
  Serial.println("SD_FORMAT_RESTARTING one-time format of matched card");Serial.flush();delay(50);ESP.restart();
  return true;
}
#endif
