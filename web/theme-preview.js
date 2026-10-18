const TAU=Math.PI*2;
const roman=['XII','I','II','III','IV','V','VI','VII','VIII','IX','X','XI'];
export function drawThemePreview(canvas,c,logo,mode='clock',seconds=10*3600+8*60+24,timer=83.24){
  const g=canvas.getContext('2d');g.clearRect(0,0,360,360);g.save();g.beginPath();g.arc(180,180,179,0,TAU);g.clip();g.fillStyle=c.bg;g.fillRect(0,0,360,360);
  const ring=(r,color,width=1)=>{g.strokeStyle=color;g.lineWidth=width;g.beginPath();g.arc(180,180,r,0,TAU);g.stroke();};
  const pt=(turn,r,side=0)=>[180+Math.sin(turn*TAU)*r+Math.cos(turn*TAU)*side,180-Math.cos(turn*TAU)*r+Math.sin(turn*TAU)*side];
  const line=(a,b,color,width=1)=>{g.beginPath();g.moveTo(...a);g.lineTo(...b);g.strokeStyle=color;g.lineWidth=width;g.lineCap='round';g.stroke();};
  const label=(value,x,y,size=14,color=c.ink,font='Arial')=>{g.fillStyle=color;g.textAlign='center';g.textBaseline='top';g.font=`${size}px ${font}`;g.fillText(value,x,y);};
  const image=(side,y)=>{if(logo)g.drawImage(logo,180-side/2,y,side,side);else{g.strokeStyle=c.secondary;g.setLineDash([3,3]);g.strokeRect(180-side/2,y,side,side);g.setLineDash([]);label('YOUR',180,y+side*.26,side*.15,c.secondary);label('LOGO',180,y+side*.48,side*.15,c.secondary);}};
  const digital=(value,y)=>{
    if(!c.font||mode==='counter'){label(value,180,y,32,c.ink);return;}
    const pixels=[[14,17,19,21,25,17,14],[4,12,4,4,4,4,14],[14,17,1,2,4,8,31],[30,1,1,14,1,1,30],[2,6,10,18,31,2,2],[31,16,16,30,1,1,30],[14,16,16,30,17,17,14],[31,1,2,4,8,8,8],[14,17,17,14,17,17,14],[14,17,17,15,1,1,14]],segments=[63,6,91,79,102,109,125,7,127,111],rects=[[4,0,18,3],[22,3,3,13],[22,19,3,13],[4,32,18,3],[1,19,3,13],[1,3,3,13],[4,16,18,3]];
    let x=180-value.length*16;g.fillStyle=c.ink;
    for(const char of value){if(char===':'){g.fillRect(x+12,y+8,4,4);g.fillRect(x+12,y+24,4,4);}else if(c.font===1){for(let r=0;r<7;r++)for(let col=0;col<5;col++)if(pixels[Number(char)][r]&(16>>col))g.fillRect(x+col*5,y+r*5,4,4);}else for(let i=0;i<7;i++)if(segments[Number(char)]&(1<<i))g.fillRect(x+rects[i][0],y+rects[i][1],rects[i][2],rects[i][3]);x+=32;}
  };
  ring(176,c.accent,2);g.globalAlpha=.35;ring(171,c.ink);g.globalAlpha=1;
  if(mode==='radar'){
    g.globalAlpha=.18;for(const r of [42,84,126])ring(r,c.ink);line([14,180],[346,180],c.ink);line([180,60],[180,346],c.ink);g.globalAlpha=1;
    const turn=seconds/6;
    if(c.sweep===2){for(let i=12;i>=0;i--){g.globalAlpha=1-i/14;g.fillStyle=c.accent;g.beginPath();g.arc(...pt(turn-i*.006,145),3,0,TAU);g.fill();}g.globalAlpha=1;}
    else{for(let i=10;i>=1;i--){g.globalAlpha=(45-i*4)/255;line([180,180],pt(turn-i*.008,164),c.accent,2);}g.globalAlpha=1;line([180,180],pt(turn,164),c.accent,2);if(c.sweep===1)line([180,180],pt(turn+.5,164),c.secondary,2);}
    for(const [angle,r] of [[.18,104],[.4,60],[.67,123]]){g.fillStyle=c.secondary;g.beginPath();g.arc(...pt(angle,r),4,0,TAU);g.fill();}
    image(42,17);label('BLE + WI-FI',180,70,12,c.secondary);label('PREVIEW / SIMULATED SIGNALS',180,299,10,c.secondary);
  }else if(mode==='counter'){
    image(42,20);label(c.locksport?'LOCKSPORT TIMER':'CHRONOGRAPH',180,74,20,c.accent);label(c.locksport?'P I C K I N G':'M E A S U R I N G',180,106,12,c.accent);
    const mm=String(Math.floor(timer/60)).padStart(2,'0'),ss=(timer%60).toFixed(2).padStart(5,'0');digital(`${mm}:${ss}`,136);label(c.locksport?'OPEN stops this attempt':'No laps yet',180,184,14,c.secondary);
    for(const [x,labelText] of [[55,'PAUSE'],[184,c.locksport?'OPEN':'LAP']]){g.strokeStyle=c.secondary;g.strokeRect(x,220,122,47);label(labelText,x+61,235,14);}
    label(c.locksport?'SPLIT     LOG     TOOLS':'LAPS              TOOLS',180,286,12,c.secondary);
    for(let i=0;i<7;i++){g.globalAlpha=(45+i*35)/255;g.fillStyle=c.accent;g.beginPath();g.arc(...pt(timer/10-(6-i)/120,174),i===6?3:2,0,TAU);g.fill();}g.globalAlpha=1;
  }else{
    if(c.markers!==3){for(let i=0;i<60;i++){g.globalAlpha=i%5?.35:1;line(pt(i/60,i%5?164:156),pt(i/60,168),i%5?c.ink:c.secondary,i%5?1:2);}g.globalAlpha=1;
      if(c.markers<2)for(let i=0;i<12;i++){if(c.layout===1&&i>=5&&i<=7)continue;const p=pt(i/12,140);label(c.markers?roman[i]:String(i||12),p[0],p[1]-8,14);}}
    image(72+c.logoSize*18,c.layout===2?56:62);
    const hour=Math.floor(seconds/3600)%24,minute=Math.floor(seconds/60)%60,second=Math.floor(seconds)%60;
    const hand=(turn,length,minute)=>{const color=minute?c.secondary:c.ink;line(pt(turn,-12),pt(turn,length),c.bg,c.hands===1?10:7);line(pt(turn,-10),pt(turn,length),color,c.hands===1?6:3);
      if(c.hands===2){g.fillStyle=color;g.beginPath();g.moveTo(...pt(turn,length));g.lineTo(...pt(turn,length-22,8));g.lineTo(...pt(turn,length-22,-8));g.closePath();g.fill();}
      if(c.hands===3){line(pt(turn,-9),pt(turn,length*.4),c.accent,9);line(pt(turn,length-10),pt(turn,length,-7),color,4);}};
    if(c.layout!==2){hand((hour%12+minute/60+second/3600)/12,83,false);hand((minute+second/60)/60,116,true);g.fillStyle=c.accent;g.beginPath();g.arc(180,180,6,0,TAU);g.fill();}
    if(c.seconds){g.fillStyle=c.accent;g.beginPath();g.arc(...pt(second/60,173),3,0,TAU);g.fill();}
    if(c.layout){const y=c.layout===2?197:260;g.fillStyle=c.bg;g.fillRect(93,y-3,174,43);digital(`${String(c.hour24?hour:hour%12||12).padStart(2,'0')}:${String(minute).padStart(2,'0')}`,y);if(!c.hour24)label(hour<12?'AM':'PM',180,y-22,14,c.secondary);}
    if(c.date)label(new Date().toLocaleDateString('sv-SE'),180,c.layout===2?254:c.layout===1?305:278,14,c.secondary);
  }
  g.restore();
}
