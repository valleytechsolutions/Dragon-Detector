#pragma once
// Original MIT implementation. Fixed little-endian DDG1 interchange format.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
namespace dragon { namespace lab {
constexpr unsigned MaxAttributes=128, MaxValue=512, MaxAdvertisement=512;
enum Kind:uint8_t {Service,Characteristic,Descriptor};
enum Flags:uint8_t {ReadFailed=1,ValueTruncated=2,NotRead=4};
#pragma pack(push,1)
struct Attribute {uint8_t kind,properties,flags,reserved;uint16_t parent,handle,length;char uuid[37];uint8_t value[MaxValue];};
struct Profile {char magic[4];uint16_t version,count;char name[48];uint8_t mac[6],addressType,flags;uint16_t advertisementLength,scanBoundary;uint8_t advertisement[MaxAdvertisement];Attribute attributes[MaxAttributes];};
#pragma pack(pop)
constexpr size_t HeaderSize=offsetof(Profile,attributes);
inline size_t bytes(const Profile& p){return HeaderSize+size_t(p.count)*sizeof(Attribute);}
inline uint32_t checksum(const uint8_t* p,size_t n){uint32_t c=0xffffffff;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return c^0xffffffff;}
inline bool uuidValid(const char* u){size_t n=0;while(n<37&&u[n])++n;if(n!=4&&n!=8&&n!=36)return false;for(size_t i=0;i<n;++i){char c=u[i];if(n==36&&(i==8||i==13||i==18||i==23)){if(c!='-')return false;}else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}return true;}
inline void uuidCanonical(const char* u,char out[37]){size_t n=strlen(u);if(n==4)snprintf(out,37,"0000%s-0000-1000-8000-00805f9b34fb",u);else if(n==8)snprintf(out,37,"%s-0000-1000-8000-00805f9b34fb",u);else snprintf(out,37,"%s",u);for(unsigned i=0;out[i];++i)if(out[i]>='A'&&out[i]<='F')out[i]+='a'-'A';}
inline bool uuidEqual(const char* a,const char* b){char x[37],y[37];uuidCanonical(a,x);uuidCanonical(b,y);return !strcmp(x,y);}
inline bool declaration(const char* u){return uuidEqual(u,"2800")||uuidEqual(u,"2801")||uuidEqual(u,"2802")||uuidEqual(u,"2803");}
inline bool advertisementValid(const uint8_t* p,size_t n){for(size_t i=0;i<n;){uint8_t k=p[i++];if(!k){for(;i<n;++i)if(p[i])return false;return true;}if(k>n-i)return false;i+=k;}return true;}
inline bool valid(const Profile& p,size_t size){
 if(size<HeaderSize||memcmp(p.magic,"DDG1",4)||p.version!=1||p.count>MaxAttributes||size!=bytes(p)||p.addressType>3||p.flags>1||!memchr(p.name,0,sizeof(p.name))||p.advertisementLength>MaxAdvertisement||p.scanBoundary>p.advertisementLength)return false;
 if(!advertisementValid(p.advertisement,p.scanBoundary)||!advertisementValid(p.advertisement+p.scanBoundary,p.advertisementLength-p.scanBoundary))return false;
 unsigned services=0,chars=0;
 for(unsigned i=0;i<p.count;++i){const auto& a=p.attributes[i];if(a.kind>Descriptor||a.reserved||a.flags>7||a.length>MaxValue||!uuidValid(a.uuid))return false;
  if(a.kind==Service){if(a.parent!=0xffff||a.length||a.properties||a.flags||++services>16)return false;}
  else{if(a.parent>=i||p.attributes[a.parent].kind!=(a.kind==Characteristic?Service:Characteristic))return false;if(a.kind==Characteristic&&++chars>64)return false;if(a.kind==Descriptor&&declaration(a.uuid))return false;}
 }
 return true;
}
inline bool safeName(const char* s){size_t n=0;for(;s[n];++n)if(n>=32||!((s[n]>='a'&&s[n]<='z')||(s[n]>='A'&&s[n]<='Z')||(s[n]>='0'&&s[n]<='9')||s[n]=='-'||s[n]=='_'))return false;return n>0;}
inline const char* addressName(uint8_t type,const uint8_t* mac){if(!(type&1))return type==2?"Public identity":"Public";switch(mac[0]>>6){case 3:return "Random static";case 1:return "Resolvable private";case 0:return "Non-resolvable private";default:return "Reserved random type";}}
inline void init(Profile& p,const char* name){memset(&p,0,sizeof(p));memcpy(p.magic,"DDG1",4);p.version=1;snprintf(p.name,sizeof(p.name),"%s",name);}
inline Attribute* append(Profile& p,Kind k,unsigned parent,const char* uuid,uint16_t handle=0){if(p.count>=MaxAttributes){p.flags|=1;return nullptr;}auto& a=p.attributes[p.count++];a={};a.kind=k;a.parent=parent;a.handle=handle;if(uuid[0]=='0'&&uuid[1]=='x')uuid+=2;snprintf(a.uuid,sizeof(a.uuid),"%s",uuid);return &a;}
inline void demo(Profile& p){init(p,"Dragon Lab Demo");auto* s=append(p,Service,0xffff,"180f");(void)s;auto* c=append(p,Characteristic,0,"2a19");c->properties=0x12;c->length=1;c->value[0]=87;
 append(p,Service,0xffff,"dd980001-7114-452b-8e26-5f44c65a4b90");c=append(p,Characteristic,2,"dd980002-7114-452b-8e26-5f44c65a4b90");c->properties=0x1e;c->length=10;memcpy(c->value,"Dragon Lab",10);auto* d=append(p,Descriptor,3,"2901");d->length=13;memcpy(d->value,"Lab loopback",13);
}
struct Difference {unsigned added=0,removed=0,changed=0;};
// UUID + parent UUID + occurrence index preserve duplicate instances.
inline bool samePath(const Profile& a,unsigned ai,const Profile& b,unsigned bi){const auto& x=a.attributes[ai];const auto& y=b.attributes[bi];if(x.kind!=y.kind||!uuidEqual(x.uuid,y.uuid))return false;if(x.kind!=Service&&!samePath(a,x.parent,b,y.parent))return false;unsigned ax=0,bx=0;for(unsigned i=0;i<ai;++i)if(a.attributes[i].kind==x.kind&&a.attributes[i].parent==x.parent&&uuidEqual(a.attributes[i].uuid,x.uuid))++ax;for(unsigned i=0;i<bi;++i)if(b.attributes[i].kind==y.kind&&b.attributes[i].parent==y.parent&&uuidEqual(b.attributes[i].uuid,y.uuid))++bx;return ax==bx;}
inline Difference compare(const Profile& a,const Profile& b){Difference d;bool seen[MaxAttributes]{};for(unsigned i=0;i<a.count;++i){unsigned j=0;for(;j<b.count;++j)if(samePath(a,i,b,j))break;if(j==b.count){++d.removed;continue;}seen[j]=true;auto& x=a.attributes[i];auto& y=b.attributes[j];if(x.properties!=y.properties||x.flags!=y.flags||x.length!=y.length||memcmp(x.value,y.value,x.length))++d.changed;}for(unsigned j=0;j<b.count;++j)if(!seen[j])++d.added;return d;}
}}
