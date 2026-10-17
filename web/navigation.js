// Hash links must remain reachable inside the collapsible theme gallery.
export function revealLocation(root=document, location=window.location) {
  const reveal=()=>{
    let id;
    try{id=decodeURIComponent(location.hash.slice(1));}catch{return;}
    const target=id&&root.getElementById(id);
    if(!target)return;
    for(let parent=target.parentElement;parent;parent=parent.parentElement){
      if(parent.tagName==='DETAILS')parent.open=true;
    }
    target.scrollIntoView({block:'start'});
  };
  window.addEventListener('hashchange',reveal);
  if(root.readyState==='loading')root.addEventListener('DOMContentLoaded',reveal,{once:true});else reveal();
}
