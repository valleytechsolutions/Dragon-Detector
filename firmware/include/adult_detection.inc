// Public BLE protocol identifiers; independently implemented passive matching.
// Sources and boundaries: docs/ADULT-MODE.md. Included inside namespace dragon.
struct AdultEvidence {
  bool uart=false,lovense=false,wevibe=false,satisfyer=false;
  void service16(uint16_t id){uart|=id==0xfff0;}
  void service128(const uint8_t* p){
    static const uint8_t nordic[]={0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,1,0,0x40,0x6e};
    static const uint8_t we[]={0,0,0,0,0,0,0,0xb0,0,0x40,0x51,0x04,3,0xbb,0,0xf0};
    static const uint8_t sat[]={0xe8,0x80,0x9d,0xc9,0xeb,0x47,0x6e,0x8a,0xc7,0x47,0xe7,0xc5,0,0x15,0x36,0x51};
    // Lovense's documented generation-three service varies only in X/Y/Z.
    static const uint8_t lv[]={0x53,0x56,0x4c,0x0e,0x92,0xa6,0xd5,0xbb,0xd4,0x4b};
    uart|=!memcmp(p,nordic,16);
    wevibe|=!memcmp(p,we,16);
    satisfyer|=!memcmp(p,sat,16);
    lovense|=!memcmp(p,lv,10)&&(p[10]==0x23||p[10]==0x24)&&p[11]==0&&p[12]==1&&p[13]==0&&p[14]==0x30&&(p[15]>>4==4||p[15]>>4==5);
  }
  static bool brandName(const char* name){
    const char* suffix=nullptr;
    if(!strncmp(name,"LVS-",4))suffix=name+4;
    else if(!strncmp(name,"LOVE-",5))suffix=name+5;
    if(!suffix||!*suffix)return false;
    for(const char* p=suffix;*p;++p)if(!isalnum((unsigned char)*p))return false;
    return true;
  }
  Match match(const char* name)const{
    if(satisfyer)return {Family::Adult,Confidence::Medium,"Satisfyer control service advertised; model/use/owner unknown"};
    if(wevibe)return {Family::Adult,Confidence::Medium,"We-Vibe service advertised; model/use/owner unknown"};
    if(lovense)return {Family::Adult,Confidence::Medium,"Lovense custom service advertised; model/use/owner unknown"};
    if(brandName(name))return {Family::Adult,uart?Confidence::Medium:Confidence::Low,uart?"Lovense-style name + UART service; spoofable, owner/use unknown":"Lovense-style advertised name only; unverified"};
    // Generic Nordic UART / FFF0 services and names such as 'Nova' or 'Sync'
    // are used by unrelated devices, so cannot identify adult products.
    return {Family::Ble,Confidence::None,"BLE advertisement"};
  }
};
