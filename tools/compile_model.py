#!/usr/bin/env python3
"""Compile into an isolated output tree using the integrated compiler only."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from common import ROOT, sha, verify_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firrtl', type=Path)
    parser.add_argument('--preclock', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/model')
    parser.add_argument('--config', type=Path, default=ROOT / 'configs/integrated-current.json')
    parser.add_argument('--java', default=os.environ.get('RTEAAL_JAVA', 'java'))
    parser.add_argument('--heap', default='256g')
    parser.add_argument('--processors', type=int, default=12)
    args = parser.parse_args()
    source = verify_source('RTeAAL-Sim')
    source_bin = ROOT / 'RTeAAL-Sim/utils/bin'
    jars = [source_bin / 'essent.jar']
    if not args.preclock:
        jars.insert(0, source_bin / 'firrtl-fast-accesses-2.13.jar')
    for path in [args.firrtl, *jars]:
        if not path.is_file():
            parser.error('Missing required input: ' + str(path))
    config = json.loads(args.config.read_text())
    if config['optional_passes'] != dict(normalize_mux_index=False, project_slices=False, compose_masked_updates=False):
        parser.error('This entry point currently supports the explicit integrated-current configuration only')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    for path in ['kernel/json', 'kernel/txt', 'kernel/yaml', 'utils/bin']:
        (output / path).mkdir(parents=True)
    work = output / 'utils/bin'
    shutil.copyfile(source_bin / 'header_parser.py', work / 'header_parser.py')
    properties = dict(config['jvm_properties'], **{
        'rteaal.design':config['design'], 'rteaal.preserved':str(ROOT / 'configs/preserved-signals.txt'),
        'rteaal.preClockDump':str(output / 'SimTop.preclock.lo.fir'),
        'rteaal.postGraphDump':str(output / 'graph.bin')})
    if args.preclock:
        properties['rteaal.preClockInput'] = 'true'
    command = [args.java, '-XX:ActiveProcessorCount=' + str(args.processors), '-Xmx' + args.heap, '-Xss64m',
               *['-D' + key + '=' + value for key, value in properties.items()],
               '-cp', ':'.join(str(path) for path in jars), 'essent.Driver',
               *config['compiler_options'], '--firrtl-log-level', 'info', str(args.firrtl.resolve())]
    manifest = dict(status='pending', source_commit=source, input_sha256=sha(args.firrtl),
                    config=config, command=command, jars={str(p.relative_to(ROOT)):sha(p) for p in jars})
    (output / 'compile.json').write_text(json.dumps(manifest, indent=2) + '\n')
    with (output / 'compile.log').open('w') as log:
        subprocess.run(command, cwd=work, stdout=log, stderr=log, check=True)
    required = [work / ('memory_sizes_' + config['design'] + '.json'), output / 'graph.bin']
    for path in required:
        if not path.is_file():
            raise RuntimeError('Compiler output missing: ' + str(path))
    manifest['status'] = 'compiled_not_preprocessed_or_validated'
    (output / 'compile.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
