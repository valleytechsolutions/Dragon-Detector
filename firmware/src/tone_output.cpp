// Original MIT PCM generator; ES8311 driver is separately Apache-2.0.
#include "app.h"
#include "board_pins.h"
#if defined(BOARD_LILYGO_175) || defined(BOARD_XIAO_ROUND) || defined(BOARD_CROWPANEL_21)
// These boards have no configured speaker, codec or buzzer.
bool toneOutputBegin(){return false;}
void toneOutput(uint16_t,uint16_t,uint8_t){}
#elif defined(BOARD_STOPWATCH)
#include <M5Unified.h>
bool toneOutputBegin(){return M5.Speaker.begin();}
void toneOutput(uint16_t hz,uint16_t ms,uint8_t volume){M5.Speaker.setVolume(unsigned(volume)*255/100);if(hz&&ms&&volume)M5.Speaker.tone(hz,ms);else M5.Speaker.stop();}
#else
#include <driver/i2s_std.h>
#include <math.h>
#if defined(BOARD_WATCHER) || defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175)
#include <es8311.h>
#endif
static i2s_chan_handle_t tx=nullptr;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
#ifdef BOARD_WATCHER
static volatile uint32_t outputWrites=0,outputErrors=0,audibleBuffers=0;
void watcherToneDiagnostics(){Serial.printf("WATCHER_I2S writes=%lu errors=%lu audible_buffers=%lu\n",outputWrites,outputErrors,audibleBuffers);}
#endif
static uint16_t frequency=0;static uint8_t gain=0;static uint32_t until=0;
static void outputTask(void*){
 int16_t samples[256*2];float phase=0,amplitude=0;
 for(;;){
  portENTER_CRITICAL(&lock);uint16_t hz=frequency;uint8_t volume=gain;uint32_t end=until;portEXIT_CRITICAL(&lock);
  if(int32_t(millis()-end)>=0)hz=0;
  float target=hz?volume*327.0f:0;
  for(unsigned i=0;i<256;++i){amplitude+=(target-amplitude)*0.015f;phase+=hz*6.283185307f/16000;if(phase>6.283185307f)phase-=6.283185307f;int16_t v=int16_t(sinf(phase)*amplitude);samples[2*i]=samples[2*i+1]=v;}
  size_t written=0;esp_err_t result=i2s_channel_write(tx,samples,sizeof(samples),&written,100);
#ifdef BOARD_WATCHER
  if(result==ESP_OK&&written==sizeof(samples)){++outputWrites;if(hz&&volume)++audibleBuffers;}
  else ++outputErrors;
#endif
  if(result!=ESP_OK)vTaskDelay(1);
 }
}
bool toneOutputBegin(){
#ifdef BOARD_WATCHER
 extern void watcherAudioPower(bool);watcherAudioPower(false);
#else
 pinMode(AUDIO_MUTE_PIN,OUTPUT);digitalWrite(AUDIO_MUTE_PIN,LOW);
#endif
 i2s_chan_config_t cfg=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);cfg.dma_desc_num=4;cfg.dma_frame_num=256;
 if(i2s_new_channel(&cfg,&tx,nullptr)!=ESP_OK)return false;
 i2s_std_config_t cfgStd{};cfgStd.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000);cfgStd.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
 cfgStd.gpio_cfg.mclk=gpio_num_t(AUDIO_I2S_MCK_IO);cfgStd.gpio_cfg.bclk=gpio_num_t(AUDIO_I2S_BCK_IO);cfgStd.gpio_cfg.ws=gpio_num_t(AUDIO_I2S_WS_IO);cfgStd.gpio_cfg.dout=gpio_num_t(AUDIO_I2S_DO_IO);cfgStd.gpio_cfg.din=GPIO_NUM_NC;
 if(i2s_channel_init_std_mode(tx,&cfgStd)!=ESP_OK||i2s_channel_enable(tx)!=ESP_OK)return false;
#if defined(BOARD_WATCHER) || defined(BOARD_AMOLED_175C) || defined(BOARD_AMOLED_175)
 auto codec=es8311_create(0,ES8311_ADDRESS_0);if(!codec)return false;
 es8311_clock_config_t clock{};clock.mclk_from_mclk_pin=true;clock.mclk_frequency=4096000;clock.sample_frequency=16000;
#if defined(BOARD_WATCHER) || defined(BOARD_AMOLED_175)
 // Driver percent maps directly to DAC register: 100 -> 0xFF (+32 dB).
 // 65 -> 0xA5 (-13 dB), keeping headroom even at full application volume.
 constexpr int codecVolume=65;
#else
 constexpr int codecVolume=100;
#endif
 bool ok=es8311_init(codec,&clock,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16)==ESP_OK&&es8311_voice_volume_set(codec,codecVolume,nullptr)==ESP_OK&&es8311_voice_mute(codec,false)==ESP_OK;es8311_delete(codec);if(!ok)return false;
#endif
 if(xTaskCreate(outputTask,"dragon-tone",3072,nullptr,2,nullptr)!=pdPASS)return false;
#ifdef BOARD_WATCHER
 watcherAudioPower(true);
#else
 digitalWrite(AUDIO_MUTE_PIN,HIGH);
#endif
 return true;
}
void toneOutput(uint16_t hz,uint16_t ms,uint8_t volume){portENTER_CRITICAL(&lock);frequency=hz;gain=volume;until=millis()+ms;portEXIT_CRITICAL(&lock);}
#endif
