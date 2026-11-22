import {NodeBusyMessage} from './serial.js?v=0.14.9';
// Only repeat read-only requests. Native USB can hold a short response until
// the next host write; retrying a settings query releases it without side effects.
// A running node answers every one of these, so a node refusal seen here
// belongs to an earlier command and is retried too.
export async function deviceQuery(link,command,match){
  if(!/^(badge info|display status|tone settings|tone get \d+|clock status|clock hardware|orientation status|radar status|theme info|theme read \d+ \d+)$/.test(command))throw new Error('Only read-only device queries may be retried.');
  for(let attempt=0;attempt<3;attempt++){
    await link.write(command);
    try{return await link.wait(match,2000);}
    catch(error){if(attempt===2||link.failure||!(error.message.includes('timed out')||error.message===NodeBusyMessage))throw error;}
  }
}
