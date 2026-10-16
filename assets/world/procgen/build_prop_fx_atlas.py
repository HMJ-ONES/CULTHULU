#!/usr/bin/env python3
"""CULT-ULHU wave 37: texture atlas pass (optimization).

Packs the five small single-model prop textures into one 512x512 RGBA
atlas and remaps the UVs of the five models that use them, so the world
renderer binds one texture instead of five and the decoded payload drops
~5 texture files to 1.

    bark-dead.png  ( 64x64) -> dead-bush.glb
    ember-glow.png (128x128) -> ember-cluster.glb
    wood-dark.png  (128x128) -> beams-collapsed.glb
    mist-soft.png  (256x256 RGBA) -> mist-bank.glb
    scorch-dark.png(256x256 RGBA) -> scorched-patch.glb

Layout (512x512, rects may touch; a half-texel UV inset keeps samples
strictly inside each rect so bilinear filtering cannot bleed):

    mist-soft    at (0,   0)  256x256
    ember-glow   at (256, 0)  128x128
    wood-dark    at (0, 256)  128x128
    bark-dead    at (128,256)  64x64
    scorch-dark  at (256,256) 256x256

GLB UV convention: v=0 is the TOP row of the image, so the remap is
    u' = (x0 + 0.5 + u*(w-1)) / 512
    v' = (y0 + 0.5 + v*(h-1)) / 512

The script rewrites the five .glb.b64 sidecars (image uri + TEXCOORD_0
values in the BIN chunk), writes prop-fx-atlas.png + its .b64 sidecar,
and removes the five obsolete texture .b64 files. Only .b64 files are
tracked in git; decoded working copies are git-ignored.
"""

import base64
import json
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("PIL (pillow) required: pip install pillow")

HERE = os.path.dirname(os.path.abspath(__file__))          # assets/world/procgen
ROOT = os.path.dirname(os.path.dirname(HERE))               # assets/
TEX = os.path.join(ROOT, "world", "textures")
PROPS = os.path.join(ROOT, "world", "props")
ATLAS_SIZE = 512
ATLAS_NAME = "prop-fx-atlas.png"
ATLAS_URI = "../textures/" + ATLAS_NAME

# src texture -> (model glb, atlas origin x/y)
ENTRIES = {
    "bark-dead.png":  ("props/dead-bush.glb",       (128, 256)),
    "ember-glow.png": ("props/ember-cluster.glb",   (256, 0)),
    "wood-dark.png":  ("props/beams-collapsed.glb", (0,   256)),
    "mist-soft.png":  ("props/mist-bank.glb",       (0,   0)),
    "scorch-dark.png": ("terrain/scorched-patch.glb", (256, 256)),
}


def decode_b64(path):
    with open(path + ".b64") as f:
        return base64.b64decode(f.read())


def encode_b64(path, data):
    with open(path + ".b64", "w") as f:
        f.write(base64.b64encode(data).decode("ascii"))


def parse_glb(raw):
    assert raw[:4] == b"glTF", "not a GLB"
    total, = struct.unpack("<I", raw[8:12])
    assert total == len(raw)
    pos = 12
    chunks = []
    while pos < len(raw):
        clen, ctype = struct.unpack("<II", raw[pos:pos + 8])
        chunks.append((ctype, raw[pos + 8:pos + 8 + clen]))
        pos += 8 + clen
    assert chunks[0][0] == 0x4E4F534A, "first chunk must be JSON"
    j = json.loads(chunks[0][1])
    bin_data = bytearray(chunks[1][1]) if len(chunks) > 1 else bytearray()
    return j, bin_data


def pack_glb(j, bin_data):
    jb = json.dumps(j, separators=(",", ":")).encode("utf-8")
    jb += b" " * ((4 - len(jb) % 4) % 4)
    while len(bin_data) % 4:
        bin_data.append(0)
    total = 12 + 8 + len(jb) + (8 + len(bin_data) if bin_data else 0)
    out = bytearray()
    out += b"glTF" + struct.pack("<II", 2, total)
    out += struct.pack("<II", len(jb), 0x4E4F534A) + jb
    if bin_data:
        out += struct.pack("<II", len(bin_data), 0x004E4942) + bytes(bin_data)
    return bytes(out)


def main():
    # 1. Compose the atlas from the decoded source PNGs.
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE), (0, 0, 0, 0))
    rects = {}
    for tex, (model, (x0, y0)) in ENTRIES.items():
        src = os.path.join(TEX, tex)
        assert os.path.exists(src), f"missing decoded texture {src} (run assets/decode_assets.py)"
        img = Image.open(src).convert("RGBA")
        w, h = img.size
        assert x0 + w <= ATLAS_SIZE and y0 + h <= ATLAS_SIZE, "layout overflow"
        # Overlap check against already-placed rects (touching is fine:
        # the half-texel inset keeps samples inside each rect).
        for other, (ox, oy, ow, oh) in rects.items():
            if not (x0 + w <= ox or ox + ow <= x0 or
                    y0 + h <= oy or oy + oh <= y0):
                raise AssertionError(f"layout collision: {tex} vs {other}")
        atlas.paste(img, (x0, y0))
        rects[tex] = (x0, y0, w, h)
        print(f"placed {tex} ({w}x{h}) at ({x0},{y0})")
    atlas_path = os.path.join(TEX, ATLAS_NAME)
    atlas.save(atlas_path, optimize=True)
    kb = os.path.getsize(atlas_path) // 1024
    print(f"wrote {ATLAS_NAME}: {ATLAS_SIZE}x{ATLAS_SIZE} RGBA, {kb} KB")
    assert kb <= 1024, "atlas over budget"
    encode_b64(atlas_path, open(atlas_path, "rb").read())
    print(f"wrote {ATLAS_NAME}.b64")

    # 2. Remap the five models.
    for tex, (model_rel, _) in ENTRIES.items():
        glb_path = os.path.join(ROOT, "world", model_rel)
        raw = decode_b64(glb_path)
        j, bindata = parse_glb(raw)

        # Each of these models references exactly one texture.
        images = j.get("images", [])
        assert len(images) == 1, f"{model_rel}: expected 1 image, got {len(images)}"
        old_uri = images[0].get("uri")
        assert old_uri == "../textures/" + tex, \
            f"{model_rel}: unexpected image uri {old_uri}"
        images[0]["uri"] = ATLAS_URI

        # Which TEXCOORD sets do the textured primitives use?
        texcoord_idxs = set()
        for mesh in j.get("meshes", []):
            for prim in mesh["primitives"]:
                for name in ("TEXCOORD_0", "TEXCOORD_1"):
                    if name in prim.get("attributes", {}):
                        texcoord_idxs.add(int(name.rsplit("_", 1)[1]))
        assert texcoord_idxs, f"{model_rel}: no texcoords found"

        x0, y0, w, h = rects[tex]
        n_uvs = 0
        for mesh in j.get("meshes", []):
            for prim in mesh["primitives"]:
                for ti in texcoord_idxs:
                    acc_idx = prim["attributes"][f"TEXCOORD_{ti}"]
                    acc = j["accessors"][acc_idx]
                    assert acc["type"] == "VEC2" and acc["componentType"] == 5126, \
                        f"{model_rel}: TEXCOORD_{ti} not float VEC2"
                    bv = j["bufferViews"][acc["bufferView"]]
                    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
                    count = acc["count"]
                    stride = bv.get("byteStride", 8)
                    assert stride == 8, f"{model_rel}: interleaved UVs unsupported"
                    for i in range(count):
                        p = off + i * 8
                        u, v = struct.unpack_from("<ff", bindata, p)
                        assert 0.0 <= u <= 1.0 and 0.0 <= v <= 1.0, \
                            f"{model_rel}: UV out of range ({u},{v})"
                        # Half-texel inset: samples land on texel centers,
                        # so adjacent rects cannot bleed into each other.
                        u2 = (x0 + 0.5 + u * (w - 1)) / ATLAS_SIZE
                        v2 = (y0 + 0.5 + v * (h - 1)) / ATLAS_SIZE
                        struct.pack_into("<ff", bindata, p, u2, v2)
                        n_uvs += 1
        new_raw = pack_glb(j, bindata)
        with open(glb_path, "wb") as f:
            f.write(new_raw)
        encode_b64(glb_path, new_raw)
        print(f"remapped {model_rel}: {n_uvs} UVs -> {ATLAS_URI}")

    # 3. Retire the five obsolete texture .b64 files.
    for tex in ENTRIES:
        p = os.path.join(TEX, tex)
        os.remove(p + ".b64")
        if os.path.exists(p):  # decoded working copy is untracked anyway
            os.remove(p)
        print(f"retired {tex}")

    print("wave 37 atlas pass complete")


if __name__ == "__main__":
    main()
