"""Push a reviewed file manifest over a fresh remote HEAD without changing checkout.

Binary-safe Git plumbing; never run this script just to inspect a backup.
The JSON manifest lists repository-relative files. An optional separate delete
manifest can retire reviewed .bak files and the known duplicate editor target.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--delete-manifest', help='Explicit reviewed source backups/duplicate target removals')
    parser.add_argument('--base', default='game-core')
    parser.add_argument('--branch', required=True)
    parser.add_argument('--message', required=True)
    parser.add_argument('--update-existing', action='store_true',
                        help='Append to the existing branch; --base must equal --branch')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    def git(*command, data=None, env=None):
        return subprocess.check_output(['git', *command], cwd=root, input=data, env=env)

    assert args.branch.startswith('codex/'), 'Use a dedicated codex/ backup branch'
    git('check-ref-format', 'refs/heads/' + args.branch)
    git('fetch', 'origin', args.base)
    parent = git('rev-parse', 'FETCH_HEAD').decode().strip()
    live = git('ls-remote', 'origin', 'refs/heads/' + args.base).decode().split()[0]
    assert live == parent, 'Remote changed during fetch; retry'
    existing = git('ls-remote', 'origin', 'refs/heads/' + args.branch).decode().strip()
    if args.update_existing:
        assert args.base == args.branch and existing.split()[0] == parent, 'Update must append to the live branch HEAD'
    else:
        assert not existing, 'Branch already exists'
    paths = json.loads(Path(args.manifest).read_text(encoding='utf-8'))
    assert isinstance(paths, list) and paths and len(paths) == len(set(paths))
    deletions = json.loads(Path(args.delete_manifest).read_text(encoding='utf-8')) if args.delete_manifest else []
    assert isinstance(deletions, list) and len(deletions) == len(set(deletions))
    assert not set(paths) & set(deletions)
    upstream = {}
    for record in git('ls-tree', '-r', '-z', parent).split(b'\0'):
        if record:
            metadata, name = record.split(b'\t', 1)
            mode, _, blob = metadata.decode().split()
            upstream[name.decode()] = (mode, blob)
    for relative in deletions:
        backup=relative.startswith('unreal 5.8/Source/') and relative.endswith('.bak')
        duplicate_target=relative=='unreal 5.8/Source/CultUlhu/CultUlhuEditor.Target.cs'
        assert backup or duplicate_target, relative
        if duplicate_target: assert (root/'unreal 5.8/Source/CultUlhuEditor.Target.cs').is_file()
        assert relative in upstream and not (root / relative).exists(), 'Preserve the local backup before retiring it'
        assert (root / relative).resolve().is_relative_to(root)
    with tempfile.TemporaryDirectory(prefix='cult-git-index-') as temporary:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / 'index'))
        git('read-tree', parent, env=env)
        changed, total, entries, expected = [], 0, [], []
        for relative in paths:
            path = root / relative
            parts = relative.replace('\\', '/').split('/')
            assert not path.is_symlink() and path.resolve().is_relative_to(root)
            assert not set(parts) & {'.git', 'Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', '__pycache__'}
            assert path.is_file() and path.stat().st_size < 50 * 1024**2, relative
            data = path.read_bytes()
            digest = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            old = upstream.get(relative)
            if old and old[1] == digest:
                continue
            blob = git('hash-object', '-w', '--stdin', data=data).decode().strip()
            assert blob == digest
            mode = old[0] if old else '100644'
            entries.append(f'{mode} {blob}\t{relative}\0'.encode())
            expected.append((relative, blob, data))
            changed.append(relative)
            total += len(data)
        assert changed or deletions, 'No changed files to back up'
        # One batch read verifies every written blob byte-for-byte, including binaries.
        stored = git('cat-file', '--batch', data=''.join(blob+'\n' for _,blob,_ in expected).encode())
        position = 0
        for relative, blob, data in expected:
            end = stored.index(b'\n', position)
            assert stored[position:end].decode() == f'{blob} blob {len(data)}', relative
            position = end + 1
            assert stored[position:position+len(data)] == data, relative
            position += len(data) + 1
        assert position == len(stored)
        entries.extend(('0 ' + '0'*40 + '\t' + relative + '\0').encode() for relative in deletions)
        git('update-index', '-z', '--index-info', data=b''.join(entries), env=env)
        tree = git('write-tree', env=env).decode().strip()
        tree_paths = set(git('ls-tree', '-r', '--name-only', tree).decode().splitlines())
        assert not set(deletions) & tree_paths, 'Retired backup still in proposed tree'
        commit = git('-c', 'user.name=Codex', '-c', 'user.email=codex@localhost',
                     'commit-tree', tree, '-p', parent,
                     data=args.message.encode()).decode().strip()
    # No force pushes, branch switches, or working-index changes.
    git('push', 'origin', commit + ':refs/heads/' + args.branch)
    actual = git('ls-remote', 'origin', 'refs/heads/' + args.branch).decode().split()[0]
    assert actual == commit, 'Remote verification failed'
    print(json.dumps({'branch': args.branch, 'commit': commit, 'parent': parent,
                      'files': len(changed), 'deleted_files': len(deletions), 'bytes': total, 'verified': True}))


if __name__ == '__main__':
    main()
