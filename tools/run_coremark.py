#!/usr/bin/env python3
"""Run a reference-checked fixed CoreMark prefix with explicit CPU affinity."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import time
from common import LOCK, ROOT, sha, verify_workload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('backend', choices=['verilator', 'cpu'])
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--program', type=Path, default=ROOT / LOCK['workload']['path'])
    parser.add_argument('--cycles', type=int, default=1024)
    parser.add_argument('--cpu', type=int, required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--reference', type=Path)
    args = parser.parse_args()
    if not 1 <= args.cycles <= 100000:
        parser.error('cycles must be 1..100000')
    if args.cpu not in os.sched_getaffinity(0):
        parser.error('Selected CPU is outside the allowed CPU set')
    if not args.label or args.label in ('.', '..') or Path(args.label).name != args.label:
        parser.error('label must be one directory name')
    if args.backend == 'cpu' and args.reference is None:
        parser.error('A Verilator reference trace is required for CPU acceptance')
    binary = args.binary or ROOT / ('build/cpu/coremark_thp_prefault' if args.backend == 'cpu'
                                   else 'build/verilator/obj/coremark_reference')
    verify_workload(args.program)
    folder = ROOT / 'build/runs' / args.label
    folder.mkdir(parents=True, exist_ok=False)
    reference = None
    if args.reference:
        reference = folder / 'reference.trace'
        with args.reference.open('rb') as src, reference.open('wb') as dst:
            for _ in range(args.cycles):
                line = src.readline()
                if not line:
                    raise RuntimeError('Reference has fewer cycles than requested')
                dst.write(line)
        if args.cycles == 100000 and sha(reference) != LOCK['historical_reference']['sha256']:
            raise RuntimeError('Full reference differs from the accepted 100K trace')
    trace = folder / 'actual.trace'
    command = [str(binary.resolve())]
    if args.backend == 'cpu':
        command += ['xiangshan-nanhu-indexed', str(args.program.resolve()), str(ROOT / 'configs/preserved-signals.txt')]
    else:
        command += [str(args.program.resolve())]
    command += [str(args.cycles), str(trace)]
    if reference:
        command.append(str(reference))
    command = ['taskset', '-c', str(args.cpu), *command]
    metadata = dict(command=command, binary_sha256=sha(binary), program_sha256=sha(args.program),
                    platform=platform.platform(), cpu=args.cpu, scope='Fixed post-reset cycle prefix; no completed CoreMark score')
    (folder / 'command.json').write_text(json.dumps(metadata, indent=2) + '\n')
    started = time.monotonic()
    with (folder / 'result.out').open('w') as out, (folder / 'run.log').open('w') as log:
        subprocess.run(command, stdout=out, stderr=log, check=True)
    value = json.loads((folder / 'result.out').read_text())
    if value['observed_cycles'] != args.cycles:
        raise RuntimeError('Runner observed a different cycle count')
    if reference and (not value['online_reference_comparison'] or sha(trace) != sha(reference)):
        raise RuntimeError('Reference comparison failed or was disabled')
    if args.cycles == 100000 and sha(trace) != LOCK['historical_reference']['sha256']:
        raise RuntimeError('Trace differs from accepted full prefix')
    value.update(process_wall_seconds=time.monotonic()-started, trace_sha256=sha(trace),
                 validation_status='passed' if reference else 'reference_generated')
    (folder / 'result.json').write_text(json.dumps(value, indent=2) + '\n')
    print(json.dumps({key:value[key] for key in ['validation_status', 'observed_cycles', 'cycles_per_second', 'trace_sha256']}, indent=2))


if __name__ == '__main__':
    main()
