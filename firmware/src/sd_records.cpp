#include "sd_records.h"
#ifndef BOARD_RADAR_LITE
void SdRecords::fail(const char* reason){
  accepting_=closing_=false;failed_=true;
  dropped+=queuedRecords_;queuedRecords_=0;
  queue_.reset(memory_,QueueBytes);
  if(file_){file_.flush();file_.close();}
  // Retain .part for recovery; never pretend a partial record is a valid file.
  snprintf(message,sizeof(message),"%s; .part retained",reason);
  ++storageDrops;
}
bool SdRecords::openSegment(){
  if(!sdReady||!bleLabStorageSpace(SegmentBytes)){fail("SD full or unavailable");return false;}
  snprintf(path_,sizeof(path_),"%s/%s-%08lx-%04lu.%s",folder_,prefix_,
           (unsigned long)session_,(unsigned long)segment_++,extension_);
  if(fs_->exists(path_)||fs_->exists(String(path_)+".part")){fail("SD filename collision");return false;}
  file_=fs_->open(String(path_)+".part",FILE_WRITE);
  if(!file_||file_.write(header_,headerSize_)!=headerSize_){fail("SD header write failed");return false;}
  segmentBytes_=headerSize_;bytes+=headerSize_;file_.flush();flushed_=checked_=millis();
  snprintf(message,sizeof(message),"Recording /dragon%s",folder_);return true;
}
bool SdRecords::finishSegment(){
  if(!file_)return false;
  file_.flush();file_.close();
  if(!fs_->rename(String(path_)+".part",path_)){fail("SD finalize failed");return false;}
  return true;
}
bool SdRecords::begin(const char* folder,const char* prefix,const char* extension,const void* header,size_t length){
  if(busy())return false;
  failed_=false;bytes=records=0;dropped=queuedRecords_=0;
  if(!bleLabStorageOnCard()||!sdReady||!header||length>sizeof(header_)){
    strlcpy(message,"Mounted SD required",sizeof(message));return false;
  }
  fs_=bleLabStorage();if(!fs_){strlcpy(message,"Storage busy",sizeof(message));return false;}
  if(!memory_)memory_=(uint8_t*)ps_malloc(QueueBytes);
  if(!memory_){strlcpy(message,"SD buffer allocation failed",sizeof(message));return false;}
  queue_.reset(memory_,QueueBytes);
  if(!fs_->exists(folder)&&!fs_->mkdir(folder)){strlcpy(message,"Cannot create SD folder",sizeof(message));return false;}
  strlcpy(folder_,folder,sizeof(folder_));strlcpy(prefix_,prefix,sizeof(prefix_));
  strlcpy(extension_,extension,sizeof(extension_));memcpy(header_,header,length);headerSize_=length;
  session_=esp_random();segment_=0;
  accepting_=openSegment();return accepting_;
}
bool SdRecords::append(const void* data,size_t length){
  if(!accepting_)return false;
  if(!queue_.push((const uint8_t*)data,length)){++dropped;++storageDrops;return false;}
  ++queuedRecords_;
  return true;
}
void SdRecords::loop(){
  if(!busy())return;
  if(!sdReady){fail("SD unavailable");return;}
  // Main task shares the card with the existing audio/logging adapter.
  if(!sdMutex||xSemaphoreTake(sdMutex,0)!=pdTRUE)return;
  uint8_t data[dragon::wardrive::Records::MaxRecord];uint32_t at=micros();
  for(unsigned i=0;i<4;++i){
    size_t n=queue_.front(data,sizeof(data));if(!n)break;
    if(segmentBytes_+n>SegmentBytes&&!finishSegment())break;
    if(!file_&&!openSegment())break;
    if(file_.write(data,n)!=n){fail("SD record write failed");break;}
    queue_.pop(n);--queuedRecords_;segmentBytes_+=n;bytes+=n;++records;
    if(micros()-at>3000)break;
  }
  if(file_&&millis()-flushed_>=1000){file_.flush();flushed_=millis();}
  if(file_&&millis()-checked_>=10000){
    checked_=millis();if(!bleLabStorageSpace(0))fail("SD reserve reached");
  }
  if(closing_&&!queue_.size()){
    if(finishSegment())snprintf(message,sizeof(message),"Saved /dragon%s",folder_);
    closing_=false;
  }
  xSemaphoreGive(sdMutex);
}
#endif
