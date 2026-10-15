#pragma once
#include "ble_profile.h"
#include <memory>
static void bleProfileTests(){using namespace dragon::lab;
 auto a=std::unique_ptr<Profile>(new Profile{}),b=std::unique_ptr<Profile>(new Profile{});demo(*a);assert(valid(*a,bytes(*a)));assert(HeaderSize==580&&sizeof(Attribute)==559);
 assert(checksum((const uint8_t*)"123456789",9)==0xcbf43926);*b=*a;
 auto diff=compare(*a,*b);assert(!diff.added&&!diff.removed&&!diff.changed);
 b->attributes[1].value[0]=40;diff=compare(*a,*b);assert(diff.changed==1&&!diff.added&&!diff.removed);
 b->count--;diff=compare(*a,*b);assert(diff.removed==1);assert(!valid(*a,bytes(*a)-1));
 *b=*a;b->count=129;assert(!valid(*b,sizeof(Profile)));*b=*a;b->attributes[3].parent=3;assert(!valid(*b,bytes(*b)));
 *b=*a;b->attributes[4].parent=0;assert(!valid(*b,bytes(*b)));*b=*a;b->attributes[1].length=513;assert(!valid(*b,bytes(*b)));
 *b=*a;memset(b->name,'x',48);assert(!valid(*b,bytes(*b)));*b=*a;strcpy(b->attributes[0].uuid,"hello");assert(!valid(*b,bytes(*b)));
 *b=*a;b->advertisementLength=3;b->scanBoundary=3;memcpy(b->advertisement,"\x02\x01\x06",3);assert(valid(*b,bytes(*b)));b->advertisement[0]=4;assert(!valid(*b,bytes(*b)));
 assert(safeName("test-Profile_1")&&!safeName("")&&!safeName("../badge")&&!safeName("a.ddg")&&!safeName("a/b"));
 assert(uuidEqual("180f","0000180F-0000-1000-8000-00805F9B34FB"));assert(declaration("2803")&&!declaration("2902"));
 *b=*a;strcpy(b->attributes[4].uuid,"2803");assert(!valid(*b,bytes(*b)));*b=*a;strcpy(b->attributes[0].uuid,"0000180F-0000-1000-8000-00805F9B34FB");assert(compare(*a,*b).changed==0&&compare(*a,*b).added==0);
 uint8_t mac[6]={0xc0};assert(!strcmp(addressName(1,mac),"Random static"));mac[0]=0x40;assert(!strcmp(addressName(1,mac),"Resolvable private"));assert(!strcmp(addressName(0,mac),"Public"));
 // Same UUID in a second service instance must remain a separate path.
 init(*b,"duplicate");append(*b,Service,0xffff,"180f");append(*b,Characteristic,0,"2a19");append(*b,Service,0xffff,"180f");append(*b,Characteristic,2,"2a19");assert(valid(*b,bytes(*b)));assert(!samePath(*b,1,*b,3));
 // Every independent byte mutation is safe to validate; corruption is rejected by CRC on disk.
 const size_t size=bytes(*a);uint32_t crc=checksum((uint8_t*)a.get(),size);for(size_t i=0;i<size;++i){*b=*a;((uint8_t*)b.get())[i]^=0xff;assert(checksum((uint8_t*)b.get(),size)!=crc);valid(*b,size);}
 puts("PASS BLE Lab: bounds, UUIDs, address types, profile hierarchy, CRC, comparison");
}
