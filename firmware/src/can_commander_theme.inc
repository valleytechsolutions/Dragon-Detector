// CAN-inspired instrument graphics; no CAN transceiver or vehicle telemetry.
extern lv_img_dsc_t radar_logo_can_commander,watch_logo_can_commander;
static constexpr unsigned CanCommanderTheme=35,CanCommanderWatch=38;
static constexpr uint32_t CanBlack=0x02040c,CanPanel=0x101d29,CanSteel=0xc8d2dc,CanBlue=0x72c5e8;
#if SCREEN_RES>=400
static const lv_font_t *CanSmall=&lv_font_montserrat_14,*CanTitle=&lv_font_montserrat_20,*CanDigits=&lv_font_montserrat_40;
#elif SCREEN_RES>=360
static const lv_font_t *CanSmall=&lv_font_montserrat_10,*CanTitle=&lv_font_montserrat_16,*CanDigits=&lv_font_montserrat_32;
#else
static const lv_font_t *CanSmall=&lv_font_montserrat_8,*CanTitle=&lv_font_montserrat_10,*CanDigits=&lv_font_montserrat_22;
#endif
static lv_point_t canPoint(float turns,int radius){
  float a=turns*6.28318530718f;
  return {lv_coord_t(CENTER+sinf(a)*S(radius)),lv_coord_t(CENTER-cosf(a)*S(radius))};
}
static void canBusRim(lv_draw_ctx_t* ctx,uint32_t phase,bool active){
  capsuleRing(ctx,173,0x435b6d,2);capsuleRing(ctx,168,0x172b3c,1);
  capsuleRing(ctx,159,0x2d4f64,1);capsuleRing(ctx,153,0x586977,1);
  for(int i=0;i<8;++i){
    float turn=(i+.5f)/8;auto a=canPoint(turn,153),b=canPoint(turn,169);
    line(ctx,a.x,a.y,b.x,b.y,0x597284,std::max(1,S(1)));
    int x=b.x*360/SCREEN_RES-5,y=b.y*360/SCREEN_RES-5;
    capsulePanel(ctx,x,y,10,10,CanPanel,0x91a5b5,2);
  }
  if(active)for(int rail=0;rail<2;++rail){
    float turn=float(phase%6000)/6000.f;auto p=canPoint(turn,rail?153:159);
    disc(ctx,p.x,p.y,S(4),rail?0x33404d:0x254b60);disc(ctx,p.x,p.y,S(2),rail?CanSteel:CanBlue);
  }
}
static void canWave(lv_draw_ctx_t* ctx,int x,int y,int width,uint32_t pattern,unsigned shift,int amplitude=5){
  // Equal recessive levels; dominant bits separate CAN-H and CAN-L.
  for(int rail:{-1,1}){
    uint32_t ink=rail<0?CanBlue:CanSteel;int previous=y;
    for(int bit=0;bit<16;++bit){
      bool dominant=((pattern>>((bit+shift)%24))&1)==0;
      int level=y+(dominant?rail*amplitude:0),left=x+width*bit/16,right=x+width*(bit+1)/16;
      if(bit)themeStroke(ctx,left,previous,left,level,ink);
      themeStroke(ctx,left,level,right,level,ink);previous=level;
    }
  }
}
static void canRadarField(lv_draw_ctx_t* ctx,uint32_t frame,bool active){
  disc(ctx,CENTER,CENTER,S(179),0x5c7180);disc(ctx,CENTER,CENTER,S(176),CanBlack);
  canBusRim(ctx,frame,active);
  for(int r:{44,88,130})capsuleRing(ctx,r,0x182935);
  themeStroke(ctx,49,180,311,180,0x182935);themeStroke(ctx,180,121,180,286,0x182935);
  themeArtwork(ctx,radar_logo_can_commander,17);
  capsuleText(ctx,"CAN COMMANDER",80,234,200,CanTitle,CanSteel);
  capsuleText(ctx,"CAN-H",65,258,58,CanSmall,CanBlue);
  capsuleText(ctx,"CAN-L",237,258,58,CanSmall,CanSteel);
  canWave(ctx,125,265,110,0x4d4a35,active?frame/160:0);
}
static void canRadarSweep(lv_draw_ctx_t* ctx,float turns){
  float a=turns*6.28318530718f;
  auto point=[&](int along,int side){return lv_point_t{lv_coord_t(CENTER+sinf(a)*S(along)+cosf(a)*S(side)),lv_coord_t(CENTER-cosf(a)*S(along)+sinf(a)*S(side))};};
  for(int side:{-1,1}){
    auto previous=point(10,side*2);
    for(int r=10;r<=146;r+=8){
      int displacement=((r/16)%2?5:2)*side;auto next=point(r,displacement),corner=point(r,(r==10?2:((r-8)/16)%2?5:2)*side);
      line(ctx,previous.x,previous.y,corner.x,corner.y,side<0?CanBlue:CanSteel,std::max(1,S(2)),180);
      line(ctx,corner.x,corner.y,next.x,next.y,side<0?CanBlue:CanSteel,std::max(1,S(2)),180);previous=next;
    }
  }
  auto p=point(148,0);disc(ctx,p.x,p.y,S(4),CanBlue);disc(ctx,CENTER,CENTER,S(5),CanPanel);disc(ctx,CENTER,CENTER,S(2),CanSteel);
}
static void canCommanderWatch(lv_draw_ctx_t* ctx,const dragon::Calendar& date,bool valid,uint32_t frame){
  disc(ctx,CENTER,CENTER,S(179),0x5c7180);disc(ctx,CENTER,CENTER,S(176),CanBlack);
  canBusRim(ctx,frame,true);
  for(int i=0;i<60;++i){auto a=canPoint(i/60.f,i%5?145:141),b=canPoint(i/60.f,148);line(ctx,a.x,a.y,b.x,b.y,i%5?0x3e5668:0xa2b5c4,std::max(1,S(1)));}
  themeArtwork(ctx,watch_logo_can_commander,28);
  capsuleText(ctx,"CAN COMMANDER",64,127,232,CanTitle,CanSteel);
  capsuleText(ctx,"MATTHEW KUKANICH",72,149,216,CanSmall,0x92a9ba);
  const char* fields[]={"HOUR","MIN","SEC"};const int values[]={date.hour,date.minute,date.second};char text[40];
  for(int i=0;i<3;++i){
    int x=69+i*76;capsulePanel(ctx,x,177,70,61,CanPanel,i==2?0x48758c:0x384e60,5);
    capsuleText(ctx,fields[i],x,181,70,CanSmall,0x92a9ba);
    if(valid)snprintf(text,sizeof(text),"%02d",values[i]);else strcpy(text,"--");
    capsuleText(ctx,text,x,197,70,CanDigits,i==2?CanBlue:0xe8edf2);
  }
  if(valid)snprintf(text,sizeof(text),"%04d . %02d . %02d",date.year,date.month,date.day);else strcpy(text,"SET DATE / TIME");
  capsuleText(ctx,text,70,244,220,CanSmall,CanSteel);
  capsuleText(ctx,"CAN-H",52,267,58,CanSmall,CanBlue);capsuleText(ctx,"CAN-L",250,267,58,CanSmall,CanSteel);
  uint32_t data=valid?(uint32_t(date.hour)<<16)|(uint32_t(date.minute)<<8)|date.second:0xffffff;
  canWave(ctx,115,275,130,data,frame/180);
  capsuleText(ctx,"CLOCK FRAME / 0x4D4 / 3 BYTES",62,291,236,CanSmall,0x92a9ba);
  if(valid)snprintf(text,sizeof(text),"DATA  %02X  %02X  %02X",date.hour,date.minute,date.second);else strcpy(text,"DATA  --  --  --");
  capsuleText(ctx,text,84,308,192,CanSmall,CanBlue);
  if(valid){auto p=canPoint(date.second/60.f,173);disc(ctx,p.x,p.y,S(3),0xecf6fb);}
}
