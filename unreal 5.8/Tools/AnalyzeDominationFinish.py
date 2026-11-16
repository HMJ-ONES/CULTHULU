"""Summarize the native frozen-world legacy/refined A-B-B-A render windows.

This measures the combined presentation change on the local editor backbuffer,
not a shader-only experiment, live-match FPS or low-spec performance.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import math
import statistics

PROJECT = Path(__file__).resolve().parents[1]


def analyze(directory=None):
    directory = Path(directory) if directory is not None else PROJECT / 'Saved/Reports/Captures/MatchSurface'
    if not directory.is_absolute():
        directory = PROJECT / directory
    windows = []
    all_rows = []
    for index in range(4):
        path = directory / f'DominationFinish_{index}.csv'
        state = int(index in (1, 2))
        rows = []
        with path.open(newline='', encoding='utf-8-sig') as stream:
            for row in csv.DictReader(stream):
                # Unreal appends metadata/footer rows to these CSVs. Require
                # the complete numeric frame and both explicit native counters.
                try:
                    gpu = float(row['GPUTime'])
                    frame = float(row['FrameTime'])
                    enabled = float(row['CultRuntime/ArenaStoneEnabled'])
                    tagged = float(row['CultRuntime/ArenaTaggedSurfaces'])
                except (TypeError, ValueError, KeyError):
                    continue
                if not all(math.isfinite(x) for x in (gpu, frame, enabled, tagged)):
                    continue
                if enabled != state or tagged <= 0 or not tagged.is_integer() or gpu < 0 or frame < 0:
                    continue
                rows.append((gpu, frame, tagged))
        assert len(rows) >= 100, f'Insufficient native frames: {path}'
        windows.append({'window': index, 'refined': bool(state), 'frames': len(rows),
                        'gpuMeanMs': statistics.mean(r[0] for r in rows),
                        'frameMeanMs': statistics.mean(r[1] for r in rows),
                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        all_rows.append(rows)
    counts = {r[2] for rows in all_rows for r in rows}
    assert len(counts) == 1, f'Arena surface population changed across render windows: {counts}'
    legacy = [r[0] for i in (0, 3) for r in all_rows[i]]
    refined = [r[0] for i in (1, 2) for r in all_rows[i]]
    return {'scope': 'Combined material/relief in one frozen editor world and player view; A-B-B-A order; CSV footers excluded; counts are not milliseconds.',
            'taggedSurfaces': int(next(iter(counts))), 'windows': windows,
            'legacyGpuMeanMs': statistics.mean(legacy), 'refinedGpuMeanMs': statistics.mean(refined),
            'observedGpuDifferenceMs': statistics.mean(refined) - statistics.mean(legacy),
            'limits': 'One-host frozen-view sample. No statistical performance gain, live-match FPS, fixed exported-image-resolution cost or low-spec certification.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory')
    args = parser.parse_args()
    print(json.dumps(analyze(args.directory), indent=2))
