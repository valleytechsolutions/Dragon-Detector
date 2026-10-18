export function setupWorkspaceTabs(){
  const buttons=[...document.querySelectorAll('[data-workspace]')],firmware=document.getElementById('firmware-workspace'),studio=document.getElementById('theme-studio');
  function show(){const personal=location.hash==='#theme-studio';firmware.hidden=personal;studio.hidden=!personal;for(const b of buttons){const selected=(b.dataset.workspace==='theme-studio')===personal;b.setAttribute('aria-selected',String(selected));b.tabIndex=selected?0:-1;}}
  buttons.forEach((b,i)=>{b.onclick=()=>{location.hash=b.dataset.workspace==='theme-studio'?'theme-studio':'install';};b.onkeydown=e=>{if(e.ctrlKey||e.metaKey||e.altKey)return;if(!['ArrowLeft','ArrowRight','Home','End'].includes(e.key))return;e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?buttons.length-1:(i+1)%buttons.length;buttons[next].click();buttons[next].focus();};});
  addEventListener('hashchange',show);show();
}
