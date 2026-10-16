#include "notebook.h"
#include "vendor_registry.h"
#include "flash_budget.h"
static void notebookTests(){
 using namespace dragon;using namespace notebook;
 assert(flashReserve(100)==21&&flashReserve(101)==22&&storageFits(100,78,1)&&!storageFits(100,79,1)&&!storageFits(100,101,0)&&!storageFits(100,1,size_t(-1)));
 Advert rows[2];uint8_t mac[6]={0x58,0xb9,0x65,1,2,3};uint8_t data[]={2,1,6,2,9,'X'};
 assert(observeAdvert(rows,2,mac,1,-60,data,6,3,10));assert(rows[0].count==1&&rows[0].changes==0);
 data[5]='Y';assert(observeAdvert(rows,2,mac,1,-61,data,6,3,11)&&rows[0].changes==0); // scan response only
 data[2]=5;assert(observeAdvert(rows,2,mac,1,-60,data,6,3,12)&&rows[0].changes==1);
 assert(!observeAdvert(rows,2,mac,1,-60,data,6,3,11));assert(!observeAdvert(rows,2,mac,4,-60,data,6,3,13));
 assert(observeAdvert(rows,2,mac,0,-60,data,6,3,13)&&rows[1].used&&rows[1].type==0); // address type is part of identity
 mac[5]++;assert(observeAdvert(rows,2,mac,0,-60,data,6,3,14)&&rows[0].mac[5]==4); // oldest replacement
 assert(!observeAdvert(rows,2,mac,0,-60,data,5,3,15));
 uint8_t axon[]={0,0x25,0xdf,1,2,3},apple[]={0x58,0xb9,0x65,1,2,3};
 assert(vendorRegistryCount()==40333);assert(strstr(registeredVendor(axon),"Axon"));assert(strstr(registeredVendor(apple),"Apple"));
 assert(!registeredVendor(apple,true,1)&&!registeredVendor(apple,true,3));assert(registeredVendor(apple,true,2));
 apple[0]|=2;assert(!registeredVendor(apple));assert(!registeredVendor(nullptr));
 uint32_t rng=12;uint8_t fuzz[540];for(unsigned i=0;i<25000;++i){for(auto& b:fuzz){rng=rng*1664525+1013904223;b=rng>>24;}size_t n=rng%sizeof(fuzz),boundary=(rng>>10)%(n+2);observeAdvert(rows,2,mac,rng%5,-60,fuzz,n,boundary,i);}
 puts("PASS notebook payload completeness, identity, eviction, scan-response churn, 25000 fuzz inputs; IEEE lookup and randomized-address rejection");
}
