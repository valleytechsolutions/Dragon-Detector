#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdint.h>
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_MEM_CUSTOM 1
#ifdef BOARD_AMOLED_175C
// Long recon lists must not consume the internal heap needed by both radios.
// The board driver allocates its DMA display tile separately.
#define LV_MEM_CUSTOM_INCLUDE "lv_memory.h"
#define LV_MEM_CUSTOM_ALLOC dragon_lv_alloc
#define LV_MEM_CUSTOM_FREE heap_caps_free
#define LV_MEM_CUSTOM_REALLOC dragon_lv_realloc
#else
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#endif
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#if defined(BOARD_XIAO_ROUND) || defined(BOARD_RADAR_LITE)
#define LV_FONT_MONTSERRAT_8 1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_DEFAULT &lv_font_montserrat_10
#elif defined(BOARD_AMOLED_175C) || defined(BOARD_STOPWATCH)
// 466 px panel: each 360 px layout font moves up to keep the same proportions.
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_DEFAULT &lv_font_montserrat_18
#else
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#endif
#define LV_USE_LOG 0
#define LV_USE_SNAPSHOT 1
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_MUSIC 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0
#endif
