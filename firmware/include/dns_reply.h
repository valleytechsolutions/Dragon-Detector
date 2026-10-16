#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace dragon {
// Bounded one-question IPv4 captive-DNS responder. Refuse compressed/malformed questions.
inline size_t captiveDns(uint8_t* p,size_t n,size_t capacity){if(!p||n<17||n>capacity||p[2]&0x80||p[2]&0x78||p[4]||p[5]!=1||p[6]||p[7]||p[8]||p[9])return 0;size_t at=12;unsigned labels=0;while(at<n&&p[at]){size_t z=p[at++];if(z>63||z>n-at||++labels>127)return 0;at+=z;}if(at>=n||n-++at<4)return 0;bool ipv4=p[at]==0&&p[at+1]==1&&p[at+2]==0&&p[at+3]==1;at+=4;if(capacity-at<(ipv4?16:0))return 0;p[2]=0x81;p[3]=0x80;p[6]=0;p[7]=ipv4?1:0;p[8]=p[9]=p[10]=p[11]=0;if(ipv4){const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};memcpy(p+at,answer,sizeof(answer));at+=sizeof(answer);}return at;}
}
