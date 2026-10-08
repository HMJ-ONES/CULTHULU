#!/usr/bin/env python3
"""Decode base64-packed binary assets (*.glb.b64 / *.png.b64 -> originals).
GitHub's push path mangles raw binaries, so binaries live in the repo as
base64 text. Run once after cloning:  python3 assets/decode_assets.py
"""
import base64, glob, os, sys

root = os.path.dirname(os.path.abspath(__file__))
done = 0
for p in glob.glob(os.path.join(root, '**', '*.b64'), recursive=True):
    out = p[:-4]  # strip .b64
    with open(p) as f:
        data = base64.b64decode(f.read())
    with open(out, 'wb') as f:
        f.write(data)
    done += 1
    print('decoded', os.path.relpath(out, root))
print(f'{done} asset(s) decoded.')
