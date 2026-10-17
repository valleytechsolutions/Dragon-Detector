// Original vector ornament inspired by the user's supplied community artwork.
// All geometry uses the common 360px design space; no animation allocations.
static void paintDrip(lv_draw_ctx_t* ctx,int x,int y,int length,int width,uint32_t ink){
  watchLine(ctx,x,y,x,y+length,ink,width);
  disc(ctx,S(x),S(y+length),std::max(1,S(width/2)),ink);
  if(width>=5)watchLine(ctx,x-1,y+2,x-1,y+length-2,watchShade(ink,40));
}
static void paintMushroom(lv_draw_ctx_t* ctx,int x,int y,int size,uint32_t ink){
  const int h=size*5/4;
  lv_point_t stem[]={{(lv_coord_t)(x-size/6),(lv_coord_t)y},{(lv_coord_t)(x+size/5),(lv_coord_t)y},{(lv_coord_t)(x+size/9),(lv_coord_t)(y+h)},{(lv_coord_t)(x-size/4),(lv_coord_t)(y+h)}};
  watchPoly(ctx,stem,4,0xc4f4ff);watchLine(ctx,x+size/8,y+4,x,y+h-2,0x9d52dd,2);
  watchCurve(ctx,{(lv_coord_t)(x-size/4),(lv_coord_t)(y+h)},{(lv_coord_t)(x-size/2),(lv_coord_t)(y+h+4)},{(lv_coord_t)(x+size/2),(lv_coord_t)(y+h+4)},{(lv_coord_t)(x+size/9),(lv_coord_t)(y+h)},0xeac0ff);
  for(int i=0;i<18;++i){float a=i*3.14159265f/18,b=(i+1)*3.14159265f/18;
    lv_point_t tri[]={{(lv_coord_t)x,(lv_coord_t)y},{(lv_coord_t)(x+lroundf(cosf(a)*size)),(lv_coord_t)(y-lroundf(sinf(a)*size*.65f))},{(lv_coord_t)(x+lroundf(cosf(b)*size)),(lv_coord_t)(y-lroundf(sinf(b)*size*.65f))}};
    watchPoly(ctx,tri,3,ink);watchLine(ctx,tri[1].x,tri[1].y,tri[2].x,tri[2].y,0xeac5ff);
  }
  watchLine(ctx,x-size,y,x+size,y,0x49236f,3);
  for(int i=-3;i<=3;++i)watchLine(ctx,x+i*size/4,y,x+i*size/10,y+size/5,0xe8bdff);
  for(int i=-2;i<=2;++i){int xx=x+i*size/3,yy=y-size*(i%2?2:3)/8;disc(ctx,S(xx),S(yy),std::max(1,S(size/10)),i%2?0x65eff4:0xfff77b);}
}
static void radarAwokArt(lv_draw_ctx_t* ctx){
  paintMushroom(ctx,77,128,17,0x753395);paintMushroom(ctx,282,225,16,0x4c428d);
  paintDrip(ctx,113,79,20,5,0x773782);paintDrip(ctx,265,107,27,4,0x235b72);
  paintDrip(ctx,87,233,22,4,0x56377c);
}
static void watchAwok(lv_draw_ctx_t* ctx){
  for(int i=0;i<12;++i){uint32_t c=i%3==0?0x43dce7:i%3==1?0xd452e7:0xe9cf5e;
    for(int j=0;j<3;++j)watchRay(ctx,(i*10+j)/120.f,164,168,c,2);
  }
  paintDrip(ctx,100,62,27,5,0xdc59ec);paintDrip(ctx,113,59,17,3,0x38d9e1);
  paintDrip(ctx,270,97,23,5,0x38cbd8);paintDrip(ctx,73,133,28,4,0x9d4bde);
  paintDrip(ctx,281,215,23,4,0xea7ce7);
  paintMushroom(ctx,105,205,25,0xa938e5);paintMushroom(ctx,252,187,23,0xdd56d8);
  paintMushroom(ctx,266,108,10,0x7043d0);
  for(int i=0;i<9;++i){int x=136+i*11,y=237+(i%3)*4;disc(ctx,S(x),S(y),S(i%3?1:2),i%2?0x3adce8:0xdb65ee);}
  watchLogo(ctx,watch_logo_awok,74,0xffffff,true);
}
static void occultSigil(lv_draw_ctx_t* ctx,int x,int y,int kind,uint32_t ink){
  if(kind%3==0){watchLine(ctx,x,y-6,x-5,y+4,ink);watchLine(ctx,x-5,y+4,x+5,y+4,ink);watchLine(ctx,x+5,y+4,x,y-6,ink);watchLine(ctx,x-6,y,x+6,y,ink);}
  else if(kind%3==1){watchCircle(ctx,x,y-2,4,ink);watchLine(ctx,x,y+2,x,y+9,ink);watchLine(ctx,x-4,y+6,x+4,y+6,ink);}
  else{watchLine(ctx,x-4,y-5,x+4,y+5,ink);watchLine(ctx,x+4,y-5,x-4,y+5,ink);watchLine(ctx,x,y-8,x,y+8,ink);watchCircle(ctx,x,y,7,ink);}
}
static void occultStar(lv_draw_ctx_t* ctx,int x,int y,int r,uint32_t ink){
  for(int i=0;i<5;++i){auto a=watchPolar(i/5.f,r,x,y),b=watchPolar((i+2)/5.f,r,x,y);watchLine(ctx,a.x,a.y,b.x,b.y,ink);}
  watchCircle(ctx,x,y,r+3,ink);
}
static void radarBaphArt(lv_draw_ctx_t* ctx){
  occultStar(ctx,180,180,87,0x30203d);
  for(int i=0;i<8;++i){auto p=watchPolar((i+.5f)/8,151);occultSigil(ctx,p.x,p.y,i,0x66447a);}
}
static void watchBaph(lv_draw_ctx_t* ctx){
  for(int i=0;i<36;++i){auto p=watchPolar((i+.5f)/36,170);watchStar(ctx,p.x,p.y,2,i%3?0x6a4b7f:0xc3955c);}
  occultStar(ctx,180,200,54,0x44304f);watchCircle(ctx,180,200,61,0x24182e);
  for(int i=0;i<6;++i){auto p=watchPolar((i+.5f)/6,108);if(p.y>105)occultSigil(ctx,p.x,p.y,i,0x966bac);}
  watchCircle(ctx,180,124,51,0xb28b54);watchCircle(ctx,180,124,54,0x4c3159);
  watchLogo(ctx,watch_logo_baph,76,0,false,true);
  // Waxing, full and waning moons below the arbor.
  for(int side:{-1,1}){disc(ctx,S(180+side*24),S(230),S(7),0xc1a8d6);disc(ctx,S(180+side*24+side*4),S(228),S(6),0x0b070f);}
  watchCircle(ctx,180,230,8,0xd1b878);disc(ctx,S(180),S(230),S(4),0x695077);
}
template<size_t N> static void mapOutline(lv_draw_ctx_t* ctx,const lv_point_t (&p)[N],uint32_t ink){
  for(unsigned i=0;i<N;++i)watchLine(ctx,p[i].x,p[i].y,p[(i+1)%N].x,p[(i+1)%N].y,ink);
}
static void worldMap(lv_draw_ctx_t* ctx,uint32_t land,uint32_t grid){
  // Stylized geographic silhouettes, deliberately not a live location display.
  for(int y=139;y<=243;y+=26)watchLine(ctx,67,y,293,y,grid);
  for(int x=76;x<=284;x+=26)watchLine(ctx,x,137,x,243,grid);
  static const lv_point_t north[]={{72,150},{88,142},{108,143},{120,137},{136,144},{142,153},{132,160},{127,168},{118,175},{123,183},{116,183},{109,173},{100,171},{97,161},{86,159},{78,165},{72,159}};
  static const lv_point_t south[]={{125,183},{140,182},{148,190},{161,194},{158,207},{152,217},{145,225},{142,237},{138,244},{133,233},{134,220},{128,207},{127,195},{121,189}};
  static const lv_point_t greenland[]={{145,136},{159,138},{157,150},{151,155},{145,147}};
  static const lv_point_t europe[]={{181,151},{188,147},{190,139},{195,139},{198,151},{204,152},{202,161},{194,164},{188,160},{184,163},{178,158}};
  static const lv_point_t africa[]={{179,166},{191,163},{208,168},{214,179},{221,181},{213,191},{210,208},{199,220},{192,216},{189,202},{181,194},{174,179}};
  static const lv_point_t asia[]={{203,149},{215,142},{234,143},{248,139},{271,144},{284,151},{280,158},{266,160},{263,170},{253,179},{245,179},{246,193},{239,189},{231,180},{226,193},{220,185},{219,174},{210,171},{202,162}};
  static const lv_point_t australia[]={{259,218},{271,212},{282,217},{287,229},{279,235},{266,232},{258,234},{252,226}};
  mapOutline(ctx,north,land);mapOutline(ctx,south,land);mapOutline(ctx,greenland,land);mapOutline(ctx,europe,land);mapOutline(ctx,africa,land);mapOutline(ctx,asia,land);mapOutline(ctx,australia,land);
  watchLine(ctx,267,178,271,171,land);watchLine(ctx,269,184,272,179,land);watchLine(ctx,247,199,259,204,land);watchLine(ctx,260,205,273,206,land);watchLine(ctx,290,237,286,243,land);
}
static void signalHalo(lv_draw_ctx_t* ctx,int x,int y,uint32_t ink){
  disc(ctx,S(x),S(y),S(2),ink);
  lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.color=color(ink);d.width=std::max(1,S(1));lv_point_t c{(lv_coord_t)S(x),(lv_coord_t)S(y)};
  for(int r:{6,11,16})lv_draw_arc(ctx,&d,&c,S(r),218,322);
}
static void radarWdgArt(lv_draw_ctx_t* ctx){
  worldMap(ctx,0x19414e,0x0a242d);
  for(int side:{-1,1})for(int i=0;i<4;++i){int x=180+side*(114+i*5);watchLine(ctx,x,111+i*3,x,122+i*3,side<0?0x19637a:0x652954,2);}
  signalHalo(ctx,108,157,0x29788c);signalHalo(ctx,242,196,0x6b2e65);
}
static void watchWdg(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_wdgwars,52);
  worldMap(ctx,0x3c8190,0x0b2730);
  signalHalo(ctx,106,160,0x43dfea);signalHalo(ctx,197,161,0xda60c9);signalHalo(ctx,264,219,0x43dfea);
  for(int side:{-1,1})for(int i=0;i<5;++i){int x=180+side*(96+i*4);watchLine(ctx,x,107-i*2,x,120,side<0?0x32b7cc:0xc04ca9,2);}
  for(int i=0;i<12;++i){auto p=watchPolar((i+.5f)/12,167);watchStar(ctx,p.x,p.y,2,i%2?0xea65cc:0x48d8e6);}
}
static void ghostEyes(lv_draw_ctx_t* ctx,int x,int y,float scale){
  // Faceted slits from the supplied banner, drawn at native panel resolution.
  static const lv_point_t slit[]={{-64,-17},{-15,3},{-11,16},{-59,-3}};
  for(int side:{-1,1}){
    lv_point_t p[4];for(int i=0;i<4;++i)p[i]={(lv_coord_t)(x+lroundf(slit[i].x*side*scale)),(lv_coord_t)(y+lroundf(slit[i].y*scale))};
    for(int layer=0;layer<3;++layer){const uint32_t ink[]={0x21102e,0x4c2065,0x9c4bc9};
      for(int i=0;i<4;++i)watchLine(ctx,p[i].x,p[i].y,p[(i+1)%4].x,p[(i+1)%4].y,ink[layer],std::max(1,int((10-layer*3)*scale)));
    }
    watchPoly(ctx,p,4,0xe8b9ff);
    watchLine(ctx,p[0].x,p[0].y,p[1].x,p[1].y,0xfff5ff,std::max(1,int(2*scale)));
  }
}
static void ghostFacets(lv_draw_ctx_t* ctx,uint32_t ink){
  for(int side:{-1,1}){
    watchLine(ctx,180+side*88,117,180+side*106,170,ink);
    watchLine(ctx,180+side*106,170,180+side*72,228,ink);
    watchLine(ctx,180+side*72,228,180+side*18,245,ink);
    watchLine(ctx,180+side*88,117,180+side*72,177,ink);
    watchLine(ctx,180+side*72,177,180+side*18,223,ink);
    watchLine(ctx,180+side*17,153,180+side*7,215,ink);
    watchLine(ctx,180+side*126,141,180+side*126,186,ink,2);
    watchLine(ctx,180+side*126,186,180+side*115,197,ink,2);
  }
}
static void radarGhostArt(lv_draw_ctx_t* ctx){
  ghostFacets(ctx,0x34283f);ghostEyes(ctx,180,37,.55f);
  for(int side:{-1,1})for(int i=0;i<3;++i)watchLine(ctx,180+side*105,102+i*5,180+side*(117+i*5),102+i*5,0x654079);
}
static void watchGhost(lv_draw_ctx_t* ctx){
  ghostFacets(ctx,0x332e3b);ghostEyes(ctx,180,110,1.f);
  for(int i=0;i<12;++i){auto p=watchPolar((i+.5f)/12,167);watchStar(ctx,p.x,p.y,2,0xa37ac4);}
  watchText(ctx,"Safety is an Illusion.",58,237,244,F12,0xd1bfdc);
}
static void radarN3rdArt(lv_draw_ctx_t* ctx){
  for(int i=0;i<48;++i)watchRay(ctx,i/48.f,153,i%4?156:160,i%4?0x4b3e25:0x9d8149);
  for(int side:{-1,1}){
    watchLine(ctx,180+side*104,109,180+side*123,128,0x665330);
    watchLine(ctx,180+side*123,128,180+side*123,142,0x665330);
  }
}
static void watchN3rd(lv_draw_ctx_t* ctx){
  watchCircle(ctx,180,119,51,0xe7c775,2);watchCircle(ctx,180,119,54,0x5d4928);
  watchLogo(ctx,watch_logo_n3rdsec,71,0,false,true);
  // Fine engine-turning and applied gold lozenges frame the original skull.
  for(int i=0;i<48;++i)watchRay(ctx,(i+.5f)/48,115,118,0x30291c);
  for(int side:{-1,1})for(int i=0;i<4;++i){
    int x=180+side*(54+i*9),y=224-i*4;
    lv_point_t p[]={{(lv_coord_t)x,(lv_coord_t)(y-4)},{(lv_coord_t)(x+2),(lv_coord_t)y},{(lv_coord_t)x,(lv_coord_t)(y+4)},{(lv_coord_t)(x-2),(lv_coord_t)y}};
    watchPoly(ctx,p,4,i%2?0x8d7443:0xd6b86d);
  }
  watchLine(ctx,157,238,203,238,0x8d7443);watchStar(ctx,180,238,4,0xe7c775);
}
static void hellfire(lv_draw_ctx_t* ctx,uint32_t frame,bool radar){
  // Bounded, allocation-free tongues. A shared frame time prevents tiled redraw
  // seams; animation is independent of wall-clock settings and radio scheduling.
  float phase=(frame%8000)*WatchTau/8000.f;
  for(int side:{-1,1})for(int i=0;i<4;++i){
    int x=180+side*(81+i*8),base=245-i*5;
    float wave=phase*2+i*1.7f+side*.8f;
    int height=39+i*7+lroundf(12*sinf(wave)),bend=lroundf(6*sinf(wave+.8f));
    for(int layer=0;layer<4;++layer){
      int h=height*(100-layer*19)/100,w=11-layer*2,top=base-h;
      lv_point_t p[]={{(lv_coord_t)(x-w),(lv_coord_t)base},
        {(lv_coord_t)(x-w),(lv_coord_t)(base-h/3)},
        {(lv_coord_t)(x+bend-3),(lv_coord_t)(top+h/4)},
        {(lv_coord_t)(x+bend+side*5),(lv_coord_t)top},
        {(lv_coord_t)(x+w),(lv_coord_t)(base-h/3)},
        {(lv_coord_t)(x+w),(lv_coord_t)base}};
      const uint32_t bright[]={0x39104e,0x88166e,0xe52b99,0xffa4d3};
      const uint32_t dim[]={0x2c1039,0x4c1646,0x7c245d,0xa74d77};
      for(int j=0;j<5;++j){lv_point_t tri[]={{(lv_coord_t)x,(lv_coord_t)base},p[j],p[j+1]};watchPoly(ctx,tri,3,radar?dim[layer]:bright[layer]);}
    }
    int travel=(frame/45+i*23)%92;
    disc(ctx,S(x+lroundf(4*sinf(wave))),S(241-travel),std::max(1,S(i%2?1:2)),radar?0x8a4c6f:0xffb9df);
  }
}
static void radarHaleArt(lv_draw_ctx_t* ctx){
  hellfire(ctx,radarFrameAt,true);
  for(int i=0;i<12;++i){auto p=watchPolar((i+.5f)/12,158);watchStar(ctx,p.x,p.y,3,i%2?0x6b2e63:0x913875);}
}
static void watchHale(lv_draw_ctx_t* ctx){
  hellfire(ctx,watchFrameAt,false);
  watchLogo(ctx,watch_logo_halehound_splash,67);
  watchLogo(ctx,watch_logo_halehound,191);
  for(int i=0;i<24;++i){auto p=watchPolar((i+.5f)/24,167);watchStar(ctx,p.x,p.y,i%2?1:2,i%2?0x733864:0xd26dae);}
}
static void scopeTrace(lv_draw_ctx_t* ctx,int x,int y,int width,uint32_t ink,int amplitude){
  for(int i=0;i<width;i+=3){float a=i*WatchTau/48,b=(i+3)*WatchTau/48;
    watchLine(ctx,x+i,y+lroundf(sinf(a)*amplitude),x+i+3,y+lroundf(sinf(b)*amplitude),ink,2);
  }
}
static void radarWaveArt(lv_draw_ctx_t* ctx){
  for(int y=116;y<=244;y+=32)watchLine(ctx,76,y,284,y,0x243617);
  for(int x=84;x<=276;x+=32)watchLine(ctx,x,112,x,248,0x243617);
  scopeTrace(ctx,91,222,177,0x466c29,10);
}
static void watchWave(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_waveshare,66);
  for(int y:{211,224,237})watchLine(ctx,105,y,255,y,0x22321a);
  for(int x=108;x<=252;x+=18)watchLine(ctx,x,210,x,238,0x22321a);
  scopeTrace(ctx,108,224,144,0xaed463,9);
  for(int side:{-1,1}){int x=180+side*109;watchLine(ctx,x,149,x,205,0x627762);watchLine(ctx,x,149,x-side*8,149,0xaecda0);}
  for(int i=0;i<12;++i){auto p=watchPolar((i+.5f)/12,167);watchScrew(ctx,p.x,p.y,0x71856b,0x030803);}
}
static void moduleBrackets(lv_draw_ctx_t* ctx,uint32_t ink){
  for(int side:{-1,1}){
    int x=180+side*80;
    for(int y:{112,229}){watchLine(ctx,x,y,x-side*16,y,ink,2);watchLine(ctx,x,y,x,y+(y<180?16:-16),ink,2);}
    for(int i=0;i<5;++i){int y=150+i*11;watchLine(ctx,x,y,x+side*8,y,ink,2);watchCircle(ctx,x+side*11,y,2,ink);}
    watchLine(ctx,x-side*10,245,x-side*51,245,ink);watchLine(ctx,x-side*10,245,x,235,ink);
  }
}
static void radarM5Art(lv_draw_ctx_t* ctx){moduleBrackets(ctx,0x244d63);}
static void watchM5(lv_draw_ctx_t* ctx){
  moduleBrackets(ctx,0x3d6d88);watchLogo(ctx,watch_logo_m5stack,103);
  for(int side:{-1,1})for(int i=0;i<3;++i){int x=180+side*(114+i*5);watchLine(ctx,x,154,x,206,0x21465e);}
  watchLine(ctx,122,99,238,99,0x285d79);watchLine(ctx,122,99,115,106,0x68caff);watchLine(ctx,238,99,245,106,0x68caff);
}
static void labTraces(lv_draw_ctx_t* ctx,uint32_t cyan,uint32_t pink){
  for(int side:{-1,1})for(int i=0;i<4;++i){
    int x=180+side*(109+i*6),y=146+i*8;uint32_t ink=side<0?cyan:pink;
    watchLine(ctx,x,y,x,y+66-i*6,ink);watchLine(ctx,x,y+66-i*6,x-side*8,y+74-i*6,ink);watchCircle(ctx,x,y-4,2,ink);
  }
}
static void radarLabArt(lv_draw_ctx_t* ctx){labTraces(ctx,0x165d73,0x623071);}
static void watchLab(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_lab5,43);labTraces(ctx,0x2789a2,0xb04ca7);
  for(int i=0;i<14;++i){int x=111+i*10;watchLine(ctx,x,242,x,242-(i%5+1)*2,i%3?0x675099:0x38b9cc,3);}
  for(int side:{-1,1})for(int i=0;i<6;++i){int x=180+side*(93+i*8);disc(ctx,S(x),S(112-i*2),S(i%2?1:2),i%2?0xd164bb:0x5ccede);}
}
static void pine(lv_draw_ctx_t* ctx,int x,int y,int height,uint32_t ink){
  watchLine(ctx,x,y-height,x,y+3,ink,2);
  for(int n=0;n<4;++n){int yy=y-height+n*height/4,w=3+n*3;watchLine(ctx,x,yy,x-w,yy+height/4,ink,2);watchLine(ctx,x,yy,x+w,yy+height/4,ink,2);}
}
static void radarSasquachArt(lv_draw_ctx_t* ctx){
  for(int side:{-1,1})for(int i=0;i<4;++i)pine(ctx,180+side*(80+i*13),215+(i%2)*16,28+(i%3)*7,0x2c5140);
  scopeTrace(ctx,115,239,126,0x346958,5);
}
static void watchSasquach(lv_draw_ctx_t* ctx){
  for(int side:{-1,1}){pine(ctx,180+side*104,233,42,0x557d62);pine(ctx,180+side*116,209,29,0x2c4e3e);}
  // Original furry body beneath the supplied cartoon head: the character's
  // arms rotate from the chest, in the tradition of animated character watches.
  lv_point_t body[]={{153,149},{207,149},{215,185},{207,225},{196,238},{164,238},{153,225},{145,185}};
  watchPoly(ctx,body,8,0x24170e);
  lv_point_t fur[]={{156,152},{204,152},{211,185},{203,222},{194,233},{166,233},{157,222},{149,185}};
  watchPoly(ctx,fur,8,0x77502c);
  lv_point_t chest[]={{168,161},{192,161},{199,199},{191,220},{169,220},{161,199}};
  watchPoly(ctx,chest,6,0xa17a47);
  for(int side:{-1,1}){
    int x=180+side*20;watchLine(ctx,x,225,x+side*4,241,0x2c1b0f,15);watchLine(ctx,x,225,x+side*4,241,0x79512c,10);
    watchLine(ctx,x,244,x+side*11,244,0x24170e,11);watchLine(ctx,x,243,x+side*10,243,0xa88655,6);
    for(int i=0;i<5;++i){int yy=183+i*8;watchLine(ctx,180+side*27,yy,180+side*22,yy+5,0x4b321d);}
  }
  watchLogo(ctx,watch_logo_sasquach,57,0,false,true);
  for(int i=0;i<12;++i){auto p=watchPolar((i+.5f)/12,167);watchStar(ctx,p.x,p.y,2,i%2?0xc4a76c:0x7fc0aa);}
}
static void sasquachArm(lv_draw_ctx_t* ctx,float turns,int length,bool minute){
  lv_point_t arm[]={watchHandPoint(turns,0,-8),watchHandPoint(turns,length/2,-10),watchHandPoint(turns,length-20,-5),watchHandPoint(turns,length-20,5),watchHandPoint(turns,length/2,10),watchHandPoint(turns,0,8)};
  for(int i=0;i<6;++i)watchLine(ctx,arm[i].x,arm[i].y,arm[(i+1)%6].x,arm[(i+1)%6].y,0x150e08,4);
  watchPoly(ctx,arm,6,minute?0xa47b46:0x77502c);
  auto a=watchHandPoint(turns,10,-3),b=watchHandPoint(turns,length-23,-2);watchLine(ctx,a.x,a.y,b.x,b.y,minute?0xd1ac71:0xb18b53,3);
  for(int i=0;i<4;++i){auto p=watchHandPoint(turns,14+i*(length-38)/4,5),q=watchHandPoint(turns,18+i*(length-38)/4,2);watchLine(ctx,p.x,p.y,q.x,q.y,0x50361e);}
  auto palm=watchHandPoint(turns,length-12,0);disc(ctx,S(palm.x),S(palm.y),S(9),0x17120c);disc(ctx,S(palm.x),S(palm.y),S(7),0xffedcb);
  // Rounded pointing finger, curled fingers and a separate thumb silhouette.
  for(int thumb=0;thumb<2;++thumb){auto from=watchHandPoint(turns,length-(thumb?18:10),thumb?-5:-2),to=watchHandPoint(turns,length+(thumb?-6:6),thumb?-11:-2);
    watchLine(ctx,from.x,from.y,to.x,to.y,0x17120c,9);watchLine(ctx,from.x,from.y,to.x,to.y,0xffedcb,5);
  }
  for(int i=0;i<2;++i){auto p=watchHandPoint(turns,length-13+i*4,1),q=watchHandPoint(turns,length-12+i*4,5);watchLine(ctx,p.x,p.y,q.x,q.y,0x977e55);}
}
static constexpr uint32_t NyanColors[]={0xf650ca,0x2198d1,0x8b46c6,0xe7c526,0x8da72f,0xf2a129};
static void pixelBlock(lv_draw_ctx_t* ctx,int x,int y,int w,int h,uint32_t ink){
  lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color(ink);
  lv_area_t a{(lv_coord_t)S(x),(lv_coord_t)S(y),(lv_coord_t)(S(x+w)-1),(lv_coord_t)(S(y+h)-1)};lv_draw_rect(ctx,&d,&a);
}
static void nyanCat(lv_draw_ctx_t* ctx,int x,int y,int scale,uint32_t frame){
  auto block=[&](int xx,int yy,int w,int h,uint32_t ink){pixelBlock(ctx,x+xx*scale,y+yy*scale,w*scale,h*scale,ink);};
  int step=(frame/180)%2;
  for(int i=0;i<6;++i){block(-23,2+i*2,23,2,NyanColors[i]);block(-29,2+i*2+step,6,2,NyanColors[i]);}
  block(-4,9-step*2,6,3,0x121018);block(-4,9-step*2,4,1,0xa1a0aa);
  for(int xx:{4,16,24}){block(xx+step,14,4,3,0x121018);block(xx+step,14,2,2,0xaaa5b2);}
  block(0,1,23,14,0x121018);block(2,2,19,12,0xf2c590);block(4,3,15,10,0xf68ace);
  for(auto p: {lv_point_t{6,5},lv_point_t{12,4},lv_point_t{17,7},lv_point_t{8,10},lv_point_t{14,11}})block(p.x,p.y,1,1,0xc747a1);
  block(19,5,14,10,0x121018);block(20,2,4,7,0x121018);block(28,2,4,7,0x121018);
  block(21,4,2,4,0xb5afbc);block(29,4,2,4,0xb5afbc);block(20,7,12,7,0xb5afbc);
  block(22,8,2,2,0x16121d);block(28,8,2,2,0x16121d);block(25,10,1,1,0x16121d);
  block(24,12,4,1,0x16121d);block(21,11,2,1,0xf587c9);block(30,11,2,1,0xf587c9);
}
static void radarNyanArt(lv_draw_ctx_t* ctx){
  for(int i=0;i<9;++i){int x=87+(i*41)%188,y=117+(i*29)%102;watchStar(ctx,x,y,i%3?1:3,i%2?0x5b6a8d:0x774879);}
  int x=110+(radarFrameAt/50)%130,y=217+lroundf(3*sinf(radarFrameAt/600.f));
  nyanCat(ctx,x,y,2,radarFrameAt);
}
static void watchNyan(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_nyan,66);
  for(int i=0;i<12;++i)for(int j=-2;j<=2;++j)watchRay(ctx,(i*10+j)/120.f,166,169,NyanColors[i%6],2);
  for(int side:{-1,1})for(int i=0;i<3;++i)watchStar(ctx,180+side*(102-i*8),137+i*32,3,NyanColors[(i+(side>0?3:0))%6]);
  nyanCat(ctx,177,223,1,0);
}

// DMAC's two acrylic McTickler references, drawn as separate native profiles.
// The broad ergonomic handles carry copper ribbon inlays and pearl rivets;
// the hooked steel tips remain bright against the dark teal dial.
static void dmacPick(lv_draw_ctx_t* ctx,float turn,bool minute){
  const float scale=minute?1.f:.73f;
  auto pt=[&](float along,float across){return watchHandPoint(turn,lroundf(along*scale),lroundf(across*scale));};
  const int along[]={-16,-10,0,15,30,45,57,67,77,86,94,101};
  const int centerA[]={-3,-1,1,3,4,3,0,-3,-4,-2,0,0};
  const int centerB[]={-5,-3,0,4,6,5,1,-4,-5,-3,0,0};
  const int width[]={1,4,7,9,9,8,7,9,11,10,6,2};
  for(unsigned i=0;i<11;++i){
    int c=minute?centerA[i]:centerB[i],n=minute?centerA[i+1]:centerB[i+1];
    lv_point_t quad[]={pt(along[i],c-width[i]),pt(along[i+1],n-width[i+1]),pt(along[i+1],n+width[i+1]),pt(along[i],c+width[i])};
    watchPoly(ctx,quad,4,0x15191e);
    watchLine(ctx,quad[0].x,quad[0].y,quad[1].x,quad[1].y,0xc3cbd1);
    watchLine(ctx,quad[2].x,quad[2].y,quad[3].x,quad[3].y,0x596a78);
    auto a=pt(along[i],c-width[i]*.45f),b=pt(along[i+1],n-width[i+1]*.45f);
    watchLine(ctx,a.x,a.y,b.x,b.y,0x3b454e,2);
  }
  // Asymmetric flowing inlay lines are clipped by their hand-authored bounds.
  for(int pos:{9,32,56,79}){
    float bend=minute?0:2;
    watchCurve(ctx,pt(pos-5,-4+bend),pt(pos+8,-7),pt(pos-4,8),pt(pos+10,5),0xbd884d);
    watchCurve(ctx,pt(pos-4,-4+bend),pt(pos+9,-7),pt(pos-3,8),pt(pos+11,5),0xecd2a1);
  }
  for(int pos:{22,75}){auto rivet=pt(pos,minute?2:3);disc(ctx,S(rivet.x),S(rivet.y),std::max(1,S(minute?2:1)),0xf6dfb8);}
  // Thin shaft widens into the resin ferrule; its tip curls like the photos.
  lv_point_t steel[]={pt(95,-3),pt(105,-1),pt(128,-1),pt(136,-4),pt(139,-4),pt(136,-1),pt(128,2),pt(95,3)};
  for(unsigned i=1;i<7;++i){lv_point_t tri[]={steel[0],steel[i],steel[i+1]};watchPoly(ctx,tri,3,0xc3cdd3);}
  for(unsigned i=0;i<8;++i)watchLine(ctx,steel[i].x,steel[i].y,steel[(i+1)%8].x,steel[(i+1)%8].y,i<4?0xffffff:0x7e949f);
  auto a=pt(104,0),b=pt(129,0);watchLine(ctx,a.x,a.y,b.x,b.y,0xf9fbff);
}
static void watchDmac(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_dmac,67,0,false,true);
  watchRing(ctx,155,0x1c4050);watchRing(ctx,118,0x14313e);
  for(int i=0;i<12;++i){auto a=watchPolar(i/12.f,153),b=watchPolar(i/12.f,159);watchLine(ctx,a.x,a.y,b.x,b.y,i%2?0x67d5eb:0xf08abd,2);}
  // Copper motifs echo the handmade acrylic inlays without crowding the logo.
  for(int side:{-1,1}){
    watchCurve(ctx,{(lv_coord_t)(180+side*107),218},{(lv_coord_t)(180+side*105),242},{(lv_coord_t)(180+side*69),217},{(lv_coord_t)(180+side*57),246},0x9e754b);
    watchCurve(ctx,{(lv_coord_t)(180+side*111),222},{(lv_coord_t)(180+side*95),251},{(lv_coord_t)(180+side*74),232},{(lv_coord_t)(180+side*53),247},0x4b6671);
  }
}

static void seeedTraces(lv_draw_ctx_t* ctx,bool dial){
  // Branching PCB traces meet leaf-shaped pads: Seeed's maker/nature palette.
  uint32_t trace=dial?0x40796d:0x24574f,leaf=dial?0x9bc82f:0x4b7534;
  for(int side:{-1,1}){
    int x=180+side*63;
    watchLine(ctx,x,242,x,207,trace,2);watchLine(ctx,x,207,x+side*27,180,trace,2);
    for(int i=0;i<3;++i){int y=204+i*12,xx=x+side*(19-i*5);
      watchLine(ctx,x,y+15,xx,y,trace);watchCircle(ctx,xx,y,2,leaf);
    }
    lv_point_t a{(lv_coord_t)(x+side*27),180},b{(lv_coord_t)(x+side*39),163};
    watchCurve(ctx,a,{(lv_coord_t)(x+side*24),165},{(lv_coord_t)(x+side*31),158},b,leaf,2);
    watchCurve(ctx,a,{(lv_coord_t)(x+side*44),179},{(lv_coord_t)(x+side*46),170},b,leaf,2);
    watchLine(ctx,a.x,a.y,b.x,b.y,trace);
  }
}
static void radarSeeedArt(lv_draw_ctx_t* ctx){seeedTraces(ctx,false);}
static void watchSeeed(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_seeed,61,0,false,true);seeedTraces(ctx,true);
  for(int i=0;i<24;++i)watchRay(ctx,i/24.f,166,169,i%2?0x46766c:0x95c11f,2);
  watchLine(ctx,143,245,217,245,0x315c58);watchCircle(ctx,139,245,2,0x98c530);watchCircle(ctx,221,245,2,0x98c530);
}
static void dagTalons(lv_draw_ctx_t* ctx,uint32_t ink){
  for(int side:{-1,1})for(int i=0;i<3;++i){
    int x=180+side*(94+i*8),y=153+i*8;
    watchCurve(ctx,{(lv_coord_t)x,(lv_coord_t)y},{(lv_coord_t)(x+side*8),(lv_coord_t)(y+24)},{(lv_coord_t)(x+side*5),(lv_coord_t)(y+46)},{(lv_coord_t)(x-side*6),(lv_coord_t)(y+63)},ink,i?1:2);
  }
}
static void radarDagArt(lv_draw_ctx_t* ctx){dagTalons(ctx,0xbcbcb7);}
// 52656e7446726565206920536565206c6f6c2c2077656972646f73
static void watchDag(lv_draw_ctx_t* ctx){
  watchLogo(ctx,watch_logo_dagnazty,60,0,false,true);dagTalons(ctx,0x93938d);
  for(int side:{-1,1}){int x=180+side*25;lv_point_t fang[]={{(lv_coord_t)(x-4),223},{(lv_coord_t)(x+4),223},{(lv_coord_t)x,245}};watchPoly(ctx,fang,3,0x252525);}
  watchCircle(ctx,180,180,118,0xe3e3de);
}

// Viewfinder corners and an optical-scale rim echo the supplied camera-raven.
static void deflockCorners(lv_draw_ctx_t* ctx,int left,int top,int right,int bottom,uint32_t ink){
  for(int sx:{-1,1})for(int sy:{-1,1}){int x=sx<0?left:right,y=sy<0?top:bottom;
    watchLine(ctx,x,y,x-sx*15,y,ink,2);watchLine(ctx,x,y,x,y-sy*15,ink,2);
  }
}
static void radarDeflockArt(lv_draw_ctx_t* ctx){
  deflockCorners(ctx,83,109,277,250,0x6a8ca5);
  for(int side:{-1,1})for(int i=0;i<7;++i){int x=180+side*118,y=139+i*13;watchLine(ctx,x,y,x-side*(i==3?12:5),y,0x45627a);}
}
static void watchDeflock(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  const auto& p=watches[28];watchRing(ctx,175,p.muted);watchRing(ctx,168,0x2d465e);
  for(int i=0;i<60;++i)watchRay(ctx,i/60.f,i%5?165:160,168,i%5?0x405c74:p.accent,i%5?1:2);
  watchLogo(ctx,watch_logo_deflock,49);deflockCorners(ctx,93,47,267,214,0x7099b5);
  watchText(ctx,"LOCAL / 24H",115,218,130,F12,p.accent);
  char text[32];if(valid)snprintf(text,sizeof(text),"%02d:%02d",d.hour,d.minute);else strcpy(text,"--:--");
  watchText(ctx,text,45,235,270,FWATCH,p.ink);
  if(valid)snprintf(text,sizeof(text),"%02d / %02d    :%02d",d.month,d.day,d.second);else strcpy(text,"SYNC IN SETTINGS");
  watchText(ctx,text,78,302,204,F12,p.accent);
  if(valid){auto second=watchPolar(d.second/60.f,168);disc(ctx,S(second.x),S(second.y),std::max(1,S(3)),p.ink);}
}
