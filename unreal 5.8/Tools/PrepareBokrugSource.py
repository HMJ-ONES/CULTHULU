"""Repair the Frost Predator export's centimeter-space morph positions.

The licensed GLB and its animation/texture buffers stay intact. Only the
unit error in each POSITION delta is corrected for the Unreal import.
"""
import hashlib
import json
from pathlib import Path
import struct

PROJECT = Path(__file__).resolve().parents[1]
config_path = PROJECT / "Config/ApprovedCharacterModels.json"
config = json.loads(config_path.read_text(encoding="utf-8-sig"))
selection = config["characters"]["bokrug"]
source = PROJECT / "Saved/CharacterSources/bokrug/source.glb"
original = source.read_bytes()
assert hashlib.sha256(original).hexdigest() == selection["original_source_sha256"]
magic, version, size, json_size, chunk = struct.unpack_from("<4sIIII", original)
assert (magic, version, size, chunk) == (b"glTF", 2, len(original), 0x4e4f534a)
model = json.loads(original[20:20 + json_size])
binary_size, chunk = struct.unpack_from("<II", original, 20 + json_size)
buffer = bytearray(original[28 + json_size:])
assert len(buffer) == binary_size and chunk == 0x004e4942


def positions(index):
    accessor = model["accessors"][index]
    view = model["bufferViews"][accessor["bufferView"]]
    assert view["buffer"] == 0 and accessor["type"] == "VEC3" and accessor["componentType"] == 5126
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", 12)
    return accessor, range(start, start + stride * accessor["count"], stride)


corrected = 0
base_bounds = []
for mesh in model["meshes"]:
    for primitive in mesh["primitives"]:
        base_accessor, base_offsets = positions(primitive["attributes"]["POSITION"])
        base_bounds.append((base_accessor["min"], base_accessor["max"]))
        base = list(struct.unpack_from("<3f", buffer, offset) for offset in base_offsets)
        for target in primitive.get("targets", []):
            accessor, target_offsets = positions(target["POSITION"])
            assert len(target_offsets) == len(base)
            deltas = [struct.unpack_from("<3f", buffer, offset) for offset in target_offsets]
            fixed = [tuple((delta - 99 * v) / 100 for v, delta in zip(vertex, change))
                     for vertex, change in zip(base, deltas)]
            assert max(map(abs, (value for delta in fixed for value in delta))) < .5, "Unexpected morph displacement"
            for offset, delta in zip(target_offsets, fixed):
                struct.pack_into("<3f", buffer, offset, *delta)
            accessor["min"] = [min(axis) for axis in zip(*fixed)]
            accessor["max"] = [max(axis) for axis in zip(*fixed)]
            corrected += 1

assert corrected == 12
header = json.dumps(model, separators=(",", ":")).encode()
header += b" " * (-len(header) % 4)
prepared = struct.pack("<4sIIII", magic, version, 28 + len(header) + len(buffer), len(header), 0x4e4f534a)
prepared += header + struct.pack("<II", len(buffer), 0x004e4942) + buffer
destination = source.parent / "corrected_morph_units" / "source.glb"
destination.parent.mkdir(parents=True, exist_ok=True)
if destination.exists():
    assert destination.read_bytes() == prepared, "Keep existing prepared source"
else:
    destination.write_bytes(prepared)
digest = hashlib.sha256(prepared).hexdigest()
if selection.get("prepared_source_sha256") != digest:
    selection.pop("reviewed_gameplay_binding", None)
selection.update(source_file=destination.relative_to(PROJECT).as_posix(), prepared_source_sha256=digest)
# Interchange's CPU skin preview can put the vertically rotated importer root
# into bounds calculations. The authored mesh is Y-up and faces glTF +Z.
minimum = [min(box[0][axis] for box in base_bounds) * 100 for axis in range(3)]
maximum = [max(box[1][axis] for box in base_bounds) * 100 for axis in range(3)]
selection.update(rotation=[0, -90, 0], reference_bounds_rotation=[0, -90, 0],
                 reference_bounds_sha256=digest, reference_bounds_profile="default",
                 reference_bounds=[minimum[2], -maximum[0], minimum[1], maximum[2], -minimum[0], maximum[1]])
config_path.write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("Corrected", corrected, "morph positions for Bokrug.", selection["prepared_source_sha256"])
