#pragma once
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "flash_budget.h"
#include <LittleFS.h>
#include <vfs_api.h>

// A view of an existing mounted filesystem, not a second mount or formatter.
// Keep all SD research files below /dragon; internal paths stay compatible.
class ReconFS : public fs::FS {
  fs::LittleFSFS extra_;
  fs::LittleFSFS* internal_ = &LittleFS;
  bool card_ = false;
public:
  ReconFS() : fs::FS(std::make_shared<VFSImpl>()) {}
  bool begin(bool format, const char* base, uint8_t files, const char* label) {
    if (!extra_.begin(format, base, files, label)) return false;
    internal_ = &extra_;
    card_ = false;
    _impl->mountpoint(extra_.mountpoint());
    return true;
  }
  bool select() {
#if defined(BOARD_AMOLED_175)
    if (sdReady) {
      if (!CardFS.exists("/dragon") && !CardFS.mkdir("/dragon")) return false;
      card_ = true;
      _impl->mountpoint("/sdcard/dragon");
      return true;
    }
#endif
    card_ = false;
    internal_ = &LittleFS;
    if (!internal_->totalBytes()) return false;
    _impl->mountpoint(internal_->mountpoint());
    return true;
  }
  bool onCard() const { return card_; }
  const char* medium() const { return card_ ? "SD /dragon" : "internal flash"; }
  uint64_t totalBytes() {
#if defined(BOARD_AMOLED_175)
    if (card_) return sdReady ? CardFS.totalBytes() : 0;
#endif
    return internal_->totalBytes();
  }
  uint64_t usedBytes() {
#if defined(BOARD_AMOLED_175)
    if (card_) return sdReady ? CardFS.usedBytes() : 0;
#endif
    return internal_->usedBytes();
  }
  bool fits(size_t additional) {
    const uint64_t total = totalBytes(), used = usedBytes();
    const uint64_t reserve = card_ ? 32ULL * 1024 * 1024 : dragon::flashReserve(size_t(total));
    return total && used <= total && additional <= total - used &&
           total - used - additional >= reserve;
  }
};
#endif
