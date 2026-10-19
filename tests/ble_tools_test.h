#include "ibeacon.h"
#include "tracker_sound.h"
static void bleToolsTests(){
 using namespace dragon;
 ibeacon::Identity value,parsed;uint8_t data[64],small[29];memset(small,0xa5,sizeof(small));
 assert(ibeacon::parseUuid("00112233-4455-6677-8899-aAbBcCdDeEfF",value.uuid));char uuid[37];ibeacon::uuidText(value,uuid);assert(!strcmp(uuid,"00112233-4455-6677-8899-aabbccddeeff"));
 value.major=0x1234;value.minor=0xabcd;value.power=-128;assert(ibeacon::encode(data,30,value)==30);assert(data[25]==0x12&&data[26]==0x34&&data[27]==0xab&&data[28]==0xcd&&data[29]==128);
 assert(ibeacon::decode(data,30,parsed)&&parsed.major==value.major&&parsed.minor==value.minor&&parsed.power==-128&&!memcmp(parsed.uuid,value.uuid,16));
 assert(!ibeacon::encode(small,29,value));for(auto x:small)assert(x==0xa5);
 for(unsigned n=0;n<30;++n)assert(!ibeacon::decode(data,n,parsed));assert(!ibeacon::decode(nullptr,30,parsed));
 uint8_t unchanged[16];memcpy(unchanged,value.uuid,16);assert(!ibeacon::parseUuid("00112233-4455-6677-8899-aabbccddeefg",value.uuid));assert(!memcmp(unchanged,value.uuid,16));
 data[30]=7;assert(!ibeacon::decode(data,31,parsed));data[30]=0;assert(ibeacon::decode(data,31,parsed));data[3]=25;assert(!ibeacon::decode(data,30,parsed));
 using namespace tracker;uint16_t status=999;uint8_t accepted[]={2,3,0,3,0,0},rejected[]={2,3,0,3,1,0},done[]={3,3};
 assert(response(Protocol::Dult,accepted,6,status)==Response::Accepted&&status==0);
 assert(response(Protocol::Dult,rejected,6,status)==Response::Rejected&&status==1);
 assert(response(Protocol::Dult,done,2,status)==Response::Completed);
 for(unsigned n=0;n<6;++n)assert(response(Protocol::Dult,accepted,n,status)==Response::None);
 accepted[2]=1;assert(response(Protocol::Dult,accepted,6,status)==Response::None);assert(response(Protocol::Fmna,done,2,status)==Response::Received);
 assert(sounds[0].command[0]==0xaf&&sounds[0].length==1&&!sounds[0].responses);assert(sounds[1].length==3&&sounds[1].command[2]==3&&sounds[1].responses);assert(sounds[2].length==2&&sounds[2].command[0]==0&&sounds[2].command[1]==3);
 puts("PASS configurable iBeacon wire format and tracker response/result boundaries");
}
