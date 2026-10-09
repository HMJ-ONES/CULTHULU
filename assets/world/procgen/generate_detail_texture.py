#!/usr/bin/env python3
"""CULT-ULHU procedural detail texture (wave 32).

Generates a single 512px "eldritch detail" texture — grime, cracks, and
faint rune-scratches — intended as a multiply/overlay detail layer in the
UE5 master materials. One texture drags all 50+ props toward the art
direction at once for ~100KB. See unreal/Docs/ArtImportAndPerf.md.

Neutral mid-grey base (128): as a multiply layer it leaves albedo
unchanged where clean, darkens where grimed. Deterministic (seeded).
"""

import math
import random
import struct
import sys
import zlib

SIZE = 512


def write_png(path, pixels):
    """pixels: SIZE*SIZE bytes, grayscale."""
    raw = b"".join(b"\x00" + bytes(pixels[y * SIZE:(y + 1) * SIZE])
                   for y in range(SIZE))
    comp = zlib.compress(raw, 9)

    def chunk(typ, data):
        c = struct.pack(">I", len(data)) + typ + data
        c += struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)
        return c

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 0, 0, 0, 0)) +
           chunk(b"IDAT", comp) +
           chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 20261009
    out = sys.argv[2] if len(sys.argv) > 2 else \
        "assets/world/textures/detail-eldritch.png"
    rng = random.Random(seed)

    # Base: mid-grey with low-frequency mottling.
    field = [[128.0] * SIZE for _ in range(SIZE)]
    blobs = 90
    centers = [(rng.uniform(0, SIZE), rng.uniform(0, SIZE),
                rng.uniform(30, 120), rng.uniform(-38, 22))
               for _ in range(blobs)]
    for y in range(SIZE):
        for x in range(SIZE):
            v = 128.0
            for cx, cy, rad, amp in centers:
                dx, dy = x - cx, y - cy
                # wrap distance for tileability
                dx -= SIZE * round(dx / SIZE)
                dy -= SIZE * round(dy / SIZE)
                d2 = dx * dx + dy * dy
                if d2 < rad * rad * 4:
                    v += amp * math.exp(-d2 / (rad * rad))
            field[y][x] = v

    # Cracks: dark random walks.
    for _ in range(26):
        x, y = rng.uniform(0, SIZE), rng.uniform(0, SIZE)
        ang = rng.uniform(0, 2 * math.pi)
        steps = rng.randint(40, 140)
        for _ in range(steps):
            ang += rng.uniform(-0.6, 0.6)
            x += math.cos(ang) * 2.0
            y += math.sin(ang) * 2.0
            ix, iy = int(x) % SIZE, int(y) % SIZE
            dark = rng.uniform(35, 70)
            field[iy][ix] = min(field[iy][ix], dark)
            # slight bleed to neighbours
            for ox, oy in ((1, 0), (0, 1)):
                jx, jy = (ix + ox) % SIZE, (iy + oy) % SIZE
                field[jy][jx] = min(field[jy][jx], dark + 25)

    # Rune scratches: short angular glyph strokes, faint.
    for _ in range(60):
        x, y = rng.uniform(0, SIZE), rng.uniform(0, SIZE)
        strokes = rng.randint(2, 4)
        ang = rng.uniform(0, 2 * math.pi)
        for _ in range(strokes):
            leng = rng.uniform(6, 22)
            x2 = x + math.cos(ang) * leng
            y2 = y + math.sin(ang) * leng
            n = max(2, int(leng))
            for i in range(n + 1):
                px = int(x + (x2 - x) * i / n) % SIZE
                py = int(y + (y2 - y) * i / n) % SIZE
                field[py][px] = min(field[py][px], rng.uniform(85, 110))
            x, y = x2, y2
            ang += rng.uniform(-1.2, 1.2)

    pixels = [max(0, min(255, int(field[y][x])))
              for y in range(SIZE) for x in range(SIZE)]
    write_png(out, pixels)

    import os
    kb = os.path.getsize(out) / 1024
    print(f"{out}: {SIZE}x{SIZE} grayscale, {kb:.1f} KB")
    assert kb <= 1024, "texture over budget"


if __name__ == "__main__":
    main()
