#pragma once
namespace dragon {
// FAT rename cannot overwrite a destination. Retain the last complete file
// until the checked staging write is installed; readers recover interrupted swaps.
template<class Fs,class Path>
bool recoverFile(Fs& fs,const Path& path){
  const auto previous=path+".previous";
  return fs.exists(path)||!fs.exists(previous)||fs.rename(previous,path);
}
template<class Fs,class Staged,class Path>
bool replaceFile(Fs& fs,const Staged& staged,const Path& path){
  if(!fs.exists(staged)||!recoverFile(fs,path))return false;
  const auto previous=path+".previous";
  const bool had=fs.exists(path);
  if(had){
    if(fs.exists(previous)&&!fs.remove(previous))return false;
    if(!fs.rename(path,previous))return false;
  }
  if(!fs.rename(staged,path)){
    if(had)fs.rename(previous,path);
    return false;
  }
  if(had)fs.remove(previous);
  return true;
}
}
