#!/usr/bin/env python3
"""CULT-ULHU procedural prop generator (wave 32).

Generates original low-poly GLB props in-house — no third-party IP, no
binaries to license, tiny triangle counts. This script IS the pipeline:
edit the builders below, re-run, and the .glb files (plus .glb.b64
sidecars for git) regenerate deterministically.

Usage:
    python3 generate_props.py [--out DIR]

New in wave 32: fills the wave-17 honest gaps — stalactites, dark
crystals, hanging chains — for the cave zones (cavern_mouth, Vale of
Pnath), which had no vertical dressing at all.

Scale: 1 unit = 1 metre. All props target <= 2000 tris (world-prop
budget; see tests/tests_wave19.cpp).
"""

import argparse
import base64
import json
import math
import os
import random
import struct
import sys

# ---------------------------------------------------------------------------
# Minimal GLB writer (binary glTF 2.0, one mesh, POSITION+NORMAL, one material)
# ---------------------------------------------------------------------------

def _face_normal(a, b, c):
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    l = math.sqrt(nx * nx + ny * ny + nz * nz)
    if l < 1e-9:
        return (0.0, 1.0, 0.0)
    return (nx / l, ny / l, nz / l)


def write_glb(path, triangles, base_color=(0.5, 0.5, 0.5, 1.0),
              emissive=(0.0, 0.0, 0.0)):
    """triangles: iterable of (a, b, c) vertex triples. Flat normals."""
    positions = []
    normals = []
    for a, b, c in triangles:
        n = _face_normal(a, b, c)
        for v in (a, b, c):
            positions.extend(v)
            normals.extend(n)
    pos_bytes = struct.pack("<%df" % len(positions), *positions)
    nrm_bytes = struct.pack("<%df" % len(normals), *normals)

    # min/max for accessors
    xs = positions[0::3]
    ys = positions[1::3]
    zs = positions[2::3]

    bin_blob = pos_bytes + nrm_bytes
    # pad BIN to 4-byte alignment
    while len(bin_blob) % 4:
        bin_blob += b"\x00"

    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bytes),
         "target": 34962},
        {"buffer": 0, "byteOffset": len(pos_bytes),
         "byteLength": len(nrm_bytes), "target": 34962},
    ]
    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": len(positions) // 3,
         "type": "VEC3",
         "min": [min(xs), min(ys), min(zs)],
         "max": [max(xs), max(ys), max(zs)]},
        {"bufferView": 1, "componentType": 5126, "count": len(normals) // 3,
         "type": "VEC3"},
    ]
    doc = {
        "asset": {"version": "2.0",
                  "generator": "cult-ulhu procgen (wave 32)"},
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1},
            "material": 0,
        }]}],
        "materials": [{
            "pbrMetallicRoughness": {
                "baseColorFactor": list(base_color),
                "metallicFactor": 0.0,
                "roughnessFactor": 0.95,
            },
            "emissiveFactor": list(emissive),
        }],
        "buffers": [{"byteLength": len(bin_blob)}],
        "bufferViews": views,
        "accessors": accessors,
    }
    json_bytes = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4:
        json_bytes += b" "

    total = 12 + 8 + len(json_bytes) + 8 + len(bin_blob)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(json_bytes), 0x4E4F534A))
        f.write(json_bytes)
        f.write(struct.pack("<II", len(bin_blob), 0x004E4942))
        f.write(bin_blob)
    return len(triangles)


# ---------------------------------------------------------------------------
# Primitive builders (each yields triangle triples)
# ---------------------------------------------------------------------------

def _xform(tris, matrix=None, translate=(0, 0, 0), scale=(1, 1, 1),
           rot_y=0.0, rot_x=0.0, rot_z=0.0):
    cy, sy = math.cos(rot_y), math.sin(rot_y)
    cx, sx = math.cos(rot_x), math.sin(rot_x)
    cz, sz = math.cos(rot_z), math.sin(rot_z)
    out = []
    for tri in tris:
        nt = []
        for (x, y, z) in tri:
            x, y, z = x * scale[0], y * scale[1], z * scale[2]
            # rot_x
            y, z = y * cx - z * sx, y * sx + z * cx
            # rot_z
            x, y = x * cz - y * sz, x * sz + y * cz
            # rot_y
            x, z = x * cy + z * sy, -x * sy + z * cy
            nt.append((x + translate[0], y + translate[1],
                       z + translate[2]))
        out.append(tuple(nt))
    return out


def cone(radius, height, sides, rng, jitter=0.25, tip_jitter=0.35):
    """Cone along +Y, base at y=0, tip at y=height. Jittered ring."""
    ring = []
    for i in range(sides):
        a = 2 * math.pi * i / sides
        jr = 1.0 + rng.uniform(-jitter, jitter)
        ring.append((math.cos(a) * radius * jr, 0.0,
                     math.sin(a) * radius * jr))
    tip = (rng.uniform(-tip_jitter * radius, tip_jitter * radius), height,
           rng.uniform(-tip_jitter * radius, tip_jitter * radius))
    tris = []
    for i in range(sides):
        a = ring[i]
        b = ring[(i + 1) % sides]
        tris.append((a, b, tip))
    # base cap (fan around center)
    c = (0.0, 0.0, 0.0)
    for i in range(sides):
        tris.append((c, ring[(i + 1) % sides], ring[i]))
    return tris


def octahedron(sx, sy, sz):
    px, nx = (sx, 0, 0), (-sx, 0, 0)
    py, ny = (0, sy, 0), (0, -sy, 0)
    pz, nz = (0, 0, sz), (0, 0, -sz)
    return [
        (py, px, pz), (py, pz, nx), (py, nx, nz), (py, nz, px),
        (ny, pz, px), (ny, nx, pz), (ny, nz, nx), (ny, px, nz),
    ]


def torus(R, r, sides, rings):
    tris = []
    for i in range(rings):
        for j in range(sides):
            a0 = 2 * math.pi * i / rings
            a1 = 2 * math.pi * (i + 1) / rings
            b0 = 2 * math.pi * j / sides
            b1 = 2 * math.pi * (j + 1) / sides

            def pt(a, b):
                return ((R + r * math.cos(b)) * math.cos(a),
                        r * math.sin(b),
                        (R + r * math.cos(b)) * math.sin(a))

            p00, p10, p11, p01 = pt(a0, b0), pt(a1, b0), pt(a1, b1), pt(a0, b1)
            tris.append((p00, p10, p11))
            tris.append((p00, p11, p01))
    return tris


# ---------------------------------------------------------------------------
# Prop assemblers — wave-32 gap fillers
# ---------------------------------------------------------------------------

def stalactite_cluster(rng):
    """Hangs from cave ceilings: 4-6 displaced cones, tips down."""
    tris = []
    n = rng.randint(4, 6)
    for _ in range(n):
        h = rng.uniform(2.0, 5.5)
        rad = rng.uniform(0.35, 0.8)
        ox = rng.uniform(-2.2, 2.2)
        oz = rng.uniform(-2.2, 2.2)
        c = cone(rad, h, 7, rng)
        # flip to point down, top at y=0
        c = _xform(c, rot_x=math.pi, translate=(ox, 0, oz))
        tris.extend(c)
    return tris


def stalagmite_cluster(rng):
    """Rises from cave floors: 3-5 displaced cones, tips up."""
    tris = []
    n = rng.randint(3, 5)
    for _ in range(n):
        h = rng.uniform(1.2, 3.5)
        rad = rng.uniform(0.4, 0.9)
        ox = rng.uniform(-2.5, 2.5)
        oz = rng.uniform(-2.5, 2.5)
        tris.extend(_xform(cone(rad, h, 7, rng), translate=(ox, 0, oz)))
    return tris


def dark_crystal_cluster(rng):
    """Cluster of stretched black-violet octahedrons, tilted outward."""
    tris = []
    n = rng.randint(4, 6)
    for _ in range(n):
        sx = rng.uniform(0.25, 0.5)
        sy = rng.uniform(1.0, 2.6)
        sz = rng.uniform(0.25, 0.5)
        ox = rng.uniform(-1.4, 1.4)
        oz = rng.uniform(-1.4, 1.4)
        tilt = rng.uniform(-0.35, 0.35)
        c = octahedron(sx, sy, sz)
        c = _xform(c, rot_x=tilt, rot_z=rng.uniform(-0.35, 0.35),
                   translate=(ox, sy * 0.45, oz))
        tris.extend(c)
    return tris


def hanging_chain(rng, links=7):
    """Vertical chain of torus links, alternating 90 degrees."""
    tris = []
    R, r = 0.28, 0.09
    y = 0.0
    for i in range(links):
        link = torus(R, r, 8, 12)
        link = _xform(link, rot_y=math.pi / 2 if i % 2 else 0.0,
                      translate=(0, y, 0))
        tris.extend(link)
        y -= 2 * R * 1.55
    return tris


PROPS = [
    ("stalactite-cluster", stalactite_cluster,
     (0.16, 0.15, 0.19, 1.0), (0.0, 0.0, 0.0)),
    ("stalagmite-cluster", stalagmite_cluster,
     (0.19, 0.18, 0.22, 1.0), (0.0, 0.0, 0.0)),
    ("dark-crystal-cluster", dark_crystal_cluster,
     (0.10, 0.05, 0.16, 1.0), (0.05, 0.0, 0.12)),
    ("hanging-chain", hanging_chain,
     (0.12, 0.11, 0.13, 1.0), (0.0, 0.0, 0.0)),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None,
                    help="output dir (default: props/ next to this script)")
    ap.add_argument("--seed", type=int, default=20261009)
    args = ap.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    out = args.out or os.path.join(os.path.dirname(script_dir), "props")
    os.makedirs(out, exist_ok=True)

    for name, builder, color, emissive in PROPS:
        rng = random.Random(args.seed + hash(name) % 100000)
        tris = builder(rng)
        path = os.path.join(out, name + ".glb")
        ntris = write_glb(path, tris, base_color=color, emissive=emissive)
        # .b64 sidecar for git (push_files corrupts raw binaries)
        with open(path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("ascii")
        with open(path + ".b64", "w") as f:
            f.write(b64)
        print(f"{name}.glb: {ntris} tris, "
              f"{os.path.getsize(path) / 1024:.1f} KB")
        assert ntris <= 2000, f"{name} exceeds world-prop tri budget"


if __name__ == "__main__":
    main()
