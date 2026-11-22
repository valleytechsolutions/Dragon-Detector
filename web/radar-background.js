// Decorative instrument animation: these marks are not nearby detections.
export function startRadarBackground(canvas,button){
  const ctx=canvas?.getContext('2d');if(!ctx)return;
  const motion=matchMedia('(prefers-reduced-motion: reduce)');
  let paused=false,frame=0,last=0,phase=0,w=0,h=0;
  try{paused=localStorage.getItem('radar-motion')==='paused';}catch{}
  const contacts=[[.28,.62],[.52,2.2],[.76,3.8],[.43,5.25],[.87,1.3],[.65,4.7]];
  const field=document.createElement('canvas'), sweep=document.createElement('canvas');
  let radius=0;
  function cacheDial(){
    const r=radius,side=Math.ceil(2*r+8);
    field.width=field.height=sweep.width=sweep.height=side;
    const dial=field.getContext('2d'),beam=sweep.getContext('2d');
    dial.translate(side/2,side/2);beam.translate(side/2,side/2);
    const face=dial.createRadialGradient(0,-r*.12,0,0,0,r);
    face.addColorStop(0,'#142029');face.addColorStop(.9,'#080a0b');face.addColorStop(1,'#181713');
    dial.fillStyle=face;dial.beginPath();dial.arc(0,0,r,0,Math.PI*2);dial.fill();
    for(const [fraction,color,width] of [[1,'#d6b46e',2],[.975,'#6d5b38',1],[.9,'#a78750',1],[.6,'#484039',1],[.3,'#484039',1]]){
      dial.strokeStyle=color;dial.lineWidth=width;dial.beginPath();dial.arc(0,0,r*fraction,0,Math.PI*2);dial.stroke();
    }
    for(let i=0;i<60;i++){
      const a=i*Math.PI/30,major=i%5===0,inner=major?.914:.935;
      dial.strokeStyle=major?'#f2dfad':'#6d5b38';dial.lineWidth=major?2:1;dial.beginPath();
      dial.moveTo(Math.cos(a)*r*inner,Math.sin(a)*r*inner);dial.lineTo(Math.cos(a)*r*.957,Math.sin(a)*r*.957);dial.stroke();
    }
    for(let i=0;i<32;i++){
      beam.fillStyle='rgba(69,211,255,'+(.32*(1-i/32)**2)+')';beam.beginPath();beam.moveTo(0,0);
      beam.arc(0,0,r*.895,-(i+1)*.022,-i*.022);beam.closePath();beam.fill();
    }
    beam.shadowColor='#65ecff';beam.shadowBlur=10;beam.strokeStyle='#9ff4ff';beam.lineWidth=2.5;
    beam.beginPath();beam.moveTo(0,0);beam.lineTo(r*.895,0);beam.stroke();
  }
  function paint(){
    ctx.clearRect(0,0,w,h);
    const x=w*.82,y=h*.44,r=radius;
    ctx.save();ctx.translate(x,y);ctx.drawImage(field,-field.width/2,-field.height/2);
    ctx.save();ctx.rotate(phase);ctx.drawImage(sweep,-sweep.width/2,-sweep.height/2);ctx.restore();
    for(const [distance,bearing] of contacts){
      const age=(phase-bearing+Math.PI*4)%(Math.PI*2);
      ctx.globalAlpha=.25+.75*Math.exp(-age*1.5);ctx.fillStyle='#bff6ff';
      ctx.beginPath();ctx.arc(Math.cos(bearing)*distance*r,Math.sin(bearing)*distance*r,3,0,Math.PI*2);ctx.fill();
    }
    ctx.restore();
  }
  function tick(time){
    frame=requestAnimationFrame(tick);
    if(time-last<40)return;
    phase=(phase+Math.min(time-last,100)*Math.PI/6000)%(Math.PI*2);last=time;paint();
  }
  function update(){
    cancelAnimationFrame(frame);frame=0;last=0;
    const stopped=paused||motion.matches;
    button.textContent=motion.matches?'Radar motion off':paused?'Resume radar':'Pause radar';
    button.disabled=motion.matches;button.setAttribute('aria-pressed',String(!stopped));
    paint();if(!stopped&&!document.hidden)frame=requestAnimationFrame(tick);
  }
  function resize(){w=innerWidth;h=innerHeight;const dpr=Math.min(devicePixelRatio||1,1.5);canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);radius=Math.max(205,Math.min(560,Math.max(w,h)*.43));cacheDial();paint();}
  button.addEventListener('click',()=>{paused=!paused;try{localStorage.setItem('radar-motion',paused?'paused':'running');}catch{}update();});
  addEventListener('resize',resize);document.addEventListener('visibilitychange',update);motion.addEventListener('change',update);
  resize();update();
}
