#!/usr/bin/env python3
"""Resolve the pinned Mill classpath, then elaborate single-core Nanhu."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import shutil
from common import ROOT, verify_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--java', default=os.environ.get('RTEAAL_JAVA', 'java'))
    parser.add_argument('--mill', default='mill')
    parser.add_argument('--heap', default='128g')
    parser.add_argument('--processors', type=int, default=12)
    args = parser.parse_args()
    verify_source('XiangShan-nanhu-legacy')
    verify_source('RTeAAL-Sim')
    checkout = ROOT / 'XiangShan-nanhu-legacy'
    override = ROOT / 'RTeAAL-Sim/utils/bin/firrtl-fast-accesses-2.12.jar'
    if not override.is_file():
        parser.error('Build the compiler and FIRRTL overrides first')
    java = Path(shutil.which(args.java) or args.java).resolve()
    env = dict(os.environ, NOOP_HOME=str(checkout), JAVA_HOME=str(java.parents[1]))
    env['JAVA_TOOL_OPTIONS'] = '-XX:ActiveProcessorCount=' + str(args.processors)
    subprocess.run([args.mill, '-i', 'XiangShan.test.runClasspath'], cwd=checkout, env=env, check=True)
    entries = json.loads((checkout / 'out/XiangShan/test/runClasspath.json').read_text())['value']
    paths = [entry.split(':', 3)[3] for entry in entries]
    output = ROOT / 'build/elaboration'
    output.mkdir(parents=True, exist_ok=False)
    command = [args.java, '-Xmx' + args.heap, '-Xss64m', '-cp', ':'.join([str(override), *paths]),
               'top.SimTop', '--target-dir', str(output), '--config', 'DefaultConfig', '--num-cores', '1',
               '--compiler', 'high', '--enable-difftest', '--disable-perf', '--disable-all']
    (output / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (output / 'elaboration.log').open('w') as log:
        subprocess.run(command, cwd=checkout, env=env, stdout=log, stderr=log, check=True)
    verify_source('XiangShan-nanhu-legacy')


if __name__ == '__main__':
    main()
