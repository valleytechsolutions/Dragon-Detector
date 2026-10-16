#pragma once
#include <stddef.h>
#include <stdint.h>

namespace dragon::tx {
enum class AttackMode:uint8_t {
  Beacon, RandomBeacons, SsidList, CloneBeacon, Probe, Authentication,
  Deauthentication, Disassociation, ActiveEapol, BadMessage, AssociationSleep,
  SaeCommit, ChannelSwitch, Quiet,
  SwiftPair, FastPair, Samsung, Flipper, AppleJuice, SourApple, BleMix,
  ApSpam, Rickroll, FunnyBeacons,
  IBeacon, EddystoneUid, EddystoneTlm, BTHome, CustomBle,
  Count
};
struct AttackDefinition {const char* name;bool ble,needsAp,needsClient;};
extern const AttackDefinition attackDefinitions[unsigned(AttackMode::Count)];
inline bool bleRotatesAddress(AttackMode mode){return mode>=AttackMode::SwiftPair&&mode<=AttackMode::BleMix;}
struct AttackTarget {
  uint8_t bssid[6]{},client[6]{};
  uint8_t channel=1,destinationChannel=6;
  char ssid[33]="Dragon-Lab";
  bool hasAp=false,hasClient=false;
};
bool unicastAddress(const uint8_t mac[6]);
bool parseMac(const char* text,uint8_t mac[6],bool requireUnicast=true);
bool validAttackTarget(AttackMode mode,const AttackTarget& target);
// Pure packet construction: no radio, files, global state or heap allocation.
// entropy is supplied by the caller; tests can reproduce every output.
size_t buildWifiAttack(uint8_t* out,size_t capacity,AttackMode mode,
                      const AttackTarget& target,const uint8_t source[6],
                      uint16_t sequence,const uint8_t entropy[96]);
size_t buildBleAttack(uint8_t* out,size_t capacity,AttackMode mode,
                     const uint8_t entropy[96]);
size_t copyBleAdvertisement(uint8_t* out,size_t capacity,const uint8_t* data,size_t length);
}

