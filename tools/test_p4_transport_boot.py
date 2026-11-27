"""Exercise the exact retained boot-ticket policy and its pre-interaction boundary."""
from pathlib import Path
import subprocess
import sys
import tempfile

project = Path(__file__).resolve().parents[1]
main = (project / "firmware/src/main.cpp").read_text()
setup = main.split("void setup(){",1)[1].split("void loop(){",1)[0]
assert setup.index("p4TransportBootWindow();") < setup.index("uiLoop();")
assert setup.index("p4TransportBootWindow();") < setup.index("bleLabBegin();")
assert main.count("p4TransportBootWindow();") == 1
driver = (project / "firmware/src/p4_coproc.cpp").read_text()
assert driver.index("enumResult.load();",driver.index("__wrap_esp_extconn_sdio_init")) < driver.index("return __real_esp_extconn_sdio_init")
assert "++cmd0Invalid>=dragon::p4::cmd0InvalidLimit" in driver
assert "if(p4TransportRestartBlock())return false;" in driver
for active in ("stopwatch.running","AudioMode::Recording","AudioMode::Playing","missionActive()","usbDriveOn()","wardriveBusy()","captureEnabled"):
    assert active in driver.split("const char* p4TransportRestartBlock(){",1)[1].split("return nullptr;",1)[0]
code = r"""
#include "p4_transport_boot.h"
#include <cassert>
using namespace dragon::p4;
int main(){
    assert(enumerationClock(0,40000)==40000);
    for(unsigned i=1;i<=10;i++)assert(enumerationClock(i,40000)==20000);
    assert(enumerationClock(5,10000)==10000);
    assert(companionEnumerated(true,true,2));
    assert(!companionEnumerated(true,true,1));
    assert(!companionEnumerated(true,false,2));
    assert(!companionEnumerated(false,true,2));
    assert(cmd0InvalidLimit==3);
    assert(transportBootLimitMs>15000 && transportBootLimitMs<25000);
    for(const uint32_t unknown : {0u,1234u,0xFFFFFFFFu}){
        assert(beginTransport(unknown,false)==BootTicket::FirstActive);
    }
    assert(beginTransport(uint32_t(BootTicket::RetryPending),false)==BootTicket::RetryActive);
    assert(beginTransport(uint32_t(BootTicket::FirstActive),false)==BootTicket::RetryActive);
    assert(beginTransport(uint32_t(BootTicket::RetryActive),false)==BootTicket::Blocked);
    assert(beginTransport(uint32_t(BootTicket::Blocked),false)==BootTicket::Blocked);
    assert(beginTransport(uint32_t(BootTicket::Blocked),true)==BootTicket::FirstActive);
    for(auto b : {BootTicket::FirstActive,BootTicket::RetryActive,BootTicket::Blocked}){
        for(bool vendor : {true,false})for(bool failed : {true,false})for(bool timeout : {true,false}){
            assert(retryBeforeInteraction(b,vendor,failed,timeout)==
                   (b==BootTicket::FirstActive&&vendor&&(failed||timeout)));
        }
    }
}
"""
with tempfile.TemporaryDirectory() as temp:
    source=Path(temp)/"test.cpp";source.write_text("#include <initializer_list>\n"+code)
    binary=Path(temp)/("boot.exe" if sys.platform=="win32" else "boot")
    subprocess.run([sys.executable,"-m","ziglang","c++","-UNDEBUG","-std=c++17",
                    "-I",str(project/"firmware/include"),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print("P4 one-shot reboot, crash lockout, SDIO retry and card validation, session guard, and setup boundary passed.")
