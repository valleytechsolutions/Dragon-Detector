#include "recipes.h"
#ifndef BOARD_RADAR_LITE
#include "recon_recipe.h"
#include "recon_internal.h"
#include "ble_lab.h"
#include "ble_insights.h"
#include "missions.h"
#include "app.h"
namespace {
dragon::recipe::Plan plan;unsigned step=0;bool running=false,waitTool=false;uint32_t due=0,began=0;dragon::ReconMode previous;uint8_t previousLock=0;uint16_t oldSeconds=0,oldRate=0;char result[100]="No recipe running",recipeName[33]{};
void finish(const char* reason){if(!running)return;running=false;reconStop();missionStop();reconSetMode(previous);channelLock=previousLock;dragon::toolkit::seconds=oldSeconds;dragon::toolkit::rate=oldRate;strlcpy(result,reason,sizeof(result));}
String path(const char* name){return String("/recipes/")+name+".txt";}
}
bool recipeActive(){return running;}
void recipeStop(){finish("Stopped; radio mode restored");}
void recipeStatus(char* out,size_t capacity){snprintf(out,capacity,"RECON_RECIPE active=%u step=%u total=%u name=%s elapsed=%lu message=%s",running,step,plan.count,recipeName,running?(millis()-began)/1000:0,result);}
void recipeLoop(){using namespace dragon::recipe;using namespace dragon::toolkit;if(!running)return;if(millis()-began>3700000){finish("Runtime limit; radio restored");return;}if(bleLabOwnsRadio()||reconOwnsRadio()&&!status.active){finish("Another radio mode started");return;}if(int32_t(millis()-due)<0||status.saving)return;if(waitTool){if(status.active)return;waitTool=false;}if(step>=plan.count){finish("Completed; radio mode restored");return;}auto s=plan.steps[step++];bool ok=true;char name[33];snprintf(name,sizeof(name),"recipe-%08lx-%u",began,step);
 switch(s.kind){case Wait:due=millis()+s.a*1000;break;case Mode:ok=reconSetMode(dragon::ReconMode(s.a));break;case CaptureWifi:ok=captureStart(false);break;case CaptureBle:ok=captureStart(true);break;case CaptureStop:captureStop();break;case CaptureSave:ok=captureSave();break;case MissionWifi:case MissionBle:ok=missionStart(s.kind==MissionBle,name);break;case MissionStop:missionStop();break;case Tool:seconds=s.b;rate=s.c;ok=start(dragon::tx::AttackMode(s.a));waitTool=ok;due=millis()+s.b*1000;break;case Stop:reconStop();missionStop();break;}
 if(!ok)finish("Step failed; stopped without discarding capture");
}
bool recipeCommand(const String& c){if(!c.startsWith("recipe "))return false;auto* fs=bleLabStorage();
 if(c=="recipe status"){char out[240];recipeStatus(out,sizeof(out));Serial.printf("%s\nRECON_OK\n",out);}
 else if(c=="recipe stop"){recipeStop();Serial.println("RECON_OK");}
 else if(c=="recipe list"){if(fs){File d=fs->open("/recipes");for(File f=d.openNextFile();f;f=d.openNextFile()){String name=f.name();name=name.substring(name.lastIndexOf('/')+1);if(!f.isDirectory()&&name.endsWith(".txt")){name.remove(name.length()-4);if(dragon::lab::safeName(name.c_str()))Serial.printf("RECON_RECIPE_FILE %s\n",name.c_str());}f.close();}d.close();}Serial.println("RECON_OK");}
 else if(c.startsWith("recipe put ")){int split=c.indexOf(' ',11);String name=split>0?c.substring(11,split):"";uint8_t data[513];size_t n=0;dragon::recipe::Plan check;bool ok=!running&&fs&&dragon::lab::safeName(name.c_str())&&split>0&&dragon::lab::decodeHex(c.c_str()+split+1,data,512,n)&&dragon::recipe::parse((char*)data,n,check)&&!fs->exists(path(name.c_str()))&&bleLabStorageSpace(32768);
  if(ok){fs->mkdir("/recipes");File d=fs->open("/recipes");unsigned count=0;for(File f=d.openNextFile();f;f=d.openNextFile()){++count;f.close();}d.close();ok=count<32;}
  if(ok){File f=fs->open(path(name.c_str())+".part",FILE_WRITE);ok=f&&f.write(data,n)==n;f.flush();f.close();if(ok)ok=fs->rename(path(name.c_str())+".part",path(name.c_str()));if(!ok)fs->remove(path(name.c_str())+".part");}Serial.println(ok?"RECON_OK saved; explicit Run required":"RECON_ERROR invalid recipe, duplicate name, or storage");
 }
 else if(c.startsWith("recipe run ")){String name=c.substring(11);bool ok=fs&&dragon::lab::safeName(name.c_str())&&!running&&!missionActive()&&!bleLabOwnsRadio()&&!reconOwnsRadio()&&!reconStorageBusy()&&!dragon::toolkit::status.recording&&radioEnabled;
  if(ok){File f=fs->open(path(name.c_str()));uint8_t data[513];size_t n=f?f.size():0;ok=n&&n<=512&&f.read(data,n)==n&&dragon::recipe::parse((char*)data,n,plan);f.close();}
  if(ok){previous=reconMode();previousLock=channelLock;oldSeconds=dragon::toolkit::seconds;oldRate=dragon::toolkit::rate;strlcpy(recipeName,name.c_str(),sizeof(recipeName));step=0;running=true;waitTool=false;due=began=millis();strlcpy(result,"Running reviewed recipe",sizeof(result));}Serial.println(ok?"RECON_OK running":"RECON_ERROR recipe or busy");
 }
 else if(c.startsWith("recipe read ")||c.startsWith("recipe remove ")){bool remove=c.startsWith("recipe remove ");String name=c.substring(remove?14:12);bool ok=fs&&!running&&dragon::lab::safeName(name.c_str());if(ok&&remove)ok=fs->remove(path(name.c_str()));else if(ok){File f=fs->open(path(name.c_str()));ok=f&&f.size()<=512;if(ok){Serial.print("RECON_RECIPE_HEX ");while(f.available())Serial.printf("%02x",f.read());Serial.println();}f.close();}Serial.println(ok?"RECON_OK":"RECON_ERROR file or busy");}
 else Serial.println("RECON_ERROR recipe command");return true;
}
#endif
