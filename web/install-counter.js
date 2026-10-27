export const isProductionOrigin = origin => ['https://valleytechsolutions.github.io','https://valleytech-dragon-detector.pages.dev'].includes(origin);
export function createInstallCounter(element,{fetcher=fetch,production=isProductionOrigin(location.origin),endpoint='https://dragon-detector-visits.valleytechsolutions-dd.workers.dev/installs',newId=()=>crypto.randomUUID()}={}){
  let displayed=-1;
  const title='Completed web-flasher installs since October 26, 2026. Updates and reinstalls count; downloads and page visits do not.';
  function show(count){
    if(!Number.isSafeInteger(count)||count<0)throw new Error('Invalid counter');
    if(element&&count>=displayed){displayed=count;element.textContent=new Intl.NumberFormat('en-US').format(count);element.closest('[data-install-counter]')?.setAttribute('title',title);}
  }
  function unavailable(){if(element&&displayed<0){element.textContent='—';element.closest('[data-install-counter]')?.setAttribute('title','The install count is temporarily unavailable.');}}
  async function request(method,id){
    const options={method,credentials:'omit',cache:'no-store',signal:AbortSignal.timeout(5000)};
    if(id){options.headers={'Content-Type':'application/json'};options.body=JSON.stringify({id});}
    const response=await fetcher(endpoint,options);
    if(!response.ok){const error=new Error('Counter unavailable');error.retry=response.status>=500;throw error;}
    show((await response.json()).count);
  }
  async function refresh(){try{await request('GET');}catch{unavailable();}}
  async function recordInstall(){
    if(!production)return;
    try{
      const id=newId();
      try{await request('POST',id);}catch(error){if(error.retry===false)throw error;await request('POST',id);}
    }catch{unavailable();}
  }
  return {refresh,recordInstall};
}
