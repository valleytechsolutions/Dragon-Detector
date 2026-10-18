// Adapter for the pinned ESP Web Tools 10.4.0 dialog. Its post-flash initialize
// hook runs after the loader releases USB and the dialog reopens it. Close via
// the normal lifecycle so our clock writer never shares a port with the loader.
// Keep these hooks covered when changing the pinned dependency in index.html.
export function clockInstallDialogClass(Base){
  for(const method of ['_initialize','_startInstall','_closeDialog'])
    if(typeof Base.prototype[method]!=='function')throw new Error('USB installer integration changed. Reload or contact support.');
  return class extends Base{
    installMode='update';
    autoClock=true;
    autoTheme=false;
    installSucceeded=false;
    _startInstall(){super._startInstall(this.installMode==='factory');}
    async _initialize(justInstalled=false){
      await super._initialize(justInstalled);
      // This page already selected Update or Fresh installation. Avoid a
      // contradictory second erase chooser; _startInstall enforces that mode.
      // Published manifests still ask when opened in a generic installer.
      if(this._manifest)this._manifest.new_install_prompt_erase=false;
      if(justInstalled&&this._state!=='ERROR'&&this._installState?.state==='finished')this.installSucceeded=true;
      if(this.installSucceeded&&(this.autoClock||this.autoTheme)){
        await this.updateComplete;
        this._closeDialog();
      }
    }
  };
}
