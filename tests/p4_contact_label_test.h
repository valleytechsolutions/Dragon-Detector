#pragma once
#include <cassert>
#include "p4_contact_label.h"

static void p4ContactLabelTests(){
  char title[64];
  Contact contact{};
  contact.mac[4]=0xab;
  contact.mac[5]=0xcd;
  strcpy(contact.name," Studio Wi-Fi ");
  assert(dragon::p4::contactTitle(title,contact,2,false)==0 && !strcmp(title,"Studio Wi-Fi"));
  dragon::p4::contactTitle(title,contact,2,true);
  assert(!strcmp(title," St..."));
  strcpy(contact.name,"   ");
  assert(dragon::p4::contactTitle(title,contact,2,false)==5 && !strcmp(title,"Hidden AP ABCD"));
  contact.ble=true;
  strcpy(contact.name,"Pocket speaker");
  dragon::p4::contactTitle(title,contact,2,false);
  assert(!strcmp(title,"Pocket speaker"));
  *contact.name=0;
  contact.match={dragon::Family::Ble,dragon::Confidence::None,""};
  assert(!dragon::p4::signatureFamily(contact));
  dragon::p4::contactTitle(title,contact,2,false);
  assert(!strcmp(title,"BLE ABCD"));
  dragon::p4::contactTitle(title,contact,2,true);
  assert(!strcmp(title,"BLE #3"));
  contact.match={dragon::Family::IBeacon,dragon::Confidence::Strong,""};
  assert(!strcmp(dragon::p4::signatureFamily(contact),"iBeacon"));
  auto suffix=dragon::p4::contactTitle(title,contact,2,false);
  assert(!strcmp(title,"iBeacon ABCD"));
  auto mono=[](const char*,size_t length){return int(length)*6;};
  dragon::p4::fitTitle(title,suffix,60,mono);
  assert(!strcmp(title,"iB... ABCD"));
  strcpy(contact.name,"A long advertised BLE name");
  suffix=dragon::p4::contactTitle(title,contact,2,false);
  dragon::p4::fitTitle(title,suffix,72,mono);
  assert(!strcmp(title,"A long ad..."));
  assert(!strcmp(contact.name,"A long advertised BLE name"));
}
