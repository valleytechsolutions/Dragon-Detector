#pragma once
#include <stdint.h>

enum class P4GattKind : uint8_t { Service, Characteristic, Descriptor };
struct P4GattEntry {
    P4GattKind kind;
    uint16_t handle;
    uint8_t properties;
    char uuid[37];
};
struct P4GattView {
    uint32_t revision;
    bool busy, complete, truncated;
    uint8_t address[6], addressType, count;
    char status[96];
    P4GattEntry entries[64];
};

// A contact's advert can be inspected without connecting. Start is called
// only when the user explicitly requests discovery on the selected contact.
const char* p4GattUnavailable(const uint8_t address[6],uint8_t type);
bool p4GattStart(const uint8_t address[6],uint8_t type);
void p4GattCancel();
void p4GattSnapshot(P4GattView& out);
bool p4GattBusy();
void p4GattRun(bool (*wanted)(),bool radioIdle);
void p4GattPrintStatus();
void p4GattPrintInventory(bool privacy);
