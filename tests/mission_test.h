#include "mission_format.h"
#include "recon_recipe.h"
static void missionTests(){using namespace dragon::mission;
 uint8_t mac[]={2,3,4,5,6,7},record[45];observation(record,0x78563412,mac,-62,11,'a');assert(record[0]==0x12&&record[3]==0x78&&!memcmp(record+12,mac,6)&&record[18]==194&&record[19]==11&&record[20]=='a');for(unsigned i=4;i<12;++i)assert(!record[i]);
 uint8_t ssid[32];for(unsigned i=0;i<32;++i)ssid[i]=i;probe(record,55,mac,-80,6,ssid,32);assert(record[20]==24&&!memcmp(record+21,ssid,24));probe(record,55,mac,-80,6,ssid,3);for(unsigned i=24;i<45;++i)assert(record[i]==0);
 Seen rows[2]{};bool full;assert(accept(rows,2,mac,'b',0,-50,10,full)==1&&!full);assert(!accept(rows,2,mac,'b',0,-51,200,full));assert(accept(rows,2,mac,'b',0,-70,200,full)==2);assert(accept(rows,2,mac,'b',1,-70,201,full)==1);assert(accept(rows,2,mac,'b',1,-70,30201,full)==2);mac[5]++;assert(!accept(rows,2,mac,'b',1,-70,30202,full)&&full);assert(rows[0].mac[5]==7);
 dragon::recipe::Plan plan;const char* text="MODE 0\nMISSION WIFI\nWAIT 8\nMISSION STOP\nTOOL 24 2 1\nSTOP\n";assert(dragon::recipe::parse(text,strlen(text),plan)&&plan.count==6&&plan.duration==10);for(auto bad:{"TOOL 29 2 1","TOOL 1 61 1","WAIT 0","MODE 8","WAIT 3 x","CAPTURE ERASE","LOOP 9","WAIT -1"})assert(!dragon::recipe::parse(bad,strlen(bad),plan));
 puts("PASS recon mission RCN1/PRB1 wire bytes, no-GPS fields, 24-byte SSID compatibility, repeat gate, typed identities and overflow accounting");
}
