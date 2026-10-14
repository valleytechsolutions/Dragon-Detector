// Independent protocol fixtures: SIG UUID representations, not captured IDs.
static std::vector<uint8_t> serviceField(uint16_t uuid,unsigned width,uint8_t type,const std::vector<uint8_t>& body={}){
  std::vector<uint8_t> d;
  if(width==16)d={0xfb,0x34,0x9b,0x5f,0x80,0,0,0x80,0,0x10,0,0,uint8_t(uuid),uint8_t(uuid>>8),0,0};
  else{d={uint8_t(uuid),uint8_t(uuid>>8)};d.resize(width);}
  std::vector<uint8_t> p{uint8_t(d.size()+body.size()+1),type};append(p,d);append(p,body);return p;
}
static void serviceEncodingTests(){
  const uint16_t ids[]={0xfc81,0xfe6b,0xfe6c,0x3080,0x3081,0x3082,0x3083,0xfd44,0xfd5a,0xfeed,0xfeec,0x3100,0x3200,0x3300,0x3400,0x3500,0xfd5f,0xfeb7,0xfeb8,0xfe45,0xfeaa,0xfffa,0x1101};
  for(auto id:ids){auto expected=ble(serviceField(id,2,3));assert(id==0xfe45 || expected.confidence!=Confidence::None);
    for(unsigned width:{2u,4u,16u})for(unsigned kind:{0u,1u,2u}){
      uint8_t type=kind==2?(width==2?0x16:width==4?0x20:0x21):(width==2?2:width==4?4:6)+kind;
      auto field=serviceField(id,width,type);auto result=ble(field);
      assert(result.family==expected.family&&result.confidence==expected.confidence);
      if(kind<2){auto multi=serviceField(0x180f,width,type);multi[0]+=width;multi.insert(multi.end(),field.begin()+2,field.end());assert(ble(multi).family==expected.family);}
    }
  }
  for(unsigned width:{2u,4u,16u}){
    uint8_t type=width==2?0x16:width==4?0x20:0x21;
    auto p=serviceField(0x3082,width,type);append(p,{3,0x19,0,0x86});assert(ble(p).family==Family::Flipper&&ble(p).confidence==Confidence::Medium);
    std::vector<uint8_t> body(21);body[0]=0x40;auto hub=serviceField(0xfeaa,width,type,body);assert(ble(hub).family==Family::GoogleTag&&ble(hub).confidence==Confidence::Strong);
    auto pair=serviceField(0xfe2c,width,type);assert(parseBle(pair.data(),pair.size()).fastPair);
    auto snap=serviceField(0xfe45,width,type);assert(ble(snap).confidence==Confidence::None);append(snap,{3,0xff,0xc2,3});assert(ble(snap).family==Family::Meta&&ble(snap).confidence==Confidence::Medium);
    ODID_BasicID_data id;odid_initBasicIDData(&id);id.IDType=ODID_IDTYPE_SERIAL_NUMBER;id.UAType=ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;strcpy(id.UASID,"TEST-UUID-WIDTH");
    ODID_BasicID_encoded encoded;assert(encodeBasicIDMessage(&encoded,&id)==ODID_SUCCESS);
    std::vector<uint8_t> rid{0x0d,7};rid.insert(rid.end(),(uint8_t*)&encoded,(uint8_t*)&encoded+25);
    auto advert=serviceField(0xfffa,width,type,rid);auto decoded=parseBle(advert.data(),advert.size());assert(decoded.match.family==Family::Drone&&decoded.drone.hasId&&!strcmp(decoded.drone.id,"TEST-UUID-WIDTH"));
    rid.pop_back();assert(ble(serviceField(0xfffa,width,type,rid)).confidence==Confidence::Low);
  }
  auto custom=serviceField(0xfc81,16,7);custom[2]^=1;assert(ble(custom).confidence==Confidence::None);
  auto wider=serviceField(0xfc81,4,5);wider[4]=1;assert(ble(wider).confidence==Confidence::None);
  auto wider128=serviceField(0xfc81,16,7);wider128[16]=1;assert(ble(wider128).confidence==Confidence::None);
  for(auto p:std::vector<std::vector<uint8_t>>{{4,4,0,0,0},{4,5,0,0,0},{4,0x20,0,0,0},{5,0x21,0,0,0,0}})assert(!parseBle(p.data(),p.size()).valid);
  auto boundary=serviceField(0xfc81,4,5);assert(!parseBle(boundary.data(),boundary.size(),4).valid);
  puts("PASS: 23 service clues across 16/32/base-128 UUID lists and data; payload offsets, custom-UUID exclusions and malformed boundaries");
}
