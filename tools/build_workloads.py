#!/usr/bin/env python3
"""Rebuild the checked RV64I loops without requiring an existing reference image."""
import argparse
import json
from pathlib import Path
import subprocess
from common import ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', default='riscv64-linux-gnu-')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/workloads')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    for loops in (1, 1000):
        base = args.output / ('checked-loop-' + str(loops))
        if base.with_suffix('.bin').exists():
            raise RuntimeError('Use a fresh output directory: ' + str(base))
        command = [args.prefix + 'gcc', '-nostdlib', '-nostartfiles', '-static', '-march=rv64i', '-mabi=lp64',
                   '-Wl,--build-id=none', '-Wl,-T,' + str(ROOT / 'workloads/checked-loop.ld'),
                   '-DLOOP_COUNT=' + str(loops), str(ROOT / 'workloads/checked-loop.S'), '-o', str(base.with_suffix('.elf'))]
        subprocess.run(command, check=True)
        subprocess.run([args.prefix + 'objcopy', '-O', 'binary', str(base.with_suffix('.elf')), str(base.with_suffix('.bin'))], check=True)
        records.append(dict(loops=loops, command=command, sha256=sha(base.with_suffix('.bin'))))
    (args.output / 'manifest.json').write_text(json.dumps(records, indent=2) + '\n')
    print(json.dumps(records, indent=2))


if __name__ == '__main__':
    main()
