#include "app.h"
#include "board_pins.h"
#include <AudioFileSource.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>

static portMUX_TYPE statusMux=portMUX_INITIALIZER_UNLOCKED;
static AppAudioStatus status;
static QueueHandle_t commands;
struct Command { uint8_t action; int value; char path[256]; };
static void report(AudioMode mode,const char* message,uint32_t seconds=0,uint16_t peak=0){
  portENTER_CRITICAL(&statusMux);status.mode=mode;strlcpy(status.message,message,sizeof(status.message));status.seconds=seconds;status.peak=peak;portEXIT_CRITICAL(&statusMux);
}
AppAudioStatus audioStatus(){portENTER_CRITICAL(&statusMux);AppAudioStatus s=status;portEXIT_CRITICAL(&statusMux);return s;}
class LockedSource:public AudioFileSource {
  File file;
public:
  bool open(const char* p)override{xSemaphoreTake(sdMutex,portMAX_DELAY);file=SD_MMC.open(p,FILE_READ);xSemaphoreGive(sdMutex);return bool(file);}
  uint32_t read(void* p,uint32_t n)override{xSemaphoreTake(sdMutex,portMAX_DELAY);uint32_t result=file.read((uint8_t*)p,n);xSemaphoreGive(sdMutex);return result;}
  bool seek(int32_t p,int dir)override{xSemaphoreTake(sdMutex,portMAX_DELAY);bool r=file.seek(p,dir==SEEK_SET?SeekSet:dir==SEEK_CUR?SeekCur:SeekEnd);xSemaphoreGive(sdMutex);return r;}
  bool close()override{xSemaphoreTake(sdMutex,portMAX_DELAY);file.close();xSemaphoreGive(sdMutex);return true;}
  bool isOpen()override{return bool(file);} uint32_t getSize()override{return file.size();}uint32_t getPos()override{return file.position();}
  ~LockedSource(){close();}
};
static void wavHeader(File& f,uint32_t bytes){
  uint8_t h[44]{};memcpy(h,"RIFF",4);uint32_t size=bytes+36;memcpy(h+4,&size,4);memcpy(h+8,"WAVEfmt ",8);
  uint32_t fmt=16,rate=16000,byteRate=32000;uint16_t pcm=1,channels=1,align=2,bits=16;
  memcpy(h+16,&fmt,4);memcpy(h+20,&pcm,2);memcpy(h+22,&channels,2);memcpy(h+24,&rate,4);memcpy(h+28,&byteRate,4);memcpy(h+32,&align,2);memcpy(h+34,&bits,2);memcpy(h+36,"data",4);memcpy(h+40,&bytes,4);f.seek(0);f.write(h,44);
}
static void audioTask(void*){
  AudioGenerator* decoder=nullptr;LockedSource* source=nullptr;AudioOutputI2S* output=nullptr;
  File recording;bool mic=false;uint32_t bytes=0,started=0,lastCheckpoint=0;char current[100]{};uint8_t volume=50;
  auto stop=[&](){
    if(decoder){decoder->stop();delete decoder;decoder=nullptr;}
    if(output){output->stop();delete output;output=nullptr;}
    if(source){delete source;source=nullptr;}
    if(mic){i2s_stop(I2S_NUM_1);i2s_driver_uninstall(I2S_NUM_1);mic=false;}
    if(recording){xSemaphoreTake(sdMutex,portMAX_DELAY);wavHeader(recording,bytes);recording.flush();recording.close();xSemaphoreGive(sdMutex);}
    digitalWrite(AUDIO_MUTE_PIN,LOW);report(AudioMode::Idle,"Ready");
  };
  for(;;){
    Command c{};
    if(xQueueReceive(commands,&c,0)==pdTRUE){
      if(c.action==3){volume=constrain(c.value,0,100);if(output)output->SetGain(volume/150.0f);portENTER_CRITICAL(&statusMux);status.volume=volume;portEXIT_CRITICAL(&statusMux);}
      else{
        stop();
        if(c.action!=0 && !sdReady)report(AudioMode::Error,"Insert a FAT32 SD card and restart");
        else if(c.action==1){
          source=new LockedSource();String path=c.path;path.toLowerCase();
          if(source->open(c.path) && (path.endsWith(".mp3")||path.endsWith(".wav"))){
            output=new AudioOutputI2S(0,AudioOutputI2S::EXTERNAL_I2S,12);
            output->SetPinout(AUDIO_I2S_BCK_IO,AUDIO_I2S_WS_IO,AUDIO_I2S_DO_IO);output->SetGain(volume/150.0f);
            decoder=path.endsWith(".mp3")?static_cast<AudioGenerator*>(new AudioGeneratorMP3()):static_cast<AudioGenerator*>(new AudioGeneratorWAV());
            if(decoder->begin(source,output)){strlcpy(current,c.path,sizeof(current));started=millis();digitalWrite(AUDIO_MUTE_PIN,HIGH);report(AudioMode::Playing,current);}
            else{stop();report(AudioMode::Error,"Audio format could not be decoded");}
          }else{stop();report(AudioMode::Error,"Cannot open MP3/WAV file");}
        }else if(c.action==2){
          bytes=0;
          char path[80];snprintf(path,sizeof(path),"/recordings/rec-%08lx.wav",(unsigned long)esp_random());
          xSemaphoreTake(sdMutex,portMAX_DELAY);recording=SD_MMC.open(path,FILE_WRITE);if(recording)wavHeader(recording,0);xSemaphoreGive(sdMutex);
          if(!recording){report(AudioMode::Error,"Cannot create recording");continue;}
          i2s_config_t cfg{};cfg.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX);cfg.sample_rate=16000;cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT;
          cfg.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;cfg.communication_format=I2S_COMM_FORMAT_STAND_I2S;cfg.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1;cfg.dma_buf_count=6;cfg.dma_buf_len=256;
          i2s_pin_config_t pins{};pins.mck_io_num=-1;pins.bck_io_num=MIC_I2S_SCK;pins.ws_io_num=MIC_I2S_WS;pins.data_out_num=-1;pins.data_in_num=MIC_I2S_SD;
          esp_err_t err=i2s_driver_install(I2S_NUM_1,&cfg,0,nullptr);mic=err==ESP_OK;
          if(mic)err=i2s_set_pin(I2S_NUM_1,&pins);
          if(err!=ESP_OK){stop();report(AudioMode::Error,"Microphone initialization failed");continue;}
          bytes=0;started=lastCheckpoint=millis();strlcpy(current,path,sizeof(current));report(AudioMode::Recording,current);
        }
      }
    }
    if(decoder){if(!decoder->loop()){stop();}else report(AudioMode::Playing,current,(millis()-started)/1000);}
    else if(mic){
      int32_t raw[512];int16_t pcm[256];size_t got=0;
      esp_err_t err=i2s_read(I2S_NUM_1,raw,sizeof(raw),&got,pdMS_TO_TICKS(30));
      if(err!=ESP_OK){stop();report(AudioMode::Error,"Microphone read error");continue;}
      size_t count=got/8;uint64_t energy[2]{};
      for(size_t i=0;i<count;++i){energy[0]+=abs(raw[i*2]>>16);energy[1]+=abs(raw[i*2+1]>>16);}
      // Both microphone revisions use a single I2S slot; select the active slot.
      int ch=energy[1]>energy[0]?1:0;uint16_t peak=0;
      for(size_t i=0;i<count;++i){int32_t v=raw[i*2+ch]>>14;v=constrain(v,-32768,32767);pcm[i]=v;peak=std::max<uint16_t>(peak,abs(v));}
      xSemaphoreTake(sdMutex,portMAX_DELAY);size_t written=recording.write((uint8_t*)pcm,count*2);bytes+=written;
      if(millis()-lastCheckpoint>2000){lastCheckpoint=millis();wavHeader(recording,bytes);recording.seek(44+bytes);recording.flush();}xSemaphoreGive(sdMutex);
      if(written!=count*2){stop();report(AudioMode::Error,"SD full or write error; recording saved");continue;}
      report(AudioMode::Recording,current,(millis()-started)/1000,peak);
      if(bytes>=32*1024*1024){stop();report(AudioMode::Idle,"Recording saved (32 MB limit)");}
    }else vTaskDelay(pdMS_TO_TICKS(5));
    if(decoder)vTaskDelay(1);
  }
}
static void send(uint8_t action,const String& path="",int value=0){if(!commands)return;Command c{};c.action=action;c.value=value;strlcpy(c.path,path.c_str(),sizeof(c.path));if(xQueueSend(commands,&c,0)!=pdTRUE)report(AudioMode::Error,"Audio busy; try again");}
void audioBegin(){pinMode(AUDIO_MUTE_PIN,OUTPUT);digitalWrite(AUDIO_MUTE_PIN,LOW);commands=xQueueCreate(4,sizeof(Command));xTaskCreatePinnedToCore(audioTask,"audio",8192,nullptr,2,nullptr,1);}
void audioPlay(const String& path){send(1,path);}void audioRecord(){send(2);}void audioStop(){send(0);}void audioVolume(int value){send(3,"",value);}
