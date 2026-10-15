#pragma once
#include "detection.h"
namespace dragon {
enum class ReconMode:uint8_t {Radar,Axon,Alpr,Sky,Wifi,Ble,WifiCapture,BleCapture,Count};
struct ModeDefinition {const char* name;const char* detail;uint8_t radio;bool all;};
inline constexpr ModeDefinition reconModes[]={
 {"Dragon radar","All detector families / balanced radio",0,false},
 {"Axon watch","Axon / TASER signatures on BLE and Wi-Fi",0,false},
 {"ALPR watch","Flock, Raven and ALPR vendor evidence",0,false},
 {"Sky watch","Drone Remote ID / BLE and Wi-Fi",0,false},
 {"Wi-Fi survey","All Wi-Fi contacts / continuous reception",1,true},
 {"BLE survey","All BLE advertisements / passive reception",2,true},
 {"Wi-Fi capture","PCAP recorder / channels 1-11",1,true},
 {"BLE capture","Raw advertisement CSV / no connection traffic",2,true}
};
inline bool modeAccepts(ReconMode mode,Family family,bool ble){
 switch(mode){
 case ReconMode::Axon:return family==Family::Axon;
 case ReconMode::Alpr:return family==Family::Flock||family==Family::Raven||family==Family::Alpr;
 case ReconMode::Sky:return family==Family::Drone;
 case ReconMode::Wifi:case ReconMode::WifiCapture:return !ble;
 case ReconMode::Ble:case ReconMode::BleCapture:return ble;
 default:return true;
 }
}
inline bool validMode(unsigned mode){return mode<unsigned(ReconMode::Count);}
}
