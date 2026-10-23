from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];work=root/'.work/upload-tests';work.mkdir(parents=True,exist_ok=True)
source=r'''
#include "upload_policy.h"
#include <cassert>
#include <string>
using namespace dragon::upload;
int main(){
 Config c;auto parseText=[&](std::string s,bool sd=true,bool gps=true){return parse(s.data(),s.size(),sd,gps,c);};
 const std::string base="config_version=1\nuploads_enabled=true\nterms_reviewed=true\nwigle_enabled=true\nwigle_api_name=synthetic\nwigle_api_token=fixture\n";
 assert(parseText(base)&&c.enabled&&c.wigle&&!c.wdgwars);
 assert(!parseText(base,false,true)&&!c.enabled&&!c.apiToken[0]);
 assert(!parseText(base,true,false)&&!c.enabled);
 assert(!parseText(base+"wigle_enabled=false\n")&&!c.enabled);
 assert(!parseText(base+"endpoint=evil\n")&&!c.apiToken[0]);
 assert(!parseText("config_version=2\n"));assert(!parseText(std::string(2049,'a')));
 assert(!parseText("config_version=1\nuploads_enabled=true\n"));
 assert(!parseText("config_version=1\nwigle_api_token=abc\rX-Evil: yes\n"));
 assert(!parseText("config_version=1\nwigle_api_name=abc:def\n"));
 assert(parseText("config_version=1\nwigle_enabled=true\n")&&!c.enabled&&!c.wigle);
 assert(parseText("config_version=1\nuploads_enabled=true\nterms_reviewed=true\nwdgwars_enabled=true\nwdgwars_api_key="+std::string(64,'a'))&&c.wdgwars);
 assert(result(200,true,true,0).state==State::Accepted);
 assert(result(200,false,true,0).state==State::Uncertain);
 assert(result(0,false,true,0).state==State::Uncertain);
 assert(result(0,false,false,0).state==State::Retry);
 assert(result(401,false,true,0).state==State::Blocked);
 assert(result(403,false,true,0).state==State::Blocked);
 assert(result(413,false,true,0).state==State::Rejected);
 assert(result(302,false,true,0).state==State::Blocked);
 assert(result(429,false,true,0,3600).delaySeconds==3600);
 assert(result(429,false,true,0,100000).state==State::Blocked);
 assert(result(500,false,true,4).delaySeconds==960);
 assert(result(500,false,true,5).state==State::Blocked);
}
'''
p=work/'policy.cpp';p.write_text(source);exe=work/'policy.exe'
subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG','-I'+str(root/'firmware/include'),str(p),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True);print('PASS upload config fail-closed, capability gates, no secret diagnostics, retry/uncertain policy (no networking)')
