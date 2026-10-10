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

Wave 35: 8 more gap fillers — 4 forest variants (ashen_grove mid-storey:
ash-thicket, hanging-moss, fungal-shelf, root-tangle) and 4 deep-ruins
variants (shattered_court masonry: ruined-column-b, fallen-lintel,
flagstone-slab, broken-obelisk). These are multi-tone via COLOR_0 vertex
colors; wave-35 builders return (tris, per-triangle RGBA).
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
              emissive=(0.0, 0.0, 0.0), colors=None,
              gen="cult-ulhu procgen (wave 32)"):
    """triangles: iterable of (a, b, c) vertex triples. Flat normals.
    colors: optional per-triangle RGBA list -> COLOR_0 vertex colors
    (multiplies the material's white base color)."""
    positions = []
    normals = []
    color_data = []
    for idx, (a, b, c) in enumerate(triangles):
        n = _face_normal(a, b, c)
        col = colors[idx] if colors is not None else None
        for v in (a, b, c):
            positions.extend(v)
            normals.extend(n)
            if col is not None:
                color_data.extend(col)
    pos_bytes = struct.pack("<%df" % len(positions), *positions)
    nrm_bytes = struct.pack("<%df" % len(normals), *normals)
    col_bytes = struct.pack("<%df" % len(color_data), *color_data) \
        if color_data else b""

    # min/max for accessors
    xs = positions[0::3]
    ys = positions[1::3]
    zs = positions[2::3]

    bin_blob = pos_bytes + nrm_bytes + col_bytes
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
    attributes = {"POSITION": 0, "NORMAL": 1}
    material_color = list(base_color)
    if colors is not None:
        views.append({"buffer": 0,
                      "byteOffset": len(pos_bytes) + len(nrm_bytes),
                      "byteLength": len(col_bytes), "target": 34962})
        accessors.append({"bufferView": 2, "componentType": 5126,
                          "count": len(color_data) // 4, "type": "VEC4"})
        attributes["COLOR_0"] = 2
        material_color = [1.0, 1.0, 1.0, 1.0]  # vertex colors carry the tint
    doc = {
        "asset": {"version": "2.0",
                  "generator": gen},
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{
            "attributes": attributes,
            "material": 0,
        }]}],
        "materials": [{
            "pbrMetallicRoughness": {
                "baseColorFactor": material_color,
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


def cylinder(r_bot, r_top, height, sides, segments=1, rng=None, jitter=0.0,
             y0=0.0):
    """Tapered cylinder along +Y, base at y=y0. Per-ring radial jitter."""
    rings = []
    for s in range(segments + 1):
        y = y0 + height * s / segments
        r = r_bot + (r_top - r_bot) * s / segments
        ring = []
        for i in range(sides):
            a = 2 * math.pi * i / sides
            jr = 1.0 + (rng.uniform(-jitter, jitter) if rng else 0.0)
            ring.append((math.cos(a) * r * jr, y, math.sin(a) * r * jr))
        rings.append(ring)
    tris = []
    for s in range(segments):
        lo, hi = rings[s], rings[s + 1]
        for i in range(sides):
            j = (i + 1) % sides
            tris.append((lo[i], hi[i], hi[j]))
            tris.append((lo[i], hi[j], lo[j]))
    cb, ct = (0.0, y0, 0.0), (0.0, y0 + height, 0.0)
    for i in range(sides):
        j = (i + 1) % sides
        tris.append((cb, rings[0][i], rings[0][j]))   # bottom, -Y
        tris.append((ct, rings[-1][j], rings[-1][i]))  # top, +Y
    return tris


def box(w, h, d, cx=0.0, cy=0.0, cz=0.0):
    """Axis-aligned box centered on (cx, cy, cz). 12 tris, outward winding."""
    x, y, z = w / 2.0, h / 2.0, d / 2.0
    c = [(-x, -y, -z), (x, -y, -z), (x, y, -z), (-x, y, -z),
         (-x, -y, z), (x, -y, z), (x, y, z), (-x, y, z)]
    faces = [(4, 5, 6), (4, 6, 7),       # +Z
             (1, 0, 3), (1, 3, 2),       # -Z
             (5, 1, 2), (5, 2, 6),       # +X
             (0, 4, 7), (0, 7, 3),       # -X
             (3, 7, 6), (3, 6, 2),       # +Y
             (0, 1, 5), (0, 5, 4)]       # -Y
    return [tuple((c[k][0] + cx, c[k][1] + cy, c[k][2] + cz)
                  for k in face) for face in faces]


# ---------------------------------------------------------------------------
# Prop assemblers — wave-35 gap fillers
# ---------------------------------------------------------------------------
# ashen_grove (dead forest) had only trees/dead trunks/tombstones; it needed
# mid-storey dressing. shattered_court (ruined courtyard) needed more broken
# masonry variety, also reused at the cavern_mouth "deep ruins" edge.
# Wave-35 props are multi-tone via COLOR_0 vertex colors; builders return
# (tris, per-triangle RGBA colors).

_BARK = (0.18, 0.16, 0.20, 1.0)    # ash-choked bark
_ASHLEAF = (0.44, 0.42, 0.47, 1.0)  # pale ash-grey foliage
_MOSS = (0.46, 0.52, 0.40, 1.0)    # lichen strands
_FUNGUS = (0.58, 0.52, 0.66, 1.0)  # pale violet shelf fungus
_ROOT = (0.13, 0.12, 0.15, 1.0)    # dark exposed roots
_STONE = (0.23, 0.22, 0.26, 1.0)   # deep-ruin stone
_STONE2 = (0.30, 0.29, 0.33, 1.0)  # broken faces catch a little more light


def _merge(parts):
    tris, colors = [], []
    for t, c in parts:
        tris.extend(t)
        colors.extend([c] * len(t))
    return tris, colors


def ash_thicket(rng):
    """Gnarled multi-trunk tree: 3 leaning trunks with ash-grey canopies."""
    parts = []
    for _ in range(3):
        h = rng.uniform(6.5, 9.0)
        ang = rng.uniform(0.0, 2 * math.pi)
        rad = rng.uniform(0.4, 1.1)
        t = cylinder(0.55, 0.26, h, 6, 2, rng, 0.18)
        t = _xform(t, rot_z=rng.uniform(-0.14, 0.14),
                   rot_x=rng.uniform(-0.10, 0.10),
                   translate=(math.cos(ang) * rad, 0.0,
                              math.sin(ang) * rad))
        parts.append((t, _BARK))
        f = cone(rng.uniform(2.0, 2.9), rng.uniform(3.2, 4.6), 7, rng,
                 jitter=0.35)
        fx = math.cos(ang) * rad + rng.uniform(-0.5, 0.5)
        fz = math.sin(ang) * rad + rng.uniform(-0.5, 0.5)
        parts.append((_xform(f, translate=(fx, h * 0.72, fz)), _ASHLEAF))
    return _merge(parts)


def hanging_moss(rng):
    """Horizontal dead branch draped with pale lichen strands."""
    parts = []
    br = cylinder(0.20, 0.12, 4.2, 5, 1, rng, 0.10)
    br = _xform(br, rot_z=math.pi / 2 - 0.06, translate=(0.0, 5.2, 0.0))
    parts.append((br, _BARK))
    for _ in range(rng.randint(5, 7)):
        h = rng.uniform(1.4, 2.8)
        s = cone(rng.uniform(0.10, 0.18), h, 5, rng, jitter=0.3)
        # flip to hang tips-down, top attached near the branch
        s = _xform(s, rot_x=math.pi,
                   translate=(rng.uniform(-1.9, 1.9), 5.1,
                              rng.uniform(-0.3, 0.3)))
        parts.append((s, _MOSS))
    return _merge(parts)


def fungal_shelf(rng):
    """Dead stub sprouting pale-violet bracket fungi."""
    parts = []
    parts.append((cylinder(0.62, 0.42, 2.3, 7, 1, rng, 0.12), _BARK))
    for _ in range(rng.randint(6, 8)):
        w = rng.uniform(0.9, 1.4)
        shelf = box(w, 0.22, w * 0.7)
        y = rng.uniform(0.5, 2.0)
        a = rng.uniform(0.0, 2 * math.pi)
        shelf = _xform(shelf, rot_y=a, rot_x=rng.uniform(-0.15, 0.15),
                       translate=(math.cos(a) * 0.75, y,
                                  math.sin(a) * 0.75))
        parts.append((shelf, _FUNGUS))
    return _merge(parts)


def root_tangle(rng):
    """Exposed roots radiating half-buried from a central hummock."""
    parts = []
    for _ in range(rng.randint(6, 8)):
        h = rng.uniform(2.8, 4.2)
        r = cylinder(0.34, 0.07, h, 5, 2, rng, 0.25)
        a = rng.uniform(0.0, 2 * math.pi)
        r = _xform(r, rot_x=math.pi / 2 - rng.uniform(0.25, 0.5), rot_y=a,
                   translate=(math.cos(a) * 0.8,
                              rng.uniform(0.5, 0.9),
                              math.sin(a) * 0.8))
        parts.append((r, _ROOT))
    return _merge(parts)


def ruined_column_b(rng):
    """Snapped fluted column: sheared upper chunk + fallen capital."""
    parts = []
    h1 = rng.uniform(3.2, 4.2)
    parts.append((cylinder(0.95, 0.72, h1, 10, 2, rng, 0.05), _STONE))
    h2 = rng.uniform(1.2, 1.8)
    chunk = cylinder(0.72, 0.60, h2, 10, 1, rng, 0.08)
    chunk = _xform(chunk, rot_z=rng.uniform(0.15, 0.3),
                   rot_x=rng.uniform(-0.1, 0.1),
                   translate=(rng.uniform(0.1, 0.3), h1 - 0.1,
                              rng.uniform(-0.2, 0.2)))
    parts.append((chunk, _STONE2))
    cap = box(1.7, 0.7, 1.7)
    cap = _xform(cap, rot_y=rng.uniform(0.0, 3.14),
                 translate=(rng.uniform(1.8, 2.6), 0.35,
                            rng.uniform(-1.5, 1.5)))
    parts.append((cap, _STONE2))
    return _merge(parts)


def fallen_lintel(rng):
    """Long beam lying on the ground with a snapped end chunk."""
    parts = []
    L = rng.uniform(4.6, 6.0)
    beam = box(L, 1.0, 1.2)
    beam = _xform(beam, rot_y=rng.uniform(0.0, 3.14),
                  rot_z=rng.uniform(-0.04, 0.04),
                  translate=(0.0, 0.5, 0.0))
    parts.append((beam, _STONE))
    c = box(rng.uniform(0.8, 1.2), 0.9, 1.1)
    c = _xform(c, rot_y=rng.uniform(0.0, 3.14),
               translate=(L * 0.5 + 0.9, 0.45, rng.uniform(-0.4, 0.4)))
    parts.append((c, _STONE2))
    return _merge(parts)


def flagstone_slab(rng):
    """Three cracked flagstones, slightly offset and tilted."""
    parts = []
    for i in range(3):
        s = box(rng.uniform(1.9, 2.3), 0.25, rng.uniform(1.5, 1.9))
        s = _xform(s, rot_y=rng.uniform(-0.12, 0.12),
                   translate=(i * rng.uniform(1.9, 2.1) - 2.0,
                              rng.uniform(0.10, 0.16),
                              rng.uniform(-0.3, 0.3)))
        parts.append((s, _STONE if i % 2 == 0 else _STONE2))
    return _merge(parts)


def broken_obelisk(rng):
    """Snapped square obelisk with its tip fallen beside it."""
    parts = []
    h = rng.uniform(4.2, 5.4)
    shaft = cylinder(1.05, 0.50, h, 4, 2, rng, 0.04)
    shaft = _xform(shaft, rot_y=math.pi / 4)
    parts.append((shaft, _STONE))
    tip = box(rng.uniform(1.1, 1.4), rng.uniform(0.7, 1.0),
              rng.uniform(1.1, 1.4))
    tip = _xform(tip, rot_y=rng.uniform(0.0, 3.14),
                 translate=(rng.uniform(2.0, 2.8), 0.4,
                            rng.uniform(-1.2, 1.2)))
    parts.append((tip, _STONE2))
    return _merge(parts)


PROPS_WAVE35 = [
    ("ash-thicket", ash_thicket, (0.0, 0.0, 0.0)),
    ("hanging-moss", hanging_moss, (0.0, 0.0, 0.0)),
    ("fungal-shelf", fungal_shelf, (0.03, 0.02, 0.06)),
    ("root-tangle", root_tangle, (0.0, 0.0, 0.0)),
    ("ruined-column-b", ruined_column_b, (0.0, 0.0, 0.0)),
    ("fallen-lintel", fallen_lintel, (0.0, 0.0, 0.0)),
    ("flagstone-slab", flagstone_slab, (0.0, 0.0, 0.0)),
    ("broken-obelisk", broken_obelisk, (0.0, 0.0, 0.0)),
]


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
    ap.add_argument("--wave35", action="store_true",
                    help="build only the wave-35 props (skip wave-32 list, "
                         "whose seeding is not stable across runs)")
    args = ap.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    out = args.out or os.path.join(os.path.dirname(script_dir), "props")
    os.makedirs(out, exist_ok=True)

    if not args.wave35:
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

    # Wave 35: multi-tone props (builders return (tris, per-tri colors)).
    for name, builder, emissive in PROPS_WAVE35:
        rng = random.Random(args.seed + hash(name) % 100000)
        tris, colors = builder(rng)
        path = os.path.join(out, name + ".glb")
        ntris = write_glb(path, tris, emissive=emissive, colors=colors,
                          gen="cult-ulhu procgen (wave 35)")
        with open(path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode("ascii")
        with open(path + ".b64", "w") as f:
            f.write(b64)
        print(f"{name}.glb: {ntris} tris, "
              f"{os.path.getsize(path) / 1024:.1f} KB")
        assert ntris <= 2000, f"{name} exceeds world-prop tri budget"


if __name__ == "__main__":
    main()
