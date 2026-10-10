#!/usr/bin/env python3
"""Audit actual preprocessed experimental handlers/slots, not FNC numbers.

--legacy records/verifies the original PR2 table provenance. Without it, verify
the checked-in provenance snapshot against the actual experimental source.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TABLES = ('FUNC_BASIC', 'FUNC_EXT_F', 'FUNC_EXT_0')
SNAPSHOT = ROOT / 'docs/legacy-instruction-inventory.json'

def preprocess(path):
    return subprocess.check_output(['cc', '-E', '-P', '-x', 'c',
        '-DPLC_RUNTIME_EXPERIMENTAL=1', '-I'+str(ROOT/'tools/tests/runtime'),
        '-I'+str(ROOT/'inc'), str(path)], text=True)

def functions(code):
    result = {}
    for match in re.finditer(r'\bvoid\s+(\w+)\s*\([^;{}]*\)\s*\{', code):
        start=match.end(); end=start; depth=1
        while depth:
            if code[end]=='{': depth+=1
            elif code[end]=='}': depth-=1
            end+=1
        result[match[1]]=code[start:end-1].strip()
    return result

def inventory(path):
    code=preprocess(path); bodies=functions(code)
    tables={}
    for name in TABLES:
        match=re.search(re.escape(name)+r'\[\]\)\(void\)\s*=\s*\{(.*?)\}', code, re.S)
        assert match, 'Missing table '+name
        tables[name]=re.findall(r'\b\w+\b',match[1])
    handlers=sorted(set(sum(tables.values(),[]))-{'ERR'})
    states={}
    for name in handlers+['FUNC_TIMER','FUNC_COUNTER']:
        body=bodies.get(name)
        if body is None: state='missing-definition'
        elif re.fullmatch(r'PLC_Legacy_Missing\(\);',body): state='missing-fault-stub'
        elif re.fullmatch(r'PLC_Legacy_Incomplete\(\);',body) or (not body and name!='NOP'): state='empty-incomplete'
        elif re.fullmatch(r'PLC_PROG_ERROR\([^;]*\);',body): state='explicit-unsupported'
        else: state='body-present-unvalidated' if name!='NOP' else 'intentional-nop'
        states[name]=state
    # The FF switch is nested dispatch, not an additional function-pointer table.
    ff=bodies['FUNC_FF']
    cases=re.findall(r'case\s+(0x[0-9A-Fa-f]+)\s*:',ff)
    ff_states={}
    for match in re.finditer(r'case\s+(0x[0-9A-Fa-f]+)\s*:(.*?)(?=case\s+0x|default:)',ff,re.S):
        body=match[2].strip()
        ff_states[str(int(match[1],16))]='empty-incomplete' if body=='break;' or \
            'PLC_Legacy_Incomplete' in body else 'body-present-unvalidated'
    return {'tables':tables,'handlers':states,'ff_cases':[int(c,16) for c in cases],
            'ff_case_status':ff_states}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--legacy',type=Path)
    parser.add_argument('--write',action='store_true')
    args=parser.parse_args()
    current=inventory(ROOT/'src/ladder.c')
    if args.legacy:
        original=inventory(args.legacy)
        original['source_sha256']=hashlib.sha256(args.legacy.read_bytes()).hexdigest()
        restored_text=(ROOT/'src/ladder_legacy.inc').read_text()
        original['commit']=re.search(r'Provenance: ([0-9a-f]{40}) ',restored_text)[1]
    else:
        original=json.loads(SNAPSHOT.read_text())['original']
    assert original['tables']==current['tables'], 'Original full dispatch tables changed'
    assert original['ff_cases']==current['ff_cases'], 'Original FF dispatch cases changed'
    assert original['ff_case_status']==current['ff_case_status'], 'Original FF case classifications changed'
    assert set(original['handlers'])==set(current['handlers']), 'Missing/restored handler inventory mismatch'
    assert 'missing-definition' not in current['handlers'].values(), 'Null dispatch target remains'
    rows={}
    for table, entries in current['tables'].items():
        rows[table]={'slots':len(entries),'error_slots':entries.count('ERR'),
                     'non_error_slots':len(entries)-entries.count('ERR'),
                     'distinct_non_error_targets':len(set(entries)-{'ERR'})}
    counts={state:list(current['handlers'].values()).count(state)
            for state in sorted(set(current['handlers'].values()))}
    routers={'FUNC_0','FUNC_F','FUNC_FF'}
    named=set(current['handlers'])-routers-{'FUNC_TIMER','FUNC_COUNTER'}
    named_counts={state:sum(current['handlers'][n]==state for n in named)
                  for state in sorted(set(current['handlers'].values()))}
    report={'original':original,'restored':current,'table_counts':rows,
            'handler_counts':counts,'distinct_table_targets':len(current['handlers'])-2,
            'named_handlers_excluding_routers':len(named),'named_handler_counts':named_counts,
            'timer_counter_targets':2,'ff_cases':len(current['ff_cases']),
            'note':'Counts describe repository dispatch, not working FX1N instructions or official FNC numbers.'}
    if args.write: SNAPSHOT.write_text(json.dumps(report,indent=2)+'\n')
    else:
        saved=json.loads(SNAPSHOT.read_text())
        assert saved==report, 'Inventory stale: regenerate with --write'
    print(json.dumps({key:report[key] for key in ('table_counts','handler_counts',
          'distinct_table_targets','named_handlers_excluding_routers','named_handler_counts',
          'timer_counter_targets','ff_cases')},indent=2))
    print('PASS: every original full table slot and FF case retained; all targets defined.')

if __name__=='__main__': main()
