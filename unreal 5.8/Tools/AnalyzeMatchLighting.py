"""Measure real matched-camera PNG captures without extra imaging dependencies.

Measurements describe visibility; they are not a score for beauty or fun.
Input folders must be inside the canonical project's Saved directory.
"""
import argparse
import json
from pathlib import Path
import struct
import zlib

PROJECT = Path(__file__).resolve().parents[1]


def pixels(path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    position, compressed = 8, bytearray()
    while position < len(data):
        length = struct.unpack_from(">I", data, position)[0]
        kind = data[position + 4:position + 8]
        body = data[position + 8:position + 8 + length]
        crc = struct.unpack_from(">I", data, position + 8 + length)[0]
        assert zlib.crc32(kind + body) & 0xffffffff == crc, path
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and color in (2, 6) and compression == filtering == interlace == 0, path
        elif kind == b"IDAT":
            compressed.extend(body)
        elif kind == b"IEND":
            break
        position += 12 + length
    channels = 4 if color == 6 else 3
    stride = width * channels
    raw = zlib.decompress(compressed)
    assert len(raw) == height * (stride + 1), path
    previous = bytearray(stride)
    rows = []
    for y in range(height):
        start = y * (stride + 1)
        method, row = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        assert 0 <= method <= 4, path
        if method:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                above = previous[x]
                if method == 1:
                    prediction = left
                elif method == 2:
                    prediction = above
                elif method == 3:
                    prediction = (left + above) // 2
                else:
                    upper_left = previous[x - channels] if x >= channels else 0
                    p = left + above - upper_left
                    a, b, c = abs(p - left), abs(p - above), abs(p - upper_left)
                    prediction = left if a <= b and a <= c else above if b <= c else upper_left
                row[x] = (row[x] + prediction) & 255
        rows.append(row)
        previous = row
    return width, height, channels, rows


def region(image, box):
    width, height, channels, rows = image
    values = []
    for y in range(int(box[1] * height), int(box[3] * height), 2):
        row = rows[y]
        for x in range(int(box[0] * width), int(box[2] * width), 2):
            offset = x * channels
            values.append(.2126 * row[offset] + .7152 * row[offset + 1] + .0722 * row[offset + 2])
    return values


def stats(values):
    ordered = sorted(values)
    return {"samples": len(values), "meanSrgb255": round(sum(values) / len(values), 3),
            "p10Srgb255": round(ordered[int(len(ordered) * .1)], 3),
            "medianSrgb255": round(ordered[len(ordered) // 2], 3),
            "p90Srgb255": round(ordered[int(len(ordered) * .9)], 3),
            "nearBlackFractionBelow8": round(sum(x < 8 for x in values) / len(values), 5)}


def analyze(folder, mode):
    report = {"directory": str(folder.relative_to(PROJECT)), "views": []}
    for view in (0, 1):
        regions = {"playArea": (.1, .35, .9, .92), "ground": (.1, .6, .9, .92),
                   "grayLeft": (.432, .48, .452, .52), "grayRight": (.548, .48, .568, .52)}
        images = {role: pixels(folder / f"Room{mode}_lighting_{role}_{view}.png") for role in ("host", "guest")}
        assert images["host"][:2] == images["guest"][:2]
        row = {"view": view, "width": images["host"][0], "height": images["host"][1], "regions": {}}
        for name, box in regions.items():
            host, guest = [region(images[role], box) for role in ("host", "guest")]
            row["regions"][name] = {"host": stats(host), "guest": stats(guest),
                                    "hostGuestMeanAbsoluteDifferenceSrgb255": round(sum(abs(a-b) for a, b in zip(host, guest)) / len(host), 3)}
        report["views"].append(row)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", help="Capture folder relative to the canonical project Saved directory")
    parser.add_argument("--mode", type=int, choices=(0, 1, 2), default=2)
    args = parser.parse_args()
    folder = (PROJECT / "Saved" / args.folder).resolve()
    assert folder.is_relative_to((PROJECT / "Saved").resolve()) and folder.is_dir()
    result = analyze(folder, args.mode)
    (folder / "LightingPixels.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
