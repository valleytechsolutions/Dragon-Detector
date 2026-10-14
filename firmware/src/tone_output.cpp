// Original MIT PCM generator; ES8311 driver is separately Apache-2.0.
#include "app.h"
#include "board_pins.h"
#ifdef BOARD_XIAO_ROUND
// The Seeed Round Display has no speaker, codec or buzzer pins.
bool toneOutputBegin(){return false;}
void toneOutput(uint16_t,uint16_t,uint8_t){}
#elif defined(BOARD_STOPWATCH)
#include <M5Unified.h>
bool toneOutputBegin(){return M5.Speaker.begin();}
void toneOutput(uint16_t hz,uint16_t ms,uint8_t volume){M5.Speaker.setVolume(unsigned(volume)*255/100);if(hz&&ms&&volume)M5.Speaker.tone(hz,ms);else M5.Speaker.stop();}
#else
#include <driver/i2s_std.h>
#include <math.h>
#ifdef BOARD_AMOLED_175C
#include <es8311.h>
#endif
static i2s_chan_handle_t tx=nullptr;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static uint16_t frequency=0;static uint8_t gain=0;static uint32_t until=0;
static void outputTask(void*){
 int16_t samples[256*2];float phase=0,amplitude=0;
 for(;;){
  portENTER_CRITICAL(&lock);uint16_t hz=frequency;uint8_t volume=gain;uint32_t end=until;portEXIT_CRITICAL(&lock);
  if(int32_t(millis()-end)>=0)hz=0;
  float target=hz?volume*327.0f:0;
  for(unsigned i=0;i<256;++i){amplitude+=(target-amplitude)*0.015f;phase+=hz*6.283185307f/16000;if(phase>6.283185307f)phase-=6.283185307f;int16_t v=int16_t(sinf(phase)*amplitude);samples[2*i]=samples[2*i+1]=v;}
  size_t written=0;if(i2s_channel_write(tx,samples,sizeof(samples),&written,100)!=ESP_OK)vTaskDelay(1);
 }
}
bool toneOutputBegin(){
 pinMode(AUDIO_MUTE_PIN,OUTPUT);digitalWrite(AUDIO_MUTE_PIN,LOW);
 i2s_chan_config_t cfg=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);cfg.dma_desc_num=4;cfg.dma_frame_num=256;
 if(i2s_new_channel(&cfg,&tx,nullptr)!=ESP_OK)return false;
 i2s_std_config_t cfgStd{};cfgStd.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000);cfgStd.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
 cfgStd.gpio_cfg.mclk=gpio_num_t(AUDIO_I2S_MCK_IO);cfgStd.gpio_cfg.bclk=gpio_num_t(AUDIO_I2S_BCK_IO);cfgStd.gpio_cfg.ws=gpio_num_t(AUDIO_I2S_WS_IO);cfgStd.gpio_cfg.dout=gpio_num_t(AUDIO_I2S_DO_IO);cfgStd.gpio_cfg.din=GPIO_NUM_NC;
 if(i2s_channel_init_std_mode(tx,&cfgStd)!=ESP_OK||i2s_channel_enable(tx)!=ESP_OK)return false;
#ifdef BOARD_AMOLED_175C
 auto codec=es8311_create(0,ES8311_ADDRESS_0);if(!codec)return false;
 es8311_clock_config_t clock{};clock.mclk_from_mclk_pin=true;clock.mclk_frequency=4096000;clock.sample_frequency=16000;
 bool ok=es8311_init(codec,&clock,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16)==ESP_OK&&es8311_voice_volume_set(codec,100,nullptr)==ESP_OK;es8311_delete(codec);if(!ok)return false;
#endif
 if(xTaskCreate(outputTask,"dragon-tone",3072,nullptr,2,nullptr)!=pdPASS)return false;
 digitalWrite(AUDIO_MUTE_PIN,HIGH);return true;
}
void toneOutput(uint16_t hz,uint16_t ms,uint8_t volume){portENTER_CRITICAL(&lock);frequency=hz;gain=volume;until=millis()+ms;portEXIT_CRITICAL(&lock);}
#endif
