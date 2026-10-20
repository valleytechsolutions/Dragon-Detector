#pragma once
#include "storage_replace.h"
#include <map>
static void storageReplaceTests(){
  struct Fat {
    std::map<std::string,std::string> files;
    unsigned renames=0,failAt=0;bool failRemove=false;
    bool exists(const std::string& p){return files.count(p);}
    bool remove(const std::string& p){return !failRemove&&files.erase(p);}
    bool rename(const std::string& a,const std::string& b){
      if(++renames==failAt||!exists(a)||exists(b))return false;
      files[b]=files[a];files.erase(a);return true;
    }
  };
  const std::string live="/session.json",part="/session.json.part",old=live+".previous";
  Fat fs;fs.files[part]="first";
  assert(dragon::replaceFile(fs,part,live)&&fs.files[live]=="first");
  for(unsigned i=0;i<20;++i){fs.files[part]="next";assert(dragon::replaceFile(fs,part,live));}
  assert(fs.files.size()==1&&fs.files[live]=="next");
  fs.files[part]="bad";fs.failAt=fs.renames+2;
  assert(!dragon::replaceFile(fs,part,live)); // install fails; previous is restored
  assert(fs.files[live]=="next"&&fs.files[part]=="bad"&&!fs.exists(old));
  fs.failAt=fs.renames+1;
  assert(!dragon::replaceFile(fs,part,live)&&fs.files[live]=="next");
  fs.failAt=0;assert(fs.rename(live,old)); // simulated power loss between renames
  assert(dragon::recoverFile(fs,live)&&fs.files[live]=="next");
  fs.files[old]="older";fs.failRemove=true;
  assert(!dragon::replaceFile(fs,part,live)&&fs.files[live]=="next");
  fs.failRemove=false;assert(dragon::replaceFile(fs,part,live)&&fs.files[live]=="bad");
  assert(!dragon::replaceFile(fs,part,live));
  puts("PASS FAT replacement: repeated mission updates, rollback, interrupted-swap recovery, failed cleanup, missing staging file");
}
