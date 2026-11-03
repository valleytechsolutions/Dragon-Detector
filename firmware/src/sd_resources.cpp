#include "sd_resources.h"
#if BOARD_HAS_SD
#include "app.h"
#include "resource_index.h"
#ifdef BOARD_WAVESHARE_185B
#include "ble_label_index.h"
#endif
#include <esp_heap_caps.h>
namespace {
dragon::ResourceIndex resourceIndex;
uint8_t* memory=nullptr;
uint32_t loadUs=0,lookupMaxUs=0,lookups=0;size_t loadedBytes=0;
const char* state="missing card";
#ifdef BOARD_WAVESHARE_185B
dragon::BleLabelIndex bleLabels;
uint8_t* labelMemory=nullptr;
size_t labelBytes=0;const char* labelState="missing card";
#endif
}
void sdResourcesBegin(){
 // Startup only, before radioBegin. No card I/O, locks, allocation or eviction
 // during a lookup. Pointers remain valid even if the SD card is removed.
 if(memory||!sdReady||!sdMutex
#ifdef BOARD_WAVESHARE_185B
    ||labelMemory
#endif
 )return;
 uint32_t started=micros();
 if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(100))!=pdTRUE){state="storage busy";return;}
 File f=CardFS.open("/dragon/resources/vendors.ddr",FILE_READ);
#ifdef BOARD_WAVESHARE_185B
 state="missing SD vendor labels";
#else
 state="missing optional pack";
#endif
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
 }
#ifdef BOARD_WAVESHARE_185B
 // Loaded once before radio startup, just like the IEEE registry. Card writes
 // or removal never invalidate pointers already returned to UI/notebook code.
 File labels=CardFS.open("/dragon/resources/ble-labels.ddb",FILE_READ);
 labelState="missing SD BLE labels";
 if(labels){
  size_t n=labels.size();labelState="invalid size";
  if(n>=dragon::BleLabelIndex::Header&&n<=dragon::BleLabelIndex::MaxBytes){
   labelState="insufficient PSRAM";
   if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)>=n+2*1024*1024)labelMemory=(uint8_t*)ps_malloc(n);
   if(labelMemory){size_t at=0;labelState="short read";
    while(at<n){size_t chunk=std::min<size_t>(4096,n-at);if(labels.read(labelMemory+at,chunk)!=chunk)break;at+=chunk;delay(1);}
    if(at==n){bleLabels.load(labelMemory,n);labelState=bleLabels.error;}
    if(bleLabels.count())labelBytes=n;else{free(labelMemory);labelMemory=nullptr;}
   }
  }labels.close();
 }
#endif
 xSemaphoreGive(sdMutex);loadUs=micros()-started;
 sdResourcesStatus();
}
const char* sdResourceVendor(const uint8_t* mac,bool ble,uint8_t type){
 uint32_t started=micros();const char* name=resourceIndex.lookup(mac,ble,type);
 ++lookups;lookupMaxUs=std::max(lookupMaxUs,uint32_t(micros()-started));return name;
}
bool sdResourcesReady(){return resourceIndex.count()!=0;}
uint32_t sdResourceVendorCount(){return resourceIndex.count();}
#ifdef BOARD_WAVESHARE_185B
const char* sdResourceBleLabel(uint8_t kind,uint16_t value){return bleLabels.lookup(kind,value);}
uint32_t sdResourceBleLabelCount(){return bleLabels.count();}
bool sdLookupNamesReady(){return resourceIndex.count()&&bleLabels.count();}
const char* sdResourceVendorState(){return state;}
const char* sdResourceBleLabelState(){return labelState;}
#endif
void sdResourcesStatus(){Serial.printf("RESOURCES status=%s schema=1 snapshot=%lu entries=%lu bytes=%u load_us=%lu lookup_max_us=%lu lookups=%lu fallback=%s\n",state,(unsigned long)resourceIndex.snapshot,(unsigned long)resourceIndex.count(),unsigned(loadedBytes),(unsigned long)loadUs,(unsigned long)lookupMaxUs,(unsigned long)lookups,
#ifdef BOARD_WAVESHARE_185B
 "unresolved");
 Serial.printf("BLE_LABELS status=%s schema=1 snapshot=%lu entries=%lu bytes=%u source=SD fallback=raw_identifiers\n",labelState,(unsigned long)bleLabels.snapshot,(unsigned long)bleLabels.count(),unsigned(labelBytes));
#else
 "internal");
#endif
}
#endif

