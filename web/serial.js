export class BadgeSerial {
  constructor(port){this.port=port;this.lines=[];this.waiter=null;this.failure=null;this.listeners=new Set();}
  async open(){await this.port.open({baudRate:115200});this.reader=this.port.readable.getReader();this.writer=this.port.writable.getWriter();this.reading=this.pump();}
  async pump(){let text='',decoder=new TextDecoder();try{while(true){const {value,done}=await this.reader.read();if(done)break;text+=decoder.decode(value,{stream:true});let end;while((end=text.indexOf('\n'))>=0){const line=text.slice(0,end).trim();text=text.slice(end+1);for(const listener of this.listeners)listener(line);this.lines.push(line);if(this.lines.length>200)this.lines.shift();this.dispatch();}if(text.length>8192)text='';}}catch(error){this.failure=error;}finally{this.failure??=new Error('USB disconnected. Reconnect to try again.');for(const listener of this.listeners)listener(null);if(this.waiter){clearTimeout(this.waiter.timer);this.waiter.reject(this.failure);this.waiter=null;}this.reader.releaseLock();}}
  onLine(listener){this.listeners.add(listener);return ()=>this.listeners.delete(listener);}
  dispatch(){if(!this.waiter)return;const index=this.lines.findIndex(this.waiter.match);if(index<0)return;const line=this.lines.splice(index,1)[0], waiter=this.waiter;this.waiter=null;clearTimeout(waiter.timer);waiter.resolve(line);}
  wait(match,timeout=7000){if(this.failure)return Promise.reject(this.failure);return new Promise((resolve,reject)=>{this.waiter={match,resolve,reject,timer:setTimeout(()=>{this.waiter=null;reject(new Error('Board response timed out. Check the firmware and reconnect.'));},timeout)};this.dispatch();});}
  write(text){return this.writer.write(typeof text==='string'?new TextEncoder().encode(text+'\n'):text);}
  async close(){try{await this.reader.cancel();await this.reading;}catch{}try{this.writer.releaseLock();await this.port.close();}catch{}}
}
