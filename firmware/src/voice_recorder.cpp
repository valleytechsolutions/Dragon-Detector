// Original MIT Voice Recorder and WAV player for boards with a microphone
// driver (BOARD_HAS_MIC). Recording: the microphones over I2S (full duplex
// with the tone transmitter), mixed to mono 16 kHz, a PSRAM ring, then a card
// task that writes 16-bit PCM WAV files to /recordings. Playback: a WAV file
// from the card, mixed to mono and converted to 16 kHz, through the tone
// transmitter (tone_output.cpp asks voicePlaybackFill for each buffer).
#include "board_pins.h"
#if BOARD_HAS_MIC
#include "app.h"
#include "voice_recorder.h"
#include "wav_format.h"
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#ifdef BOARD_WAVESHARE_P4_KNOB
#include <freertos/idf_additions.h>
#endif
#if BOARD_HAS_MP3
#include "mp3dec.h"   // libhelix (chmorgan__esp-libhelix-mp3)
#endif

bool audioInputReady();bool audioInputEnable(bool on);size_t audioInputRead(void* data,size_t bytes,uint32_t timeoutMs);void audioAmplifier(bool on);
uint8_t audioVolumePercent();
#ifdef BOARD_WAVESHARE_P4_KNOB
bool p4KnobMicBegin();void p4KnobMicEnd();
static bool micBegin(){return p4KnobMicBegin();}
static void micEnd(){p4KnobMicEnd();}
#endif

namespace {
using namespace dragon;
constexpr uint32_t Rate=16000;
constexpr uint32_t RecRingSamples=1u<<19;   // 1 MiB of PSRAM: 32 s of sound while the card is busy
constexpr uint32_t PlayRingSamples=1u<<15;  // 64 KiB: 2 s ahead of the speaker
constexpr uint32_t WriteSamples=4096;       // one card write per sdMutex hold: 8 KiB
constexpr uint32_t ReadBytes=4096;          // one card read per sdMutex hold
constexpr uint32_t PlayOutMax=ReadBytes+8;  // 8 kHz mono doubles: 2048 frames -> 4096 samples
constexpr uint32_t HeaderEveryMs=3000;      // sizes rewritten and flushed: a cut power keeps a playable file
constexpr uint64_t CardReserve=1u<<20;      // stop with 1 MiB left on the card
constexpr const char* Folder="/recordings";

enum Op:uint8_t {OpRefresh,OpDelete,OpRecStart,OpRecStop,OpPlay,OpPlayStop};
struct Command {Op op=OpRefresh;bool clockValid=false;int16_t year=0;uint8_t month=0,day=0,hour=0,minute=0,second=0;uint32_t uptime=0;char path[128]{};};

QueueHandle_t commands=nullptr;
TaskHandle_t ioTask=nullptr,micTask=nullptr;
portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
VoiceStatus status;     // under lock
VoiceListing listing;   // under lock
wav::Ring<int16_t> recRing,playRing;
std::atomic<bool> capturing{false},captureRunning{false},recActive{false},micWanted{false};
std::atomic<bool> playActive{false},playOpen{false},playPaused{false},playEof{false},fillBusy{false};
std::atomic<uint32_t> captured{0},dropped{0},played{0},pending{0};
std::atomic<uint8_t> peak{0},volume{50};
std::atomic<bool> noticeReady{false};char notice[96]{};
bool micOn=false;      // main loop: ES7210 powered
// Card task state.
File recFile,playFile;
uint32_t recData=0,lastHeaderAt=0,lastWriteAt=0;uint64_t recRoom=0;bool recFailed=false;
wav::Format format;uint32_t playLeft=0;wav::Resampler resampler;
// Card buffers stay in internal RAM so the SD driver can DMA whole blocks;
// the converter's output only feeds the PSRAM ring and lives in PSRAM too.
int16_t writeBuffer[WriteSamples];uint8_t readBuffer[ReadBytes];int16_t* outBuffer=nullptr;
#if BOARD_HAS_MP3
// MP3 playback (card task only). One decoded frame (1152 samples/channel) feeds the
// same resampler and ring as WAV, so playback, position and volume are shared.
constexpr uint32_t Mp3BufBytes=4096;         // holds >2 frames; a frame is <=1440 bytes
HMP3Decoder mp3dec=nullptr;uint8_t* mp3Buf=nullptr;int16_t* mp3Pcm=nullptr;
uint32_t mp3Have=0;bool playMp3=false,mp3Fed=false;
bool isMp3Name(const char* p){size_t n=strlen(p);return n>4&&p[n-4]=='.'&&(p[n-3]|32)=='m'&&(p[n-2]|32)=='p'&&p[n-1]=='3';}
#else
bool isMp3Name(const char*){return false;}
#endif

template<size_t N> void textCopy(char (&out)[N],const char* text){strlcpy(out,text?text:"",N);}
void setRec(VoiceRec state,const char* why){portENTER_CRITICAL(&lock);status.rec=state;if(why)textCopy(status.recWhy,why);portEXIT_CRITICAL(&lock);}
void setPlay(VoicePlay state,const char* why){portENTER_CRITICAL(&lock);status.play=state;if(why)textCopy(status.playWhy,why);portEXIT_CRITICAL(&lock);}
const char* baseName(const char* path){const char* slash=strrchr(path,'/');return slash?slash+1:path;}
bool send(const Command& c){++pending;if(commands&&xQueueSend(commands,&c,pdMS_TO_TICKS(200))==pdTRUE)return true;--pending;return false;}
bool cardLock(uint32_t ms){return xSemaphoreTake(sdMutex,pdMS_TO_TICKS(ms))==pdTRUE;}
void cardUnlock(){xSemaphoreGive(sdMutex);}

// ---------------------------------------------------------------- capture task
void micLoop(void*){
  static int16_t frames[512],mono[256];
  for(;;){
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    if(!capturing)continue;
    captureRunning=true;
    if(!audioInputEnable(true)){textCopy(notice,"Microphone input did not start");noticeReady=true;capturing=false;captureRunning=false;continue;}
    unsigned skip=4,silent=0;uint8_t meter=0; // the first 64 ms carry the ADC's start-up
    while(capturing){
      const size_t n=audioInputRead(frames,sizeof(frames),100)/4;
      if(!n){if(++silent>=20){textCopy(notice,"No sound data from the microphones");noticeReady=true;capturing=false;}continue;}
      silent=0;int top=0;
      for(size_t i=0;i<n;++i){mono[i]=wav::mix(frames[2*i],frames[2*i+1]);int v=abs(int(mono[i]));if(v>top)top=v;}
      if(skip){--skip;continue;}
      const uint8_t level=wav::meterPercent(top);meter=level>meter?level:meter>4?meter-4:0;peak=meter;
      const uint32_t written=recRing.write(mono,n);captured+=n;if(written<n)dropped+=n-written;
    }
    audioInputEnable(false);peak=0;captureRunning=false;
  }
}

// ---------------------------------------------------------------- card task: recording
bool patchHeader(){
  uint8_t size[4];bool ok=recFile.seek(4);wav::put32(size,wav::riffSize(recData));ok=ok&&recFile.write(size,4)==4;
  ok=ok&&recFile.seek(40);wav::put32(size,recData);ok=ok&&recFile.write(size,4)==4;
  ok=ok&&recFile.seek(0,SeekEnd);recFile.flush();return ok;
}
void publishRec(){portENTER_CRITICAL(&lock);status.recBytes=recFile?wav::HeaderBytes+recData:status.recBytes;status.recSeconds=captured/Rate;status.dropped=dropped;status.peak=peak;portEXIT_CRITICAL(&lock);}
void refresh();
// Ends a recording: the capture stops, the ring drains, the sizes go in, the file closes.
void recFinish(const char* why,bool error){
  capturing=false;micWanted=false;
  for(unsigned i=0;i<100&&captureRunning;++i)vTaskDelay(pdMS_TO_TICKS(5));
  char text[96];
  if(recFile){
    while(recRing.used()&&!recFailed){
      if(!cardLock(1000)){recFailed=true;break;}
      uint32_t n=recRing.read(writeBuffer,std::min<uint32_t>(WriteSamples,uint32_t(std::min<uint64_t>(recRoom/2,WriteSamples))));
      if(!n){cardUnlock();break;} // no room left: the rest is dropped
      const size_t bytes=recFile.write((const uint8_t*)writeBuffer,n*2);recData+=bytes;recRoom-=std::min<uint64_t>(recRoom,bytes);
      if(bytes!=n*2)recFailed=true;
      cardUnlock();
    }
    recData-=recData%2;
    bool closed=cardLock(10000);
    if(closed){closed=patchHeader();recFile.close();cardUnlock();}
    else recFile.close(); // the card has been held for 10 s: close what the file has
    if(!closed&&!error){error=true;why="Card write failed: card full or removed";}
    else if(recFailed&&!why)why="Card write failed: the end is missing";
    const uint32_t seconds=recData/(2*Rate);
    // A file whose sizes went in plays, even after an error: say how much it holds.
    if(!closed)snprintf(text,sizeof(text),"%s",why?why:"Card write failed");
    else if(seconds>=3600)snprintf(text,sizeof(text),"%s%sSaved %u:%02u:%02u",why?why:"",why?" / ":"",unsigned(seconds/3600),unsigned(seconds/60%60),unsigned(seconds%60));
    else snprintf(text,sizeof(text),"%s%sSaved %u:%02u",why?why:"",why?" / ":"",unsigned(seconds/60),unsigned(seconds%60));
    portENTER_CRITICAL(&lock);status.recBytes=wav::HeaderBytes+recData;status.recSeconds=seconds;portEXIT_CRITICAL(&lock);
  }else snprintf(text,sizeof(text),"%s",why?why:"Stopped");
  recRing.reset();peak=0;
  setRec(error?VoiceRec::Error:VoiceRec::Idle,text);
  recActive=false;
  refresh();
}
void recStart(const Command& c){
  recData=0;recFailed=false;char path[64]="";
  if(!sdReady){recFinish("Insert a card",true);return;}
  if(!cardLock(3000)){recFinish("Card busy; try again",true);return;}
  bool ok=CardFS.exists(Folder)||CardFS.mkdir(Folder);
  for(unsigned attempt=1;ok&&attempt<100;++attempt){
    ok=wav::recordingName(path,sizeof(path),c.clockValid,c.year,c.month,c.day,c.hour,c.minute,c.second,c.uptime,attempt);
    if(ok&&!CardFS.exists(path))break;
    if(attempt==99)ok=false;
  }
  if(ok){recFile=CardFS.open(path,FILE_WRITE);ok=bool(recFile);}
  uint64_t space=0;
  if(ok){uint64_t total=CardFS.totalBytes(),used=CardFS.usedBytes();space=total>used?total-used:0;}
  uint8_t header[wav::HeaderBytes];wav::header(header,Rate,1,wav::Unknown);
  if(ok){ok=recFile.write(header,sizeof(header))==sizeof(header);recFile.flush();if(!ok){recFile.close();CardFS.remove(path);}}
  cardUnlock();
  if(!ok){recFinish(*path?"Cannot create the recording file":"Cannot make a new file name",true);return;}
  recRoom=std::min<uint64_t>(wav::FileLimit-wav::HeaderBytes,space>CardReserve+wav::HeaderBytes?space-CardReserve-wav::HeaderBytes:0);
  recRoom-=recRoom%2;lastHeaderAt=lastWriteAt=millis();
  portENTER_CRITICAL(&lock);textCopy(status.recFile,path);status.recWhy[0]=0;status.recBytes=wav::HeaderBytes;portEXIT_CRITICAL(&lock);
  if(!recRoom)recFinish("Card full",false);
}
// Moves what the microphones gave to the card: bounded writes, each under sdMutex.
void recStep(){
  if(!recFile)return; // the start has not reached the card task yet
  if(!capturing&&!captureRunning){recFinish(noticeReady?notice:nullptr,noticeReady);return;}
  for(unsigned round=0;round<8;++round){
    // Whole 8 KiB writes, or what there is every 250 ms so the size keeps up.
    const uint32_t have=recRing.used();
    if(!have||(have<WriteSamples&&(round||millis()-lastWriteAt<250)))break;
    if(!sdReady){recFinish("Card removed",true);return;}
    if(recRoom<2){recFinish(recData+wav::HeaderBytes>=wav::FileLimit-2?"Reached the 4 GB WAV limit":"Card full",false);return;}
    if(!cardLock(200))break; // the ring keeps the sound until the card is free
    const uint32_t n=recRing.read(writeBuffer,std::min<uint32_t>(have,uint32_t(std::min<uint64_t>(recRoom/2,WriteSamples))));
    const size_t bytes=recFile.write((const uint8_t*)writeBuffer,n*2);recData+=bytes;recRoom-=std::min<uint64_t>(recRoom,bytes);lastWriteAt=millis();
    bool ok=bytes==n*2;
    if(ok&&millis()-lastHeaderAt>=HeaderEveryMs){lastHeaderAt=millis();ok=patchHeader();}
    cardUnlock();
    if(!ok){recFailed=true;recFinish("Card write failed: card full or removed",true);return;}
  }
  publishRec();
}

// ---------------------------------------------------------------- card task: playback
void playClose(VoicePlay state,const char* why){
  const bool finished=playEof&&state==VoicePlay::Idle;
  playActive=false;for(unsigned i=0;i<50&&fillBusy;++i)vTaskDelay(1);
  if(playFile){if(cardLock(1000)){playFile.close();cardUnlock();}else playFile.close();}
  playOpen=false;playRing.reset();playEof=false;playPaused=false;
#if BOARD_HAS_MP3
  playMp3=false;mp3Have=0;mp3Fed=false;
  // Free the decoder so the next MP3 starts with an empty bit reservoir (no
  // stale main data from the previous track); playOpenMp3 re-inits it.
  if(mp3dec){MP3FreeDecoder(mp3dec);mp3dec=nullptr;}
#endif
  if(finished){portENTER_CRITICAL(&lock);status.playMs=status.playTotalMs;portEXIT_CRITICAL(&lock);}
  setPlay(state,why);
}
#if BOARD_HAS_MP3
// Under cardLock. Skips an ID3v2 tag, finds the first frame, reads its rate and
// channels, and leaves the compressed buffer primed. Returns an error or null.
const char* playOpenMp3(uint32_t size){
  if(!mp3dec&&!(mp3dec=MP3InitDecoder()))return "Not enough memory for MP3";
  uint32_t start=0;uint8_t id3[10];
  if(playFile.seek(0)&&playFile.read(id3,10)==10&&!memcmp(id3,"ID3",3))
    start=10+((uint32_t(id3[6]&0x7f)<<21)|(uint32_t(id3[7]&0x7f)<<14)|(uint32_t(id3[8]&0x7f)<<7)|uint32_t(id3[9]&0x7f)); // syncsafe tag size
  if(start>=size||!playFile.seek(start))return "No MP3 audio found";
  mp3Have=playFile.read(mp3Buf,Mp3BufBytes);mp3Fed=mp3Have<Mp3BufBytes;
  int sync=mp3Have?MP3FindSyncWord(mp3Buf,int(mp3Have)):-1;
  if(sync<0)return "No MP3 audio found";
  if(sync>0){memmove(mp3Buf,mp3Buf+sync,mp3Have-sync);mp3Have-=sync;}
  MP3FrameInfo fi;
  if(MP3GetNextFrameInfo(mp3dec,&fi,mp3Buf)!=ERR_MP3_NONE||fi.samprate<int(wav::MinRate)||fi.samprate>int(wav::MaxRate)||fi.nChans<1||fi.nChans>2)
    return "Unsupported MP3 (need 8-48 kHz, mono/stereo)";
  format=wav::Format{};format.format=1;format.rate=uint32_t(fi.samprate);format.channels=uint16_t(fi.nChans);format.bits=16;
  const uint32_t total=fi.bitrate>0?uint32_t(uint64_t(size-start)*8000u/uint32_t(fi.bitrate)):0; // VBR: an estimate
  portENTER_CRITICAL(&lock);status.playTotalMs=total;portEXIT_CRITICAL(&lock);
  return nullptr;
}
#endif
void playStart(const Command& c){
  if(playActive||playOpen)playClose(VoicePlay::Idle,nullptr);
  portENTER_CRITICAL(&lock);textCopy(status.playFile,c.path);status.playMs=status.playTotalMs=status.playRate=0;status.playChannels=0;status.playWhy[0]=0;portEXIT_CRITICAL(&lock);
  if(recActive){setPlay(VoicePlay::Error,"Stop recording first");return;}
  if(!sdReady){setPlay(VoicePlay::Error,"Insert a card");return;}
  if(!cardLock(3000)){setPlay(VoicePlay::Error,"Card busy; try again");return;}
  const char* why=nullptr;const bool mp3=isMp3Name(c.path);
  playFile=CardFS.open(c.path,FILE_READ);
  if(!playFile||playFile.isDirectory())why="Cannot open the file";
#if BOARD_HAS_MP3
  else if(mp3)why=(mp3Buf&&mp3Pcm)?playOpenMp3(uint32_t(playFile.size())):"Not enough memory for MP3";
#endif
  else{
    const uint32_t size=uint32_t(playFile.size());
    auto read=[&](uint32_t offset,uint8_t* data,uint32_t length){return playFile.seek(offset)&&playFile.read(data,length)==length;};
    auto parsed=wav::parse(read,size,format);
    if(parsed!=wav::Status::Ok)why=wav::statusText(parsed);
    else if((why=wav::unsupported(format))==nullptr&&!playFile.seek(format.dataOffset))why="Cannot read the file";
  }
  if(why&&playFile)playFile.close();
  playOpen=!why;
  cardUnlock();
  if(why){setPlay(VoicePlay::Error,why);return;}
  resampler.begin(format.rate,Rate);playRing.reset();played=0;playEof=false;playPaused=false;
  if(mp3){
#if BOARD_HAS_MP3
    playMp3=true;
#endif
  }else{playLeft=format.dataBytes;portENTER_CRITICAL(&lock);status.playTotalMs=wav::durationMs(format);portEXIT_CRITICAL(&lock);}
  portENTER_CRITICAL(&lock);status.playRate=format.rate;status.playChannels=format.channels;portEXIT_CRITICAL(&lock);
  setPlay(VoicePlay::Playing,nullptr);
  playActive=true;
}
#if BOARD_HAS_MP3
// Decodes MP3 frames into the same resampler and ring as WAV. Bounded card
// reads (each under sdMutex); a bad frame drops one byte and resyncs.
void playStepMp3(){
  portENTER_CRITICAL(&lock);status.playMs=status.playTotalMs?std::min<uint32_t>(played/(Rate/1000),status.playTotalMs):played/(Rate/1000);portEXIT_CRITICAL(&lock);
  if(playEof){if(!playRing.used())playClose(VoicePlay::Idle,"Finished");return;}
  for(unsigned round=0;round<4&&playRing.space()>=PlayOutMax;++round){
    if(mp3Have<Mp3BufBytes&&!mp3Fed){          // keep the buffer full so a whole frame is always present
      if(!sdReady){playClose(VoicePlay::Error,"Card removed");return;}
      const uint32_t want=Mp3BufBytes-mp3Have;
      if(!cardLock(100))break;
      const size_t got=playFile.read(mp3Buf+mp3Have,want);
      cardUnlock();
      mp3Have+=got;if(got<want)mp3Fed=true;
    }
    if(!mp3Have){playEof=true;break;}
    int sync=MP3FindSyncWord(mp3Buf,int(mp3Have));
    if(sync<0){if(mp3Fed){playEof=true;break;}if(mp3Have>3){memmove(mp3Buf,mp3Buf+mp3Have-3,3);mp3Have=3;}continue;}
    if(sync>0){memmove(mp3Buf,mp3Buf+sync,mp3Have-sync);mp3Have-=sync;}
    unsigned char* in=mp3Buf;int left=int(mp3Have);
    const int err=MP3Decode(mp3dec,&in,&left,mp3Pcm,0);
    if(err==ERR_MP3_MAINDATA_UNDERFLOW){
      // The bit reservoir was not primed yet, but this frame's bytes WERE
      // consumed into it; advance and decode the next frame (do not stop).
      const uint32_t used=mp3Have-uint32_t(left<0?0:left);
      if(used&&used<=mp3Have){memmove(mp3Buf,mp3Buf+used,mp3Have-used);mp3Have-=used;}
      else if(mp3Have){memmove(mp3Buf,mp3Buf+1,mp3Have-1);--mp3Have;} // no progress: nudge to resync
      continue;
    }
    if(err==ERR_MP3_INDATA_UNDERFLOW){
      // Short of a full frame. At the end of the file that is the end; with the
      // buffer already full it is a corrupt header, so skip a byte and resync.
      if(mp3Fed){playEof=true;break;}
      if(mp3Have>=Mp3BufBytes){memmove(mp3Buf,mp3Buf+1,mp3Have-1);--mp3Have;}
      continue;   // otherwise the next round tops the buffer up and retries
    }
    if(err){memmove(mp3Buf,mp3Buf+1,mp3Have-1);--mp3Have;continue;} // bad frame: resync
    const uint32_t consumed=mp3Have-uint32_t(left<0?0:left);
    if(consumed&&consumed<=mp3Have){memmove(mp3Buf,mp3Buf+consumed,mp3Have-consumed);mp3Have-=consumed;}
    MP3FrameInfo fi;MP3GetLastFrameInfo(mp3dec,&fi);
    const int n=fi.outputSamps;uint32_t outputs=0;
    auto emit=[&](int16_t v){if(outputs<PlayOutMax)outBuffer[outputs++]=v;};
    if(fi.nChans==1)for(int i=0;i<n;++i)resampler.push(mp3Pcm[i],emit);
    else for(int i=0;i+1<n;i+=2)resampler.push(wav::mix(mp3Pcm[i],mp3Pcm[i+1]),emit);
    playRing.write(outBuffer,outputs);
  }
}
#endif
void playStep(){
  if(!playActive)return;
#if BOARD_HAS_MP3
  if(playMp3){playStepMp3();return;}
#endif
  portENTER_CRITICAL(&lock);status.playMs=std::min<uint32_t>(played/(Rate/1000),status.playTotalMs);portEXIT_CRITICAL(&lock);
  if(playEof){if(!playRing.used())playClose(VoicePlay::Idle,"Finished");return;}
  for(unsigned round=0;round<4&&playRing.space()>=PlayOutMax;++round){
    if(!playLeft){playEof=true;break;}
    if(!sdReady){playClose(VoicePlay::Error,"Card removed");return;}
    if(!cardLock(100))break;
    const uint32_t want=std::min<uint32_t>(playLeft,ReadBytes);
    const size_t got=playFile.read(readBuffer,want);
    cardUnlock();
    if(got<want){playLeft=0;playEof=true;if(!got)break;} // a short file: play what is there
    else playLeft-=want;
    const auto* pcm=reinterpret_cast<const int16_t*>(readBuffer);uint32_t outputs=0;
    auto emit=[&](int16_t v){if(outputs<PlayOutMax)outBuffer[outputs++]=v;};
    if(format.channels==1)for(size_t i=0;i<got/2;++i)resampler.push(pcm[i],emit);
    else for(size_t i=0;i<got/4;++i)resampler.push(wav::mix(pcm[2*i],pcm[2*i+1]),emit);
    playRing.write(outBuffer,outputs);
  }
}

// ---------------------------------------------------------------- card task: list, delete
void refresh(){
  VoiceListing next;next.ready=sdReady;
  if(sdReady&&cardLock(3000)){
    File dir=CardFS.open(Folder);unsigned seen=0;
    if(dir&&dir.isDirectory()){
      for(File f=dir.openNextFile();f;f=dir.openNextFile()){
        const char* name=baseName(f.name());const size_t length=strlen(name);
        if(!f.isDirectory()&&wav::isWavName(name)){
          ++next.total;
          if(length<sizeof(next.name[0])){ // newest first: names carry the date and time
            unsigned at=next.count;while(at&&strcmp(name,next.name[at-1])>0)--at;
            if(at<VoiceListMax){
              const unsigned last=std::min<unsigned>(next.count,VoiceListMax-1);
              for(unsigned i=last;i>at;--i){memcpy(next.name[i],next.name[i-1],sizeof(next.name[0]));next.bytes[i]=next.bytes[i-1];}
              textCopy(next.name[at],name);next.bytes[at]=uint32_t(f.size());if(next.count<VoiceListMax)++next.count;
            }
          }
        }
        f.close();
        // Long folders: let the other card users in now and then.
        if(++seen%32==0){cardUnlock();vTaskDelay(1);if(!cardLock(3000)){dir.close();next.ready=false;goto done;}}
        if(seen>=4000)break;
      }
    }
    dir.close();
    {uint64_t total=CardFS.totalBytes(),used=CardFS.usedBytes();next.freeBytes=total>used?total-used:0;}
    cardUnlock();
  }
done:
  portENTER_CRITICAL(&lock);next.revision=listing.revision+1;listing=next;portEXIT_CRITICAL(&lock);
}
void removeFile(const char* path){
  if(sdReady&&cardLock(3000)){CardFS.remove(path);cardUnlock();}
  refresh();
}
void ioLoop(void*){
  for(;;){
    Command c;
    const bool busy=recActive||playActive;
    if(xQueueReceive(commands,&c,busy?pdMS_TO_TICKS(20):portMAX_DELAY)==pdTRUE){
      switch(c.op){
        case OpRefresh:refresh();break;
        case OpDelete:removeFile(c.path);break;
        case OpRecStart:recStart(c);break;
        case OpRecStop:if(recActive)recFinish(nullptr,false);break;
        case OpPlay:playStart(c);break;
        case OpPlayStop:if(playActive||playOpen)playClose(VoicePlay::Idle,"Stopped");break;
      }
      --pending;
    }
    if(recActive)recStep();
    playStep();
  }
}
bool ready(){
  if(ioTask)return true;
  if(!recRing.data){auto* store=(int16_t*)heap_caps_malloc(RecRingSamples*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!store)return false;recRing.attach(store,RecRingSamples);}
  if(!playRing.data){auto* store=(int16_t*)heap_caps_malloc(PlayRingSamples*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!store)return false;playRing.attach(store,PlayRingSamples);}
  if(!outBuffer&&!(outBuffer=(int16_t*)heap_caps_malloc(PlayOutMax*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)))return false;
#if BOARD_HAS_MP3
  // CPU-owned media buffers must not consume the coprocessor's DMA reserve.
  if(!mp3Buf&&!(mp3Buf=(uint8_t*)heap_caps_malloc(Mp3BufBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)))return false;
  if(!mp3Pcm&&!(mp3Pcm=(int16_t*)heap_caps_malloc(1152*2*sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)))return false;
#endif
  if(!commands&&!(commands=xQueueCreate(6,sizeof(Command))))return false;
  if(!micTask&&xTaskCreate(micLoop,"dd-mic",3072,nullptr,5,&micTask)!=pdPASS){micTask=nullptr;return false;}
#ifdef BOARD_WAVESHARE_P4_KNOB
  // This resident worker accesses SD and decodes audio; it never writes flash.
  if(xTaskCreateWithCaps(ioLoop,"dd-voice",6144,nullptr,2,&ioTask,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS){ioTask=nullptr;return false;}
#else
  if(xTaskCreate(ioLoop,"dd-voice",6144,nullptr,2,&ioTask)!=pdPASS){ioTask=nullptr;return false;}
#endif
  return true;
}
// Serial replies wait (main loop) until the card task has handled what was asked.
template<class Done> bool waitUntil(uint32_t ms,Done&& done){
  const uint32_t start=millis();
  while(millis()-start<ms){voiceLoop();if(!pending&&done())return true;delay(5);}
  return false;
}
void printRec(){
  auto s=voiceStatus();
  Serial.printf("REC_STATUS state=%s seconds=%lu bytes=%lu peak=%u file=%s dropped=%lu why=\"%s\"\n",s.rec==VoiceRec::Recording?"recording":s.rec==VoiceRec::Error?"error":"idle",
    (unsigned long)s.recSeconds,(unsigned long)s.recBytes,s.peak,*s.recFile?s.recFile:"-",(unsigned long)s.dropped,s.recWhy);
}
void printPlay(){
  auto s=voiceStatus();
  Serial.printf("PLAY_STATUS state=%s ms=%lu total_ms=%lu rate=%lu channels=%u volume=%u file=%s why=\"%s\"\n",
    s.play==VoicePlay::Playing?"playing":s.play==VoicePlay::Paused?"paused":s.play==VoicePlay::Error?"error":"idle",
    (unsigned long)s.playMs,(unsigned long)s.playTotalMs,(unsigned long)s.playRate,s.playChannels,unsigned(volume),*s.playFile?s.playFile:"-",s.playWhy);
}
}

// Output task (tone_output.cpp): stereo frames for the transmitter, 0 when no file plays.
size_t voicePlaybackFill(int16_t* stereo,size_t frames){
  if(!playActive)return 0;
  fillBusy=true;
  if(!playActive){fillBusy=false;return 0;}
  int16_t mono[256];size_t n=0;
  if(!playPaused){n=playRing.read(mono,std::min<size_t>(frames,256));const unsigned gain=volume;for(size_t i=0;i<n;++i)stereo[2*i]=stereo[2*i+1]=wav::scale(mono[i],gain);played+=n;}
  for(size_t i=n;i<frames;++i)stereo[2*i]=stereo[2*i+1]=0;
  fillBusy=false;return frames;
}
VoiceStatus voiceStatus(){
  VoiceStatus s;portENTER_CRITICAL(&lock);s=status;portEXIT_CRITICAL(&lock);
  if(s.rec==VoiceRec::Recording){s.recSeconds=captured/Rate;s.peak=peak;s.dropped=dropped;}
  if(s.play==VoicePlay::Playing&&playPaused)s.play=VoicePlay::Paused;
  return s;
}
bool voiceRecording(){return recActive;}
bool voicePlaying(){return playActive;}
bool voiceBusy(){return recActive||playActive;}
bool voiceRecordStart(char* why,size_t size){
  auto fail=[&](const char* text){if(why)strlcpy(why,text,size);setRec(VoiceRec::Error,text);return false;};
  if(recActive)return fail("Already recording");
  if(!sdReady)return fail("Insert a card");
  if(!audioReady()||!audioInputReady())return fail("Microphone input unavailable");
  if(!ready())return fail("Not enough memory for the recorder");
  if(playActive)voicePlayStop();
  if(!micOn){if(!micBegin()){micEnd();return fail("Microphones did not answer (ES7210)");}micOn=true;}
  micWanted=true;audioStop();audioAmplifier(false);
  Command c;c.op=OpRecStart;c.uptime=millis()/1000;
  if(wallTime()){dragon::Calendar d;clockLocal(d);c.clockValid=true;c.year=d.year;c.month=d.month;c.day=d.day;c.hour=d.hour;c.minute=d.minute;c.second=d.second;}
  recRing.reset();captured=0;dropped=0;peak=0;noticeReady=false;notice[0]=0;
  portENTER_CRITICAL(&lock);status.rec=VoiceRec::Recording;status.recFile[0]=status.recWhy[0]=0;status.recBytes=status.recSeconds=status.dropped=0;portEXIT_CRITICAL(&lock);
  recActive=true;capturing=true;xTaskNotifyGive(micTask);
  if(!send(c)){capturing=false;recActive=false;micWanted=false;return fail("Recorder busy; try again");}
  return true;
}
void voiceRecordStop(){if(!recActive)return;capturing=false;Command c;c.op=OpRecStop;send(c);}
bool voicePlay(const char* path,char* why,size_t size){
  auto fail=[&](const char* text){if(why)strlcpy(why,text,size);portENTER_CRITICAL(&lock);textCopy(status.playFile,path);status.playMs=status.playTotalMs=0;portEXIT_CRITICAL(&lock);setPlay(VoicePlay::Error,text);return false;};
  if(!path||*path!='/'||strlen(path)>=sizeof(Command::path))return fail("Give a full path, such as /music/song.wav");
  if(recActive)return fail("Stop recording first");
  if(!sdReady)return fail("Insert a card");
  if(!ready())return fail("Not enough memory for the player");
  volume=audioVolumePercent();audioStop();
  Command c;c.op=OpPlay;textCopy(c.path,path);
  // Playing until the card task opens it, so the screen and tones agree at once.
  portENTER_CRITICAL(&lock);textCopy(status.playFile,path);status.playMs=status.playTotalMs=0;status.playWhy[0]=0;status.play=VoicePlay::Playing;portEXIT_CRITICAL(&lock);
  if(!send(c))return fail("Player busy; try again");
  return true;
}
void voicePause(bool paused){if(playActive)playPaused=paused;}
void voicePlayStop(){
  if(!ioTask)return;
  playActive=false; // the speaker goes quiet now; the card task closes the file
  Command c;c.op=OpPlayStop;send(c);
}
void voiceRefresh(){if(!ready())return;Command c;c.op=OpRefresh;send(c);}
VoiceListing voiceListing(){VoiceListing l;portENTER_CRITICAL(&lock);l=listing;portEXIT_CRITICAL(&lock);return l;}
bool voiceDelete(const char* path,char* why,size_t size){
  auto fail=[&](const char* text){if(why)strlcpy(why,text,size);return false;};
  if(!path||strncmp(path,"/recordings/",12)||strstr(path,"..")||!wav::isWavName(path)||strlen(path)>=sizeof(Command::path))return fail("Only recordings can be deleted here");
  auto s=voiceStatus();
  if((recActive&&!strcmp(s.recFile,path))||(playActive&&!strcmp(s.playFile,path)))return fail("Stop it first");
  if(!ready())return fail("Recorder unavailable");
  Command c;c.op=OpDelete;textCopy(c.path,path);
  return send(c)||fail("Recorder busy; try again");
}
void voiceLoop(){
  volume=audioVolumePercent();
  if(micOn&&!micWanted&&!captureRunning){micEnd();micOn=false;audioAmplifier(true);}
  static VoiceRec shown=VoiceRec::Idle;
  auto s=status.rec;
  if(s!=shown){
    // A recording that ended by itself (card full, card removed) says so on any page.
    if(shown==VoiceRec::Recording&&s!=VoiceRec::Recording&&!recActive){
      char why[96];portENTER_CRITICAL(&lock);textCopy(why,status.recWhy);portEXIT_CRITICAL(&lock);
      if(s==VoiceRec::Error||strstr(why,"full")||strstr(why,"limit"))uiNotify("RECORDING STOPPED",why,nullptr,false);
    }
    shown=s;
  }
}
bool voiceShutdown(uint32_t waitMs){
  if(!ioTask)return true;
  voiceRecordStop();voicePlayStop();
  const uint32_t start=millis();
  while(millis()-start<waitMs){voiceLoop();if(!pending&&!recActive&&!playActive&&!playOpen)return true;delay(5);}
  return false;
}
bool voiceCommand(const String& c){
  if(c=="rec status"){printRec();return true;}
  if(c=="rec start"){char why[96];if(voiceRecordStart(why,sizeof(why)))waitUntil(3000,[]{return true;});printRec();return true;}
  if(c=="rec stop"){voiceRecordStop();waitUntil(5000,[]{return !recActive;});printRec();return true;}
  if(c=="play status"){printPlay();return true;}
  if(c=="play about"){
    Serial.println("Helix DNA Client technology included. Copyright (c) RealNetworks, Inc., 1995-2002. All rights reserved.");
    Serial.println("Decoder source under the RealNetworks Public Source License: helix-mp3-source.zip inside the P4 Beta 2 release package.");
    Serial.println("https://github.com/valleytechsolutions/Dragon-Detector/releases/tag/v0.14.12-p4-beta.2");
    return true;
  }
  if(c=="play stop"){voicePlayStop();waitUntil(2000,[]{return !playOpen;});printPlay();return true;}
  if(c=="play pause"||c=="play resume"){voicePause(c=="play pause");printPlay();return true;}
  if(c.startsWith("play ")){char why[96];if(voicePlay(c.c_str()+5,why,sizeof(why)))waitUntil(3000,[]{return true;});printPlay();return true;}
  return false;
}
#endif
