"""Exercise the 1.43C profile, pin map and input logic without hardware."""
from pathlib import Path
import argparse
import configparser
import csv
import os
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.work' / 'waveshare143c-tests'
WORK.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--cxx')
args = parser.parse_args()
compiler = [args.cxx] if args.cxx else [sys.executable, '-m', 'ziglang', 'c++']
flags = ['-std=c++17', '-UNDEBUG', '-I' + str(ROOT / 'firmware/include')]
build_env = os.environ.copy()
build_env['ZIG_GLOBAL_CACHE_DIR'] = str(WORK / 'zig-global')
build_env['ZIG_LOCAL_CACHE_DIR'] = str(WORK / 'zig-local')

# The 1.43C is the 8 MB ESP32-S3-PICO-1-N8R8 with octal PSRAM. One 4 MB
# application slot and the rest as user files, ending exactly at 8 MB.
config = configparser.ConfigParser(interpolation=None)
config.read(ROOT / 'platformio.ini')
profile = config['env:waveshare_143c']
assert profile['board_upload.flash_size'] == '8MB'
assert profile['board_build.arduino.memory_type'] == 'qio_opi'
assert '-DBOARD_WAVESHARE_143C' in profile['build_flags']
assert '-DDRAGON_WIFI_CLOCK' in profile['build_flags'], 'no RTC: saved Wi-Fi is the only clock source away from USB'
assert '-<board_waveshare_143c.cpp>' not in profile['build_src_filter']
# The radio adapter needs -zmuldefs, which also hides duplicate board hooks:
# every file that defines them without its own board guard must stay excluded.
assert '-Wl,-zmuldefs' in profile['build_flags']
for name in ('display.cpp', 'board_amoled175c.cpp', 'board_stopwatch.cpp', 'board_xiao_round.cpp',
             'board_waveshare_lite.cpp', 'ui_lite.cpp', 'lite_support.cpp'):
    assert f'-<{name}>' in profile['build_src_filter'], name
assert 'lib_deps' not in profile, 'the driver needs only Wire, esp_lcd and the bundled CO5300 and ES8311 libraries'
for name, other in config.items():
    if name.startswith('env:') and name != 'env:waveshare_143c' and 'build_src_filter' in other:
        assert '-DBOARD_WAVESHARE_143C' not in other.get('build_flags', ''), name
end, partitions = 0, {}
for row in csv.reader((ROOT / profile['board_build.partitions']).read_text().splitlines()):
    if not row or row[0].startswith('#'):
        continue
    offset, size = int(row[3], 0), int(row[4], 0)
    assert offset >= end and offset + size <= 0x800000
    end = offset + size
    partitions[row[0]] = offset, size
assert partitions == {'nvs': (0x9000, 0x5000), 'otadata': (0xe000, 0x2000),
                      'app0': (0x10000, 0x400000), 'spiffs': (0x410000, 0x3f0000)}
assert end == 0x800000
assert profile['board_build.psram_type'] == 'opi'
assert profile['board_build.partitions'] == 'hardware/waveshare-amoled-143c/partitions.csv'
assert 'BOARD_RADAR_LITE' not in profile['build_flags']
# The shared defaults stay 16 MB; only this profile overrides them.
assert config['env']['board_upload.flash_size'] == '16MB'
# Packaging keeps three more copies of "this board is 8 MB" and of its folder,
# and ships the vendor notices for the panel values and the codec driver.
packaging = (ROOT / 'tools/build_release.py').read_text(encoding='utf-8')
assert "'waveshare_143c':('Waveshare ESP32-S3-Touch-AMOLED-1.43C','waveshare-amoled-143c')" in packaging
assert "shutil.copyfile(ROOT/'third_party/waveshare-143c'/name,out/('DISPLAY-'+name))" in packaging
for tool in ('build_release.py', 'validate_release.py'):
    assert re.search(r"0x800000 if board in \([^)]*'waveshare_143c'", (ROOT / 'tools' / tool).read_text(encoding='utf-8')), tool
for name in ('NOTICE.md', 'LICENSE'):
    assert (ROOT / 'third_party/waveshare-143c' / name).is_file(), name
workflow = (ROOT / '.github/workflows/build.yml').read_text(encoding='utf-8')
assert 'python tools/test_waveshare143c.py --cxx g++' in workflow
assert re.search(r'board: \[[^\]]*\bwaveshare_143c\b', workflow)

# The touch mirror in waveshare_143c_input.h is only right for the factory
# 180-degree panel orientation, so the two must change together.
driver = (ROOT / 'firmware/src/board_waveshare_143c.cpp').read_text(encoding='utf-8')
assert re.search(r'\{0x36,\(uint8_t\[\]\)\{0xC0\},1,0\}', driver), 'MADCTL must stay 0xC0 while touch is mirrored'
assert 'esp_lcd_panel_set_gap(panel,LCD_COL_OFFSET,0)' in driver
assert 'io.on_color_trans_done=flushComplete' in driver
# Waveshare's start-up values for this panel, in the vendor's order.
rows = re.findall(r'\{0x([0-9A-Fa-f]{2}),\(uint8_t\[\]\)\{([^}]*)\},(\d+),(\d+)\}', driver)
table = [(int(command, 16), tuple(int(value, 16) for value in data.split(','))[:int(count)], int(delay))
         for command, data, count, delay in rows]
assert [(command, data) for command, data, _ in table] == [
    (0xFE, (0x00,)), (0xC4, (0x80,)), (0x35, (0x00,)), (0x53, (0x20,)), (0x51, (0x00,)), (0x36, (0xC0,)),
    (0x63, (0xFF,)), (0x2A, (0x00, 0x06, 0x01, 0xD7)), (0x2B, (0x00, 0x00, 0x01, 0xD1)), (0x11, ())], table
assert table[-1][2] >= 100, 'sleep-out needs the vendor 100 ms before display-on'
for setting in ('io.spi_mode=0', 'io.pclk_hz=40000000', 'io.lcd_cmd_bits=32', 'io.flags.quad_mode=true',
                'cfg.bits_per_pixel=16', 'esp_lcd_panel_disp_on_off(panel,true)'):
    assert setting in driver, setting
# Every other profile and the Elecrow source glob compile this file too.
conditionals = [line.strip() for line in driver.splitlines() if line.startswith('#if')]
assert conditionals[0] == '#ifdef BOARD_WAVESHARE_143C' and driver.rstrip().endswith('#endif')
# Free internal heap is scarce here. A task of its own for BOOT took 3.5 KiB
# of it and Wi-Fi then refused to join, so the key is sampled from the
# existing system timer task instead.
assert 'xTaskCreate' not in driver and 'esp_timer_start_periodic(timer,10000)' in driver
# Touch has no task either. LVGL reads it once per UI pass, about every 0.2 s
# while the radar redraws, so the driver also reads it while each display
# transfer is in flight and replays a tap that ended between two LVGL reads.
for glue in ('disp_drv.wait_cb=flushWait', 'static void flushWait(lv_disp_drv_t*){touchPoll(millis());}',
             'if(!touchReady||!touchCadence.due(now))return;', 'data->continue_reading=step.more;',
             'touchDevice=lv_indev_drv_register(&indev_drv);', 'lv_indev_wait_release(touchDevice)'):
    assert glue in driver, glue
# The UI ignores a second navigation step until it has built the page, later
# in the same pass, so the key queue is drained one step at a time.
assert 'for(unsigned n=0;n<8&&!navigated&&xQueueReceive(keyQueue,&key,0)==pdTRUE;++n){' in driver
assert 'navigated=waveshare143c::endsDrain(route);' in driver
# The LVGL draw buffer is internal DMA memory too, and wearable.cpp adds a
# rotation buffer of the same size. With the 1.75 boards' 40 rows a Wi-Fi join
# left 31 KB free on the board, under the lab portal's 35,000-byte floor; with
# 32 rows it left 46 KB and the portal started. Buffer, bus limit and LVGL
# size must all follow the one constant.
buffer_rows = int(re.search(r'const size_t rows=(\d+);', driver).group(1))
assert buffer_rows % 2 == 0 and 16 <= buffer_rows <= 32, buffer_rows
for use in ('heap_caps_calloc(rows*SCREEN_RES,sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)',
            'bus.max_transfer_sz=SCREEN_RES*rows*2', 'lv_disp_draw_buf_init(&draw_buf,buffer,NULL,SCREEN_RES*rows)'):
    assert use in driver, use
assert 'heap_caps_malloc(driver->draw_buf->size*sizeof(lv_color_t),rotationCaps)' in (
    ROOT / 'firmware/src/wearable.cpp').read_text(encoding='utf-8'), 'rotation buffer no longer follows the draw buffer'


def text(path):
    return (ROOT / path).read_text(encoding='utf-8')


def condition_before(path, marker, whole_line=False):
    """The innermost preprocessor branch that holds the first line with marker."""
    stack = []
    for line in text(path).splitlines():
        if (line.strip() == marker) if whole_line else (marker in line):
            return stack[-1]
        directive = line.strip()
        if directive.startswith('#if'):
            stack.append(directive)
        elif directive.startswith(('#elif', '#else')):
            stack[-1] = directive
        elif directive.startswith('#endif'):
            stack.pop()
    raise AssertionError(f'{marker!r} not found in {path}')


# The default empty hooks would win the link silently (-zmuldefs).
assert '!defined(BOARD_WAVESHARE_143C)' in condition_before('firmware/src/audio.cpp', 'void boardBegin(){}')
# ES8311 setup, the measured -13 dB codec class and silence before the amplifier.
tone = text('firmware/src/tone_output.cpp')
for marker in ('#include <es8311.h>', 'auto codec=es8311_create(', 'constexpr int codecVolume=65;', 'int16_t silence[128]{};'):
    assert 'defined(BOARD_WAVESHARE_143C)' in condition_before('firmware/src/tone_output.cpp', marker), marker
assert tone.count('defined(BOARD_WAVESHARE_143C)') == 4
# The resident TCP/IP task is started before the Wi-Fi memory check and the
# check uses the lower floor, so a second join in one session is not refused.
network = text('firmware/src/network_tools.cpp')
assert 'defined(BOARD_WAVESHARE_143C)' in condition_before('firmware/src/network_tools.cpp', 'constexpr unsigned minimumHeap=18000;')
assert network.count('BOARD_WAVESHARE_143C') == 2 and '&& !defined(BOARD_WAVESHARE_143C)\n esp_err_t err=esp_netif_init();' in network
# 3.94 MiB of shared storage takes the 1 MiB capture and 256 KiB session
# classes; no RTC is promised; the rotation buffer stays DMA-capable.
for path in ('firmware/src/recon.cpp', 'firmware/src/missions.cpp', 'firmware/src/wearable.cpp', 'firmware/src/ble_lab.cpp'):
    assert 'BOARD_WAVESHARE_143C' not in text(path), path
# One-button navigation is shared with the 1.85B.
assert 'defined(BOARD_WAVESHARE_143C)' in condition_before('firmware/src/ui.cpp', 'void ui185bCycle(){')
assert 'defined(BOARD_WAVESHARE_143C)' in condition_before('firmware/src/ui.cpp', 'static bool uiSideKeyModalOpen(){')
# The font generators must write the same board list as the committed tables,
# or regenerating would silently give this board the 360 px glyphs.
for generator, table in (('tools/encode_ascii_fonts.py', 'firmware/src/ascii_fonts.c'),
                         ('tools/encode_watch_digits.py', 'firmware/src/watch_digits.c')):
    condition = re.search(r"'(#if defined\(BOARD_[^']+)'", text(generator)).group(1)
    assert 'defined(BOARD_WAVESHARE_143C)' in condition and condition + '\n' in text(table), generator


def expanded(path, board):
    """Preprocess one file's own conditionals for a board, without its includes."""
    body = ''.join(line for line in text(path).splitlines(True) if not line.lstrip().startswith('#include'))
    probe = WORK / f'{Path(path).name}.{board}.cpp'
    probe.write_text(f'#define {board} 1\n#define SCREEN_RES 466\n' + body, encoding='utf-8')
    # -dD keeps the #define lines, which is all that lv_conf.h consists of.
    output = subprocess.run(compiler + ['-E', '-P', '-dD', str(probe)],
                            check=True, capture_output=True, text=True, env=build_env).stdout
    return ''.join(line for line in output.splitlines(True) if line.strip() != f'#define {board} 1')


def source_function(path, signature):
    """One production function from a source file, by brace matching."""
    source = text(path)
    start = source.index(signature)
    end, depth = source.index('{', start) + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def ui_function(signature):
    return source_function('firmware/src/ui.cpp', signature)


# Taps sampled before the picture changed are dropped by the touch driver.
# The UI reports a built page and every notice and dialog that appears or goes
# away; on every other board these lines do not exist.
PICTURE_HOOKS = {
    'firmware/src/ui.cpp': ('  waveshare143cPictureChanged(true);\n',
                            '  waveshare143cPictureChanged(false);lv_obj_add_event_cb(notice,[](lv_event_t*){waveshare143cPictureChanged(true);},LV_EVENT_DELETE,nullptr);\n'),
    'firmware/src/ui_theme.cpp': ('  waveshare143cPictureChanged(false);lv_obj_add_event_cb(box,[](lv_event_t*){waveshare143cPictureChanged(true);},LV_EVENT_DELETE,nullptr);\n',),
}
for path, hooks in PICTURE_HOOKS.items():
    assert text(path).count('waveshare143cPictureChanged(') == sum(hook.count('waveshare143cPictureChanged(') for hook in hooks), path
    for hook in hooks:
        assert text(path).count(hook) == 1 and condition_before(path, hook.strip(), True) == '#ifdef BOARD_WAVESHARE_143C', (path, hook)
assert condition_before('firmware/include/ui_theme.h', 'void waveshare143cPictureChanged(bool removed);') == '#ifdef BOARD_WAVESHARE_143C'
assert source_function('firmware/src/ui.cpp', 'static void build(){').count(PICTURE_HOOKS['firmware/src/ui.cpp'][0]) == 1
assert PICTURE_HOOKS['firmware/src/ui.cpp'][1] in source_function('firmware/src/ui.cpp', 'void uiNotify(')
assert PICTURE_HOOKS['firmware/src/ui_theme.cpp'][0] in source_function('firmware/src/ui_theme.cpp', 'lv_obj_t* uiMessageBox(')
# Every dialog is made by uiMessageBox, every notice by uiNotify.
for path in Path(ROOT / 'firmware/src').glob('*'):
    if path.suffix in ('.cpp', '.inc', '.c') and path.name != 'ui_theme.cpp':
        assert 'lv_msgbox_create' not in path.read_text(encoding='utf-8'), path.name
assert text('firmware/src/ui.cpp').count('notice=lv_obj_create(') == 1


def without_hooks(output, path):
    hooks = [hook.strip() for hook in PICTURE_HOOKS.get(path, ())]
    lines = [line for line in output.splitlines(True) if line.strip() not in hooks]
    assert len(output.splitlines()) - len(lines) == len(hooks), path
    return ''.join(lines)


# The 466 px fonts, clock digits and ASCII glyph classes are chosen by board
# name, and a miss in three of them is silent. They must expand exactly as for
# the 1.75C, the other 466 px CO5300 board, apart from the touch hook.
for path in ('firmware/include/lv_conf.h', 'firmware/include/lab_ui_scale.h', 'firmware/src/ascii_fonts.c',
             'firmware/src/watch_digits.c', 'firmware/src/ui_theme.cpp'):
    reference = expanded(path, 'BOARD_AMOLED_175C')
    assert reference.strip() and without_hooks(expanded(path, 'BOARD_WAVESHARE_143C'), path) == reference, path
    assert expanded(path, 'BOARD_NOT_A_BOARD') != reference, path



# No clock chip: SET YOUR CLOCK opens after every power-on without a time
# source, and BOOT is the only key. On this board a press answers "Later" once
# the prompt has been up long enough to have been seen; on the 1.85B, which
# shares the handlers, BOOT still waits. The production handlers and the
# production code that opens the prompt are compiled both ways against the
# same stubs.
assert condition_before('firmware/src/ui.cpp', 'static bool ui143cClockPromptLater(){') == '#ifdef BOARD_WAVESHARE_143C'
assert condition_before('firmware/src/ui.cpp', 'static uint32_t clockPromptOpenedAt=0;') == '#ifdef BOARD_WAVESHARE_143C'
assert condition_before('firmware/src/ui.cpp', '#include "waveshare_143c_input.h"') == '#ifdef BOARD_WAVESHARE_143C'
assert condition_before('firmware/src/clock_settings.inc', 'clockPromptOpenedAt=millis();') == '#ifdef BOARD_WAVESHARE_143C'
ui_text = text('firmware/src/ui.cpp')
page_enum = ui_text[ui_text.index('enum Page {'):ui_text.index(';', ui_text.index('enum Page {')) + 1]
prompt_source = r'''
#include <cassert>
#include <cstdint>
#include "waveshare_143c_input.h"
''' + page_enum + r'''
Page page=Radar,pending=Radar;int menuTab=0;uint32_t lastInteraction=0;
static uint32_t now=0;
uint32_t millis(){return now;}
struct lv_obj_t {};struct lv_event_t {};
enum {LV_EVENT_VALUE_CHANGED,LV_EVENT_DELETE};
lv_obj_t prompt,screen;lv_obj_t* clockPrompt=nullptr;lv_obj_t* root=&screen;
uint32_t clockPromptOpenedAt=0;
unsigned opened=0,closed=0;bool otherDialog=false,timeSet=false;
static bool clockPromptShown=false;
static bool wallTime(){return timeSet;}
static void dismissNotice(){}
static int S(int value){return value;}
static lv_obj_t* uiMessageBox(lv_obj_t*,const char* title,const char*,const char* const*,bool){assert(title[0]=='S');++opened;return &prompt;}
static void lv_obj_set_width(lv_obj_t*,int){}
static void lv_obj_center(lv_obj_t*){}
static void lv_obj_add_event_cb(lv_obj_t*,void(*)(lv_event_t*),int,void*){}
static lv_obj_t* lv_event_get_current_target(lv_event_t*){return &prompt;}
static int lv_msgbox_get_active_btn(lv_obj_t*){return 0;}
static void clockTab(int,bool){}
void lv_msgbox_close(lv_obj_t* box){assert(box==&prompt);++closed;}
static bool ui185bModalOpen(){return clockPrompt||otherDialog;}
static void go(Page next){pending=next;}
bool uiStopwatchKey(bool,uint64_t=0){return false;}
''' + source_function('firmware/src/clock_settings.inc', 'static void clockSetupPrompt(){') + '\n' + '\n'.join(
    ui_function(name) for name in (
        'static bool ui143cClockPromptLater(', 'void ui185bHome(', 'void ui185bCycle(')) + r'''
// The prompt as the UI opens it: on the radar, ten seconds after the start,
// once per power-on, while no time is set.
static void reopen(uint32_t at){now=at;clockPromptShown=false;clockPrompt=nullptr;clockSetupPrompt();assert(clockPrompt==&prompt);}
int main(){
  using waveshare143c::ClockPromptGuardMs;
  using waveshare143c::KeyMaxAgeMs;
  now=9999;clockSetupPrompt();assert(!clockPrompt&&opened==0);
  now=10000;clockSetupPrompt();assert(clockPrompt==&prompt&&opened==1);
  now=10300;clockSetupPrompt();assert(opened==1);
#ifdef BOARD_WAVESHARE_143C
  assert(clockPromptOpenedAt==10000);
  // The board's turn comes before the UI's in the next pass, so a press made on
  // the screen before, up to KeyMaxAgeMs ago, meets a prompt nobody has seen
  // yet. It is spent like a press on any dialog and the prompt stays.
  static_assert(ClockPromptGuardMs>KeyMaxAgeMs,"a queued press can be KeyMaxAgeMs old");
  ui185bCycle();assert(closed==0&&clockPrompt==&prompt&&pending==Radar&&lastInteraction==10300);
  now=10000+ClockPromptGuardMs-1;ui185bHome();assert(closed==0&&clockPrompt==&prompt&&pending==Radar);
  // Once it has been seen, a press answers "Later" and does nothing else; the
  // next one steps, and the prompt does not come back until the next power-on.
  now=10000+ClockPromptGuardMs;ui185bCycle();
  assert(closed==1&&!clockPrompt&&pending==Radar&&lastInteraction==now);
  clockSetupPrompt();assert(!clockPrompt&&opened==1);
  ui185bCycle();assert(pending==Menu&&closed==1);
  page=pending=Radar;reopen(30000);now+=ClockPromptGuardMs;ui185bHome();assert(closed==2&&!clockPrompt&&pending==Radar);
  // Every other dialog still makes BOOT wait.
  otherDialog=true;ui185bCycle();ui185bHome();assert(pending==Radar&&closed==2);
  reopen(50000);now+=ClockPromptGuardMs;ui185bCycle();assert(closed==3&&!clockPrompt&&pending==Radar);
  ui185bCycle();assert(pending==Radar&&closed==3);
  // millis() rollover while the prompt is up.
  otherDialog=false;reopen(0xfffffff0u);now=0x10;ui185bCycle();assert(closed==3&&clockPrompt==&prompt);
  now=0xfffffff0u+ClockPromptGuardMs;ui185bCycle();assert(closed==4&&!clockPrompt);
  // Setting the time closes it without a key.
  reopen(70000);timeSet=true;clockSetupPrompt();assert(closed==5&&!clockPrompt);
#else
  ui185bCycle();assert(closed==0&&clockPrompt==&prompt&&pending==Radar);
  now+=60000;ui185bHome();ui185bCycle();assert(closed==0&&clockPrompt==&prompt&&pending==Radar);
  (void)ui143cClockPromptLater;(void)clockPromptOpenedAt;
#endif
}
'''
prompt_path = WORK / 'clock_prompt.cpp'
prompt_path.write_text(prompt_source, encoding='utf-8')
for board_flag in (['-DBOARD_WAVESHARE_143C'], []):
    prompt_exe = WORK / ('clock_prompt%d%s' % (len(board_flag), '.exe' if sys.platform == 'win32' else ''))
    subprocess.run(compiler + flags + board_flag + [str(prompt_path), '-o', str(prompt_exe)], check=True, env=build_env)
    subprocess.run([str(prompt_exe)], check=True)



def driver_function(signature):
    """One production function from the board driver, by brace matching."""
    start = driver.index(signature)
    end, depth = driver.index('{', start) + 1, 1
    while depth:
        depth += (driver[end] == '{') - (driver[end] == '}')
        end += 1
    return driver[start:end]


# The driver's own touch, key and display glue, compiled against a model of
# the touch chip behind the I2C calls the driver makes, the BOOT pad, the
# clock and LVGL's read loop. Time advances in microseconds and every bus
# transfer takes time, so a sample is stamped later than the moment the caller
# looked at the clock.
for wiring in ('Wire.begin(IIC_SDA,IIC_SCL);Wire.setClock(400000);Wire.setTimeOut(20);',
               'for(unsigned attempt=0;attempt<5&&!touchReady;++attempt){touchReady=touchProbe();if(!touchReady)delay(20);}',
               'touchProbedAt=millis();', 'pinMode(BTN_PIN,INPUT_PULLUP);',
               'keyQueue=xQueueCreateStatic(8,sizeof(waveshare143c::KeyEvent),keyQueueStorage,&keyQueueControl);',
               'sampler.callback=keyTick;', 'static void keyTick(void*){pollKey();}',
               'disp_drv.flush_cb=flush;disp_drv.rounder_cb=rounder;', 'io.user_ctx=&disp_drv;io.on_color_trans_done=flushComplete;',
               'displayRegistered=true;'):
    assert wiring in driver, wiring
glue_source = r'''
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <deque>
#include "waveshare_143c_input.h"
#define TP_I2C_ADDR 0x15
#define IRAM_ATTR
static uint64_t clockUs=5000000;
uint32_t millis(){return uint32_t(clockUs/1000);}
struct lv_disp_drv_t {};struct lv_indev_drv_t {};struct lv_indev_t {};
struct lv_point_t {int x=0,y=0;};
struct lv_area_t {int x1=0,y1=0,x2=0,y2=0;};
struct lv_color_t {uint16_t full=0;};
enum {LV_INDEV_STATE_RELEASED,LV_INDEV_STATE_PRESSED};
struct lv_indev_data_t {lv_point_t point;int state=LV_INDEV_STATE_RELEASED;bool continue_reading=false;};
struct {void printf(const char*,...){}} Serial;
static unsigned lvglResets=0,pressLost=0,clicks=0,presses=0,busReads=0;static bool lvglPressed=false,lvglWaiting=false;
static uint64_t clickedUs=0;
// lv_indev.c: after lv_indev_wait_release() presses are ignored until a
// released read, which ends LVGL's press with PRESS_LOST instead of a click.
void lv_indev_wait_release(lv_indev_t*){++lvglResets;lvglWaiting=true;}
// The UI's veto: it swallows the touch that wakes a dark screen.
static bool uiAccepts=true;static unsigned uiTouches=0;
bool uiTouch(bool,int,int){++uiTouches;return uiAccepts;}

// The finger, the touch chip and the bus, behind the two I2C calls the driver
// makes. The chip answers its ID register and, until it is told to stay awake
// (0xFE), answers nothing at all while it is not touched.
using esp_err_t=int;constexpr esp_err_t ESP_OK=0,ESP_FAIL=-1;
enum class Bus {Good,Stuck,Absent};
static Bus bus=Bus::Good;
static bool chipAutoSleep=true,chipIgnoresAwake=false;
static unsigned failReads=0,shortReads=0,failWrites=0,idReads=0;
static uint64_t downUs=0,upUs=0,down2Us=0,up2Us=0,busUs=300;
static bool fingerDown(){return (clockUs>=downUs&&clockUs<upUs)||(clockUs>=down2Us&&clockUs<up2Us);}
// Normally the finger rests on (233,144). A slide moves it from y 300 to 180.
static bool sliding=false;static uint64_t slideFromUs=0,slideToUs=0;
static uint16_t fingerY(){
  if(!sliding)return 144;
  const uint64_t t=clockUs<slideFromUs?slideFromUs:clockUs>slideToUs?slideToUs:clockUs;
  return uint16_t(300-(t-slideFromUs)*120/(slideToUs-slideFromUs));
}
static esp_err_t i2cWriteReadNonStop(uint8_t port,uint16_t address,const uint8_t* out,size_t outSize,uint8_t* in,size_t inSize,uint32_t timeoutMs,size_t* got){
  assert(port==0&&address==TP_I2C_ADDR&&outSize==1&&timeoutMs==20);
  assert((out[0]==0xA7&&inSize==1)||(out[0]==0x02&&inSize==5));
  *got=0;++(out[0]==0x02?busReads:idReads);
  if(bus==Bus::Stuck){clockUs+=uint64_t(timeoutMs)*1000;return ESP_FAIL;}
  if(bus==Bus::Absent||(chipAutoSleep&&!fingerDown())){clockUs+=60;return ESP_FAIL;}
  if(out[0]==0x02&&failReads){--failReads;clockUs+=60;return ESP_FAIL;}
  clockUs+=busUs;
  // One byte short, with what looks like a finger in the bytes that did arrive.
  if(out[0]==0x02&&shortReads){--shortReads;in[0]=1;in[1]=0;in[2]=100;in[3]=0;*got=4;return ESP_OK;}
  if(out[0]==0xA7){in[0]=0xb7;*got=1;return ESP_OK;}
  const uint16_t rawX=466-1-233,rawY=uint16_t(466-1-fingerY());
  in[0]=fingerDown()?1:0;in[1]=uint8_t(rawX>>8);in[2]=uint8_t(rawX);in[3]=uint8_t(rawY>>8);in[4]=uint8_t(rawY);
  *got=5;return ESP_OK;
}
static esp_err_t i2cWrite(uint8_t port,uint16_t address,const uint8_t* out,size_t size,uint32_t timeoutMs){
  assert(port==0&&address==TP_I2C_ADDR&&size==2&&timeoutMs==20);
  if(bus==Bus::Stuck){clockUs+=uint64_t(timeoutMs)*1000;return ESP_FAIL;}
  if(bus==Bus::Absent||(chipAutoSleep&&!fingerDown())||failWrites){if(failWrites)--failWrites;clockUs+=60;return ESP_FAIL;}
  clockUs+=busUs;
  if(out[0]==0xFE&&out[1]&&!chipIgnoresAwake)chipAutoSleep=false;
  return ESP_OK;
}

static waveshare143c::TouchState touch;
static waveshare143c::TouchCadence touchCadence;
static waveshare143c::TouchInput touchInput;
static lv_indev_t device;static lv_indev_t* touchDevice=&device;
static bool touchReady=false,touchAwake=false;static uint8_t touchChip=0;
static uint32_t touchSamples=0,touchErrors=0,touchDowns=0,touchReplayed=0,touchStale=0,touchDropped=0,touchProbedAt=0;
static bool touchDiscard=false,touchRedrawn=true,panelDark=false;
static uint32_t refreshes=0,touchPictureAt=0;
''' + '\n'.join(driver_function(name) for name in (
    'static bool busRead(', 'static bool busWrite(', 'static bool touchProbe(',
    'static void touchPoll(', 'static void flushWait(', 'static void touchRead(',
    'void waveshare143cPictureChanged(')) + r'''

// The panel: a transfer is queued, and its end is reported from the SPI
// interrupt, which is what hands the draw buffer back to LVGL.
using esp_lcd_panel_handle_t=void*;using esp_lcd_panel_io_handle_t=void*;struct esp_lcd_panel_io_event_data_t {};
static esp_lcd_panel_handle_t panel=nullptr;
static bool panelFails=false;static unsigned ready=0;static int drawn[4];
static esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t,int x1,int y1,int x2,int y2,const void*){
  drawn[0]=x1;drawn[1]=y1;drawn[2]=x2;drawn[3]=y2;return panelFails?ESP_FAIL:ESP_OK;
}
static void lv_disp_flush_ready(lv_disp_drv_t*){++ready;}
// LVGL marks the last transfer of a refresh.
static bool lastChunk=false;
static bool lv_disp_flush_is_last(lv_disp_drv_t*){return lastChunk;}
static uint32_t flushes=0,flushErrors=0;static volatile bool displayRegistered=false;
// The panel's brightness command (Arduino's constrain).
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
static int panelToken;static esp_lcd_panel_io_handle_t panelIo=&panelToken;
static int sentCommand=0;static int sentBrightness=-1;
static esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int command,const void* data,size_t size){
  assert(io==panelIo&&size==1);sentCommand=command;sentBrightness=*static_cast<const uint8_t*>(data);return ESP_OK;
}
''' + '\n'.join(driver_function(name) for name in (
    'static void rounder(', 'static void flush(', 'static bool IRAM_ATTR flushComplete(', 'void set_brightness(')) + r'''

// LVGL: one pointer read per timer call, repeated while the driver asks.
// `clickChanges` stands for a click handler that changes the picture, as
// DISMISS does when it deletes the notice it is on.
static bool clickChanges=false,recordReads=false;static int reads[16];static unsigned readCount=0;
static void lvglRead(){
  lv_indev_data_t data;
  // LVGL keeps reading while the driver asks: every run must end, after at
  // most a press and a release per queued edge.
  unsigned run=0;
  do{
    assert(++run<=2*waveshare143c::TouchInput::Capacity+1);
    data=lv_indev_data_t{};
    touchRead(nullptr,&data);
    const bool pressed=data.state==LV_INDEV_STATE_PRESSED;
    if(lvglWaiting){
      if(!pressed){if(lvglPressed)++pressLost;lvglPressed=lvglWaiting=false;}
      continue;
    }
    if(recordReads&&readCount<16)reads[readCount++]=pressed?data.point.y:-1;
    if(pressed){assert(data.point.x==233&&(sliding||data.point.y==144));if(!lvglPressed)++presses;}
    else if(lvglPressed){++clicks;clickedUs=clockUs;if(clickChanges)waveshare143cPictureChanged(true);}
    lvglPressed=pressed;
  }while(data.continue_reading);
}
// A radar pass as measured on the board: 15 transfers of 32 rows, each waited
// for about 1.5 ms after 12.5 ms of drawing, then the rest of the loop.
static lv_disp_drv_t radarDisplay;
static void radarPass(){
  lvglRead();
  for(int chunk=0;chunk<15;++chunk){
    clockUs+=12500;
    lv_area_t rows{0,chunk*32,465,chunk*32+31};lv_color_t pixels[1];
    lastChunk=chunk==14;flush(&radarDisplay,&rows,pixels);lastChunk=false;
    for(const uint64_t done=clockUs+1500;clockUs<done;clockUs+=20)flushWait(nullptr);
  }
  clockUs+=5000;
}
// A page that redraws nothing: LVGL's own read is the only one.
static void quietPass(){lvglRead();clockUs+=50000;}

static uint64_t releaseSlackUs=20000;
template<typename Pass> static void sweep(Pass pass,uint64_t passUs,std::initializer_list<uint32_t> lengthsMs){
  for(uint32_t length:lengthsMs){
    for(uint64_t phase=0;phase<passUs;phase+=137){
      pass();pass();
      const unsigned before=clicks,pressesBefore=presses;
      downUs=clockUs+phase;upUs=downUs+uint64_t(length)*1000;
      while(clockUs<upUs+3*passUs)pass();
      assert(clicks==before+1&&presses==pressesBefore+1&&!lvglPressed);
      // The click arrives with the first LVGL read after the finger lifted (or
      // after a sleeping controller's release timeout), not a pass later.
      assert(clickedUs>=upUs&&clickedUs-upUs<=passUs+releaseSlackUs);
    }
  }
  assert(lvglResets==0&&touchStale==0);
}

// BOOT pulls its pad low. The system timer calls the driver's pollKey() every
// 10 ms; the queue holds eight events.
constexpr int LOW=0,HIGH=1,BTN_PIN=0;
static bool bootDown=false;
static int digitalRead(int pin){assert(pin==BTN_PIN);return bootDown?LOW:HIGH;}
static uint64_t stopwatchNow(){return clockUs;}
static std::deque<waveshare143c::KeyEvent> keys;
using QueueHandle_t=std::deque<waveshare143c::KeyEvent>*;
static QueueHandle_t keyQueue=&keys;
constexpr int pdTRUE=1;
static int xQueueReceive(QueueHandle_t queue,waveshare143c::KeyEvent* out,int){if(queue->empty())return 0;*out=queue->front();queue->pop_front();return pdTRUE;}
static int xQueueSend(QueueHandle_t queue,const waveshare143c::KeyEvent* event,int){if(queue->size()>=8)return 0;queue->push_back(*event);return pdTRUE;}
static void xQueueReset(QueueHandle_t queue){queue->clear();}
static waveshare143c::KeyDebounce keyDebounce;
static waveshare143c::KeyClicks keyClicks;
static bool keySampler=true;
static std::atomic<bool> timerInputMode{false};
static std::atomic<uint32_t> keyPresses{0},keyDrops{0};
static uint32_t keyCycles=0,keyHomes=0,keyTimerClicks=0;
// The UI as the driver sees it.
static unsigned page=0,pending=0,timerEvents=0;static bool timerPage=false,timerSingle=false;static uint64_t timerAtUs=0;
bool ui185bTimerVisible(){return timerPage;}
void ui185bCycle(){if(page!=pending)return;pending=page+1;}
void ui185bHome(){pending=100;timerPage=false;}
bool uiStopwatchKey(bool single,uint64_t at){++timerEvents;timerSingle=single;timerAtUs=at;return true;}
static void pollCharger(uint32_t){}
''' + '\n'.join(driver_function(name) for name in ('static void pollKey(', 'void boardLoop(')) + r'''
static void uiBuilds(){page=pending;}
static void keyTicks(uint32_t ms,bool down){bootDown=down;for(uint32_t t=0;t<ms;t+=10){clockUs+=10000;pollKey();}}

int main(){
  using namespace waveshare143c;
  // A controller that does not answer at start is asked again every two
  // seconds, from LVGL's read, and nothing else is read from it until then.
  touchProbedAt=millis();bus=Bus::Absent;
  downUs=clockUs+500000;upUs=downUs+300000;
  for(const uint64_t until=clockUs+5000000;clockUs<until;)radarPass();
  assert(!touchReady&&idReads>=2&&idReads<=3&&busReads==0&&presses==0&&touchSamples==0&&touchErrors==0);
  // When it answers, it is identified and told to stay awake, and taps count.
  bus=Bus::Good;downUs=clockUs+1000;upUs=downUs+2500000;
  while(clockUs<upUs+300000)radarPass();
  assert(touchReady&&touchChip==0xb7&&touchAwake&&!chipAutoSleep&&presses==1&&clicks==1&&idReads<=5);
  // The order to stay awake can fail after the chip has answered. The driver
  // remembers that, because such a chip reports no lift (see the stall below).
  failWrites=1;assert(touchProbe()&&!touchAwake&&touchChip==0xb7&&failWrites==0);
  assert(touchProbe()&&touchAwake);
  const unsigned idReadsWhenFound=idReads;

  // Radar: every tap of 20 ms or more is exactly one click, wherever it starts.
  sweep(radarPass,15*14000+5000,{20,30,50,80,100,150,200,250,400});
  assert(touchReplayed>0&&touchErrors==0&&idReads==idReadsWhenFound);
  // A slower bus read crosses more millisecond boundaries.
  busUs=900;sweep(radarPass,15*14000+5000,{20,50,100,250});busUs=300;
  // A quiet page is read every 50 ms: taps longer than that are never lost.
  sweep(quietPass,50000,{60,80,100,150,250,400});
  assert(touchErrors==0);
  // A controller that falls asleep again answers only while touched. Its
  // release is the driver's own timeout, and taps still count.
  chipAutoSleep=true;releaseSlackUs=40000+TouchHoldMs*1000;
  sweep(radarPass,15*14000+5000,{30,50,100,250});
  chipAutoSleep=false;releaseSlackUs=20000;
  assert(touchErrors>0);

  // The UI swallows the touch that wakes a dark screen: LVGL sees no press.
  radarPass();radarPass();
  unsigned clicksAt=clicks,pressesAt=presses;const unsigned touchesAt=uiTouches;
  uiAccepts=false;downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(clicks==clicksAt&&presses==pressesAt&&!lvglPressed&&uiTouches>touchesAt);
  uiAccepts=true;downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(clicks==clicksAt+1&&presses==pressesAt+1);

  // One bad read after a pass that did not sample for 221 ms (a history
  // write) is not an outage: a held touch stays one touch.
  quietPass();quietPass();
  clicksAt=clicks;pressesAt=presses;
  downUs=clockUs+1000;upUs=downUs+1200000;
  quietPass();quietPass();assert(lvglPressed);
  clockUs+=221000;failReads=1;
  while(clockUs<upUs+300000)quietPass();
  assert(presses==pressesAt+1&&clicks==clicksAt+1&&!lvglPressed&&failReads==0);
  // A read that comes back short is a failed read, whatever its bytes say.
  quietPass();
  {const uint32_t errorsAt=touchErrors;const unsigned shortAt=presses;
   shortReads=3;for(int pass=0;pass<4;++pass)quietPass();
   assert(shortReads==0&&touchErrors==errorsAt+3&&presses==shortAt&&!lvglPressed);}
  // An outage that lasts does release it, once.
  downUs=clockUs+1000;upUs=downUs+3000000;
  quietPass();quietPass();assert(lvglPressed);
  bus=Bus::Absent;for(int pass=0;pass<8;++pass)quietPass();
  assert(!lvglPressed&&presses==pressesAt+2&&clicks==clicksAt+2);
  bus=Bus::Good;upUs=clockUs;quietPass();quietPass();

  // Input that waited through a stall of more than a second is not replayed.
  // A tap: no click, and LVGL's pointer is reset.
  radarPass();radarPass();
  clicksAt=clicks;pressesAt=presses;
  downUs=clockUs+30000;upUs=downUs+80000;radarPass();
  clockUs+=1200000;radarPass();radarPass();
  assert(clicks==clicksAt&&presses==pressesAt&&lvglResets==1&&touchStale==1&&!lvglPressed);
  // A press LVGL already holds, released just before the stall: cancelled.
  downUs=clockUs+1000;upUs=downUs+600000;radarPass();radarPass();assert(lvglPressed&&presses==pressesAt+1);
  while(clockUs<upUs+20000){flushWait(nullptr);clockUs+=20;}
  clockUs+=1200000;radarPass();
  assert(!lvglPressed&&clicks==clicksAt&&lvglResets==2&&touchStale==2);
  // A finger that went down before the stall and is still down: nothing until
  // it lifts, and the next tap is a tap again.
  radarPass();pressesAt=presses;
  downUs=clockUs+30000;upUs=downUs+2500000;radarPass();
  clockUs+=1200000;radarPass();radarPass();
  assert(presses==pressesAt&&!lvglPressed&&lvglResets==3&&touchStale==3);
  while(clockUs<upUs+300000)radarPass();
  assert(presses==pressesAt&&clicks==clicksAt&&!lvglPressed);
  downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(presses==pressesAt+1&&clicks==clicksAt+1&&lvglResets==3);
  // Nothing is read during the stall itself. A finger that landed in it and is
  // still down cannot be dated: nothing until it lifts, then touch is back.
  pressesAt=presses;clicksAt=clicks;
  downUs=clockUs+600000;upUs=downUs+2500000;
  clockUs+=1200000;radarPass();radarPass();
  assert(presses==pressesAt&&!lvglPressed&&lvglResets==4&&touchStale==4);
  while(clockUs<upUs+300000)radarPass();
  assert(presses==pressesAt&&clicks==clicksAt&&!lvglPressed);
  downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(presses==pressesAt+1&&clicks==clicksAt+1&&lvglResets==4);
  // A press LVGL holds whose lift fell into the stall: cancelled, not clicked.
  downUs=clockUs+1000;upUs=downUs+700000;radarPass();radarPass();assert(lvglPressed&&presses==pressesAt+2);
  clockUs+=1200000;radarPass();
  assert(!lvglPressed&&clicks==clicksAt+1&&lvglResets==5&&touchStale==5);
  downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(presses==pressesAt+3&&clicks==clicksAt+2&&lvglResets==5);
  // A stall of less than a second is not one: the finger found down is a press.
  downUs=clockUs+300000;upUs=downUs+900000;
  clockUs+=700000;radarPass();assert(lvglPressed&&presses==pressesAt+4&&lvglResets==5);
  while(clockUs<upUs+300000)radarPass();
  assert(clicks==clicksAt+3&&!lvglPressed);
  // The stall with a controller that could not be kept awake. It answers only
  // while touched and never reports a lift: its silence is the lift, and touch
  // must come back at once.
  chipAutoSleep=true;touchAwake=false;radarPass();radarPass();
  clicksAt=clicks;pressesAt=presses;
  downUs=clockUs+30000;upUs=downUs+80000;radarPass();
  clockUs+=1200000;radarPass();radarPass();
  assert(clicks==clicksAt&&presses==pressesAt&&lvglResets==6&&!lvglPressed);
  for(int tap=0;tap<5;++tap){
    downUs=clockUs+1000;upUs=downUs+90000;
    while(clockUs<upUs+500000)radarPass();
  }
  assert(presses==pressesAt+5&&clicks==clicksAt+5&&lvglResets==6);
  // One that accepted the order to stay awake and sleeps all the same: silence
  // could be a fault, so a tap counts again after TouchMaxAgeMs of it.
  touchAwake=true;pressesAt=presses;clicksAt=clicks;
  downUs=clockUs+30000;upUs=downUs+80000;radarPass();
  clockUs+=1200000;radarPass();radarPass();
  assert(lvglResets==7&&presses==pressesAt);
  downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(presses==pressesAt&&clicks==clicksAt&&!lvglPressed);
  for(const uint64_t until=clockUs+1500000;clockUs<until;)radarPass();
  downUs=clockUs+1000;upUs=downUs+90000;
  while(clockUs<upUs+500000)radarPass();
  assert(presses==pressesAt+1&&clicks==clicksAt+1&&lvglResets==7);
  chipAutoSleep=false;radarPass();radarPass();

  // The cancel ends a press LVGL holds with PRESS_LOST, so nothing stays
  // drawn pressed: the two presses above whose lift fell into a stall.
  assert(pressLost==2);
  // A stall whose first read fails: the edge found a read later is still as
  // old as the stall, so a press whose lift fell into it is not clicked.
  radarPass();radarPass();
  {const unsigned at=clicks,lostAt=pressLost;
   downUs=clockUs+1000;upUs=downUs+700000;radarPass();radarPass();assert(lvglPressed);
   clockUs+=1200000;failReads=1;radarPass();radarPass();
   assert(!lvglPressed&&clicks==at&&pressLost==lostAt+1&&failReads==0);
   // Held into a stall, lifted and pressed again inside it, and found down
   // after it: the press LVGL holds is not trusted to be the same touch.
   downUs=clockUs+1000;upUs=downUs+500000;down2Us=upUs+300000;up2Us=down2Us+1500000;
   radarPass();radarPass();assert(lvglPressed);
   clockUs+=1200000;radarPass();
   assert(!lvglPressed&&pressLost==lostAt+2);
   while(clockUs<up2Us+300000)radarPass();
   assert(clicks==at&&!lvglPressed);
   downUs=clockUs+1000;upUs=downUs+90000;while(clockUs<upUs+500000)radarPass();
   assert(clicks==at+1);
  }

  // A page built, or a notice or dialog that comes or goes, changes the
  // picture under the finger. A tap sampled while the new picture was being
  // sent to the panel was made on the old one: it is dropped, not clicked.
  radarPass();radarPass();
  {const uint32_t droppedAt=touchDropped,staleAt=touchStale;const unsigned resetsAt=lvglResets,lostAt=pressLost;
   clicksAt=clicks;pressesAt=presses;
   waveshare143cPictureChanged(true);                         // build(), before the pass's read
   downUs=clockUs+30000;upUs=downUs+80000;radarPass();        // tapped during the first draw
   radarPass();radarPass();
   assert(clicks==clicksAt&&presses==pressesAt&&touchDropped==droppedAt+1&&lvglResets==resetsAt+1&&touchStale==staleAt);
   // Once it is on the panel, taps count again.
   downUs=clockUs+30000;upUs=downUs+80000;radarPass();radarPass();radarPass();
   assert(clicks==clicksAt+1&&presses==pressesAt+1&&touchDropped==droppedAt+1);
   // A tap made on the old picture and still waiting: a new page has deleted
   // what it was meant for, so it is dropped. A notice or dialog that only
   // appears has no place on the panel yet, and the tap reaches what it was
   // made on. After the redraw nothing is left to drop.
   downUs=clockUs+30000;upUs=downUs+80000;radarPass();
   waveshare143cPictureChanged(true);radarPass();radarPass();
   assert(clicks==clicksAt+1&&touchDropped==droppedAt+2);
   downUs=clockUs+30000;upUs=downUs+80000;radarPass();
   waveshare143cPictureChanged(false);radarPass();
   assert(clicks==clicksAt+2&&presses==pressesAt+2&&touchDropped==droppedAt+2);
   radarPass();radarPass();assert(clicks==clicksAt+2&&touchDropped==droppedAt+2);
   // A finger held while the picture changes under it: LVGL's press ends
   // without a click, and the finger must lift before it can press again.
   downUs=clockUs+1000;upUs=downUs+900000;radarPass();radarPass();assert(lvglPressed&&presses==pressesAt+3);
   waveshare143cPictureChanged(true);radarPass();
   assert(!lvglPressed&&pressLost==lostAt+1&&touchDropped==droppedAt+3);
   while(clockUs<upUs+300000)radarPass();
   assert(presses==pressesAt+3&&clicks==clicksAt+2&&!lvglPressed&&touchDropped==droppedAt+3);
   downUs=clockUs+1000;upUs=downUs+90000;while(clockUs<upUs+500000)radarPass();
   assert(presses==pressesAt+4&&clicks==clicksAt+3&&touchStale==staleAt);
   // A click that changes the picture (DISMISS deletes its notice) drops the
   // rest of its read run: two quick taps there are one click.
   clickChanges=true;
   downUs=clockUs+30000;upUs=downUs+50000;down2Us=upUs+60000;up2Us=down2Us+50000;
   radarPass();radarPass();radarPass();
   assert(clicks==clicksAt+4&&presses==pressesAt+5&&touchDropped==droppedAt+4);
   clickChanges=false;radarPass();radarPass();
   downUs=clockUs+30000;upUs=downUs+50000;down2Us=upUs+60000;up2Us=down2Us+50000;
   radarPass();radarPass();radarPass();
   assert(clicks==clicksAt+6&&presses==pressesAt+7&&touchDropped==droppedAt+4);
   // A picture change with nothing to drop costs nothing.
   waveshare143cPictureChanged(false);radarPass();radarPass();waveshare143cPictureChanged(true);radarPass();radarPass();
   assert(touchDropped==droppedAt+4&&lvglResets==resetsAt+4);
  }
  // The panel command is the 0..100 level scaled to 0..255.
  set_brightness(100);assert(sentCommand==0x02005100&&sentBrightness==255);
  set_brightness(50);assert(sentBrightness==127);
  // On a dark panel the UI swallows the touch that wakes it and lights the
  // panel in the next pass. A second tap made while it was still dark is
  // dropped instead of clicking what was under it.
  radarPass();radarPass();
  {const uint32_t droppedAt=touchDropped;clicksAt=clicks;pressesAt=presses;
   set_brightness(0);assert(sentBrightness==0);radarPass();radarPass();
   uiAccepts=false;downUs=clockUs+30000;upUs=downUs+70000;radarPass();  // tap 1, sampled
   down2Us=clockUs+60000;up2Us=down2Us+70000;radarPass();             // tap 1 swallowed; tap 2 sampled in the dark
   uiAccepts=true;set_brightness(100);radarPass();radarPass();        // lit before the next read
   assert(clicks==clicksAt&&presses==pressesAt&&touchDropped==droppedAt+1&&sentBrightness==255);
   downUs=clockUs+30000;upUs=downUs+70000;radarPass();radarPass();radarPass();
   assert(clicks==clicksAt+1&&presses==pressesAt+1);
   // Dimmed is not dark: changing the level drops nothing.
   downUs=clockUs+30000;upUs=downUs+70000;radarPass();
   set_brightness(8);radarPass();set_brightness(100);radarPass();
   assert(clicks==clicksAt+2&&touchDropped==droppedAt+1);
  }
  // A flick between two reads reaches LVGL as a press where it was first
  // sampled, a press where it lifted and the release, so LVGL sees the
  // movement and scrolls instead of clicking where the finger landed.
  radarPass();radarPass();
  sliding=true;slideFromUs=clockUs+30000;slideToUs=slideFromUs+60000;downUs=slideFromUs;upUs=slideToUs+10000;
  radarPass();readCount=0;recordReads=true;radarPass();recordReads=false;
  assert(readCount==3&&reads[0]>=250&&reads[1]==180&&reads[2]==-1&&!lvglPressed);
  radarPass();sliding=false;

  // A stuck bus costs its 20 ms timeout about four times a second, not once
  // per display transfer, and never becomes a press.
  radarPass();const unsigned readsBefore=busReads,pressesBefore=presses;
  bus=Bus::Stuck;const uint64_t stuckFrom=clockUs;
  while(clockUs<stuckFrom+2000000)radarPass();
  assert(busReads-readsBefore>=6&&busReads-readsBefore<=10&&presses==pressesBefore&&!lvglPressed);
  bus=Bus::Good;radarPass();radarPass();
  downUs=clockUs+1000;upUs=downUs+80000;const unsigned clicksBefore=clicks;
  while(clockUs<upUs+500000)radarPass();
  assert(clicks==clicksBefore+1&&idReads==idReadsWhenFound);

  // The display: the window is rounded to even starts and odd ends, a queued
  // transfer keeps LVGL's buffer until the interrupt reports its end, and a
  // transfer that could not be queued gives the buffer back at once.
  lv_area_t area{3,5,10,20};rounder(nullptr,&area);
  assert(area.x1==2&&area.y1==4&&area.x2==11&&area.y2==21);
  lv_disp_drv_t display;lv_color_t pixels[4];
  flushComplete(nullptr,nullptr,&display);assert(ready==0&&flushes==0); // start-up blanking: LVGL does not own it yet
  displayRegistered=true;
  flush(&display,&area,pixels);
  assert(ready==0&&flushErrors==0&&drawn[0]==2&&drawn[1]==4&&drawn[2]==12&&drawn[3]==22);
  assert(!flushComplete(nullptr,nullptr,&display)&&ready==1&&flushes==1);
  panelFails=true;flush(&display,&area,pixels);assert(ready==2&&flushErrors==1);

  // Two quick BOOT presses that reach the same pass are two steps, one per pass.
  const KeyEvent press{KeyAction::Single,0,millis(),false};
  keys.push_back(press);keys.push_back(press);
  boardLoop();assert(pending==1&&keys.size()==1&&keyCycles==1);uiBuilds();
  boardLoop();assert(pending==2&&keys.empty()&&keyCycles==2);uiBuilds();
  boardLoop();assert(page==2&&keyDrops.load()==0);
  // A hold ends the pass too, and what follows waits for the next one.
  KeyEvent hold{KeyAction::Home,0,millis(),false};
  keys.push_back(hold);keys.push_back(press);
  boardLoop();assert(pending==100&&keys.size()==1&&keyHomes==1);uiBuilds();
  boardLoop();assert(pending==101&&keyCycles==3);uiBuilds();
  // Timer clicks do not navigate, so several are applied in one pass.
  timerPage=true;const KeyEvent click{KeyAction::Single,123,millis(),true};
  keys.push_back(click);keys.push_back(click);keys.push_back(click);
  boardLoop();assert(keys.empty()&&timerEvents==3&&keyTimerClicks==3&&timerSingle&&timerAtUs==123);
  // A stale event is counted and does not end the pass.
  timerPage=false;KeyEvent stale{KeyAction::Single,0,millis()-KeyMaxAgeMs-1,false};
  keys.push_back(stale);keys.push_back(press);
  boardLoop();assert(keys.empty()&&keyDrops.load()==1&&keyCycles==4);uiBuilds();

  // The pad itself, through the driver's sampler: idle is high and no press.
  boardLoop();assert(!timerInputMode.load());
  keyTicks(200,false);assert(keys.empty()&&keyPresses.load()==0);
  // A 10 ms glitch is not a press.
  keyTicks(10,true);keyTicks(100,false);assert(keys.empty()&&keyPresses.load()==0);
  // A tap steps as soon as it is released.
  keyTicks(80,true);assert(keyPresses.load()==1&&keys.empty());
  keyTicks(60,false);assert(keys.size()==1);
  boardLoop();assert(keyCycles==5&&keys.empty());uiBuilds();
  // Held for 0.7 s: the main menu, once, and nothing more on release.
  keyTicks(800,true);assert(keys.size()==1&&keyPresses.load()==2);
  boardLoop();assert(keyHomes==2&&pending==100);uiBuilds();
  keyTicks(300,false);boardLoop();assert(keyHomes==2&&keyCycles==5&&keys.empty());
  // On the timer the driver tells the recogniser, which waits for a possible
  // second click and keeps the time of the press.
  timerPage=true;boardLoop();assert(timerInputMode.load());
  unsigned timerAt=timerEvents;
  const uint64_t pressedUs=clockUs;
  keyTicks(60,true);keyTicks(100,false);
  boardLoop();assert(timerEvents==timerAt);
  keyTicks(300,false);boardLoop();
  assert(timerEvents==timerAt+1&&timerSingle&&timerAtUs>pressedUs&&timerAtUs<=pressedUs+40000&&keyTimerClicks==4);
  // Two clicks are one double click, with the time of the first.
  const uint64_t firstUs=clockUs;
  keyTicks(60,true);keyTicks(80,false);keyTicks(60,true);keyTicks(60,false);boardLoop();
  assert(timerEvents==timerAt+2&&!timerSingle&&timerAtUs>firstUs&&timerAtUs<=firstUs+40000);
  keyTicks(600,false);boardLoop();assert(timerEvents==timerAt+2);
  // A hold opens the main menu from the timer too, and the recogniser hears
  // that the timer is gone before this pass ends.
  keyTicks(800,true);boardLoop();
  assert(keyHomes==3&&!timerPage&&!timerInputMode.load());uiBuilds();
  keyTicks(300,false);boardLoop();assert(keys.empty());
  // Leaving the timer by touch is heard at the start of the next pass.
  timerPage=true;boardLoop();assert(timerInputMode.load());
  timerPage=false;boardLoop();assert(!timerInputMode.load());
  // More events than the queue holds: it is emptied and the loss is counted.
  const uint32_t dropsAt=keyDrops.load();
  for(int n=0;n<8;++n)keys.push_back(press);
  keyTicks(80,true);keyTicks(60,false);
  assert(keys.empty()&&keyDrops.load()==dropsAt+1);
  // Without the timer the key is polled from the main loop instead.
  keySampler=false;bootDown=true;const uint32_t keyPressesAt=keyPresses.load();
  for(int pass=0;pass<8;++pass){clockUs+=10000;boardLoop();uiBuilds();}
  bootDown=false;const unsigned cyclesAt=keyCycles;
  for(int pass=0;pass<8;++pass){clockUs+=10000;boardLoop();uiBuilds();}
  assert(keyCycles==cyclesAt+1&&keyPresses.load()==keyPressesAt+1);
}
'''
glue_path = WORK / 'glue.cpp'
glue_path.write_text(glue_source, encoding='utf-8')
glue_exe = WORK / ('glue.exe' if sys.platform == 'win32' else 'glue')
subprocess.run(compiler + flags + ['-O1', str(glue_path), '-o', str(glue_exe)], check=True, env=build_env)
subprocess.run([str(glue_exe)], check=True)

source = r'''
#define BOARD_WAVESHARE_143C
#include "board_pins.h"
#include "waveshare_143c_input.h"
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
static_assert(SCREEN_RES==466 && BOARD_HAS_AUDIO==1 && BOARD_HAS_SD==0 && BOARD_HAS_GPS==0);
static_assert(waveshare143c::Resolution==SCREEN_RES);
static_assert(LCD_SDIO0==9 && LCD_SDIO1==10 && LCD_SDIO2==11 && LCD_SDIO3==12);
static_assert(LCD_SCLK==14 && LCD_CS==15 && LCD_RESET==13 && LCD_TE==8 && LCD_COL_OFFSET==8);
static_assert(IIC_SDA==47 && IIC_SCL==48 && TP_RESET==16 && TP_INT==17 && TP_I2C_ADDR==0x15 && BTN_PIN==0);
static_assert(AUDIO_I2S_MCK_IO==38 && AUDIO_I2S_BCK_IO==39 && AUDIO_I2S_WS_IO==40 && AUDIO_I2S_DO_IO==41 && AUDIO_I2S_DI_IO==42);
static_assert(AUDIO_MUTE_PIN==46 && AUDIO_LDO_EN==18);
static_assert(BATTERY_ADC_PIN==4 && CHARGE_STATUS_PIN==7 && BOARD_LED_PIN==5);
// 19/20 are native USB and 43/44 the UART0 pads; 26..37 belong to the
// in-package flash and octal PSRAM of the ESP32-S3-PICO-1-N8R8.
constexpr int ownedPins[]={BTN_PIN,LCD_SDIO0,LCD_SDIO1,LCD_SDIO2,LCD_SDIO3,LCD_SCLK,LCD_CS,LCD_RESET,LCD_TE,
  IIC_SDA,IIC_SCL,TP_RESET,TP_INT,AUDIO_I2S_MCK_IO,AUDIO_I2S_BCK_IO,AUDIO_I2S_WS_IO,AUDIO_I2S_DO_IO,AUDIO_I2S_DI_IO,
  AUDIO_MUTE_PIN,AUDIO_LDO_EN,BATTERY_ADC_PIN,CHARGE_STATUS_PIN,BOARD_LED_PIN,19,20,43,44};
constexpr size_t ownedCount=sizeof(ownedPins)/sizeof(*ownedPins);
constexpr bool uniquePins(){for(size_t i=0;i<ownedCount;++i)for(size_t j=0;j<i;++j)if(ownedPins[i]==ownedPins[j])return false;return true;}
constexpr bool noMemoryPins(){for(size_t i=0;i<ownedCount;++i)if(ownedPins[i]>=26&&ownedPins[i]<=37)return false;return true;}
static_assert(uniquePins(),"1.43C peripherals, native USB and UART pads must not share a pin");
static_assert(noMemoryPins(),"GPIO26..37 carry the in-package flash and octal PSRAM");

using namespace waveshare143c;

static void touchChecks(){
  TouchState touch;
  // Raw (0,0) is the corner opposite the picture origin: both axes mirror.
  uint8_t origin[]={1,0,0,0,0};
  assert(touch.update(origin,5,true,10)&&touch.pressed&&touch.x==465&&touch.y==465);
  uint8_t far[]={1,0x01,0xd1,0x01,0xd1}; // raw 465,465
  assert(touch.update(far,5,true,20)&&touch.pressed&&touch.x==0&&touch.y==0);
  uint8_t point[]={1,0x00,100,0x01,0x2c}; // raw x100, y300
  assert(touch.update(point,5,true,30)&&touch.x==365&&touch.y==165);
  uint8_t flagged[]={1,0x80|0x00,100,0x40|0x01,0x2c}; // event bits in the high nibbles are not coordinates
  assert(touch.update(flagged,5,true,40)&&touch.x==365&&touch.y==165);
  uint8_t two[]={2,0x00,100,0x01,0x2c};
  assert(touch.update(two,5,true,50)&&touch.pressed);
  // A failed, short or malformed read keeps a held finger for TouchHoldMs of
  // bad samples, counted from the first of them.
  assert(!touch.update(point,4,true,60)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,179)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,180)&&!touch.pressed);
  // Off-panel coordinates and impossible finger counts never become a press.
  uint8_t offscreenX[]={1,0x01,0xd2,0x00,10}; // raw x466
  assert(!touch.update(offscreenX,5,true,190)&&!touch.pressed);
  uint8_t offscreenY[]={1,0x00,10,0x01,0xd2};
  assert(!touch.update(offscreenY,5,true,200)&&!touch.pressed);
  uint8_t garbage[]={0xff,0xff,0xff,0xff,0xff};
  assert(!touch.update(garbage,5,true,210)&&!touch.pressed);
  uint8_t three[]={3,0x00,100,0x00,100};
  assert(!touch.update(three,5,true,220)&&!touch.pressed);
  // Release is immediate, and a release packet is a good sample.
  assert(touch.update(point,5,true,230)&&touch.pressed);
  uint8_t release[]={0,0x00,100,0x01,0x2c};
  assert(touch.update(release,5,true,231)&&!touch.pressed);
  // The controller is not read while the UI is busy elsewhere. One bad read
  // after such a gap is one bad read, not an outage as long as the gap.
  assert(touch.update(point,5,true,1000)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,1400)&&touch.pressed);
  assert(touch.update(point,5,true,1410)&&touch.pressed);
  // A good sample starts the count again.
  assert(!touch.update(nullptr,0,false,1500)&&touch.pressed);
  assert(touch.update(point,5,true,1600)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,1700)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,1819)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,1820)&&!touch.pressed);
  // Bad samples with no finger down start nothing.
  assert(!touch.update(nullptr,0,false,2000)&&!touch.pressed);
  assert(touch.update(point,5,true,2500)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,2501)&&touch.pressed);
  assert(touch.update(release,5,true,2502)&&!touch.pressed);
  // millis() rollover while a finger is held.
  assert(touch.update(point,5,true,0xffffff00u)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,0xfffffff0u)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,0x67)&&touch.pressed);
  assert(!touch.update(nullptr,0,false,0x68)&&!touch.pressed);
}

// One sample of a finger at picture position (x,y), or of no finger.
static bool feed(TouchState& touch,TouchInput& input,bool down,uint16_t x,uint16_t y,uint32_t now,bool ioOk=true){
  const uint16_t rawX=uint16_t(Resolution-1-x),rawY=uint16_t(Resolution-1-y);
  const uint8_t packet[]={uint8_t(down?1:0),uint8_t(rawX>>8),uint8_t(rawX),uint8_t(rawY>>8),uint8_t(rawY)};
  const bool valid=touch.update(packet,5,ioOk,now);
  input.sample(touch.pressed,touch.x,touch.y,valid,now);
  return valid;
}

static void touchReplayChecks(){
  TouchStep step;
  // Nothing sampled yet, and an idle panel: released, nothing waiting.
  TouchInput idle;
  step=idle.next(100);
  assert(!step.pressed&&!step.more&&!step.cancel);
  { TouchState touch;TouchInput input;
    for(uint32_t t=1000;t<1100;t+=14)feed(touch,input,false,0,0,t);
    step=input.next(1100);assert(!step.pressed&&!step.more&&!step.cancel);

    // A tap that began and ended between two LVGL reads: press at the first
    // contact, then the release, in one read. The finger moved before it
    // lifted, so LVGL is shown it there, still pressed, before the release.
    feed(touch,input,true,100,200,1114);feed(touch,input,true,104,203,1128);feed(touch,input,false,0,0,1142);
    step=input.next(1310);
    assert(step.pressed&&step.more&&!step.cancel&&step.x==100&&step.y==200);
    step=input.next(1310);
    assert(step.pressed&&step.more&&!step.cancel&&step.x==104&&step.y==203);
    step=input.next(1310);
    assert(!step.pressed&&!step.more&&step.x==104&&step.y==203);
    step=input.next(1520);assert(!step.pressed&&!step.more);

    // A held finger: the press once, then its newest position, then the release.
    feed(touch,input,true,50,60,1530);
    step=input.next(1531);assert(step.pressed&&!step.more&&step.x==50&&step.y==60);
    feed(touch,input,true,70,60,1545);feed(touch,input,true,90,60,1559);
    step=input.next(1740);assert(step.pressed&&!step.more&&step.x==90&&step.y==60);
    // Lifted and pressed again elsewhere before the next read: both edges.
    feed(touch,input,false,0,0,1750);feed(touch,input,true,300,310,1764);
    step=input.next(1950);assert(!step.pressed&&step.more);
    step=input.next(1950);assert(step.pressed&&!step.more&&step.x==300&&step.y==310);
    feed(touch,input,false,0,0,1960);
    step=input.next(2160);assert(!step.pressed&&!step.more&&!step.cancel);

    // Two taps between two reads are two presses and two releases.
    feed(touch,input,true,10,10,2170);feed(touch,input,false,0,0,2184);
    feed(touch,input,true,20,20,2198);feed(touch,input,false,0,0,2212);
    bool expected[]={true,false,true,false};
    for(unsigned i=0;i<4;++i){step=input.next(2370);assert(step.pressed==expected[i]&&step.more==(i<3));}
    // A third tap does not fit: the newest pair is forgotten, the order and
    // the final state stay right.
    for(uint32_t t=2400;t<2484;t+=28){feed(touch,input,true,uint16_t(t-2390),30,t);feed(touch,input,false,0,0,t+14);}
    for(unsigned i=0;i<4;++i){step=input.next(2580);assert(step.pressed==expected[i]&&step.more==(i<3));if(i==0)assert(step.x==10);}
    // The same with the finger left down at the end.
    feed(touch,input,true,40,40,2600);feed(touch,input,false,0,0,2614);feed(touch,input,true,41,41,2628);
    feed(touch,input,false,0,0,2642);feed(touch,input,true,42,42,2656);
    bool heldAtEnd[]={true,false,true};
    for(unsigned i=0;i<3;++i){step=input.next(2800);assert(step.pressed==heldAtEnd[i]&&step.more==(i<2));}
    assert(step.x==42&&step.y==42);
    feed(touch,input,false,0,0,2810);step=input.next(2811);assert(!step.pressed&&!step.more);
  }
  // Input that waited longer than TouchMaxAgeMs is not replayed.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,100,100,5000);feed(touch,input,false,0,0,5014);
    step=input.next(5000+TouchMaxAgeMs);                       // still fresh: replayed
    assert(step.pressed&&step.more&&!step.cancel);
    step=input.next(5000+TouchMaxAgeMs);assert(!step.pressed&&!step.more);
    feed(touch,input,false,0,0,7000);feed(touch,input,true,100,100,7000);feed(touch,input,false,0,0,7014);
    step=input.next(7001+TouchMaxAgeMs);                       // too old: discarded, LVGL reset
    assert(!step.pressed&&!step.more&&step.cancel);
    step=input.next(7002+TouchMaxAgeMs);assert(!step.pressed&&!step.cancel);
    feed(touch,input,false,0,0,8990);feed(touch,input,true,120,130,9000); // and the next tap is a tap again
    step=input.next(9001);assert(step.pressed&&!step.cancel&&step.x==120);
    feed(touch,input,false,0,0,9010);step=input.next(9011);assert(!step.pressed);
  }
  // An edge is as old as the sample before it: the finger moved somewhere in
  // between. The controller is only read while the UI runs.
  { TouchState touch;TouchInput input;
    feed(touch,input,false,0,0,1000);feed(touch,input,true,100,100,1300);
    step=input.next(1000+TouchMaxAgeMs);assert(step.pressed&&!step.cancel&&step.x==100);
  }
  { TouchState touch;TouchInput input;
    feed(touch,input,false,0,0,1000);feed(touch,input,true,100,100,1300);
    step=input.next(1001+TouchMaxAgeMs);assert(!step.pressed&&step.cancel);
  }
  // After more than TouchMaxAgeMs without a sample (a stalled UI) a finger
  // found down cannot be dated at all. It is not a press until it has lifted.
  // The pause is measured across a rollover, and a very long one stays long.
  for(uint32_t start:{1000u,0xfffffc00u,0x7ffffc00u}){
    for(uint32_t pause:{TouchMaxAgeMs,TouchMaxAgeMs+1,5000u,0x7fffffffu,0x80000001u,0xfffff000u}){
      TouchState touch;TouchInput input;
      feed(touch,input,false,0,0,start);
      const uint32_t t=start+pause;
      feed(touch,input,true,100,100,t);
      step=input.next(t);
      if(pause<=TouchMaxAgeMs){assert(step.pressed&&!step.cancel&&step.x==100);continue;}
      assert(!step.pressed&&step.cancel&&!step.more);
      feed(touch,input,true,100,100,t+10);step=input.next(t+11);assert(!step.pressed&&!step.cancel);
      feed(touch,input,false,0,0,t+20);step=input.next(t+21);assert(!step.pressed&&!step.cancel);
      feed(touch,input,true,110,120,t+30);step=input.next(t+31);assert(step.pressed&&step.x==110&&step.y==120);
    }
  }
  // A finger that went down before a stall and is still down cannot click what
  // is on the screen now. It has to lift first; then touch works again.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,200,200,1000);
    feed(touch,input,true,200,200,2400);
    step=input.next(2401);assert(!step.pressed&&step.cancel&&!step.more);
    feed(touch,input,true,200,200,2420);
    step=input.next(2421);assert(!step.pressed&&!step.cancel);
    // A release made up by the hold timeout is not a lift.
    feed(touch,input,true,200,200,2430,false);feed(touch,input,true,200,200,2600,false);
    assert(!touch.pressed);
    step=input.next(2601);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,200,200,2610);
    step=input.next(2611);assert(!step.pressed);
    feed(touch,input,false,0,0,2620);
    step=input.next(2621);assert(!step.pressed);
    feed(touch,input,true,210,220,2630);
    step=input.next(2631);assert(step.pressed&&step.x==210&&step.y==220);
  }
  // A controller that answers only while touched reports no lift, only
  // silence. From one that was told to stay awake, silence could be a fault,
  // so it takes TouchMaxAgeMs of it to end the wait. Touch is never lost for good.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,200,200,1000);
    step=input.next(2401);assert(!step.pressed&&step.cancel);
    for(uint32_t t=2410;t<=2540;t+=10)feed(touch,input,false,0,0,t,false);
    assert(!touch.pressed);
    step=input.next(2541);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,210,220,2600);                       // touched again too soon: still waiting
    step=input.next(2601);assert(!step.pressed&&!step.cancel);
    for(uint32_t t=2610;t<=3900;t+=10){
      feed(touch,input,false,0,0,t,false);
      if(t%200==0){step=input.next(t);assert(!step.pressed&&!step.cancel);}
    }
    step=input.next(3901);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,210,220,3910);
    step=input.next(3911);assert(step.pressed&&step.x==210&&step.y==220);
  }
  // The driver knows when the controller could not be kept awake, and then
  // passes its silence on as the lift it is.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,200,200,1000);
    step=input.next(2401);assert(!step.pressed&&step.cancel);
    for(uint32_t t=2410;t<=2540;t+=10){touch.update(nullptr,0,false,t);input.sample(touch.pressed,touch.x,touch.y,!touch.pressed,t);}
    step=input.next(2541);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,210,220,2600);
    step=input.next(2601);assert(step.pressed&&step.x==210&&step.y==220);
  }
  // The lift is noticed when it is sampled, whether or not LVGL read while the
  // panel was quiet, and the quiet time is counted across a rollover.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,50,50,0xfffff800u);
    step=input.next(0xfffff800u+TouchMaxAgeMs+1);assert(!step.pressed&&step.cancel);
    feed(touch,input,true,50,50,0xfffffd00u);
    for(uint32_t t=0xfffffd10u;t!=0x200;t+=16)feed(touch,input,false,0,0,t,false);
    assert(!touch.pressed);
    feed(touch,input,true,60,60,0x200);
    step=input.next(0x201);assert(step.pressed&&!step.cancel&&step.x==60);
  }
  // A real lift counts even when the next touch is sampled before LVGL reads.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,50,50,1000);
    step=input.next(2400);assert(!step.pressed&&step.cancel);
    feed(touch,input,true,50,50,2410,false);feed(touch,input,true,50,50,2600,false); // timed out, not lifted
    assert(!touch.pressed);
    step=input.next(2601);assert(!step.pressed&&!step.cancel);
    feed(touch,input,false,0,0,2610);feed(touch,input,true,60,70,2620);
    step=input.next(2621);assert(step.pressed&&step.x==60&&step.y==70);
  }
  // The finger that was held through the stall can come back from a bus
  // fault without having lifted. That is still not a press.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,50,50,1000);
    step=input.next(2400);assert(!step.pressed&&step.cancel);
    feed(touch,input,true,50,50,2410,false);feed(touch,input,true,50,50,2600,false);
    feed(touch,input,true,50,50,2610);feed(touch,input,false,0,0,2620);
    for(unsigned i=0;i<3;++i){step=input.next(2621);assert(!step.pressed&&!step.more&&!step.cancel);}
  }
  // LVGL holds a press and its release went stale: cancelled, not clicked.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,300,300,1000);
    step=input.next(1001);assert(step.pressed);
    feed(touch,input,false,0,0,1010);
    step=input.next(2500);assert(!step.pressed&&step.cancel);
    step=input.next(2501);assert(!step.pressed&&!step.cancel);
  }
  // The same when the lift is only found after a pause in sampling. The finger
  // is up, so the next tap is a tap.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,300,300,1000);
    step=input.next(1001);assert(step.pressed);
    feed(touch,input,false,0,0,1002+TouchMaxAgeMs);
    step=input.next(1002+TouchMaxAgeMs);assert(!step.pressed&&step.cancel);
    feed(touch,input,true,310,300,2100);
    step=input.next(2101);assert(step.pressed&&!step.cancel&&step.x==310);
  }
  // The sample that finds an edge can be stamped a millisecond after the time
  // the caller read first. That is not a stale edge.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,77,88,3001);
    step=input.next(3000);assert(step.pressed&&!step.cancel&&step.x==77&&step.y==88);
    feed(touch,input,false,0,0,3021);
    step=input.next(3020);assert(!step.pressed&&!step.cancel);
  }
  { TouchState touch;TouchInput input;                          // the same across rollover
    feed(touch,input,true,77,88,0x3);
    step=input.next(0xfffffffeu);assert(step.pressed&&!step.cancel);
  }
  // millis() rollover between a tap and its replay.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,5,6,0xffffff00u);feed(touch,input,false,0,0,0xffffff10u);
    step=input.next(0x40);assert(step.pressed&&step.more&&!step.cancel&&step.x==5&&step.y==6);
    step=input.next(0x40);assert(!step.pressed&&!step.more);
  }

  // A flick that LVGL would see in at most one pressed read: the lift point
  // is passed on as a press before the release, so LVGL sees the movement
  // (and scrolls) instead of clicking the row where the finger landed. Only
  // a release away from the point LVGL was last given gets one.
  { TouchState touch;TouchInput input;
    feed(touch,input,false,0,0,1000);
    feed(touch,input,true,200,300,1010);feed(touch,input,true,200,250,1024);feed(touch,input,true,200,190,1038);
    feed(touch,input,false,0,0,1052);
    step=input.next(1200);assert(step.pressed&&step.more&&step.x==200&&step.y==300);
    step=input.next(1200);assert(step.pressed&&step.more&&step.x==200&&step.y==190);
    step=input.next(1200);assert(!step.pressed&&!step.more&&!step.cancel&&step.x==200&&step.y==190);
    step=input.next(1400);assert(!step.pressed&&!step.more);
    // Pressed in one read and lifted elsewhere before the next.
    feed(touch,input,true,50,400,1410);
    step=input.next(1411);assert(step.pressed&&!step.more&&step.x==50&&step.y==400);
    feed(touch,input,true,120,400,1424);feed(touch,input,false,0,0,1438);
    step=input.next(1600);assert(step.pressed&&step.more&&step.x==120&&step.y==400);
    step=input.next(1600);assert(!step.pressed&&!step.more&&step.x==120);
    // Seen at the lift point already: just the release.
    feed(touch,input,true,60,60,1610);
    step=input.next(1611);assert(step.pressed&&step.x==60);
    feed(touch,input,true,90,60,1624);
    step=input.next(1625);assert(step.pressed&&!step.more&&step.x==90);
    feed(touch,input,false,0,0,1638);
    step=input.next(1800);assert(!step.pressed&&!step.more&&step.x==90);
    // Two flicks between two reads keep their order and their own lift points.
    feed(touch,input,true,10,10,1810);feed(touch,input,true,10,80,1824);feed(touch,input,false,0,0,1838);
    feed(touch,input,true,300,10,1852);feed(touch,input,true,300,90,1866);feed(touch,input,false,0,0,1880);
    const uint16_t ys[]={10,80,80,10,90,90};const bool down[]={true,true,false,true,true,false};
    for(unsigned i=0;i<6;++i){step=input.next(2000);assert(step.pressed==down[i]&&step.y==ys[i]&&step.more==(i<5)&&!step.cancel);}
    // A moved release that went stale is cancelled like any other.
    feed(touch,input,true,200,200,2010);
    step=input.next(2011);assert(step.pressed);
    feed(touch,input,true,230,200,2024);feed(touch,input,false,0,0,2038);
    step=input.next(2038+TouchMaxAgeMs+1);assert(!step.pressed&&!step.more&&step.cancel);
    step=input.next(2038+TouchMaxAgeMs+2);assert(!step.pressed&&!step.cancel);
  }
  // The driver asks to drop what was sampled when the picture under the
  // finger has changed since: a page was built, a notice or dialog came or
  // went, or the panel lit up. It is dropped like stale input.
  { TouchState touch;TouchInput input;
    // Nothing waiting: nothing to cancel, and the next tap is a tap.
    feed(touch,input,false,0,0,1000);
    step=input.next(1001,true);assert(!step.pressed&&!step.more&&!step.cancel);
    // A whole tap waiting: dropped, and the finger is up, so no wait follows.
    feed(touch,input,true,100,100,1010);feed(touch,input,false,0,0,1024);
    step=input.next(1100,true);assert(!step.pressed&&!step.more&&step.cancel);
    step=input.next(1101);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,110,110,1110);
    step=input.next(1111);assert(step.pressed&&step.x==110);
    feed(touch,input,false,0,0,1120);step=input.next(1121);assert(!step.pressed);
    // A press LVGL holds: its press ends without a click and the finger must
    // lift before it can press what is there now.
    feed(touch,input,true,200,210,1200);
    step=input.next(1201);assert(step.pressed);
    feed(touch,input,true,200,210,1214);
    step=input.next(1215,true);assert(!step.pressed&&step.cancel);
    feed(touch,input,true,205,210,1228);
    step=input.next(1229);assert(!step.pressed&&!step.cancel);
    feed(touch,input,false,0,0,1242);
    step=input.next(1243);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,40,50,1256);
    step=input.next(1257);assert(step.pressed&&step.x==40&&step.y==50);
    // A finger that went down after the last read and is still down: dropped too.
    feed(touch,input,false,0,0,1270);step=input.next(1271);assert(!step.pressed);
    feed(touch,input,true,70,80,1284);
    step=input.next(1285,true);assert(!step.pressed&&step.cancel&&!step.more);
    feed(touch,input,false,0,0,1298);feed(touch,input,true,71,81,1312);
    step=input.next(1313);assert(step.pressed&&step.x==71);
  }
  // A pause is not over before a sample says something. After a long pause,
  // a failed read keeps the press it assumed, and the edge found one read
  // later is still as old as the pause.
  { TouchState touch;TouchInput input;                         // the lift
    feed(touch,input,true,90,90,1000);
    step=input.next(1001);assert(step.pressed);
    feed(touch,input,true,90,90,1000+TouchMaxAgeMs+400,false);  // first read after the pause fails
    step=input.next(1000+TouchMaxAgeMs+401);assert(step.pressed&&!step.cancel);
    feed(touch,input,false,0,0,1000+TouchMaxAgeMs+410);
    step=input.next(1000+TouchMaxAgeMs+411);assert(!step.pressed&&step.cancel);
  }
  { TouchState touch;TouchInput input;                         // a controller that sleeps when idle
    feed(touch,input,true,90,90,1000);
    step=input.next(1001);assert(step.pressed);
    for(uint32_t t=2500;t<=2620;t+=10){
      touch.update(nullptr,0,false,t);input.sample(touch.pressed,touch.x,touch.y,!touch.pressed,t);
      step=input.next(t);
      if(touch.pressed)assert(step.pressed&&!step.cancel);
    }
    assert(!touch.pressed&&!step.pressed&&step.cancel);
  }
  { TouchState touch;TouchInput input;                         // the press
    feed(touch,input,false,0,0,1000);
    feed(touch,input,false,0,0,1000+TouchMaxAgeMs+400,false);
    feed(touch,input,true,80,80,1000+TouchMaxAgeMs+410);
    step=input.next(1000+TouchMaxAgeMs+411);assert(!step.pressed&&step.cancel);
    feed(touch,input,true,80,80,1000+TouchMaxAgeMs+420);
    step=input.next(1000+TouchMaxAgeMs+421);assert(!step.pressed&&!step.cancel);
    feed(touch,input,false,0,0,1000+TouchMaxAgeMs+430);feed(touch,input,true,81,80,1000+TouchMaxAgeMs+440);
    step=input.next(1000+TouchMaxAgeMs+441);assert(step.pressed&&step.x==81);
  }
  // A finger LVGL holds and that is found down again after a long pause may
  // have lifted and pressed again, elsewhere too. LVGL's press ends without a
  // click and the finger must lift.
  for(uint16_t x:{uint16_t(150),uint16_t(320)}){
    TouchState touch;TouchInput input;
    feed(touch,input,true,150,150,1000);
    step=input.next(1001);assert(step.pressed);
    feed(touch,input,true,x,150,1000+TouchMaxAgeMs+400);
    step=input.next(1000+TouchMaxAgeMs+401);assert(!step.pressed&&step.cancel&&!step.more);
    feed(touch,input,true,x,150,1000+TouchMaxAgeMs+410);feed(touch,input,false,0,0,1000+TouchMaxAgeMs+420);
    step=input.next(1000+TouchMaxAgeMs+421);assert(!step.pressed&&!step.cancel);
    feed(touch,input,true,60,70,1000+TouchMaxAgeMs+430);
    step=input.next(1000+TouchMaxAgeMs+431);assert(step.pressed&&step.x==60);
  }
  // A shorter pause is not one: the same finger stays pressed.
  { TouchState touch;TouchInput input;
    feed(touch,input,true,150,150,1000);
    step=input.next(1001);assert(step.pressed);
    feed(touch,input,true,150,150,1000+TouchMaxAgeMs);
    step=input.next(1001+TouchMaxAgeMs);assert(step.pressed&&!step.cancel);
    feed(touch,input,false,0,0,1010+TouchMaxAgeMs);
    step=input.next(1011+TouchMaxAgeMs);assert(!step.pressed&&!step.cancel);
  }

  // Read spacing: at once the first time, then every TouchPollMs. A sleeping
  // controller's quick NACK keeps that pace; a bus timeout backs off.
  TouchCadence cadence;
  assert(cadence.due(0)&&cadence.due(123456));
  cadence.done(1000,true,0);
  assert(!cadence.due(1000)&&!cadence.due(1000+TouchPollMs-1)&&cadence.due(1000+TouchPollMs));
  cadence.done(2000,false,0);
  assert(!cadence.due(2000+TouchPollMs-1)&&cadence.due(2000+TouchPollMs));
  cadence.done(3020,false,20);
  assert(!cadence.due(3020+TouchRetryMs-1)&&cadence.due(3020+TouchRetryMs));
  cadence.done(4000,true,1);
  assert(cadence.due(4000+TouchPollMs));
  cadence.done(0xfffffffbu,true,0);
  assert(!cadence.due(0xffffffffu)&&cadence.due(0x5));

  // Every tap of 20 ms or longer becomes exactly one click, whenever it starts
  // in the UI pass. Measured on the board: LVGL reads once per pass, the radar
  // redraw takes 205 ms and shows 71 transfers a second, one read per transfer.
  // Read only once per pass, a 100 ms tap was seen 48 times in 100.
  for(uint32_t length:{20u,30u,50u,80u,100u,150u,200u,250u,400u}){
    for(uint32_t phase=0;phase<210;++phase){
      TouchState touch;TouchInput input;TouchCadence pace;
      const uint32_t downAt=10000+phase,upAt=downAt+length;
      unsigned clicks=0;bool lvglPressed=false;
      auto poll=[&](uint32_t t){
        if(!pace.due(t))return;
        pace.done(t,true,0);
        feed(touch,input,t>=downAt&&t<upAt,233,144,t);
      };
      for(uint32_t pass=9790;pass<downAt+length+630;pass+=210){
        poll(pass);
        unsigned readsInRun=0;
        for(bool more=true;more;){
          assert(++readsInRun<=2*TouchInput::Capacity+1);
          const TouchStep step=input.next(pass);
          assert(!step.cancel);
          if(lvglPressed&&!step.pressed)++clicks;
          if(step.pressed)assert(step.x==233&&step.y==144);
          lvglPressed=step.pressed;more=step.more;
        }
        for(uint32_t t=pass+12;t<pass+205;t+=14)poll(t);
      }
      assert(clicks==1&&!lvglPressed);
    }
  }
}

// The drain rule against a model of the UI: a step sets the wanted page, the
// page is built later in the same pass, and a step is ignored until then.
static unsigned stepsApplied(bool drainStops){
  const KeyEvent press{KeyAction::Single,0,1000,false};
  KeyEvent queue[2]={press,press};
  unsigned waiting=2,page=0,pending=0;
  for(uint32_t pass=0;pass<3;++pass){
    bool navigated=false;
    while(waiting&&!(drainStops&&navigated)){
      const KeyRoute route=routeKey(queue[2-waiting],false,1000+pass*210);
      --waiting;
      if(route==KeyRoute::Cycle&&page==pending)pending=page+1;
      navigated=endsDrain(route);
    }
    page=pending; // the UI builds the page after the board's turn
  }
  return page;
}

struct Rig {
  KeyDebounce debounce;
  KeyClicks clicks;
  uint32_t now=1000;
  // Advances 10 ms at a time like the key task; returns the last event seen.
  KeyEvent run(bool level,uint32_t ms,bool timer){
    KeyEvent last;
    for(uint32_t t=0;t<ms;t+=10){
      now+=10;
      KeyEvent event=clicks.update(debounce.update(level,now),timer,now,uint64_t(now)*1000);
      if(event.action!=KeyAction::None)last=event;
    }
    return last;
  }
};

static void keyChecks(){
  KeyDebounce debounce;
  assert(!debounce.update(true,100)&&!debounce.update(true,124)&&debounce.update(true,125));
  assert(debounce.update(false,130)&&debounce.update(true,140)&&debounce.update(true,200)); // release bounce ignored
  assert(debounce.update(false,300)&&debounce.update(false,324)&&!debounce.update(false,325));
  KeyDebounce wrap;
  assert(!wrap.update(true,0xfffffff8u)&&wrap.update(true,0x11)); // millis() rollover

  // Outside the timer: a tap steps to the next screen as soon as it is released.
  Rig rig;
  assert(rig.run(false,100,false).action==KeyAction::None);
  assert(rig.run(true,80,false).action==KeyAction::None);
  KeyEvent tap=rig.run(false,60,false);
  assert(tap.action==KeyAction::Single&&!tap.timer);
  assert(routeKey(tap,false,rig.now)==KeyRoute::Cycle);
  assert(routeKey(tap,true,rig.now)==KeyRoute::Drop); // touch opened the timer meanwhile
  // Two quick taps are two steps, never a double click.
  rig.run(true,60,false);
  assert(rig.run(false,40,false).action==KeyAction::Single);
  rig.run(true,60,false);
  assert(rig.run(false,40,false).action==KeyAction::Single);
  // Holding opens the main menu once, and releasing adds nothing.
  KeyEvent hold=rig.run(true,800,false);
  assert(hold.action==KeyAction::Home&&routeKey(hold,false,rig.now)==KeyRoute::Home);
  assert(routeKey(hold,true,rig.now)==KeyRoute::Home);
  assert(rig.run(false,400,false).action==KeyAction::None);

  // On the timer: one click waits for a possible second, then starts or pauses
  // with the time of the original press.
  Rig timer;
  timer.run(false,100,true);
  timer.run(true,60,true);
  const uint32_t pressedAt=timer.now-60+10+KeyDebounceMs-5; // first sample at or after the debounce
  KeyEvent single=timer.run(false,400,true);
  assert(single.action==KeyAction::Single&&single.timer);
  assert(single.at>=uint64_t(pressedAt-10)*1000&&single.at<=uint64_t(pressedAt+10)*1000);
  assert(routeKey(single,true,single.queuedAt)==KeyRoute::Timer);
  assert(routeKey(single,false,single.queuedAt)==KeyRoute::Drop); // never navigate with a timer click
  // Two clicks are Lap/Finish or Reset, without a trailing single.
  timer.run(true,60,true);timer.run(false,80,true);timer.run(true,60,true);
  KeyEvent twice=timer.run(false,60,true);
  assert(twice.action==KeyAction::Double&&twice.timer&&routeKey(twice,true,timer.now)==KeyRoute::Timer);
  assert(timer.run(false,600,true).action==KeyAction::None);

  // Stale events and idle polls do nothing; freshness survives rollover.
  KeyEvent idle;
  assert(routeKey(idle,false,0)==KeyRoute::Drop&&routeKey(idle,true,0)==KeyRoute::Drop);
  KeyEvent old{KeyAction::Single,0,1000,false};
  assert(routeKey(old,false,1000+KeyMaxAgeMs)==KeyRoute::Cycle);
  assert(routeKey(old,false,1001+KeyMaxAgeMs)==KeyRoute::Drop);
  KeyEvent wrapped{KeyAction::Home,0,0xfffffff0u,false};
  assert(routeKey(wrapped,false,0x20)==KeyRoute::Home&&routeKey(wrapped,false,0x20+KeyMaxAgeMs)==KeyRoute::Drop);

  // Two quick presses that reach the same UI pass are two steps. Taking both
  // at once would lose the second.
  assert(endsDrain(KeyRoute::Cycle)&&endsDrain(KeyRoute::Home)&&!endsDrain(KeyRoute::Timer)&&!endsDrain(KeyRoute::Drop));
  assert(stepsApplied(true)==2&&stepsApplied(false)==1);
  // The second press can wait one pass for the board's turn and one more for
  // its own. The longest pass seen on the board was 253 ms of drawing plus
  // 221 ms of history writing.
  constexpr uint32_t longestPassMs=253+221;
  assert(KeyMaxAgeMs>=2*longestPassMs);
}

static void powerChecks(){
  ChargeFilter charger;
  for(int i=0;i<3;++i)assert(!charger.update(true));
  assert(charger.update(true)); // one steady second of STAT low
  for(int i=0;i<3;++i)assert(charger.update(false));
  assert(!charger.update(false));
  // A charger with no cell toggles STAT: the reported state must not follow it.
  for(int i=0;i<40;++i)assert(!charger.update(i%2==0));
  for(int i=0;i<4;++i)charger.update(true);
  assert(charger.charging);
  for(int i=0;i<40;++i)assert(charger.update(i%2==0));

  assert(batteryPercentFromMillivolts(0)==-1&&batteryPercentFromMillivolts(2499)==-1);
  assert(batteryPercentFromMillivolts(2500)==0&&batteryPercentFromMillivolts(3300)==0);
  assert(batteryPercentFromMillivolts(3400)==2&&batteryPercentFromMillivolts(3500)==5);
  assert(batteryPercentFromMillivolts(3700)==20&&batteryPercentFromMillivolts(3830)==50);
  assert(batteryPercentFromMillivolts(4150)==100&&batteryPercentFromMillivolts(4200)==100&&batteryPercentFromMillivolts(6600)==100);
  int previous=0;
  for(uint32_t mv=2500;mv<=4400;++mv){
    const int percent=batteryPercentFromMillivolts(mv);
    assert(percent>=previous&&percent<=100);previous=percent;
  }
}
int main(){assert(std::string(BOARD_NAME)=="Waveshare ESP32-S3-Touch-AMOLED-1.43C");touchChecks();touchReplayChecks();keyChecks();powerChecks();}
'''
path = WORK / 'input.cpp'
path.write_text(source, encoding='utf-8')
exe = WORK / ('input.exe' if sys.platform == 'win32' else 'input')
subprocess.run(compiler + flags + [str(path), '-o', str(exe)], check=True, env=build_env)
subprocess.run([str(exe)], check=True)
print('PASS 1.43C 8 MB profile and single-slot partitions; shared-code gates and 466 px font classes; pin map and bus isolation; mirrored touch, stale/invalid/held/rollover checks; quick taps replayed in order, stale and undatable input cancelled until the finger lifts (with PRESS_LOST), taps on a picture that changed or on a dark panel dropped, flicks passed on as movement, read spacing; the driver\'s probe, touch, display and BOOT code against a modelled chip, bus, pad and LVGL; BOOT debounce, cycle/home/timer routing, one step per pass and stale drops; BOOT answers a clock prompt that has been up long enough, here and not on the 1.85B; charger filter; battery curve')
