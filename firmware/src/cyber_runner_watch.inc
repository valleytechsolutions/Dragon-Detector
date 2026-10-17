static void watchRunnerCyber(lv_draw_ctx_t* ctx,const dragon::Calendar& d,bool valid){
  const bool hack=watchFace==32;char text[40];uint32_t ink=hack?0xffedcf:0xfff2fc,accent=hack?0xff7073:0x4ae9ff;
  if(hack){
    watchRing(ctx,174,0x63202b);watchRing(ctx,170,0x2d111a);
    themeArtwork(ctx,watch_logo_hackazillarex,90);dinoRun(ctx,watchFrameAt);
    if(valid){auto p=watchPolar(d.second/60.f,170);disc(ctx,S(p.x),S(p.y),S(3),accent);}
  }else{themeArtwork(ctx,watch_logo_m33k,68);cyberRim(ctx,watchFrameAt,valid?d.second:-1);}
  int timeY=hack?32:242,dateY=hack?74:290;
  if(valid)snprintf(text,sizeof(text),"%02d:%02d",dragon::hour12(d.hour),d.minute);else strcpy(text,"--:--");watchText(ctx,text,62,timeY,236,FTIMER,ink);
  if(valid)snprintf(text,sizeof(text),"%02d.%02d  %s  %02d",d.month,d.day,d.hour>=12?"PM":"AM",d.second);else strcpy(text,"SET TIME / USB SYNC");watchText(ctx,text,65,dateY,230,F14,accent);
  if(!hack)watchText(ctx,"m33k_t3ch",70,42,220,F16,0xff87dd);
}
