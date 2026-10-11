#!/usr/bin/env python3
"""Delete match logs older than 30 days from GitHub. Wave 42.

Runs daily via cron. Lists match-logs/, removes date directories older
than 30 days (based on YYYY-MM-DD in the path), commits the deletion.
Uses the vault credential (custom.github) via the surrogate mechanism.
"""
import datetime
import json
import sys
import urllib.request
import urllib.error

sys.path.insert(0, '/opt/hatch/skills/skill-creator/bin')
from dynamic_credentials import (
    add_surrogate_to_request, read_json_response, DynamicCredentialError)

OWNER = "HMJ-ONES"
REPO = "CULTHULU"
BRANCH = "game-core"
RETENTION_DAYS = 30


def api(method, url, data=None):
    req = urllib.request.Request(url, method=method)
    add_surrogate_to_request(req, 'custom.github',
                             allowed_hosts=('api.github.com',))
    body = json.dumps(data).encode() if data is not None else None
    try:
        with urllib.request.urlopen(req, data=body) as resp:
            return read_json_response(resp)
    except urllib.error.HTTPError as e:
        detail = e.read().decode('utf-8', errors='replace')[:300]
        raise DynamicCredentialError(f'GitHub {method} -> {e.code}: {detail}')


def main():
    base = f'https://api.github.com/repos/{OWNER}/{REPO}'
    cutoff = datetime.date.today() - datetime.timedelta(days=RETENTION_DAYS)

    ref = api('GET', f'{base}/git/ref/heads/{BRANCH}')
    head = ref['object']['sha']
    tree = api('GET', f'{base}/git/trees/{head}?recursive=1')

    # Find match-log date dirs older than the cutoff.
    old_dirs = set()
    for item in tree.get('tree', []):
        p = item['path']
        if not p.startswith('match-logs/'):
            continue
        parts = p.split('/')
        if len(parts) < 3:
            continue
        try:
            d = datetime.date.fromisoformat(parts[1])
        except ValueError:
            continue
        if d < cutoff:
            old_dirs.add('match-logs/' + parts[1])

    if not old_dirs:
        print(f'no match logs older than {RETENTION_DAYS} days')
        return

    # Build a new tree omitting the old files.
    entries = []
    for item in tree.get('tree', []):
        p = item['path']
        if item['type'] != 'blob':
            continue
        if any(p.startswith(d + '/') for d in old_dirs):
            continue  # delete
        entries.append({'path': p, 'mode': item['mode'], 'type': 'blob',
                        'sha': item['sha']})

    new_tree = api('POST', f'{base}/git/trees', {'tree': entries})
    commit = api('POST', f'{base}/git/commits', {
        'message': f'cleanup: remove match logs older than {RETENTION_DAYS} days '
                   f'({", ".join(sorted(old_dirs))})',
        'tree': new_tree['sha'],
        'parents': [head],
    })
    api('PATCH', f'{base}/git/refs/heads/{BRANCH}', {'sha': commit['sha']})
    print(f'removed {len(old_dirs)} old log dirs: {sorted(old_dirs)}')


if __name__ == '__main__':
    main()
