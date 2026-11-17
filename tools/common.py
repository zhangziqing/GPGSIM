"""Shared source identity and file hashing helpers for the reproduction scripts."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'sources.lock.json').read_text())


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def verify_source(path):
    entry = next(item for item in LOCK['repositories'] if item['path'] == path)
    repo = ROOT / path
    top = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', '--show-toplevel'], text=True).strip()
    if Path(top).resolve() != repo.resolve():
        raise RuntimeError('Initialize the submodule first: ' + path)
    head = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    if head != entry['commit']:
        raise RuntimeError('Pinned revision differs for ' + path + ': ' + head)
    dirty = subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--untracked-files=no'], text=True)
    if dirty.strip():
        raise RuntimeError('Tracked dependency changes require a separate source identity: ' + path)
    return head


def verify_workload(path):
    record = LOCK['workload']
    if Path(path).stat().st_size != record['bytes'] or sha(path) != record['sha256']:
        raise RuntimeError('CoreMark image differs from the pinned workload')
