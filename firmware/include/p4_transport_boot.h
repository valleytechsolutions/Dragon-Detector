#pragma once
#include <stdint.h>

namespace dragon::p4 {
enum class BootTicket : uint32_t {
    Clear=0, FirstActive=0xD4B00711, RetryPending=0xD4B00722,
    RetryActive=0xD4B00733, Blocked=0xD4B00744
};
inline BootTicket beginTransport(uint32_t retained,bool coldStart){
    if(coldStart)return BootTicket::FirstActive;
    switch(BootTicket(retained)){
        case BootTicket::RetryPending:case BootTicket::FirstActive:return BootTicket::RetryActive;
        case BootTicket::RetryActive:case BootTicket::Blocked:return BootTicket::Blocked;
        default:return BootTicket::FirstActive;
    }
}
inline bool retryBeforeInteraction(BootTicket attempt,bool vendorStarted,bool failed,bool timedOut){
    return attempt==BootTicket::FirstActive&&vendorStarted&&(failed||timedOut);
}
constexpr unsigned transportBootLimitMs = 18000;
constexpr unsigned cmd0InvalidLimit = 3;

inline int enumerationClock(unsigned previousAttempts, int requestedKhz) {
    return previousAttempts && requestedKhz > 20000 ? 20000 : requestedKhz;
}
inline bool companionEnumerated(bool enumerationSucceeded, bool ioCard, unsigned functions) {
    return enumerationSucceeded && ioCard && functions >= 2;
}
}
