#pragma once
#include <stdint.h>
namespace dragon {
// Public universal addresses only. A randomized BLE address or locally
// administered Wi-Fi address is not an IEEE manufacturer identity.
const char* registeredVendor(const uint8_t* mac,bool ble=false,uint8_t addressType=0);
unsigned vendorRegistryCount();
bool vendorRegistryBegin();
}
