"""Opt-in hardware check: clock/timer pause scanning and radar resumes it.

Requires idle radio tools and an empty, stopped timer. Preserves theme, watch,
clock settings and the original page. Only the test's own timer laps are reset.
"""
import argparse
import json
import re
import time
from pathlib import Path

import serial


def values(line):
    return {k: int(v) for k, v in re.findall(r'(\w+)=(-?\d+)', line)}


def run(port, output):
    link = serial.Serial()
    link.port, link.baudrate, link.timeout = port, 115200, .1
    link.dtr = link.rts = False
    link.open()
    proof = {}
    timer_owned = False
    restore = None
    original_mode = None

    def send(command):
        link.write((command + '\n').encode())

    def query(command, prefix):
        for _ in range(3):
            send(command)
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                line = link.readline().decode(errors='replace').strip()
                if any(s in line for s in ('Guru Meditation', 'CORRUPT HEAP', 'assert failed')):
                    raise AssertionError(line)
                if line.startswith(prefix):
                    return line
        raise TimeoutError(command)

    def snapshot():
        return {'scan': values(query('scan status', 'SCAN_STATUS')),
                'health': values(query('status', 'HEALTH')),
                'clock': values(query('clock status', 'CLOCK_STATUS'))}

    def quiet(page):
        send('screen ' + page)
        time.sleep(.7)
        first = snapshot()
        time.sleep(2.2)
        last = snapshot()
        assert last['scan']['standby'] == 1 and last['scan']['enabled'] == 0, (page, last)
        for key in ('wifi_ms', 'ble_ms'):
            assert last['scan'][key] == first['scan'][key], (page, key, first, last)
        for key in ('wifi', 'ble'):
            assert last['health'][key] == first['health'][key], (page, key, first, last)
        assert last['clock']['valid'] and last['clock']['epoch'] > first['clock']['epoch'], (page, last)
        proof[page] = {'before': first, 'after': last}
        print('PASS scanning paused; clock advances:', page, flush=True)

    def resumed():
        send('screen radar')
        time.sleep(.7)
        first = snapshot()
        time.sleep(7)
        last = snapshot()
        assert last['scan']['standby'] == 0 and last['scan']['enabled'] == 1
        for key in ('wifi_ms', 'ble_ms'):
            assert last['scan'][key] > first['scan'][key], (key, first, last)
        assert last['health']['wifi'] + last['health']['ble'] > first['health']['wifi'] + first['health']['ble']
        return {'before': first, 'after': last}

    try:
        identity = query('badge info', 'BADGE_INFO')
        assert re.match(r'BADGE_INFO 0\.13\.(?:1[6-9]|[2-9]\d|\d{3,}) READY', identity.replace('466 ', '').replace('360 ', '').replace('240 ', '')), identity
        lite = 'Lite' in identity
        assert values(query('scan status', 'SCAN_STATUS'))['lab'] == 0, 'Stop active radio sessions first'
        initial_timer = values(query('stopwatch status', 'STOPWATCH_STATUS'))
        assert initial_timer['running'] == initial_timer['elapsed_us'] == initial_timer['laps'] == 0, 'Requires an empty, stopped timer'
        if not lite:
            mode = values(query('tool status', 'MODE_STATUS'))
            assert mode['recording'] == 0, 'Stop capture first'
            assert 'active=0 ' in query('mission status', 'MISSION_STATUS'), 'Stop mission first'
            assert 'active=0 ' in query('recipe status', 'RECON_RECIPE'), 'Stop recipe first'
        original = query('ui status', 'UI_STATUS')
        if lite:
            restore = re.search(r'page=(\w+)', original)[1]
        else:
            page = values(original)['page']
            names = re.search(r'enum Page \{([^}]+)\}', (Path(__file__).resolve().parents[1] / 'firmware/src/ui.cpp').read_text())[1].split(',')
            restore = {'Clock': 'clock', 'Stopwatch': 'stopwatch', 'Laps': 'laps', 'Menu': 'menu',
                       'ClockSettings': 'clock-sync', 'ClockWheels': 'time-settings', 'ClockEdit': 'clock-settings'}.get(names[page], 'radar')
        assert values(query('clock status', 'CLOCK_STATUS'))['valid'] == 1, 'Set current time before testing'
        if not lite:
            original_mode = mode['mode']
            assert query('mode set 0', 'RECON_') == 'RECON_OK'
        proof['identity'] = identity
        proof['radar_before'] = resumed()
        quiet('clock')
        send('resume')
        time.sleep(.3)
        assert values(query('scan status', 'SCAN_STATUS'))['enabled'] == 0, 'Resume bypassed clock pause'
        if not lite:
            for page in ('time-settings', 'date-settings', 'clock-sync', 'laps'):
                quiet(page)
        quiet('stopwatch')
        assert query('stopwatch start', 'STOPWATCH_') == 'STOPWATCH_OK'
        timer_owned = True
        first = values(query('stopwatch status', 'STOPWATCH_STATUS'))
        quiet('stopwatch')
        last = values(query('stopwatch status', 'STOPWATCH_STATUS'))
        assert last['running'] and last['elapsed_us'] - first['elapsed_us'] >= 2_000_000
        assert query('stopwatch lap', 'STOPWATCH_') == 'STOPWATCH_OK'
        assert query('stopwatch pause', 'STOPWATCH_') == 'STOPWATCH_OK'
        paused = values(query('stopwatch status', 'STOPWATCH_STATUS'))
        quiet('stopwatch')
        assert values(query('stopwatch status', 'STOPWATCH_STATUS')) == paused
        proof['timer'] = {'running_before': first, 'running_after': last, 'paused': paused}
        assert query('stopwatch reset', 'STOPWATCH_') == 'STOPWATCH_OK'
        timer_owned = False
        proof['radar_after'] = resumed()
        proof['selftest'] = query('selftest', 'SELFTEST')
        assert 'failed=0' in proof['selftest']
        proof['result'] = 'PASS'
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(proof, indent=2))
        print('PASS timer controls, paused time, scan resume and self-tests', flush=True)
    finally:
        if timer_owned:
            query('stopwatch pause', 'STOPWATCH_')
            query('stopwatch reset', 'STOPWATCH_')
        if original_mode is not None:
            assert query('mode set ' + str(original_mode), 'RECON_') == 'RECON_OK'
        if restore:
            send('screen ' + restore)
        link.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', type=Path, default=Path('.work/clock-power.json'))
    args = parser.parse_args()
    run(args.port, args.output)
