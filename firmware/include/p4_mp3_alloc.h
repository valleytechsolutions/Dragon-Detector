#pragma once
#include <stdlib.h>
#include <esp_heap_caps.h>

// Helix state is CPU-only. Keep the P4's internal DMA heap for SDIO and I2S.
static inline void* p4Mp3Malloc(size_t bytes){
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
#define malloc p4Mp3Malloc
