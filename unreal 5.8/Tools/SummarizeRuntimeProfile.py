"""Summarize actual frame timings, excluding Unreal CSV footer metadata.

Run with the engine Python or regular Python; no Unreal imports are needed.
Default input: Saved/Profiling/CultRuntime/*.csv
Output: Saved/Reports/RuntimeProfileSummary.csv (nested scopes are not additive).
"""
import argparse
import csv
import math
from pathlib import Path
from statistics import mean


COUNT_STATS = {"PopulationDecisionCalls", "ProfilePopulation", "ProfileSkinnedComponentsAtStart", "ArenaStoneEnabled", "ArenaTaggedSurfaces"}


def summarize(path, counts=False):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames or []
        frames = []
        for row in reader:
            first = row.get(fields[0], "") if fields else ""
            # UE appends a duplicate header and a positional metadata record.
            # Some metadata fields are numeric and can look like huge timings.
            if first == fields[0] or first.startswith("["):
                continue
            try:
                if float(row.get("FrameTime", 0)) > 0:
                    frames.append(row)
            except (ValueError, TypeError):
                continue
        if not frames:
            raise ValueError(f"No positive-duration frames in {path}")
        output = []
        for stat in fields:
            is_count = stat.rsplit("/", 1)[-1] in COUNT_STATS
            if is_count != counts:
                continue
            if not counts and not (stat.startswith("CultRuntime/") or stat in {
                "FrameTime", "GameThreadTime", "GPUTime",
                "Exclusive/GameThread/EndOfFrameUpdates",
                "Exclusive/GameThread/CharacterMovement",
            }):
                continue
            values = []
            for row in frames:
                try:
                    value = float(row[stat])
                    if math.isfinite(value) and value >= 0:
                        values.append(value)
                except (ValueError, TypeError):
                    pass
            if values:
                p95 = sorted(values)[int(.95 * (len(values) - 1))]
                output.append((path.name, stat, len(values), mean(values), p95))
        return output


def main():
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="*", type=Path)
    parser.add_argument("--output", type=Path, default=project / "Saved/Reports/RuntimeProfileSummary.csv")
    args = parser.parse_args()
    captures = args.captures or sorted((project / "Saved/Profiling/CultRuntime").glob("*.csv"))
    if not captures:
        parser.error("No runtime captures found")
    rows = [row for capture in captures for row in summarize(capture)]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(("capture", "stat", "frame_samples", "mean_ms", "p95_ms"))
        writer.writerows(rows)
    print(f"Summarized {len(captures)} captures into {args.output}")
    counters = [row for capture in captures for row in summarize(capture, counts=True)]
    if counters:
        counter_path = args.output.with_name(args.output.stem + "_Counters.csv")
        with counter_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(("capture", "stat", "frame_samples", "mean_count", "p95_count"))
            writer.writerows(counters)
        print(f"Wrote count metrics separately to {counter_path}")


if __name__ == "__main__":
    main()
