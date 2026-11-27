"""Exercise the shared production contact update with synthetic survey and BLE reports."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
radio = (root / "firmware/src/radio.cpp").read_text(encoding="utf-8")
start = radio.index("static bool noteSignature(")
end = radio.index("static void checkBleBurst(", start)
bookkeeping = radio[start:end]

setup = r"""
#include <cassert>
#ifdef NDEBUG
#error Native test assertions must be enabled
#endif
#include "contacts.h"
#include "alert_policy.h"
#include "nearby.h"
using namespace dragon;
static DetectorSettings detector;
static NearbyRadios nearby;
static VisitLedger ledger;
static Visit slots[32];
static bool watched=false,ignored=false,axonPriority=false;
static unsigned logs=0,first_matches=0,refreshes=0,decisions=0;
static uint32_t now=0;
struct {bool alerted[4]{};uint32_t lastAlert[4]{};} bleBursts;
constexpr int RuleWatch=2,RuleIgnore=4;
uint16_t alertRevision(){return 7;}
DeviceKey contactKey(const Contact& c){return keyOf(c);}
unsigned deviceRules(const DeviceKey&){return (watched?RuleWatch:0)|(ignored?RuleIgnore:0);}
void visitTouch(const DeviceKey& k,uint32_t at){ledger.touch(k,at);}
void logContact(const Contact&,const char*){++logs;}
void historyObserve(Contact&,Family,Confidence,const char*){++first_matches;}
void historyRefresh(Contact&,Family,Confidence,const char*){++refreshes;}
void emitDetection(Contact& c,Family family,Confidence,const char*){
    ++decisions;
    ledger.claim(keyOf(c),unsigned(family),now);
}
#ifndef __APPLE__
static size_t copy_name(char* dest,const char* src,size_t cap){
    const size_t n=strlen(src);if(cap){const size_t used=std::min(n,cap-1);memcpy(dest,src,used);dest[used]=0;}return n;
}
#define strlcpy copy_name
#endif
"""
checks = r"""
int main(){
    ledger.begin(slots,32);
    Contact ap;
    const Match unknown{Family::Wifi,Confidence::None,"survey"};
    auto apSurvey=[&](uint32_t at){
        now=at;update(ap,{at,-55,6,1},"Test AP",unknown,6,true);
    };
    watched=true;
    apSurvey(1000);assert(ap.packets==1&&ap.channel==6&&logs==1&&decisions==0);
    apSurvey(9000);assert(ap.packets==2&&logs==1&&decisions==1&&ledger.notices==1);
    apSurvey(16000);assert(ap.packets==3&&decisions==1&&refreshes==1);
    apSurvey(33000);assert(ap.packets==4&&logs==2&&decisions==1);

    Contact ble;ble.ble=true;watched=false;
    const Match match{Family::Ble,Confidence::Strong,"synthetic advertisement"};
    now=34000;update(ble,{now,-62,0,29},"Fixture",match);
    assert(ble.packets==29&&ble.matchHits==1&&first_matches==1&&logs==3&&decisions==1);
    now=36500;update(ble,{now,-58,0,34},"",match);
    assert(ble.packets==63&&ble.matchHits==2&&first_matches==1&&decisions==2);
    assert(!strcmp(ble.name,"Fixture")&&ledger.notices==2);
    now=39000;update(ble,{now,-61,0,5},"",match);
    assert(ble.packets==68&&decisions==2&&ledger.notices==2);
    ble.packets=UINT32_MAX-1;
    now=41500;update(ble,{now,-63,0,59},"",match);assert(ble.packets==UINT32_MAX);
    for(unsigned i=1;i<=3;++i){
        nearby.listen(1800,8000*i);observeNearby(ble);nearby.tick();
        assert(nearby.near==(i==3));
    }
    assert(nearby.clock==5400);
    ignored=true;ble.mac[5]=2;observeNearby(ble);assert(nearby.pending==0);
}
"""
with TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    source = tmp / "check.cpp"
    source.write_text(setup + bookkeeping + checks)
    exe = tmp / ("check.exe" if sys.platform == "win32" else "check")
    subprocess.run([sys.executable, "-m", "ziglang", "c++", "-std=c++17", "-O1", "-UNDEBUG",
                    "-I" + str(root / "firmware/include"), "-I" + str(root / "lib/opendroneid/src"),
                    str(source), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print("Contact counts, signal and channel updates, first matches, quiet periods, and log throttling passed.")
