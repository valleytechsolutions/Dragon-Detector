#ifndef BOARD_RADAR_LITE
#include <stdint.h>
#include <esp_idf_version.h>
// Marauder-compatible raw frame adapter, MIT reference notice in
// third_party/licenses/MARAUDER.txt. Deliberately limited to the pinned SDK.
// The ordinary raw TX API still performs its argument/interface checks.
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5,1,4)
#error Revalidate the Waveshare raw frame adapter before changing ESP-IDF
#endif
extern "C" __attribute__((noinline,used)) int ieee80211_raw_frame_sanity_check(int32_t marker,int32_t,int32_t){return marker==31337?1:0;}
bool reconRawAdapter(){return ieee80211_raw_frame_sanity_check(31337,0,0)==1;}
#endif
