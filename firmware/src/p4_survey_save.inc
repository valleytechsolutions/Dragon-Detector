#include "p4_survey_csv.h"

bool p4SaveWifiSurvey(const char* requested,char* message,size_t size){
  auto fail=[&](const char* text){snprintf(message,size,"%s",text);return false;};
  const auto* sample=p4LatestWifiSurvey();
  if(!sample)return fail("Waiting for the first Wi-Fi survey.");
  const unsigned count=dragon::p4::surveyRowCount(*sample);
  if(!count)return fail("The latest survey contains no access points.");
  if(requested && !dragon::lab::safeName(requested))return fail("Invalid survey name.");
  if(usbDriveOn())return fail("Eject the SD card from USB Drive first.");
  if(!sdReady || !sdMutex)return fail("Insert and mount a microSD card first.");
  if(xSemaphoreTake(sdMutex,pdMS_TO_TICKS(150))!=pdTRUE)return fail("The SD card is busy. Try again.");
  struct Unlock { ~Unlock(){xSemaphoreGive(sdMutex);} } unlock;
  if(usbDriveOn() || !sdReady)return fail("The SD card is unavailable.");
  const uint64_t total=CardFS.totalBytes(),used=CardFS.usedBytes();
  if(total<=used || total-used<32ULL*1024*1024+8192)return fail("Insufficient free space on the SD card.");
  constexpr char folder[]="/dragon/surveys";
  if(!CardFS.exists(folder) && !CardFS.mkdir(folder))return fail("Could not create /dragon/surveys.");
  File dir=CardFS.open(folder);
  if(!dir || !dir.isDirectory())return fail("Could not open /dragon/surveys.");
  unsigned files=0;
  for(File entry=dir.openNextFile();entry && files<512;entry=dir.openNextFile()){++files;entry.close();}
  dir.close();
  if(files>=512)return fail("Survey folder is full. Copy old files off the card.");
  char path[72],temp[80],generated[24];
  uint32_t salt=esp_random();
  for(unsigned attempt=0;;++attempt){
    snprintf(generated,sizeof(generated),"survey-%08lx",(unsigned long)(salt+attempt));
    snprintf(path,sizeof(path),"%s/%s.csv",folder,requested?requested:generated);
    snprintf(temp,sizeof(temp),"%s.part",path);
    if(!CardFS.exists(path) && !CardFS.exists(temp))break;
    if(requested || attempt==15)return fail("Survey name already exists. Choose another.");
  }
  File file=CardFS.open(temp,FILE_WRITE);
  if(!file)return fail("Could not start writing the survey.");
  size_t bytes=0;
  const bool wrote=dragon::p4::writeSurveyCsv(*sample,millis(),detector.privacy,[&](const char* data,size_t length){
    const size_t written=file.write(reinterpret_cast<const uint8_t*>(data),length);
    bytes+=written;
    return written==length;
  });
  file.flush();
  const bool ioOk=!file.getWriteError();
  file.close();
  File verify=CardFS.open(temp);
  const bool complete=wrote && ioOk && verify && verify.size()==bytes;
  verify.close();
  if(!complete || CardFS.exists(path) || !CardFS.rename(temp,path)){
    CardFS.remove(temp);
    return fail("Could not finish the survey file.");
  }
  snprintf(message,size,"Saved %u of %u APs\n%s",count,unsigned(sample->total),path);
  return true;
}
