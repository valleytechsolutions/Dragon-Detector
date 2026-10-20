"""Opt-in Windows test of one synthetic tracker service; no real trackers contacted.

Requires PyWinRT and pyserial. A random advertised service UUID identifies only this
process's peripheral. Run each protocol in a fresh process so Windows releases
the previous GATT service. Private transcripts stay in ignored .work.
"""
import argparse
import asyncio
import json
import time
import uuid
from pathlib import Path

from test_ble_lab_live import Board
from winrt.windows.devices.bluetooth.genericattributeprofile import (
    GattServiceProvider, GattLocalCharacteristicParameters,
    GattCharacteristicProperties as P, GattProtectionLevel,
    GattServiceProviderAdvertisingParameters, GattWriteOption,
)
from winrt.windows.storage.streams import DataWriter, DataReader

PROTOCOLS = {
    'airtag': ('7dfc9000-7d1c-4951-86aa-8d9728f8d66c', '7dfc9001-7d1c-4951-86aa-8d9728f8d66c', 'af'),
    'fmna': ('0000fd44-0000-1000-8000-00805f9b34fb', '4f860003-943b-49ef-bed4-2f730304427a', '010003'),
    'dult': ('15190001-12f4-c226-88ed-2ac5579f2a85', '8e0c0001-1d68-fb92-bf61-48377421680e', '0003'),
}


def buffer(data):
    writer = DataWriter()
    writer.write_bytes(data)
    return writer.detach_buffer()


async def run(args):
    board = Board(args.port)
    log = {'protocol': args.protocol, 'transcript': [], 'writes': []}
    provider = None
    marker_provider = None
    errors = []
    case = 'acknowledged'
    loop = asyncio.get_running_loop()

    async def cmd(command, marker='LAB_OK'):
        board.port.reset_input_buffer()
        board.port.write((command + '\n').encode())
        result = ''
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            result += board.port.read(board.port.in_waiting).decode(errors='replace')
            if marker in result or 'LAB_ERROR' in result:
                break
            await asyncio.sleep(.025)
        log['transcript'].append({'command': command, 'response': result})
        assert marker in result and 'LAB_ERROR' not in result, (command, result)
        assert 'Guru Meditation' not in result and 'Rebooting' not in result
        return result

    async def idle():
        for _ in range(35):
            result = await cmd('lab status', 'LAB_END')
            if 'state=IDLE owned=0' in result:
                return result
            await asyncio.sleep(1)
        raise AssertionError(result)

    async def select(marker):
        for _ in range(15):
            result = await cmd('lab adverts', 'LAB_END')
            for line in result.splitlines():
                fields = line.split()
                if fields and fields[0] == 'LAB_AD' and marker.hex() in ''.join(fields[5:]):
                    await cmd('lab select ' + ' '.join(fields[1:3]))
                    return
            await asyncio.sleep(2)
        raise AssertionError('Own random service UUID was not observed')

    try:
        info=await cmd('badge info', 'BADGE_INFO')
        assert 'BADGE_INFO 0.13.' in info and ('M5Stack StopWatch' in info or 'Waveshare' in info), info
        if args.repeat: assert 'BADGE_INFO 0.13.22 ' in info, info
        before = next(line for line in (await cmd('badge checksum', 'BADGE_CHECKSUM')).splitlines() if line.startswith('BADGE_CHECKSUM '))
        board.port.write(b'screen radar\n')
        await asyncio.sleep(1)
        assert 'owned=0 ' in await cmd('lab status', 'LAB_END')
        status = await cmd('tool status', 'MODE_STATUS')
        assert 'active=0 ' in status and 'recording=0 ' in status
        service_id, char_id, opcode = PROTOCOLS[args.protocol]
        result = await GattServiceProvider.create_async(uuid.UUID(service_id))
        assert result.error == 0, result.error
        provider = result.service_provider
        params = GattLocalCharacteristicParameters()
        params.characteristic_properties = P.WRITE | (P.INDICATE if args.protocol != 'airtag' else P.NONE)
        params.write_protection_level = GattProtectionLevel.PLAIN
        params.read_protection_level = GattProtectionLevel.PLAIN
        result = await provider.service.create_characteristic_async(uuid.UUID(char_id), params)
        assert result.error == 0, result.error
        characteristic = result.characteristic

        async def handle(event, defer):
            try:
                request = await event.get_request_async()
                assert request is not None
                reader = DataReader.from_buffer(request.value)
                data = bytearray(reader.unconsumed_buffer_length)
                reader.read_bytes(data)
                log['writes'].append({'case': case, 'hex': data.hex(), 'subscribers': len(characteristic.subscribed_clients)})
                assert data.hex() == opcode
                assert request.option == GattWriteOption.WRITE_WITH_RESPONSE
                request.respond()
            except Exception as error:
                errors.append(repr(error))
            finally:
                defer.complete()
            if args.protocol == 'airtag':
                return
            try:
                await asyncio.sleep(.15)
                response = b'\x02\x03\x00\x03' + (b'\x01\x00' if case == 'rejected' else b'\x00\x00')
                await characteristic.notify_value_async(buffer(response if args.protocol == 'dult' else b'\x01\x00'))
                if case == 'completed':
                    await asyncio.sleep(.15)
                    await characteristic.notify_value_async(buffer(b'\x03\x03'))
            except Exception as error:
                errors.append(repr(error))

        def write_requested(sender, event):
            asyncio.run_coroutine_threadsafe(handle(event, event.get_deferral()), loop)

        token = characteristic.add_write_requested(write_requested)
        marker_id = uuid.uuid4()
        marker = marker_id.bytes[::-1]
        ad = GattServiceProviderAdvertisingParameters()
        ad.is_connectable = True
        ad.is_discoverable = False
        provider.start_advertising_with_parameters(ad)
        result = await GattServiceProvider.create_async(marker_id)
        assert result.error == 0, result.error
        marker_provider = result.service_provider
        ad.is_discoverable = True
        marker_provider.start_advertising_with_parameters(ad)
        if args.repeat:
            await select(marker)
            await cmd('lab sound repeat')
            deadline=time.monotonic()+90
            while time.monotonic()<deadline:
                await asyncio.sleep(1)
                if len(log['writes'])>=2:
                    status=await cmd('lab status','LAB_END')
                    if 'state=IDLE owned=0' in status:break
            assert len(log['writes'])>=2, log
            assert not errors, errors
            status=await cmd('lab beeper status','LAB_END')
            assert 'active=1 all=0 targets=1' in status, status
            if args.touch_stop:
                board.port.write(b'screen findmy-beeper\n');await asyncio.sleep(.6)
                await cmd('touch inject 290 418 1','TOUCH_INJECT_OK')
                await asyncio.sleep(.1)
                await cmd('touch inject 290 418 0','TOUCH_INJECT_OK')
            else: await cmd('lab stop')
            await idle()
            count=len(log['writes'])
            await asyncio.sleep(15)
            assert len(log['writes'])==count, 'New sound write after Stop'
            status=await cmd('lab beeper status','LAB_END')
            assert 'active=0' in status and 'current=-1' in status, status
            assert before.strip() in await cmd('badge checksum','BADGE_CHECKSUM')
            print('PASS repeated synthetic', args.protocol, '/ sequential writes / Stop prevents another attempt / badge retained', flush=True)
            log['result']='PASS';return
        cases = [args.case] if args.protocol == 'dult' else ['acknowledged']
        for case in cases:
            count = len(log['writes'])
            for attempt in range(3):
                await select(marker)
                await cmd('lab sound')
                result = await idle()
                if len(log['writes']) > count or 'connection failed' not in result:
                    break
                print('Retry synthetic peripheral connection', attempt + 1, flush=True)
                await asyncio.sleep(4)
            assert not errors, errors
            assert len(log['writes']) == count + 1, log['writes']
            record = log['writes'][-1]
            assert record['hex'] == opcode
            if args.protocol != 'airtag':
                assert record['subscribers'] == 1, record
                assert len(characteristic.subscribed_clients) == 0
            expected = {'accepted': 'start accepted', 'rejected': 'rejected by tracker (status 1)', 'completed': 'reported sound completed'}.get(case, 'write acknowledged')
            assert expected in result, result
            print('PASS', args.protocol, case, 'single write / response / disconnect / radar restored', flush=True)
            await asyncio.sleep(2)
        assert before.strip() in await cmd('badge checksum', 'BADGE_CHECKSUM')
        log['result'] = 'PASS'
    finally:
        try:
            await cmd('lab stop')
            await idle()
        finally:
            board.port.close()
            if provider:
                provider.stop_advertising()
            if marker_provider:
                marker_provider.stop_advertising()
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(log, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--touch-stop', action='store_true', help='Exercise the StopWatch Stop button through its LVGL input driver')
    parser.add_argument('--repeat', action='store_true', help='Verify two repeating requests and cancellation on 0.13.22')
    parser.add_argument('--protocol', choices=PROTOCOLS, required=True)
    parser.add_argument('--case', choices=['accepted', 'rejected', 'completed'], default='accepted')
    parser.add_argument('--output', type=Path, required=True)
    asyncio.run(run(parser.parse_args()))
