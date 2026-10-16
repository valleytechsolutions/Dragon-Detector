#include "network_packets.h"
static void networkTests(){using namespace dragon::net;uint32_t value;
 assert(ipv4("192.168.1.2",value)&&value==0xc0a80102);
 for(auto s:{"","1","1.2.3","1.2.3.4.","1.2.3.256","1.2.-1.4","1.2.3.0000","1.2.3.4junk"})assert(!ipv4(s,value));
 assert(host(0xc0a80101,0xc0a80150,0xffffff00));assert(!host(0xc0a80201,0xc0a80150,0xffffff00));assert(!host(0xc0a801ff,0xc0a80150,0xffffff00));assert(!host(0xc0a80100,0xc0a80150,0xffffff00));assert(!host(0xc0a80150,0xc0a80150,0xffffff00));assert(!host(0xc0a80101,0xc0a80150,0xff00ff00));
 assert(range(0xc0a80101,0xc0a801fe,0xc0a80150,0xffffff00));assert(!range(0xc0a80101,0xc0a80201,0xc0a80150,0xffff0000));assert(!range(0xc0a80102,0xc0a80101,0xc0a80150,0xffffff00));
 uint8_t packet[36]{};packet[0]=0x45;put16(packet+2,36);packet[8]=64;packet[9]=1;put16(packet+10,checksum(packet,20));echo(packet+20,123,1,0x12345678);
 assert(!reply(packet,36,123,1,0x12345678));packet[20]=0;packet[22]=packet[23]=0;put16(packet+22,checksum(packet+20,16));assert(reply(packet,36,123,1,0x12345678));
 assert(!reply(packet,36,123,2,0x12345678));assert(!reply(packet,36,123,1,0x12345679));for(unsigned n=0;n<36;++n)assert(!reply(packet,n,123,1,0x12345678));
 for(unsigned i=0;i<36;++i){packet[i]^=1;assert(!reply(packet,36,123,1,0x12345678));packet[i]^=1;}
 uint32_t rng=0x92718;uint8_t fuzz[256];for(unsigned i=0;i<25000;++i){for(auto& b:fuzz){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;b=rng;}volatile bool accepted=reply(fuzz,rng%256,123,1,0x12345678);(void)accepted;}
 puts("PASS: local subnet bounds, echo identity/checksum/nonce, truncation and 25000 ICMP fuzz cases");
}
