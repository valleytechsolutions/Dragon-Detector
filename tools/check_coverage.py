"""Validate the pinned signature-fact ledger and generate native test fixtures.

Optional --reference path audits the upstream tables without compiling or
importing upstream application code. CI needs only our committed fact ledger.
"""
import argparse,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[1]

def generate(reference=None):
    ledger=json.loads((ROOT/'docs/data/squachwatch-coverage.json').read_text())
    entries=ledger['entries']
    expected={'address':97,'uuid':15,'company':7,'name':7,'ssid':8}
    assert {k:sum(e['kind']==k for e in entries) for k in expected}==expected
    assert len({(e['kind'],e['value']) for e in entries})==len(entries)
    if reference:
        source=(pathlib.Path(reference)/'src/signatures.cpp').read_text(encoding='utf8')
        found={'address':set(''.join(f'{int(x,16):02X}' for x in row) for row in re.findall(r'\{\{0x([\dA-Fa-f]+),\s*0x([\dA-Fa-f]+),\s*0x([\dA-Fa-f]+)\}',source))}
        for table,kind in [('kUuidTable','uuid'),('kMfgIdTable','company'),('kBtClassicNames','name'),('kSsidPrefixes','ssid')]:
            section=source.split(table+'[] = {')[1].split('};')[0]
            found[kind]=set(re.findall(r'\{\s*"([^"]+)"',section)) if kind in ('name','ssid') else set(f'{int(v,16):04X}' for v in re.findall(r'\{\s*0x([\dA-Fa-f]+)',section))
        for kind,values in found.items():assert values=={e['value'] for e in entries if e['kind']==kind},f'Upstream {kind} changed: update coverage review'
    lines=['// Generated test facts; see docs/data/squachwatch-coverage.json.','#pragma once','struct CoverageFact {const char* kind;const char* value;dragon::Family family;};','static const CoverageFact coverageFacts[]={']
    lines += [f' {{{json.dumps(e["kind"])},{json.dumps(e["value"])},dragon::Family::{e["family"]}}},' for e in entries]
    lines += ['};','']
    out=ROOT/'.work/coverage_facts.h';out.parent.mkdir(exist_ok=True);out.write_text('\n'.join(lines))
    print(f'Coverage ledger: {len(entries)} table entries; '+', '.join(f'{n} {k}' for k,n in expected.items()))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--reference');generate(p.parse_args().reference)
