#!/usr/bin/env python3
"""CULT-ULHU dark wilds world generator.

Builds the surface world the owner asked for (Oct 14, 2026): real terrain
with altitude variation instead of a stretched cube — hills, a massive open
cavern, cave mouths, three clumped razed-city complexes, dry dead woods and
a deadly swamp with feral-beast spawns — plus the data the UE5 side needs
to rebuild it in-editor in one step (see unreal/Tools/BuildDarkWilds.py).

Outputs (all deterministic, seed 20261014, stdlib only):
  assets/world/terrain/dark_wilds_ground.obj   terrain mesh, 256x256 grid,
      real heights, computed normals, UVs, zone-tinted vertex colors
      (v x y z r g b lines — UE's OBJ importer reads them)
  assets/world/terrain/dark_wilds_height.png   8-bit heightmap (for an
      optional real UE Landscape later)
  assets/world/textures/ground-detail-dark.png 512px tiling detail texture
  assets/maps/dark_wilds.map                   zones + atmosphere + ~320
      placements (y baked from the height field) + lod + spawn lines

Run:  python3 assets/world/generate_dark_wilds.py
"""

import math
import os
import random
import struct
import sys
import zlib

SEED = 20261014
WORLD = 1200.0          # world is WORLD x WORLD meters, centered on origin
GRID = 256              # terrain mesh resolution (verts per side)
CM = 100.0              # UE imports 1 unit = 1 cm: write the OBJ in cm

HERE = os.path.dirname(os.path.abspath(__file__))
TERRAIN_DIR = os.path.join(HERE, "terrain")
TEXTURE_DIR = os.path.join(HERE, "textures")
MAP_PATH = os.path.join(HERE, "..", "maps", "dark_wilds.map")


# ---------------------------------------------------------------------------
# deterministic value noise
# ---------------------------------------------------------------------------

def _hash(ix, iz, seed):
    h = (ix * 374761393 + iz * 668265263 + seed * 144665) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFFFFFF) / 4294967295.0


def vnoise(x, z, seed):
    ix, iz = math.floor(x), math.floor(z)
    fx, fz = x - ix, z - iz
    sx = fx * fx * (3 - 2 * fx)
    sz = fz * fz * (3 - 2 * fz)
    a = _hash(ix, iz, seed)
    b = _hash(ix + 1, iz, seed)
    c = _hash(ix, iz + 1, seed)
    d = _hash(ix + 1, iz + 1, seed)
    return (a + (b - a) * sx + (c - a) * sz + (a - b - c + d) * sx * sz) * 2 - 1


def fbm2(x, z, octaves, seed=SEED):
    v, amp, f = 0.0, 0.5, 1.0
    for _ in range(octaves):
        v += amp * vnoise(x * f, z * f, seed)
        amp *= 0.5
        f *= 2.03
    return v  # ~[-1, 1]


def sstep(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a))) if b > a else 0.0
    return t * t * (3 - 2 * t)


def rect_mask(x, z, x0, z0, x1, z1, feather):
    dx = min(x - x0, x1 - x)
    dz = min(z - z0, z1 - z)
    d = min(dx, dz)
    if d <= 0:
        return 0.0
    return sstep(0.0, feather, d)


def disc_mask(x, z, cx, cz, r, feather):
    d = math.hypot(x - cx, z - cz)
    if d >= r:
        return 0.0
    return 1.0 - sstep(r - feather, r, d)


# ---------------------------------------------------------------------------
# height field
# ---------------------------------------------------------------------------

def height(x, z):
    detail = fbm2(x * 0.006, z * 0.006, 4) * 10.0
    h = detail
    # highlands: rolling hills, lifted
    m = rect_mask(x, z, -600, -350, -150, 350, 120)
    hills = 12.0 + fbm2(x * 0.003 + 7.3, z * 0.003 - 2.1, 4) * 22.0
    h = h * (1 - m) + (hills + detail * 0.5) * m
    # deep cavern: massive bowl carved through everything
    m = disc_mask(x, z, -300, -400, 200, 90)
    bowl = -26.0 + fbm2(x * 0.02 + 3.1, z * 0.02 - 8.7, 2) * 3.0
    h = h * (1 - m) + bowl * m
    # razed city: flattened plateau
    m = rect_mask(x, z, 50, -250, 450, 250, 100)
    h = h * (1 - m) + (5.0 + detail * 0.15) * m
    # dead swamp: low basin
    m = rect_mask(x, z, -50, -550, 350, -300, 90)
    h = h * (1 - m) + (-3.5 + detail * 0.1) * m
    return h


def zone_at(x, z):
    """Priority order matches the .map zone file order (specific first)."""
    if disc_mask(x, z, -300, -400, 200, 40) > 0.5:
        return "deep_cavern"
    if rect_mask(x, z, -520, -620, -80, -180, 60) > 0.0:
        return "cavern_approach"
    if rect_mask(x, z, 50, -250, 450, 250, 40) > 0.5:
        return "razed_city"
    if rect_mask(x, z, -50, -550, 350, -300, 40) > 0.5:
        return "dead_swamp"
    if rect_mask(x, z, -100, 250, 300, 550, 60) > 0.5:
        return "drywood"
    return "highlands"


# ---------------------------------------------------------------------------
# tiny PNG writer (stdlib only)
# ---------------------------------------------------------------------------

def _png_chunk(typ, data):
    c = struct.pack(">I", len(data)) + typ + data
    c += struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)
    return c


def write_png(path, w, h, rows, color_type=2):
    """rows: list of bytes, w*3 (rgb) or w (grey) per row."""
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    png = b"\x89PNG\r\n\x1a\n"
    png += _png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, color_type,
                                           0, 0, 0))
    png += _png_chunk(b"IDAT", zlib.compress(raw))
    png += _png_chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


# ---------------------------------------------------------------------------
# outputs
# ---------------------------------------------------------------------------

ZONE_TINT = {
    "highlands":       (0.34, 0.32, 0.35),
    "cavern_approach": (0.22, 0.20, 0.24),
    "deep_cavern":     (0.11, 0.10, 0.14),
    "razed_city":      (0.40, 0.37, 0.33),
    "drywood":         (0.32, 0.30, 0.22),
    "dead_swamp":      (0.15, 0.20, 0.13),
}
ROCK_TINT = (0.20, 0.19, 0.23)


def write_heightmap():
    n = 256
    rows = []
    for j in range(n):
        row = bytearray()
        for i in range(n):
            x = -WORLD / 2 + WORLD * i / (n - 1)
            z = -WORLD / 2 + WORLD * j / (n - 1)
            h = height(x, z)
            v = int(max(0, min(255, (h + 30.0) / 70.0 * 255.0)))
            row.append(v)
        rows.append(bytes(row))
    write_png(os.path.join(TERRAIN_DIR, "dark_wilds_height.png"),
              n, n, rows, color_type=0)
    print("heightmap written")


def write_detail_texture():
    n = 512
    rng = random.Random(SEED + 77)
    rows = []
    for j in range(n):
        row = bytearray()
        for i in range(n):
            g = vnoise(i * 0.08, j * 0.08, SEED + 1) * 0.5 + \
                vnoise(i * 0.31, j * 0.31, SEED + 2) * 0.28
            crack = 1.0 if vnoise(i * 0.05 + 40, j * 0.05, SEED + 3) > 0.55 \
                else 0.0
            peb = 1.0 if rng.random() < 0.012 else 0.0
            base = 0.145 + g * 0.05 - crack * 0.07 + peb * 0.12
            r = max(0.02, min(1.0, base * 1.06))
            gg = max(0.02, min(1.0, base * 0.98))
            b = max(0.02, min(1.0, base * 0.90))
            row += bytes((int(r * 255), int(gg * 255), int(b * 255)))
        rows.append(bytes(row))
    write_png(os.path.join(TEXTURE_DIR, "ground-detail-dark.png"),
              n, n, rows, color_type=2)
    print("detail texture written")


def write_terrain_obj():
    n = GRID
    step = WORLD / (n - 1)
    hs = [[0.0] * n for _ in range(n)]
    for j in range(n):
        z = -WORLD / 2 + j * step
        for i in range(n):
            x = -WORLD / 2 + i * step
            hs[j][i] = height(x, z)

    def hn(i, j):
        i = max(0, min(n - 1, i))
        j = max(0, min(n - 1, j))
        return hs[j][i]

    lines = ["# CULT-ULHU dark wilds terrain — generated, do not hand-edit",
             "# y-up; UE imports with Z-up conversion"]
    # vertices (+ uv + vertex color)
    for j in range(n):
        z = -WORLD / 2 + j * step
        for i in range(n):
            x = -WORLD / 2 + i * step
            y = hs[j][i]
            nx = (hn(i - 1, j) - hn(i + 1, j)) / (2 * step)
            nz = (hn(i, j - 1) - hn(i, j + 1)) / (2 * step)
            inv = 1.0 / math.sqrt(nx * nx + 1 + nz * nz)
            nx, ny, nz = nx * inv, inv, nz * inv
            zn = zone_at(x, z)
            tr, tg, tb = ZONE_TINT[zn]
            slope = 1.0 - ny  # 0 flat .. ~1 cliff
            rock_mix = sstep(0.18, 0.45, slope)
            r = tr * (1 - rock_mix) + ROCK_TINT[0] * rock_mix
            g = tg * (1 - rock_mix) + ROCK_TINT[1] * rock_mix
            b = tb * (1 - rock_mix) + ROCK_TINT[2] * rock_mix
            shade = 0.92 + 0.08 * vnoise(x * 0.05, z * 0.05, SEED + 9)
            lines.append(
                "v %.1f %.1f %.1f %.3f %.3f %.3f" %
                (x * CM, y * CM, z * CM, r * shade, g * shade, b * shade))
    for j in range(n):
        for i in range(n):
            lines.append("vt %.4f %.4f" %
                         ((-WORLD / 2 + i * step) / 60.0,
                          (-WORLD / 2 + j * step) / 60.0))
    # normals
    for j in range(n):
        z = -WORLD / 2 + j * step
        for i in range(n):
            x = -WORLD / 2 + i * step
            nx = (hn(i - 1, j) - hn(i + 1, j)) / (2 * step)
            nz = (hn(i, j - 1) - hn(i, j + 1)) / (2 * step)
            inv = 1.0 / math.sqrt(nx * nx + 1 + nz * nz)
            lines.append("vn %.4f %.4f %.4f" % (nx * inv, inv, nz * inv))
    # faces (upward winding, y-up): (a,c,b) (b,c,d)
    for j in range(n - 1):
        for i in range(n - 1):
            a = j * n + i + 1
            b = a + 1
            c = a + n
            d = c + 1
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a, a, a, c, c, c, b, b, b))
            lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (b, b, b, c, c, c, d, d, d))
    path = os.path.join(TERRAIN_DIR, "dark_wilds_ground.obj")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    tris = (n - 1) * (n - 1) * 2
    print("terrain OBJ written: %d verts, %d tris (%.1f MB)" %
          (n * n, tris, os.path.getsize(path) / 1048576.0))

# ---------------------------------------------------------------------------
# dark_wilds.map writer
# ---------------------------------------------------------------------------

ZONES = [
    # name, minx, miny, minz, maxx, maxy, maxz, fear  (specific first)
    ("deep_cavern",    -500, -40, -600, -100, 60, -200, 85),
    ("cavern_approach", -520, -40, -620,  -80, 60, -180, 62),
    ("razed_city",        50, -40, -250,  450, 60,  250, 50),
    ("dead_swamp",       -50, -40, -550,  350, 60, -300, 70),
    ("drywood",         -100, -40,  250,  300, 60,  550, 35),
    ("highlands",       -600, -40, -350, -150, 60,  350, 40),
]

ATMOSPHERE = [
    # the massive open cavern: lightless bowl, stars drowned
    ("deep_cavern",
     "fog=#030304,0.85 ambient=#0a0a12,0.12 sky=#000000 stars=0.0"),
    # approach: violet dark, thin starlight between the arches
    ("cavern_approach",
     "fog=#080610,0.65 ambient=#161226,0.22 sky=#02020a stars=0.15"),
    # razed city: drowned avenues, deep blue-black
    ("razed_city",
     "fog=#0a0812,0.50 ambient=#1c1626,0.30 sky=#040310 stars=0.70"),
    # dead swamp: green-black murk, heavy air
    ("dead_swamp",
     "fog=#0a120c,0.75 ambient=#14201a,0.20 sky=#030805 stars=0.10"),
    # drywood: thin dark, wind-scoured sky
    ("drywood",
     "fog=#0e0c10,0.40 ambient=#241e2e,0.32 sky=#060410 stars=0.85"),
    # highlands: cold violet dark, clear starfield, watchful
    ("highlands",
     "fog=#0c0a14,0.45 ambient=#201c34,0.35 sky=#05030f stars=0.90"),
]

P = "assets/world/props/"
B = "assets/world/buildings/"


def write_map():
    rng = random.Random(SEED + 5)
    out = []
    out.append("# ==========================================================")
    out.append("# CULT-ULHU map file: dark_wilds — GENERATED, do not hand-edit")
    out.append("# Regenerate: python3 assets/world/generate_dark_wilds.py")
    out.append("#")
    out.append("# FORMAT: same as ruined_city.map (map/zone/place/lod/"
               "atmosphere), plus:")
    out.append("#   spawn <species> <x> <y> <z> <count> <zone>")
    out.append("#       Feral creature spawn entry. <species> must be a key of")
    out.append("#       ModelCatalog::creatureModels(). y is baked from the")
    out.append("#       height field at generation time.")
    out.append("#")
    out.append("# ART DIRECTION - the dark wilds beyond the ruined city.")
    out.append("#   Palette: ash greys on the highlands; basalt near-black in")
    out.append("#   the cavern; dusty bone stone in the razed city; dead moss")
    out.append("#   browns in the drywood; green-black murk in the swamp.")
    out.append("#   Night is the default. Stars die inside the cavern bowl.")
    out.append("#   Budget: host-simulated build — every prop < 2000 tris,")
    out.append("#   terrain is one 130k-tri static mesh (single draw call).")
    out.append("# ==========================================================")
    out.append("")
    out.append("map dark_wilds")
    out.append("")
    for name, x0, y0, z0, x1, y1, z1, fear in ZONES:
        out.append("zone %s %d %d %d %d %d %d %d" %
                   (name, x0, y0, z0, x1, y1, z1, fear))
    out.append("")
    out.append("# --- atmosphere (lighting/mood data for the UE5 binding) ---")
    for name, spec in ATMOSPHERE:
        out.append("atmosphere %s %s" % (name, spec))
    out.append("")

    placed = []

    def scatter(zone, model, count, smin=0.8, smax=1.4, ymin=None):
        n = 0
        guard = 0
        while n < count and guard < count * 40:
            guard += 1
            zx = next(z for z in ZONES if z[0] == zone)
            x = rng.uniform(zx[1], zx[4])
            z = rng.uniform(zx[3], zx[6])
            if zone_at(x, z) != zone:
                continue
            y = height(x, z)
            if ymin is not None and y < ymin:
                continue
            placed.append((model, x, y, z,
                           rng.uniform(0, 360), rng.uniform(smin, smax),
                           zone))
            n += 1

    def at(model, x, z, rot=0.0, scale=1.0, zone=None, sink=0.0):
        placed.append((model, x, height(x, z) - sink, z, rot, scale,
                       zone or zone_at(x, z)))

    # ---- highlands: wind-scoured hills, dead trunks, tall rocks ----
    scatter("highlands", P + "cave-rock-tall-a.glb", 14, 0.9, 1.8)
    scatter("highlands", P + "cave-boulder-a.glb", 12, 0.8, 1.6)
    scatter("highlands", P + "rocks-tall.glb", 10, 0.8, 1.5)
    scatter("highlands", P + "dead-trunk.glb", 10, 0.9, 1.3)
    scatter("highlands", P + "tree-dead-bare.glb", 8, 1.2, 2.0)
    scatter("highlands", P + "bones-scattered.glb", 8, 0.8, 1.2)

    # ---- cavern approach: rock arches marching to the bowl rim ----
    for k in range(5):
        t = k / 4.0
        x = -480 + t * 260 + rng.uniform(-15, 15)
        z = -560 + t * 120 + rng.uniform(-15, 15)
        at(P + "rock-arch.glb",
           x, z, rot=rng.uniform(0, 360), scale=rng.uniform(1.2, 1.8))
    # stalactites hanging under two of the arches
    for (x, z) in [(-420, -530), (-300, -470)]:
        at(P + "stalactite-cluster.glb", x, z, scale=0.8,
           zone="cavern_approach", sink=-6.0)
    # cave mouths on the rim, facing outward from the bowl
    for k in range(4):
        a = math.pi * (0.15 + 0.7 * k / 3.0)
        x = -300 + math.cos(a) * 215
        z = -400 + math.sin(a) * 215
        deg = math.degrees(math.atan2(x + 300, z + 400))
        at(P + "cave-mouth.glb", x, z, rot=deg, scale=rng.uniform(1.0, 1.4))
    scatter("cavern_approach", P + "cave-boulder-c.glb", 10, 0.9, 1.7)
    scatter("cavern_approach", P + "stalagmite-cluster.glb", 8, 0.8, 1.3)

    # ---- deep cavern: the lightless bowl ----
    scatter("deep_cavern", P + "stalagmite-cluster.glb", 18, 1.0, 2.2)
    scatter("deep_cavern", P + "dark-crystal-cluster.glb", 8, 0.8, 1.4)
    scatter("deep_cavern", P + "cave-wall-rock.glb", 10, 1.0, 1.8)
    scatter("deep_cavern", P + "cave-boulder-a.glb", 8, 1.0, 2.0)
    scatter("deep_cavern", P + "bones-scattered.glb", 6, 0.8, 1.2)

    # ---- razed city: three clumped complexes on the plateau ----
    # complex A: fallen forum (colonnade ring + obelisk)
    cx, cz = 150, 0
    for k in range(8):
        a = 2 * math.pi * k / 8
        at(P + "ruined-column-b.glb", cx + math.cos(a) * 26,
           cz + math.sin(a) * 26, rot=math.degrees(a) + 90,
           scale=rng.uniform(0.9, 1.2))
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.5
        at(P + "fallen-lintel.glb", cx + math.cos(a) * 18,
           cz + math.sin(a) * 18, rot=rng.uniform(0, 360))
    at(P + "broken-obelisk.glb", cx, cz, scale=1.3)
    for k in range(6):
        at(P + "flagstone-slab.glb", cx + rng.uniform(-20, 20),
           cz + rng.uniform(-20, 20), rot=rng.uniform(0, 360),
           scale=rng.uniform(0.9, 1.3))
    # complex B: shattered ward (wall ring + ruined towers)
    cx, cz = 320, 120
    for k in range(10):
        a = 2 * math.pi * k / 10
        at(B + "stone-wall-damaged.glb", cx + math.cos(a) * 34,
           cz + math.sin(a) * 34, rot=math.degrees(a) + 90,
           scale=rng.uniform(0.9, 1.2))
    for (ox, oz) in [(-12, -10), (14, 8)]:
        at(B + "tower-square-base.glb", cx + ox, cz + oz, scale=1.1)
        at(B + "tower-square-mid.glb", cx + ox, cz + oz, scale=1.1,
           sink=-7.0)
    for _ in range(6):
        at(B + "wall-broken.glb", cx + rng.uniform(-30, 30),
           cz + rng.uniform(-30, 30), rot=rng.uniform(0, 360))
    for _ in range(8):
        at(P + "rubble-large.glb", cx + rng.uniform(-32, 32),
           cz + rng.uniform(-32, 32), rot=rng.uniform(0, 360),
           scale=rng.uniform(0.8, 1.4))
    for _ in range(4):
        at(P + "beams-collapsed.glb", cx + rng.uniform(-25, 25),
           cz + rng.uniform(-25, 25), rot=rng.uniform(0, 360))
    # complex C: drowned gate (gate arches + crypts)
    cx, cz = 220, -160
    for (ox, oz, rot) in [(-20, 0, 90), (20, 0, 90), (0, -18, 0)]:
        at(B + "arch-dungeon.glb", cx + ox, cz + oz, rot=rot, scale=1.2)
    for k in range(3):
        at(B + "crypt-ruined.glb", cx + rng.uniform(-28, 28),
           cz + rng.uniform(10, 30), rot=rng.uniform(0, 360),
           scale=rng.uniform(0.9, 1.2))
    for _ in range(8):
        at(P + "debris.glb", cx + rng.uniform(-32, 32),
           cz + rng.uniform(-32, 32), rot=rng.uniform(0, 360),
           scale=rng.uniform(0.8, 1.5))
    # scattered ruin-fill between the complexes
    scatter("razed_city", P + "bones-scattered.glb", 10, 0.8, 1.2)
    scatter("razed_city", P + "dead-bush.glb", 10, 0.8, 1.3)
    scatter("razed_city", P + "debris.glb", 12, 0.8, 1.4)

    # ---- drywood: dead forest ----
    scatter("drywood", P + "tree-dead-bare.glb", 26, 1.4, 2.4)
    scatter("drywood", P + "tree-dead-large.glb", 10, 1.2, 2.0)
    scatter("drywood", P + "pine-dead-crooked.glb", 10, 1.0, 1.8)
    scatter("drywood", P + "ash-thicket.glb", 12, 0.9, 1.5)
    scatter("drywood", P + "bone-pile.glb", 6, 0.8, 1.2)
    scatter("drywood", P + "ribcage.glb", 4, 0.9, 1.3)

    # ---- dead swamp: reeds, dead trunks, mist, bones ----
    scatter("dead_swamp", P + "reed-cluster.glb", 42, 1.0, 2.2)
    scatter("dead_swamp", P + "dead-trunk-long.glb", 12, 0.9, 1.4)
    scatter("dead_swamp", P + "dead-trunk.glb", 8, 0.9, 1.3)
    scatter("dead_swamp", P + "mist-bank.glb", 8, 1.5, 2.5)
    scatter("dead_swamp", P + "bones-scattered.glb", 10, 0.8, 1.3)
    scatter("dead_swamp", P + "root-tangle.glb", 8, 0.9, 1.4)

    out.append("# --- placements (%d) ---" % len(placed))
    for model, x, y, z, rot, scale, zone in placed:
        out.append("place %s %.1f %.1f %.1f %.0f %.2f %s" %
                   (model, x, y, z, rot, scale, zone))
    out.append("")

    # ---- feral beast spawns ----
    spawns = []
    def spawn(species, zone, count):
        zx = next(z for z in ZONES if z[0] == zone)
        n, guard = 0, 0
        while n < count and guard < count * 40:
            guard += 1
            x = rng.uniform(zx[1], zx[4])
            z = rng.uniform(zx[3], zx[6])
            if zone_at(x, z) != zone:
                continue
            spawns.append((species, x, height(x, z), z, 1, zone))
            n += 1
    spawn("skittering_ghoul", "dead_swamp", 8)
    spawn("charnel_imp", "dead_swamp", 6)
    spawn("pale_wight", "dead_swamp", 5)
    spawn("risen_dead", "dead_swamp", 5)
    spawn("pale_wight", "deep_cavern", 4)
    spawn("ossified_brute", "deep_cavern", 2)
    out.append("# --- feral beast spawns (%d entries, %d beasts) ---" %
               (len(spawns), sum(s[4] for s in spawns)))
    for species, x, y, z, count, zone in spawns:
        out.append("spawn %s %.1f %.1f %.1f %d %s" %
                   (species, x, y, z, count, zone))
    out.append("")

    # ---- lod rules ----
    out.append("# --- lod rules ---")
    models = sorted({m for m, *_ in placed})
    for m in models:
        out.append("lod %s hide=400" % m)
    # terrain is one big mesh: keep it visible far, it IS the ground
    out.append("lod assets/world/terrain/dark_wilds_ground.obj hide=1500")

    with open(MAP_PATH, "w") as f:
        f.write("\n".join(out) + "\n")
    print("map written: %d placements, %d spawns, %d distinct models" %
          (len(placed), len(spawns), len(models)))


def main():
    os.makedirs(TERRAIN_DIR, exist_ok=True)
    os.makedirs(TEXTURE_DIR, exist_ok=True)
    os.makedirs(os.path.dirname(MAP_PATH), exist_ok=True)
    write_heightmap()
    write_detail_texture()
    write_terrain_obj()
    write_map()


if __name__ == "__main__":
    main()
