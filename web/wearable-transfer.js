export function localClockEpoch(value,offset){
  const m=/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2}))?$/.exec(value);
  if(!m||!Number.isInteger(offset)||offset< -720||offset>840)throw new Error('Choose a valid local date/time and UTC offset.');
  const [y,mo,d,h,mi,s]=m.slice(1).map(v=>Number(v||0));
  const date=new Date(Date.UTC(y,mo-1,d,h,mi,s));
  if(y<2000||y>2099||date.getUTCFullYear()!==y||date.getUTCMonth()!==mo-1||date.getUTCDate()!==d||date.getUTCHours()!==h||date.getUTCMinutes()!==mi||date.getUTCSeconds()!==s)throw new Error('The date or time is invalid.');
  const epoch=date.getTime()/1000-offset*60;
  if(epoch<946684800||epoch>4102444799)throw new Error('UTC time must be between 2000 and 2099.');
  return epoch;
}
export async function setClock(link,epoch,offset){
  if(!Number.isInteger(epoch)||epoch<946684800||epoch>4102444799||!Number.isInteger(offset)||offset< -720||offset>840)throw new Error('Invalid clock value.');
  await link.write(`time ${epoch}`);
  if(await link.wait(s=>s==='TIME_OK'||s==='TIME_ERROR')!=='TIME_OK')throw new Error('The clock could not be saved. Check the RTC connection.');
  await link.write(`clock offset ${offset}`);
  if(await link.wait(s=>s==='CLOCK_OK'||s.startsWith('CLOCK_ERROR'))!=='CLOCK_OK')throw new Error('Time was set, but the UTC offset could not be saved.');
}
export async function readWearable(link){
  await link.write('clock status');const c=await link.wait(s=>s.startsWith('CLOCK_STATUS '));
  await link.write('orientation status');const o=await link.wait(s=>s.startsWith('ORIENTATION_STATUS '));
  const clock=/^CLOCK_STATUS rtc=([01]) valid=([01]) epoch=(\d+) offset=(-?\d+)$/.exec(c);
  const manual=/^ORIENTATION_STATUS manual=1 degrees=(0|90|180|270)$/.exec(o);
  const legacy=/^ORIENTATION_STATUS imu=[01] auto=[01] degrees=(0|90|180|270) /.exec(o);
  if(!clock||(!manual&&!legacy))throw new Error('Invalid clock/orientation response.');
  return {rtc:clock[1]==='1',valid:clock[2]==='1',epoch:Number(clock[3]),offset:Number(clock[4]),orientation:(manual||legacy)[1]};
}
export async function setOrientation(link,value){
  if(!['0','90','180','270'].includes(value))throw new Error('Choose an orientation.');
  await link.write(`orientation set ${value}`);
  const reply=await link.wait(s=>s==='ORIENTATION_OK'||s.startsWith('ORIENTATION_ERROR'));
  if(reply!=='ORIENTATION_OK')throw new Error(reply.replace('ORIENTATION_ERROR ','Orientation unavailable: '));
}
