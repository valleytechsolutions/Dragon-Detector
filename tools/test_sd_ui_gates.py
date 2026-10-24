"""Preprocess production UI/commands for every profile, not a duplicated menu."""
from pathlib import Path
import re,subprocess,sys
root=Path(__file__).resolve().parents[1];work=root/'.work/ui-gates';work.mkdir(parents=True,exist_ok=True)
boards={'watcher':(['BOARD_WATCHER'],True,True),'spotpear':([],True,False),'waveshare':(['BOARD_AMOLED_175C'],False,False),'waveshare_175':(['BOARD_AMOLED_175'],True,False),'waveshare_175_gps':(['BOARD_AMOLED_175','BOARD_GPS_LC76G'],True,True),'xiao_round':(['BOARD_XIAO_ROUND'],True,False),'lilygo_175':(['BOARD_LILYGO_175'],True,False),'stopwatch':(['BOARD_STOPWATCH'],False,False),'elecrow_21':(['BOARD_CROWPANEL_21'],False,False),'waveshare_lite':(['BOARD_RADAR_LITE'],False,False),'m5dial':(['BOARD_M5DIAL','BOARD_RADAR_LITE'],False,False),'invalid_gps_on_c':(['BOARD_AMOLED_175C','BOARD_GPS_LC76G'],False,False)}
for board,(flags,sd,gps) in boards.items():
    outputs={}
    for name in ['recon_ui.cpp','main.cpp']:
        source=(root/'firmware/src'/name).read_text(encoding='utf-8')
        source='#include "board_pins.h"\n'+re.sub(r'^#include[^\n]*','',source,flags=re.M)
        p=work/name;p.write_text(source,encoding='utf-8')
        outputs[name]=subprocess.check_output([sys.executable,'-m','ziglang','c++','-E','-P','-I'+str(root/'firmware/include')]+['-D'+f for f in flags]+[str(p)],text=True)
    for label in ['"GPS wardrive"','"START WARDRIVE"']:
        assert (label in outputs['recon_ui.cpp'])==gps,(board,label)
    assert ('"resources status"' in outputs['main.cpp'])==sd,board
    print('PASS production UI gates',board)
