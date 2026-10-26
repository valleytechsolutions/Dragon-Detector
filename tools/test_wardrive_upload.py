"""Native fault tests of the production uploader, fake SD/HTTP, real cJSON 1.7.18."""
from pathlib import Path
import hashlib, subprocess, sys, urllib.request

root=Path(__file__).resolve().parents[1]
work=root/'.work'/'upload-tests';dep=work/'cjson';dep.mkdir(parents=True,exist_ok=True)
hashes={'cJSON.c':'feec025be377e5f2b8f1d19fcc78527a7fa07ddd326e0dc83ac488647c86e679',
        'cJSON.h':'7cdbc7f241e7cac032fe67bf00b84eb82f52f3194245f6f127fedde2a1051921',
        'LICENSE':'b56ff5bcae5b93a530f9fa8894f41f1edf47c0d526835839f0fb76d5ce9dc971'}
for name,expected in hashes.items():
    path=dep/name
    if not path.exists():
        path.write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/'+name,timeout=30).read())
    # Git checkouts may normalize LF to CRLF. Compare the upstream LF content.
    data=path.read_bytes().replace(b'\r\n',b'\n')
    assert hashlib.sha256(data).hexdigest()==expected, f'Unexpected cJSON source: {name}'

src=(root/'firmware/src/wardrive_upload.cpp').read_text()
production='namespace {'+src.split('namespace {',1)[1].split('\nWardriveUploadSnapshot wardriveUploadSnapshot()',1)[0]
snapshot=(root/'firmware/include/wardrive_upload.h').read_text().split('struct WardriveUploadSnapshot',1)[1].split('#ifdef',1)[0]
source=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <cJSON.h>
#include "wardrive_upload_format.h"
#include "storage_replace.h"
#define DRAGON_VERSION "test"
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
using portMUX_TYPE=int;
constexpr int portMUX_INITIALIZER_UNLOCKED=0,portMAX_DELAY=0,MALLOC_CAP_SPIRAM=0,MALLOC_CAP_8BIT=0,MALLOC_CAP_INTERNAL=0;
int sdMutex=1;bool sdReady=true;
void xSemaphoreTake(int,int){} void xSemaphoreGive(int){} void vTaskDelay(unsigned){} void vTaskDelete(void*){}
unsigned pdMS_TO_TICKS(unsigned n){return n;} unsigned millis(){static unsigned n=0;return n+=250;}
void* heap_caps_calloc(size_t n,size_t size,int){return calloc(n,size);} void* ps_calloc(size_t n,size_t size){return calloc(n,size);}
size_t strlcpy(char* dst,const char* src,size_t cap){size_t n=strlen(src);if(cap){memcpy(dst,src,std::min(n,cap-1));dst[std::min(n,cap-1)]=0;}return n;}
using String=std::string;
constexpr const char* FILE_READ="r";constexpr const char* FILE_WRITE="w";
std::map<std::string,std::string> disk;
bool failOpenWrite=false,shortWrite=false,failFinalReceipt=false;unsigned renamed=0;
struct File {
 std::string path;size_t offset=0,index=0;bool valid=false,dir=false;std::vector<std::string> children;
 explicit operator bool()const{return valid;}bool isDirectory()const{return dir;}const char* name()const{return path.c_str();}
 size_t size()const{return disk.count(path)?disk.at(path).size():0;}bool available()const{return offset<size();}
 bool seek(size_t at){offset=at;return valid;}void close(){valid=false;}void flush(){}
 int read(){return available()?uint8_t(disk[path][offset++]):-1;}
 size_t read(uint8_t* out,size_t n){n=std::min(n,size()-offset);memcpy(out,disk[path].data()+offset,n);offset+=n;return n;}
 size_t write(const uint8_t* in,size_t n){if(shortWrite&&n)--n;disk[path].append((const char*)in,n);return n;}
 File openNextFile(){if(index>=children.size())return {};File f;f.path=children[index++];f.valid=true;return f;}
};
struct FS {
 bool exists(const std::string& p){if(disk.count(p))return true;for(auto& entry:disk)if(entry.first.rfind(p+"/",0)==0)return true;return false;}
 bool mkdir(const char*){return true;}bool remove(const std::string& p){return disk.erase(p);}
 bool rename(const std::string& a,const std::string& b){++renamed;if(failFinalReceipt&&renamed>=2)return false;if(!disk.count(a)||disk.count(b))return false;disk[b]=disk[a];disk.erase(a);return true;}
 File open(const std::string& p,const char* mode=FILE_READ){File f;if(mode==FILE_WRITE){if(failOpenWrite)return f;disk[p]="";}else if(!exists(p))return f;
  f.path=p;f.valid=true;f.dir=!disk.count(p);if(f.dir)for(auto& e:disk)if(e.first.rfind(p+"/",0)==0)f.children.push_back(e.first);return f;}
} CardFS;
namespace dragon {uint32_t crc32(const uint8_t* data,size_t n,uint32_t crc){while(n--){crc^=*data++;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&-(crc&1));}return crc;}}
struct FakeHttp {int code=200;std::string body="{\"success\":true}",written,url;size_t at=0,chunk=7,writeChunk=11;bool truncated=false,readError=false,writeError=false,openError=false,cancelWrite=false;int length=0;};
FakeHttp httpTemplate,httpLast;unsigned opens=0;
using esp_http_client_handle_t=FakeHttp*;
constexpr int ESP_OK=0,HTTP_METHOD_POST=1,HTTP_METHOD_GET=0;
struct esp_http_client_config_t {const char* url;int timeout_ms,buffer_size,buffer_size_tx;void* crt_bundle_attach;bool disable_auto_redirect;int method;};
void* esp_crt_bundle_attach=nullptr;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* c){assert(c->disable_auto_redirect);auto* h=new FakeHttp(httpTemplate);h->url=c->url;return h;}
void esp_http_client_set_header(FakeHttp*,const char*,const char*){}
int esp_http_client_open(FakeHttp* h,int n){++opens;h->length=n;return h->openError?-1:0;}
int esp_http_client_fetch_headers(FakeHttp*){return 0;}int esp_http_client_get_status_code(FakeHttp* h){return h->code;}
bool esp_http_client_is_complete_data_received(FakeHttp* h){return h->at==h->body.size()&&!h->truncated;}
int esp_http_client_read(FakeHttp* h,char* out,int n){if(h->readError)return -1;size_t count=std::min({size_t(n),h->chunk,h->body.size()-h->at});memcpy(out,h->body.data()+h->at,count);h->at+=count;return count;}
int esp_http_client_write(FakeHttp*,const char*,int);
void esp_http_client_close(FakeHttp*){}void esp_http_client_cleanup(FakeHttp* h){httpLast=*h;delete h;}
struct WardriveUploadSnapshot
''' + snapshot+r'''
WardriveUploadSnapshot wardriveUploadSnapshot();
''' + production+r'''
WardriveUploadSnapshot wardriveUploadSnapshot(){return snapshot;}
int esp_http_client_write(FakeHttp* h,const char* data,int n){if(h->cancelWrite)cancelled=true;if(h->writeError)return -1;size_t count=std::min(size_t(n),h->writeChunk);h->written.append(data,count);return count;}
bool line(Config& cfg,const std::string& text){std::vector<char> buf(text.begin(),text.end());buf.push_back(0);return configLine(cfg,buf.data());}
const char* filename="wigle-abcd1234-0000.csv";
const std::string csvPath="/dragon/wardrive/"+std::string(filename);
const std::string receiptBase="/dragon/wardrive-uploads/"+std::string(filename);
const std::string csv="WigleWifi-1.6,appRelease=test\nMAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n02:00:00:00:00:01,\"synthetic test\",[ESS],2026-10-26 12:00:00,1,2412,-60,40.0,-75.0,2,0,,,WIFI\n";
void reset(){disk.clear();disk[csvPath]=csv;httpTemplate=FakeHttp{};httpLast={};opens=renamed=0;shortWrite=failOpenWrite=failFinalReceipt=false;cancelled=workerDone=false;checkJobs=retryUnknown=false;service=Service::Wigle;snapshot={};}
Receipt saved(const char* suffix=".wigle"){Receipt r;assert(receipt(disk.at(receiptBase+suffix).c_str(),r));return r;}
Reply parse(std::string body,int code=200){FakeHttp h;h.body=body;h.code=code;return response(&h);}
int main(){
 Config cfg;assert(line(cfg,"# comment")&&line(cfg,"homeSsid=Test AP")&&line(cfg,"homePsk=12345678")&&networkConfigured(cfg));
 assert(!line(cfg,"homeSsid="+std::string(33,'x'))&&!line(cfg,"homePsk="+std::string(64,'x')));
 assert(line(cfg,"wigleBasicToken=YWJjZGVmZ2g=")&&line(cfg,"wdgwarsApiKey=abcdefgh12345678"));
 assert(!line(cfg,"wigleBasicToken=Basic abcdefgh")&&!line(cfg,"wdgwarsApiKey=abcdefgh\r\nX-Key:x"));
 assert(!line(cfg,"uploadNetworkProfile=8")&&line(cfg,"uploadNetworkProfile=7")&&line(cfg,"autoUpload=yes"));
 assert(leaf(filename)&&!leaf("../wigle-a.csv")&&!leaf("wigle-\"evil.csv"));char small[4];assert(!multipart(small,sizeof(small),"B",filename));
 assert(parse("{\"success\":true}").success&&!parse("{\"success\":false}").success&&!parse("{\"success\":\"true\"}").success);
 assert(!parse("{\"success\":true}garbage").success&&!parse(std::string("{\"success\":true}\0bad",21)).success);
 assert(!parse(std::string(5000,' ')+"{}").complete&&!parse(std::string(13,'[')+std::string(13,']')).success);
 FakeHttp truncated;truncated.truncated=true;assert(!response(&truncated).complete);truncated.readError=true;assert(!response(&truncated).complete);
 service=Service::WdgWars;assert(parse("{\"ok\":true,\"job_id\":42}",202).job==42);
 assert(!parse("{\"ok\":true,\"job_id\":1.5}",202).job&&!parse("{\"job_id\":4294967296}").job);
 assert(parse("{\"job\":{\"status\":\"done\"}}").done&&parse("{\"status\":\"failed\"}").failed);
 reset();worker(nullptr);assert(workerDone&&snapshot.accepted==1&&saved().state==ReceiptState::Accepted);
 assert(httpLast.written.size()==size_t(httpLast.length)&&httpLast.written.find(csv)!=std::string::npos&&httpLast.url=="https://api.wigle.net/api/v2/file/upload");
 snapshot={};worker(nullptr);assert(snapshot.skipped==1&&opens==1); // Restart skips confirmed file.
 service=Service::WdgWars;httpTemplate.code=202;httpTemplate.body="{\"ok\":true,\"job_id\":17}";snapshot={};worker(nullptr);
 assert(snapshot.pending==1&&saved(".wdg").job==17&&opens==2); // Independent destinations.
 checkJobs=true;httpTemplate.code=200;httpTemplate.body="{\"job\":{\"status\":\"done\"}}";snapshot={};worker(nullptr);
 assert(snapshot.accepted==1&&saved(".wdg").state==ReceiptState::Done&&httpLast.url=="https://wdgwars.pl/api/v2/upload-job/17");
 reset();httpTemplate.truncated=true;worker(nullptr);assert(snapshot.failed==1&&saved().state==ReceiptState::Uncertain);
 httpTemplate.truncated=false;snapshot={};worker(nullptr);assert(opens==1&&snapshot.failed==1);retryUnknown=true;worker(nullptr);assert(opens==2&&saved().state==ReceiptState::Accepted);
 reset();disk[receiptBase+".wigle"]="broken";worker(nullptr);assert(!opens&&snapshot.failed==1);retryUnknown=true;worker(nullptr);assert(opens==1&&saved().state==ReceiptState::Accepted);
 reset();disk[receiptBase+".wigle.part"]="interrupted";worker(nullptr);assert(!opens&&snapshot.failed==1);
 reset();worker(nullptr);disk[receiptBase+".wigle.previous"]=disk[receiptBase+".wigle"];disk.erase(receiptBase+".wigle");snapshot={};worker(nullptr);assert(opens==1&&snapshot.skipped==1);
 reset();failOpenWrite=true;worker(nullptr);assert(!opens&&snapshot.failed==1&&strstr(snapshot.message,"Cannot save"));
 reset();shortWrite=true;worker(nullptr);assert(!opens&&snapshot.failed==1);
 reset();failFinalReceipt=true;worker(nullptr);assert(opens==1&&strstr(snapshot.message,"receipt save failed"));
 reset();httpTemplate.writeError=true;worker(nullptr);assert(saved().state==ReceiptState::Uncertain&&snapshot.failed==1);
 reset();httpTemplate.cancelWrite=true;worker(nullptr);assert(saved().state==ReceiptState::Uncertain&&strstr(snapshot.message,"Cancelled"));
 reset();httpTemplate.body="{\"success\":false}";worker(nullptr);assert(!snapshot.accepted&&saved().state==ReceiptState::Uncertain);
 for(int code:{401,403,429}){reset();disk["/dragon/wardrive/wigle-ffff-0000.csv"]=csv;httpTemplate.code=code;worker(nullptr);assert(opens==1&&saved().state==ReceiptState::Failed&&snapshot.failed==1);}
 reset();checkJobs=true;worker(nullptr);assert(!opens&&snapshot.skipped==1);
 reset();disk[csvPath]="WigleWifi-1.6,appRelease=test\ncolumns\n";worker(nullptr);assert(!opens&&snapshot.skipped==1&&disk.count(csvPath));
 reset();disk[csvPath]="not a wardrive\ncolumns\nrow\n";worker(nullptr);assert(!opens&&snapshot.failed==1);
 puts("PASS production uploader: config, multipart partial writes, bounded JSON, WiGLE/WDG responses, queued jobs, per-service receipts, crash recovery, cancellation, auth/rate errors, SD failures and duplicate prevention");
}
'''
path=work/'upload.cpp';path.write_text(source)
cc=[sys.executable,'-m','ziglang'];obj=work/'cJSON.o';exe=work/'upload.exe'
subprocess.run(cc+['cc','-O1','-I'+str(dep),'-c',str(dep/'cJSON.c'),'-o',str(obj)],check=True)
subprocess.run(cc+['c++','-std=c++17','-O1','-I'+str(dep),'-I'+str(root/'firmware/include'),str(path),str(obj),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
