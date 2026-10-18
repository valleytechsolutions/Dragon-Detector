export const isProductionOrigin = origin => ['https://valleytechsolutions.github.io','https://valleytech-dragon-detector.pages.dev'].includes(origin);
export async function updateVisitCounter(element,{fetcher=fetch,production=isProductionOrigin(location.origin),endpoint='https://dragon-detector-visits.valleytechsolutions-dd.workers.dev/visits'}={}) {
  if(!element)return;
  try {
    // One request per page load. Hash navigation and the back/forward cache do
    // not count again; previews only read. No cookies or tracking identifiers.
    const response=await fetcher(endpoint,{method:production?'POST':'GET',credentials:'omit',cache:'no-store',signal:AbortSignal.timeout(5000)});
    if(!response.ok)throw new Error('Counter unavailable');
    const data=await response.json();
    if(!Number.isSafeInteger(data.count)||data.count<0)throw new Error('Invalid counter');
    element.textContent=new Intl.NumberFormat('en-US').format(data.count);
    element.closest('[data-visit-counter]')?.setAttribute('title','Page visits since October 16, 2026. Reloads count; this is not a unique-person count.');
  } catch {
    element.textContent='—';
    element.closest('[data-visit-counter]')?.setAttribute('title','The shared visit count is temporarily unavailable.');
  }
}
