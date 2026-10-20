#pragma once
#ifndef BOARD_RADAR_LITE
#include "app.h"
#include "recon.h"
#include "wardrive_format.h"

// Only called by the application task. Radio callbacks enqueue observations
// elsewhere; they never wait on FAT, allocate memory, or call this writer.
class SdRecords {
  static constexpr size_t QueueBytes=64*1024;
  static constexpr uint32_t SegmentBytes=32*1024*1024;
  dragon::wardrive::Records queue_;
  uint8_t* memory_=nullptr;
  fs::FS* fs_=nullptr;
  File file_;
  uint8_t header_[512]{};
  size_t headerSize_=0;
  char folder_[24]{},prefix_[16]{},extension_[6]{},path_[96]{};
  uint32_t session_=0,segment_=0,segmentBytes_=0,flushed_=0,checked_=0;
  uint32_t queuedRecords_=0;
  bool accepting_=false,closing_=false,failed_=false;
  bool openSegment();
  bool finishSegment();
  void fail(const char*);
public:
  uint64_t bytes=0,records=0;
  uint32_t dropped=0;
  char message[120]{};
  bool begin(const char* folder,const char* prefix,const char* extension,const void* header,size_t length);
  bool append(const void* data,size_t length);
  void stop(){accepting_=false;closing_=bool(file_);}
  void loop();
  bool active() const{return accepting_;}
  bool busy() const{return accepting_||closing_;}
  bool failed() const{return failed_;}
  size_t pending() const{return queue_.size();}
  const char* path() const{return path_;}
};
#endif
