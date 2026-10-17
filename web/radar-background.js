// Decorative instrument animation: these marks are not nearby detections.
export function startRadarBackground(canvas,button){
  const ctx=canvas?.getContext('2d');if(!ctx)return;
  const motion=matchMedia('(prefers-reduced-motion: reduce)');
  let paused=false,frame=0,last=0,phase=0,w=0,h=0;
  try{paused=localStorage.getItem('radar-motion')==='paused';}catch{}
  const contacts=[[.28,.62],[.52,2.2],[.76,3.8],[.43,5.25],[.87,1.3],[.65,4.7]];
  function paint(){
    ctx.clearRect(0,0,w,h);
    const x=w*.77,y=h*.48,r=Math.max(300,Math.min(650,Math.max(w,h)*.61));
    ctx.save();ctx.translate(x,y);
    ctx.strokeStyle='#83cda8';ctx.lineWidth=1;
    for(let n=1;n<=5;n++){ctx.globalAlpha=.15;ctx.beginPath();ctx.arc(0,0,r*n/5,0,Math.PI*2);ctx.stroke();}
    ctx.globalAlpha=.11;
    for(let i=-4;i<=4;i++){const a=r*i/5,b=Math.sqrt(r*r-a*a);ctx.beginPath();ctx.moveTo(a,-b);ctx.lineTo(a,b);ctx.moveTo(-b,a);ctx.lineTo(b,a);ctx.stroke();}
    for(let i=0;i<120;i++){const a=i*Math.PI/60,len=i%10===0?12:4;ctx.globalAlpha=i%10===0?.5:.18;ctx.beginPath();ctx.moveTo(Math.cos(a)*(r-len),Math.sin(a)*(r-len));ctx.lineTo(Math.cos(a)*r,Math.sin(a)*r);ctx.stroke();}
    for(let i=0;i<45;i++){ctx.globalAlpha=(45-i)/45*.055;ctx.fillStyle='#70edb0';ctx.beginPath();ctx.moveTo(0,0);ctx.arc(0,0,r,phase-i*.016-.018,phase-i*.016);ctx.closePath();ctx.fill();}
    ctx.globalAlpha=.6;ctx.strokeStyle='#c3f66d';ctx.beginPath();ctx.moveTo(0,0);ctx.lineTo(Math.cos(phase)*r,Math.sin(phase)*r);ctx.stroke();
    for(const [distance,bearing] of contacts){const age=(phase-bearing+Math.PI*4)%(Math.PI*2);ctx.globalAlpha=.09+.7*Math.exp(-age*1.3);ctx.fillStyle='#d1ff9e';ctx.beginPath();ctx.arc(Math.cos(bearing)*distance*r,Math.sin(bearing)*distance*r,3,0,Math.PI*2);ctx.fill();}
    ctx.globalAlpha=.3;ctx.fillStyle='#b2d9bd';ctx.font='10px monospace';ctx.fillText('360° / SIGNAL FIELD',-70,-r-18);ctx.restore();
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
  function resize(){w=innerWidth;h=innerHeight;const dpr=Math.min(devicePixelRatio||1,1.5);canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);paint();}
  button.addEventListener('click',()=>{paused=!paused;try{localStorage.setItem('radar-motion',paused?'paused':'running');}catch{}update();});
  addEventListener('resize',resize);document.addEventListener('visibilitychange',update);motion.addEventListener('change',update);
  resize();update();
}
