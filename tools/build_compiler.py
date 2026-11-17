#!/usr/bin/env python3
"""Build both FIRRTL overrides and the assembly from the pinned compiler source."""
import argparse
import os
from pathlib import Path
import subprocess
import urllib.request
from common import LOCK, ROOT, sha, verify_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--java', default=os.environ.get('RTEAAL_JAVA', 'java'))
    parser.add_argument('--sbt-launcher', type=Path, default=ROOT / '.cache/sbt-launch-1.9.6.jar')
    parser.add_argument('--test', action='store_true')
    args = parser.parse_args()
    verify_source('RTeAAL-Sim')
    version = subprocess.run([args.java, '-version'], text=True, capture_output=True, check=True)
    if 'version "11.' not in version.stderr + version.stdout:
        parser.error('Use JDK 11 for both Scala versions; pass --java or set RTEAAL_JAVA')
    launcher = args.sbt_launcher.resolve()
    if not launcher.exists():
        launcher.parent.mkdir(parents=True, exist_ok=True)
        temp = launcher.with_suffix('.download')
        urllib.request.urlretrieve(LOCK['sbt_launcher']['url'], temp)
        if sha(temp) != LOCK['sbt_launcher']['sha256']:
            raise RuntimeError('Downloaded sbt launcher hash mismatch')
        temp.replace(launcher)
    if sha(launcher) != LOCK['sbt_launcher']['sha256']:
        raise RuntimeError('sbt launcher hash mismatch')
    tasks = ['fastAccesses212 / checkFastAccesses', 'fastAccesses213 / checkFastAccesses', 'assembly']
    if args.test:
        tasks.insert(0, 'test')
    cache = ROOT / '.cache'
    env = dict(os.environ, COURSIER_CACHE=str(cache / 'coursier'))
    command = [args.java, '-XX:ActiveProcessorCount=8', '-Xmx8g', '-Xss8m',
               '-Dsbt.boot.directory=' + str(cache / 'sbt/boot'),
               '-Dsbt.global.base=' + str(cache / 'sbt/global'),
               '-Dsbt.ivy.home=' + str(cache / 'ivy'), '-Dsbt.supershell=false', '-jar', str(launcher), *tasks]
    subprocess.run(command, cwd=ROOT / 'RTeAAL-Sim', env=env, check=True)


if __name__ == '__main__':
    main()
