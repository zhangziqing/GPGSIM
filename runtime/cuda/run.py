#!/usr/bin/env python3
"""Verify exported assets and run a checked CoreMark prefix on CPU or CUDA."""
import argparse
import datetime
import gzip
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import time

HERE = Path(__file__).resolve().parent


def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--binary', type=Path, default=HERE / 'build/nanhu_gpu')
    p.add_argument('--data', type=Path, default=HERE / 'data')
    p.add_argument('--cycles', type=int, default=1024)
    p.add_argument('--label', default=datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    p.add_argument('--cpu', action='store_true')
    p.add_argument('--device', type=int, default=0)
    a = p.parse_args()
    if not 1 <= a.cycles <= 100000:
        p.error('cycles must be 1..100000')
    if not a.label or a.label in ('.', '..') or Path(a.label).name != a.label:
        p.error('label must be a single directory name')
    model = json.loads((a.data / 'model.json').read_text())
    for name, value in model['files'].items():
        f = a.data / name
        if f.stat().st_size != value['bytes'] or sha(f) != value['sha256']:
            raise RuntimeError('Asset hash mismatch: ' + name)
    folder = HERE / 'runs' / a.label
    folder.mkdir(parents=True, exist_ok=False)
    reference = folder / 'reference.trace'
    with gzip.open(str(a.data / 'reference-100k.trace.gz'), 'rb') as src, reference.open('wb') as dst:
        for _ in range(a.cycles):
            line = src.readline()
            if not line:
                raise RuntimeError('Reference too short')
            dst.write(line)
    if a.cycles == 100000 and sha(reference) != model['reference_sha256']:
        raise RuntimeError('Decompressed full reference hash differs')
    command = [str(a.binary.resolve()), '--data', str(a.data.resolve()), '--cycles', str(a.cycles),
        '--trace', str(folder / 'actual.trace'), '--reference', str(reference), '--device', str(a.device)]
    if a.cpu:
        command += ['--cpu']
    metadata = dict(command=command, binary_sha256=sha(a.binary), model_sha256=sha(a.data / 'model.json'),
                    platform=platform.platform(), cycles=a.cycles)
    try:
        metadata['nvidia_smi'] = subprocess.check_output(['nvidia-smi', '--query-gpu=name,driver_version,memory.total', '--format=csv'], universal_newlines=True)
    except (OSError, subprocess.CalledProcessError):
        metadata['nvidia_smi'] = None
    (folder / 'command.json').write_text(json.dumps(metadata, indent=2) + '\n')
    start = time.time()
    with (folder / 'result.out').open('w') as out, (folder / 'run.log').open('w') as err:
        process = subprocess.run(command, stdout=out, stderr=err)
    if process.returncode:
        print((folder / 'run.log').read_text()[-4000:])
        raise RuntimeError('Runner failed: ' + str(folder))
    value = json.loads((folder / 'result.out').read_text())
    if value['observed_cycles'] != a.cycles or not value['online_reference_comparison']:
        raise RuntimeError('Reference check not active or cycle count differs')
    if sha(folder / 'actual.trace') != sha(reference):
        raise RuntimeError('Full trace byte comparison failed')
    if a.cycles == 100000:
        for key, expected in model['expected_100k'].items():
            if value[key] != expected:
                raise RuntimeError('Final reference mismatch: ' + key)
    value.update(validation_status='passed', trace_sha256=sha(reference),
        wall_seconds=time.time() - start, full_100k_state_fingerprints_checked=(a.cycles == 100000 and
            {'full_data_fingerprint', 'memory64_fingerprint', 'memory8_fingerprint'} <= set(model['expected_100k'])),
        gpu_execution=(not a.cpu), requested_backend='cpu' if a.cpu else 'cuda')
    (folder / 'result.json').write_text(json.dumps(value, indent=2) + '\n')
    print(json.dumps({k: value[k] for k in ('validation_status', 'backend', 'device', 'cycles_per_second',
        'kernel_device_cycles_per_second', 'kernels_per_cycle', 'fused_small_layers', 'retired_instructions_including_fused',
        'full_100k_state_fingerprints_checked')}, indent=2))
    print('Results:', folder)


if __name__ == '__main__':
    main()
