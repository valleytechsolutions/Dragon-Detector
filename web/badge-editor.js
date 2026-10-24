import {fitCircleZoom} from './badge-codec.js?v=2026-10-23-watcher';
export function rotatedSize(width,height,turns=0){return Math.abs(turns)%2?[height,width]:[width,height];}
export function badgeTransform(width,height,size,{zoom=1,x=0,y=0,turns=0,mirror=false}={}){
  const [w,h]=rotatedSize(width,height,turns),scale=Math.max(size/w,size/h)*zoom;
  return {cx:size/2+x,cy:size/2+y,scale,angle:((turns%4+4)%4)*Math.PI/2,mirror:mirror?-1:1};
}
export function fitBadgeZoom(width,height,size,turns=0){const [w,h]=rotatedSize(width,height,turns);return fitCircleZoom(w,h,size)*.94;}
export function panFromPointer(start,point,displayWidth,size){return {x:Math.max(-size,Math.min(size,start.ox+(point.x-start.x)*size/displayWidth)),y:Math.max(-size,Math.min(size,start.oy+(point.y-start.y)*size/displayWidth))};}
export function rescalePan(x,y,previous,next){return {x:x*next/previous,y:y*next/previous};}

// Keep each line inside the circle, including its full font height. Long lines
// shrink to the available chord instead of being cut off by the screen edge.
export function badgeTextLayout(text,size,{fontPercent=7,yPercent=78}={}){
  const lines=String(text).trim().split(/\r?\n/).slice(0,3);
  if(!lines[0]&&lines.length===1)return [];
  const fontSize=size*Math.max(4,Math.min(14,Number(fontPercent)||7))/100;
  const lineHeight=fontSize*1.2,halfBlock=(lines.length-1)*lineHeight/2;
  const margin=size*.08+fontSize/2+halfBlock;
  const center=Math.max(margin,Math.min(size-margin,size*(Number(yPercent)||50)/100));
  return lines.map((text,i)=>{
    const y=center+(i-(lines.length-1)/2)*lineHeight;
    const dy=Math.abs(y-size/2)+fontSize*.6;
    const maxWidth=2*Math.sqrt(Math.max(0,(size*.46)**2-dy**2));
    return {text,x:size/2,y,fontSize,maxWidth};
  });
}
export function drawBadgeText(ctx,text,size,options={}){
  ctx.save();ctx.textAlign='center';ctx.textBaseline='middle';
  ctx.fillStyle=options.color||'#ffffff';ctx.strokeStyle='#000000';ctx.lineJoin='round';
  for(const line of badgeTextLayout(text,size,options)){
    ctx.font=`700 ${line.fontSize}px sans-serif`;ctx.lineWidth=Math.max(2,line.fontSize*.12);
    if(line.maxWidth>0){ctx.strokeText(line.text,line.x,line.y,line.maxWidth);ctx.fillText(line.text,line.x,line.y,line.maxWidth);}
  }
  ctx.restore();
}
