#!/usr/bin/env python3
"""Verify grouped tensors against the already checked RU operation stream."""
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import os

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
DESIGN = 'xiangshan-nanhu-indexed'
MODEL = Path(os.environ.get('NANHU_MODEL_ROOT', ROOT / 'build/model')).resolve()
JD = MODEL / 'kernel/json'
def load(suffix): return json.loads((JD/f'{DESIGN}_{suffix}.json').read_text())

def decode():
    n,r,s,lens=[load(x) for x in ['unswizzled_nptr','unswizzled_rptr','unswizzled_sptr','unswizzled_ss_lens_ptr']]
    ri=ni=si=0
    for layer,length in enumerate(lens):
        groups=defaultdict(list)
        for _ in range(length):
            op=n[ni];ni+=1;words=1
            if op<=2 or 17<=op<=19 or op==24:span=3
            elif 3<=op<=15:span=2
            elif op in (16,20):span=1
            elif op==21:span=4
            elif op in (22,23):span=1+r[ri]
            elif 25<=op<=27:span=4+r[ri];words=(r[ri+3]+63)//64
            elif op==28:span=7
            else:raise ValueError(op)
            groups[op].append((r[ri:ri+span],s[si:si+words]));ri+=span;si+=words
        yield layer,groups
    assert (ri,ni,si)==(len(r),len(n),len(s))

def verify():
    r,s,f=[load(x) for x in ['rptr','sptr','fptr']]
    ri=si=fi=0;layers=[];removed_memw_slots=[]
    for layer,groups in decode():
        bounds=[];ops=[];outputs=0
        for op in range(29):
            count=len(groups[op])
            bounds.extend([sum(ops),sum(ops)+count]);ops.append(count)
            for args,dst in groups[op]:
                assert r[ri:ri+len(args)]==args,(layer,op,'args',ri)
                ri+=len(args)
                if op!=28:
                    assert s[si:si+len(dst)]==dst,(layer,op,'scatter',si)
                    si+=len(dst);outputs+=len(dst)
                else:removed_memw_slots.extend(dst)
        assert f[fi:fi+58]==bounds,(layer,'bounds',f[fi:fi+58],bounds)
        fi+=58
        layers.append({'layer':layer,'outputs':outputs,'bounds':bounds,
            'argument_end':ri,'scatter_end':si,'operation_counts':[len(groups[op]) for op in range(29)]})
    assert (ri,si,fi)==(len(r),len(s),len(f))
    # RU scatters a placeholder for memw; grouped kernels drop it. Slot reuse
    # means these destinations can be read elsewhere. check_liveness.py proves
    # that a real write replaces every omitted value before its next read.
    metadata={'design':DESIGN,'layers':layers,'grouped_argument_words':ri,'grouped_outputs':si,'fptr_entries':fi,
        'max_fptr':max(f),'removed_memw_placeholders':len(removed_memw_slots),
        'removed_memw_destinations':sorted(set(removed_memw_slots)),
        'verification':'All grouped argument sequences, scatter destinations and 32-bit operation-count bounds match RU decode; excludes memw output placeholders by upstream convention',
        'source_hashes':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [JD/f'{DESIGN}_{x}.json' for x in ['rptr','sptr','fptr']]}}
    (MODEL/'layout.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print('Layout verified:',len(layers),'layers, max bound',max(f),'memw placeholders',len(removed_memw_slots),flush=True)
    return metadata

if __name__=='__main__':verify()
