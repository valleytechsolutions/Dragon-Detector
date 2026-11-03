#pragma once
#ifdef BOARD_WAVESHARE_185B
#include <SD_MMC.h>

// This board owns its dedicated SDMMC bus. Expose only the mounted card to the
// FatFs formatter; never reinterpret the Arduino singleton or use raw sectors.
class DragonSdMmc : public fs::SDMMCFS {
 public:
  DragonSdMmc();
  uint64_t identity() const;
  unsigned fatType() const;
  unsigned clusterBytes() const;
  int formatMountedCard();
};
extern DragonSdMmc dragonSdMmc;
// Called once at boot, before storage opens any files. Requests are consumed
// even when no card is mounted, so inserting another card never retries erase.
bool sdMaintenanceBoot(bool mounted);
bool sdMaintenanceCommand(const String& command);
#endif
