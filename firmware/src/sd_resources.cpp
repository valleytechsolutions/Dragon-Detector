#include "sd_resources.h"
#if BOARD_HAS_SD
#include "app.h"
#include "resource_index.h"
#include <esp_heap_caps.h>
namespace {
dragon::ResourceIndex resourceIndex;
uint8_t* memory=nullptr;
uint32_t loadUs=0,lookupMaxUs=0,lookups=0;size_t loadedBytes=0;
const char* state="missing card";
}
void sdResourcesBegin(){
 // Startup only, before radioBegin. No card I/O, locks, allocation or eviction
 // during a lookup. Pointers remain valid even if the SD card is removed.
 if(memory||!sdReady||!sdMutex)return;
 uint32_t started=micros();
 if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(100))!=pdTRUE){state="storage busy";return;}
 File f=CardFS.open("/dragon/resources/vendors.ddr",FILE_READ);
 state="missing optional pack";
 if(f){
  size_t n=f.size();state="invalid size";
  if(n>=dragon::ResourceIndex::Header&&n<=dragon::ResourceIndex::MaxBytes){
   state="insufficient PSRAM";
   if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)>=n+2*1024*1024)memory=(uint8_t*)ps_malloc(n);
   if(memory){
    size_t at=0;state="short read";
    while(at<n){size_t chunk=std::min<size_t>(4096,n-at);if(f.read(memory+at,chunk)!=chunk)break;at+=chunk;delay(1);}
    if(at==n){resourceIndex.load(memory,n);state=resourceIndex.error;}
    if(resourceIndex.count())loadedBytes=n;else{free(memory);memory=nullptr;}
   }
  }f.close();
 }xSemaphoreGive(sdMutex);loadUs=micros()-started;
 sdResourcesStatus();
}
const char* sdResourceVendor(const uint8_t* mac,bool ble,uint8_t type){
 uint32_t started=micros();const char* name=resourceIndex.lookup(mac,ble,type);
 ++lookups;lookupMaxUs=std::max(lookupMaxUs,uint32_t(micros()-started));return name;
}
void sdResourcesStatus(){Serial.printf("RESOURCES status=%s schema=1 snapshot=%lu entries=%lu bytes=%u load_us=%lu lookup_max_us=%lu lookups=%lu fallback=internal\n",state,(unsigned long)resourceIndex.snapshot,(unsigned long)resourceIndex.count(),unsigned(loadedBytes),(unsigned long)loadUs,(unsigned long)lookupMaxUs,(unsigned long)lookups);}
#endif

