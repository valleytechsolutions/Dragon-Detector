#pragma once
#include "detection.h"

namespace dragon {
// Stable family colors across every theme. Dark callout plates and leader
// outlines preserve contrast on the bright Dragon Ball field as well.
constexpr uint32_t RadarLabelBackground=0x06110d;
constexpr uint32_t FamilyColors[]={
  0xced4da, // Wi-Fi
  0xc3a6ff, // BLE
  0xff7676, // Flock
  0xff6699, // Axon / TASER
  0x76d7ea, // Meta glasses
  0xffe45e, // Wi-Fi Pineapple
  0xfc8deb, // Pwnagotchi
  0xb8f781, // nyanBOX
  0xd1b3ff, // HaleHound
  0x57e6c2, // Find My / DULT tracker
  0xffa6c9, // Skimmer
  0xffa040, // Flipper Zero
  0xd9c29f, // Biscuit
  0x7ab8ff, // Samsung SmartTag
  0xe7ec87, // Raven
  0x6beda8, // Google Find Hub
  0x61d6ff, // Tile
  0x7ce0d5, // Ring
  0xd0dfff, // Camera vendor
  0xf4c47b, // ALPR vendor
  0xbaf251, // Drone Remote ID
  0xdda0e6, // iBeacon
  0xff8f91, // Deauthentication burst
  0xffc3a0, // SSID conflict
  0xa9b2ff, // BLE spam
  0xfff4b5  // Dragon fox beacon
};
static_assert(sizeof(FamilyColors)/sizeof(FamilyColors[0])==unsigned(Family::Count),"Every family needs its own radar color");
inline uint32_t familyColor(Family family){return unsigned(family)<unsigned(Family::Count)?FamilyColors[unsigned(family)]:0xffffff;}
}
