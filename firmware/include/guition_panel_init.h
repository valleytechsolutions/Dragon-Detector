// MIT: beta-only bus ownership and raw diagnostics. Manufacturer data is in
// guition_lcd_commands.h under its original Apache-2.0 notice.
#pragma once
#include <new>
#include "guition_lcd_commands.h"

static constexpr uint32_t GuitionPanelHz=20000000,GuitionProbeHz=3000000;
static constexpr unsigned GuitionProbeTimeoutMs=25;
static constexpr spi_host_device_t GuitionSpiHost=SPI2_HOST;
static const char* guitionPanelStage="not_started";
static const char* guitionTouchStage="not_started";
static esp_err_t guitionProbeError=ESP_ERR_INVALID_STATE;
static uint8_t guitionId[4]{};
static bool guitionProbeOwned=false,guitionPanelOwned=false;
static spi_device_handle_t guitionProbeDevice=nullptr;
static spi_transaction_t guitionProbeTransaction{}; // Remains valid after a timeout.
static ESP_PanelBus_QSPI* guitionPanelBus=nullptr;

static spi_bus_config_t guitionBusConfig(){
  return ESP_PANEL_HOST_QSPI_CONFIG_DEFAULT(TFT_SCK,TFT_SDA0,TFT_SDA1,TFT_SDA2,TFT_SDA3);
}
static bool guitionProbeCleanup(){
  if(guitionProbeDevice){
    auto error=spi_bus_remove_device(guitionProbeDevice);if(error!=ESP_OK){guitionProbeError=error;return false;}
    guitionProbeDevice=nullptr;
  }
  if(guitionProbeOwned){
    auto error=spi_bus_free(GuitionSpiHost);if(error!=ESP_OK){guitionProbeError=error;return false;}
    guitionProbeOwned=false;
  }
  return true;
}
static bool guitionProbeId(){
  // A single diagnostic attempt. ID bytes never choose a display table.
  if(guitionProbeOwned||guitionProbeDevice)return false;
  auto host=guitionBusConfig();
  guitionProbeError=spi_bus_initialize(GuitionSpiHost,&host,SPI_DMA_CH_AUTO);
  if(guitionProbeError!=ESP_OK)return false; // Never free another owner's bus.
  guitionProbeOwned=true;
  spi_device_interface_config_t config{};
  config.command_bits=8;config.address_bits=24;config.clock_speed_hz=GuitionProbeHz;config.mode=0;
  config.spics_io_num=TFT_CS;config.queue_size=1;config.flags=SPI_DEVICE_HALFDUPLEX;
  config.cs_ena_pretrans=1;config.cs_ena_posttrans=1;
  guitionProbeError=spi_bus_add_device(GuitionSpiHost,&config,&guitionProbeDevice);
  if(guitionProbeError!=ESP_OK)return guitionProbeCleanup();
  guitionProbeTransaction={};
  // FASTREAD 0B + 00/04/00 on IO0; receive dummy byte + three ID bytes on IO1.
  guitionProbeTransaction.cmd=0x0b;guitionProbeTransaction.addr=0x000400;
  guitionProbeTransaction.rxlength=32;
  guitionProbeTransaction.flags=SPI_TRANS_USE_RXDATA;
  guitionProbeError=spi_device_queue_trans(guitionProbeDevice,&guitionProbeTransaction,pdMS_TO_TICKS(GuitionProbeTimeoutMs));
  if(guitionProbeError!=ESP_OK)return guitionProbeCleanup();
  spi_transaction_t* completed=nullptr;
  guitionProbeError=spi_device_get_trans_result(guitionProbeDevice,&completed,pdMS_TO_TICKS(GuitionProbeTimeoutMs));
  if(guitionProbeError==ESP_OK&&completed!=&guitionProbeTransaction)guitionProbeError=ESP_ERR_INVALID_STATE;
  if(guitionProbeError!=ESP_OK){
    // Do not free/reuse an in-flight SPI transaction. Keep its static resources,
    // leave the backlight off, and keep USB diagnostics available until reboot.
    return false;
  }
  memcpy(guitionId,guitionProbeTransaction.rx_data,sizeof(guitionId));
  return guitionProbeCleanup();
}
static void guitionPanelCleanup(){
  delete lcd;lcd=nullptr;
  // Explicitly owned host: this QSPI object's destructor deletes only its IO.
  delete guitionPanelBus;guitionPanelBus=nullptr;
  if(guitionPanelOwned&&spi_bus_free(GuitionSpiHost)==ESP_OK)guitionPanelOwned=false;
}
static bool guitionPanelBegin(){
  if(lcd||guitionPanelOwned||guitionProbeOwned)return false;
  pinMode(TFT_RST,OUTPUT);digitalWrite(TFT_RST,LOW);delay(10);digitalWrite(TFT_RST,HIGH);delay(120);
  guitionPanelStage="id_probe";
  if(!guitionProbeId()){guitionPanelStage="id_probe_ownership";return false;}
  guitionPanelStage="spi_host";
  auto host=guitionBusConfig();
  if(spi_bus_initialize(GuitionSpiHost,&host,SPI_DMA_CH_AUTO)!=ESP_OK)return false;
  guitionPanelOwned=true;
  esp_lcd_panel_io_spi_config_t io=ESP_PANEL_IO_QSPI_CONFIG_DEFAULT(TFT_CS);
  guitionPanelStage="qspi_allocate";
  guitionPanelBus=new(std::nothrow) ESP_PanelBus_QSPI(io,GuitionSpiHost);
  if(!guitionPanelBus){guitionPanelCleanup();return false;}
  guitionPanelBus->configQspiFreqHz(GuitionPanelHz); // Must precede begin().
  guitionPanelStage="qspi_begin";
  if(!guitionPanelBus->begin()){guitionPanelCleanup();return false;}
  guitionPanelStage="panel_allocate";
  lcd=new(std::nothrow) ESP_PanelLcd_ST77916(guitionPanelBus,16,TFT_RST);
  if(!lcd){guitionPanelCleanup();return false;}
  guitionPanelStage="vendor_commands";
  if(!lcd->configVendorCommands(guition_lcd_init_cmds,sizeof(guition_lcd_init_cmds)/sizeof(*guition_lcd_init_cmds))){guitionPanelCleanup();return false;}
  guitionPanelStage="panel_init";if(!lcd->init()){guitionPanelCleanup();return false;}
  guitionPanelStage="panel_reset";if(!lcd->reset()){guitionPanelCleanup();return false;}
  guitionPanelStage="panel_begin";if(!lcd->begin()){guitionPanelCleanup();return false;}
  guitionPanelStage="panel_invert";if(!lcd->invertColor(true)){guitionPanelCleanup();return false;}
  guitionPanelStage="panel_on";if(!lcd->displayOn()){guitionPanelCleanup();return false;}
  guitionPanelStage="commands_sent";return true; // Physical display remains unverified.
}
void guitionDisplayDiagnostics(){
  Serial.printf("GUITION_DISPLAY beta=1 physical=unverified table=guition-dbadb449 commands=184 qspi_hz=%lu stage=%s touch=%s\n",(unsigned long)GuitionPanelHz,guitionPanelStage,guitionTouchStage);
  Serial.printf("GUITION_ID diagnostic_only=1 hz=%lu error=%d raw=%02X,%02X,%02X,%02X retained=%u\n",(unsigned long)GuitionProbeHz,int(guitionProbeError),guitionId[0],guitionId[1],guitionId[2],guitionId[3],guitionProbeOwned||guitionProbeDevice);
}
