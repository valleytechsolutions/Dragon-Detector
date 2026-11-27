#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace dragon::p4 {
constexpr unsigned LegacyAdvertBytes=31;

struct AdvertField {
  const uint8_t* value;
  uint8_t type,size,occurrence;
};

struct AdvertFields {
  AdvertField items[LegacyAdvertBytes/2]{};
  unsigned count=0;

  const AdvertField* find(uint8_t type,unsigned occurrence) const {
    for(unsigned i=0;i<count;++i)
      if(items[i].type==type && items[i].occurrence==occurrence)return &items[i];
    return nullptr;
  }

  bool parse(const uint8_t* payload,size_t bytes){
    count=0;
    if(bytes>LegacyAdvertBytes || (!payload && bytes))return false;
    for(size_t offset=0;offset<bytes;){
      const uint8_t length=payload[offset++];
      if(!length){
        while(offset<bytes)if(payload[offset++]){count=0;return false;}
        break;
      }
      if(length>bytes-offset){count=0;return false;}
      const uint8_t type=payload[offset];
      uint8_t occurrence=1;
      for(unsigned i=0;i<count;++i)if(items[i].type==type)++occurrence;
      items[count++]={payload+offset+1,type,uint8_t(length-1),occurrence};
      offset+=length;
    }
    return true;
  }
};

enum class FieldChange {Added,Removed,Changed};
template<class Visitor>
unsigned compareFields(const AdvertFields& earlier,const AdvertFields& later,Visitor visit){
  unsigned changes=0;
  for(unsigned i=0;i<later.count;++i){
    const auto& field=later.items[i];
    const auto* before=earlier.find(field.type,field.occurrence);
    if(!before || before->size!=field.size || memcmp(before->value,field.value,field.size)){
      ++changes;
      visit(before?FieldChange::Changed:FieldChange::Added,before,&field);
    }
  }
  for(unsigned i=0;i<earlier.count;++i){
    const auto& field=earlier.items[i];
    if(!later.find(field.type,field.occurrence)){
      ++changes;
      visit(FieldChange::Removed,&field,nullptr);
    }
  }
  return changes;
}
inline const char* advertFieldName(uint8_t type){
  switch(type){
    case 0x01:return "Flags";
    case 0x02:case 0x03:return "16-bit service list";
    case 0x04:case 0x05:return "32-bit service list";
    case 0x06:case 0x07:return "128-bit service list";
    case 0x08:return "Short name";
    case 0x09:return "Local name";
    case 0x0a:return "TX power";
    case 0x12:return "Connection interval";
    case 0x16:case 0x20:case 0x21:return "Service data";
    case 0x19:return "Appearance";
    case 0x1a:return "Advertising interval";
    case 0xff:return "Manufacturer data";
    default:return "AD field";
  }
}
}
