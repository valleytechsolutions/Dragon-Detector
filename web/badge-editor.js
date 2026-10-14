import {fitCircleZoom} from './badge-codec.js';
export function rotatedSize(width,height,turns=0){return Math.abs(turns)%2?[height,width]:[width,height];}
export function badgeTransform(width,height,size,{zoom=1,x=0,y=0,turns=0,mirror=false}={}){
  const [w,h]=rotatedSize(width,height,turns),scale=Math.max(size/w,size/h)*zoom;
  return {cx:size/2+x,cy:size/2+y,scale,angle:((turns%4+4)%4)*Math.PI/2,mirror:mirror?-1:1};
}
export function fitBadgeZoom(width,height,size,turns=0){const [w,h]=rotatedSize(width,height,turns);return fitCircleZoom(w,h,size)*.94;}
export function panFromPointer(start,point,displayWidth,size){return {x:Math.max(-size,Math.min(size,start.ox+(point.x-start.x)*size/displayWidth)),y:Math.max(-size,Math.min(size,start.oy+(point.y-start.y)*size/displayWidth))};}
export function rescalePan(x,y,previous,next){return {x:x*next/previous,y:y*next/previous};}
