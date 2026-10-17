#pragma once
#include <stddef.h>
namespace dragon {
constexpr unsigned FlashReservePercent=5;
constexpr size_t flashReserve(size_t total){return total/100*FlashReservePercent+(total%100*FlashReservePercent+99)/100;}
inline bool storageFits(size_t total,size_t used,size_t additional){return used<=total&&additional<=total-used&&total-used-additional>=flashReserve(total);}
}
