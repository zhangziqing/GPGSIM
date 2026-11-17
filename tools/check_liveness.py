#!/usr/bin/env python3
"""Prove dropped memw placeholders are overwritten before any later read."""
import bisect
from collections import defaultdict
import json
from pathlib import Path
from layout import decode,HERE,ROOT,JD,DESIGN,MODEL

reads=defaultdict(list);writes=defaultdict(list);placeholders=[]
jump=list(map(int,(MODEL/f'kernel/txt/muxJT_{DESIGN}.txt').read_text().replace(',',' ').split()))
layers=0
for layer,groups in decode():
    layers+=1
    rd=set();wr=set()
    for op,entries in groups.items():
        for a,dst in entries:
            if op<=2:indices=[1,2]
            elif op==3:indices=[0]
            elif 4<=op<=15:indices=[0,1]
            elif op in (16,20):indices=[0]
            elif op==17:indices=[0,1]
            elif op==18:indices=[0,2]
            elif op==19:indices=[0,1,2]
            elif op==21:indices=[3]
            elif op in (22,23):indices=range(1,len(a))
            elif op==24:
                indices=[0]
                rd.update(jump[a[2]+v] for v in range(a[1]+1) if v & ~a[1] == 0)
            elif 25<=op<=27:indices=[1,2]+list(range(4,len(a)))
            elif op==28:indices=[0,1,5,6]
            rd.update(a[i] for i in indices)
            if op==28:placeholders.extend((layer,d) for d in dst)
            else:wr.update(dst)
    for i in rd:reads[i].append(2*layer)
    for i in wr:writes[i].append(2*layer+1)
period=2*layers
observed=set(json.loads((JD/f'dtmSignal_{DESIGN}.json').read_text()).values())
for i in observed:reads[i].append(period-1)
unsafe=[]
def next_time(events,t):
    if not events:return 10**9
    index=bisect.bisect_right(events,t)
    return events[index] if index<len(events) else events[0]+period
for layer,slot in placeholders:
    t=2*layer+1;nr=next_time(reads[slot],t);nw=next_time(writes[slot],t)
    if nr<nw:unsafe.append({'layer':layer,'slot':slot,'next_read':nr,'next_real_write':nw})
live=[]
for slot,events in reads.items():
    if events and events[0]<(writes[slot][0] if writes[slot] else 10**9):live.append(slot)
live=sorted(set(live)|observed)
report={'passed':not unsafe,'layers':layers,'memw_placeholder_writes':len(placeholders),
    'unsafe_placeholder_reaching_reads':unsafe,'live_at_cycle_boundary_count':len(live),
    'live_at_cycle_boundary':live,
    'method':'At each layer all operand/mux-table reads precede scatter; every dropped placeholder must reach a real write before any read, including cycle wrap and observations'}
(MODEL/'liveness.json').write_text(json.dumps(report,indent=2)+'\n')
print('Liveness:',report['passed'],'unsafe',len(unsafe),'live at boundary',len(live),flush=True)
if unsafe:print(unsafe[:10],flush=True)

if unsafe:
    raise SystemExit(1)
