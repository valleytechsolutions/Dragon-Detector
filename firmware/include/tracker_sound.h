#pragma once
#include <stddef.h>
#include <stdint.h>

namespace dragon::tracker {
enum class Protocol:uint8_t {AirTag,Fmna,Dult};
struct Sound {Protocol protocol;const char* name;const char* service;const char* characteristic;uint8_t command[3];uint8_t length;bool responses;};
constexpr Sound sounds[]={
  {Protocol::AirTag,"AirTag","7dfc9000-7d1c-4951-86aa-8d9728f8d66c","7dfc9001-7d1c-4951-86aa-8d9728f8d66c",{0xaf,0,0},1,false},
  {Protocol::Fmna,"Find My accessory","fd44","4f860003-943b-49ef-bed4-2f730304427a",{1,0,3},3,true},
  {Protocol::Dult,"DULT","15190001-12f4-c226-88ed-2ac5579f2a85","8e0c0001-1d68-fb92-bf61-48377421680e",{0,3,0},2,true}
};
enum class Response:uint8_t {None,Received,Accepted,Rejected,Completed};
inline Response response(Protocol protocol,const uint8_t* data,size_t length,uint16_t& status) {
  if(!data||!length)return Response::None;
  if(protocol!=Protocol::Dult)return Response::Received;
  if(length==2&&data[0]==3&&data[1]==3)return Response::Completed;
  if(length==6&&data[0]==2&&data[1]==3&&data[2]==0&&data[3]==3){status=uint16_t(data[4])|(uint16_t(data[5])<<8);return status?Response::Rejected:Response::Accepted;}
  return Response::None;
}
}
