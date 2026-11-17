#!/usr/bin/env python3
"""Generate the measured Nanhu wide-counter NU wrapper from pinned kernel source."""
import argparse
from pathlib import Path
from common import LOCK, ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    kernel = ROOT / 'RTeAAL-Sim/kernel'
    for name, expected in LOCK['kernel_sha256'].items():
        if sha(kernel / name) != expected:
            raise RuntimeError('Kernel source differs: ' + name)
    source = (kernel / 'kernel_NU.cc').read_text()
    start = source.index('ALWAYS_INLINE uint16_t op_ss_for_loop(')
    end = source.index('\ntemplate<typename T>', start)
    function = source[start:end].replace('uint16_t', 'uint32_t')
    wrapper = '#define main nanhu_unused_upstream_main\n#include "kernel_OU.cc"\n#undef main\n'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(wrapper + function + '\n')


if __name__ == '__main__':
    main()
