#include "badusb_mode.h"
#if DRAGON_BADUSB
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_private/periph_ctrl.h>
#include <soc/periph_defs.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/usb_serial_jtag_reg.h>
#include <soc/usb_pins.h>
#include <string.h>

namespace {
constexpr uint32_t KeyboardRequest=0x4b42444eu;
// RTC_NOINIT survives esp_restart() but not power loss. It is cleared on every
// boot before use, so a crash or reset in keyboard mode boots back to serial.
RTC_NOINIT_ATTR uint32_t request;
RTC_NOINIT_ATTR char requestScript[33];
bool keyboard=false;
char script[33]{};

bool otgOwnsPins(){return READ_PERI_REG(RTC_CNTL_USB_CONF_REG)&(RTC_CNTL_SW_HW_USB_PHY_SEL|RTC_CNTL_SW_USB_PHY_SEL);}
// Mirrors the Arduino core's own OTG -> serial/JTAG switch (used for its
// bootloader restart): stop OTG, select the internal serial/JTAG PHY, then
// briefly drive D-/D+ low so the host re-enumerates the serial device.
void serialJtagPins(){
  periph_module_reset(PERIPH_USB_MODULE);periph_module_disable(PERIPH_USB_MODULE);
  CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG,RTC_CNTL_SW_HW_USB_PHY_SEL|RTC_CNTL_SW_USB_PHY_SEL|RTC_CNTL_USB_PAD_ENABLE);
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG,USB_SERIAL_JTAG_PHY_SEL);
  CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG,USB_SERIAL_JTAG_USB_PAD_ENABLE);
  pinMode(USBPHY_DM_NUM,OUTPUT_OPEN_DRAIN);pinMode(USBPHY_DP_NUM,OUTPUT_OPEN_DRAIN);
  digitalWrite(USBPHY_DM_NUM,LOW);digitalWrite(USBPHY_DP_NUM,LOW);delay(20);
  SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG,USB_SERIAL_JTAG_USB_PAD_ENABLE);
}
}

namespace dragon::badusb {
void bootMode(){
  const bool requested=request==KeyboardRequest&&esp_reset_reason()==ESP_RST_SW;
  if(requested){memcpy(script,requestScript,sizeof(script));script[sizeof(script)-1]=0;}
  request=0;requestScript[0]=0;keyboard=requested;
  if(!keyboard&&otgOwnsPins())serialJtagPins();
}
bool keyboardMode(){return keyboard;}
const char* keyboardScript(){return script;}
void restartIntoKeyboard(const char* name){
  strlcpy(requestScript,name?name:"",sizeof(requestScript));request=KeyboardRequest;
  delay(100);esp_restart();
}
void restartIntoSerial(){
  request=0;requestScript[0]=0;
  serialJtagPins();esp_restart();
}
}
#else
namespace dragon::badusb {
void bootMode(){}
bool keyboardMode(){return false;}
const char* keyboardScript(){return "";}
void restartIntoKeyboard(const char*){for(;;){}}
void restartIntoSerial(){for(;;){}}
}
#endif
