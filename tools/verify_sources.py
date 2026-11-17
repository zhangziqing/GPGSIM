#!/usr/bin/env python3
"""Verify selected source revisions and optionally initialize pinned submodules."""
import argparse
import json
import subprocess
from common import LOCK, ROOT, verify_source, verify_workload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--workload', action='store_true')
    args = parser.parse_args()
    if args.fetch:
        subprocess.run(['git', 'submodule', 'update', '--init', '--recursive'], cwd=ROOT, check=True)
    verified = {entry['path']: verify_source(entry['path']) for entry in LOCK['repositories']}
    if args.workload:
        verify_workload(ROOT / LOCK['workload']['path'])
        verified['coremark_sha256'] = LOCK['workload']['sha256']
    print(json.dumps(verified, indent=2))


if __name__ == '__main__':
    main()
