#!/usr/bin/env python3
"""Lower Nanhu RTL and build the rising-edge CoreMark reference from source."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
from common import ROOT, sha, verify_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elaboration', type=Path, default=ROOT / 'build/elaboration')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/verilator')
    parser.add_argument('--java', default=os.environ.get('RTEAAL_JAVA', 'java'))
    parser.add_argument('--heap', default='192g')
    parser.add_argument('--jobs', type=int, default=12)
    parser.add_argument('--verilator', default='verilator')
    args = parser.parse_args()
    verify_source('RTeAAL-Sim')
    verify_source('XiangShan-nanhu-legacy')
    elaboration = args.elaboration.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source_bin = ROOT / 'RTeAAL-Sim/utils/bin'
    jars = [source_bin / 'firrtl-fast-accesses-2.13.jar', source_bin / 'essent.jar']
    for path in [elaboration / 'SimTop.hi.fir', *jars]:
        if not path.is_file():
            parser.error('Missing input: ' + str(path))
    commands = []
    command = [args.java, '-Xmx' + args.heap, '-Xss64m', '-cp', ':'.join(map(str, jars)),
               'firrtl.stage.FirrtlMain', '-i', str(elaboration / 'SimTop.hi.fir'),
               '-o', str(output / 'SimTop.v'), '-X', 'verilog', '--log-level', 'info']
    commands.append(command)
    with (output / 'lower.log').open('w') as log:
        subprocess.run(command, cwd=output, stdout=log, stderr=log, check=True)
    # Names and types of callbacks must still match the pinned generated declarations.
    dpi = (ROOT / 'runtime/verilator/coremark_dpi.cc').read_text()
    types = {'byte':'char', 'bit':'svBit', 'int':'int', 'longint':'long long'}
    csr_ports = None
    for path in sorted(elaboration.glob('Difftest*.v')):
        match = re.search(r'import "DPI-C" function void (\w+)\s*\((.*?)\);', path.read_text(), re.S)
        if not match:
            raise RuntimeError('Missing DPI declaration: ' + path.name)
        name, params = match.groups()
        ports = re.findall(r'input\s+(byte|bit|int|longint)\s+(\w+)', params)
        signature = 'extern "C" void ' + name + '(' + ', '.join(types[t] + ' ' + n for t,n in ports) + ')'
        if signature not in dpi:
            raise RuntimeError('DPI source differs from elaborated interface: ' + name)
        if name.endswith('_CSRState'):
            csr_ports = [n for _, n in ports if n != 'io_coreid']
    if csr_ports != json.loads((ROOT / 'configs/csr-schema.json').read_text()):
        raise RuntimeError('CSR observation schema differs')
    external = []
    for line in (elaboration / 'firrtl_black_box_resource_files.f').read_text().splitlines():
        path = Path(line)
        if not path.is_absolute():
            path = elaboration / path
        if not path.is_file():
            raise RuntimeError('Missing external RTL resource: ' + str(path))
        external.append(str(path))
    includes = [ROOT / 'runtime/common', ROOT / 'runtime/cuda/vendor']
    cflags = '-O3 -std=c++17 ' + ' '.join('-I' + str(path) for path in includes)
    command = [args.verilator, '--cc', str(output / 'SimTop.v'), *external,
               str(ROOT / 'XiangShan-nanhu-legacy/difftest/src/test/vsrc/common/ram.v'),
               str(ROOT / 'runtime/verilator/SimJTAG-idle.v'), '--exe',
               str(ROOT / 'runtime/verilator/verilator_coremark_edge.cc'),
               str(ROOT / 'runtime/verilator/coremark_dpi.cc'), '--top-module', 'SimTop',
               '--Mdir', str(output / 'obj'), '--build', '-j', str(args.jobs),
               '--output-split', '20000', '--output-split-cfuncs', '20000',
               '-CFLAGS', cflags, '-DVERILATOR_5', '-Wno-fatal', '-o', 'coremark_reference']
    commands.append(command)
    (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    with (output / 'build.log').open('w') as log:
        subprocess.run(command, cwd=output, stdout=log, stderr=log, check=True)
    binary = output / 'obj/coremark_reference'
    (output / 'build.json').write_text(json.dumps(dict(binary_sha256=sha(binary), status='built_not_executed'), indent=2) + '\n')


if __name__ == '__main__':
    main()
