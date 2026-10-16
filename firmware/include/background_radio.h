#pragma once
// Bulk USB I/O cooperatively drains RX without injecting alert text into binary
// transfers. Alerts still reach history, the screen and the audio scheduler.
extern unsigned radioTransferDepth;
void radioTransferTick();
struct RadioTransfer {
  RadioTransfer(){++radioTransferDepth;}
  ~RadioTransfer(){--radioTransferDepth;}
  RadioTransfer(const RadioTransfer&)=delete;
  RadioTransfer& operator=(const RadioTransfer&)=delete;
};
