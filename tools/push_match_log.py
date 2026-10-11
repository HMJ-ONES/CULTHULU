#!/usr/bin/env python3
"""Push a match action log to GitHub (match-logs/). Wave 42.

Usage: python3 tools/push_match_log.py <repo-path> < /dev/stdin
  <repo-path>: e.g. match-logs/2026-10-11/120301-domination-12345.log

Reads the log text from stdin, creates a blob, and commits it to the
game-core branch. Auth via CULTHULU_GITHUB_TOKEN env var (a PAT with
repo contents:write). Nothing is written to disk — stdin goes straight
to the GitHub blob API.
"""
import base64
import json
import os
import sys
import urllib.request
import urllib.error

OWNER = "HMJ-ONES"
REPO = "CULTHULU"
BRANCH = "game-core"


def api(method, url, token, data=None):
    req = urllib.request.Request(url, method=method)
    req.add_header("Authorization", f"Bearer {token}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("Content-Type", "application/json")
    body = json.dumps(data).encode() if data is not None else None
    try:
        with urllib.request.urlopen(req, data=body) as resp:
            return json.load(resp)
    except urllib.error.HTTPError as e:
        detail = e.read().decode("utf-8", errors="replace")[:300]
        print(f"GitHub {method} failed: {e.code} {detail}", file=sys.stderr)
        sys.exit(1)


def main():
    if len(sys.argv) != 2:
        print("usage: push_match_log.py <repo-path>", file=sys.stderr)
        sys.exit(1)
    path = sys.argv[1]
    token = os.environ.get("CULTHULU_GITHUB_TOKEN", "")
    if not token:
        print("CULTHULU_GITHUB_TOKEN not set", file=sys.stderr)
        sys.exit(1)

    log = sys.stdin.read()
    if not log.strip():
        print("empty log, nothing to push", file=sys.stderr)
        sys.exit(1)

    base = f"https://api.github.com/repos/{OWNER}/{REPO}"
    ref = api("GET", f"{base}/git/ref/heads/{BRANCH}", token)
    head = ref["object"]["sha"]

    blob = api("POST", f"{base}/git/blobs", token, {
        "content": base64.b64encode(log.encode("utf-8")).decode("ascii"),
        "encoding": "base64",
    })
    tree = api("POST", f"{base}/git/trees", token, {
        "base_tree": head,
        "tree": [{"path": path, "mode": "100644", "type": "blob",
                  "sha": blob["sha"]}],
    })
    commit = api("POST", f"{base}/git/commits", token, {
        "message": f"match log: {path}",
        "tree": tree["sha"],
        "parents": [head],
    })
    api("PATCH", f"{base}/git/refs/heads/{BRANCH}", token, {
        "sha": commit["sha"],
    })
    print(f"pushed {path} ({len(log)} bytes)")


if __name__ == "__main__":
    main()
