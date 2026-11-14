"""Blender background conversion of available character/creature sources to compact PLY.

Run with: blender --background --factory-startup --threads 2 --python <this script>
Never loads or saves the artist's open Blender session. Originals are read only.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import traceback

import bpy
import bmesh
assert bpy.app.background, "Run in a separate background Blender process; preserve the artist session"
import numpy as np
from mathutils import Vector

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
DEST = ROOT / "assets/instant_meshes"


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def verify(path, triangles, bounds):
    """Check the actual binary geometry, indices and world-space bounds."""
    with path.open("rb") as stream:
        lines = []
        while True:
            line = stream.readline()
            if not line or len(lines) > 100:
                raise ValueError("Invalid PLY header")
            lines.append(line.decode("ascii").strip())
            if lines[-1] == "end_header":
                break
        assert lines[0] == "ply" and "format binary_little_endian 1.0" in lines
        nv = int(next(s.split()[-1] for s in lines if s.startswith("element vertex ")))
        nf = int(next(s.split()[-1] for s in lines if s.startswith("element face ")))
        assert nv > 0 and nf == triangles, (nv, nf, triangles)
        properties = [s for s in lines if s.startswith("property ")]
        assert properties[:3] == ["property float x", "property float y", "property float z"], properties
        assert len(properties) == 4 and properties[3] in ("property list uchar uint vertex_indices", "property list uchar int vertex_indices"), properties
        vertices = np.fromfile(stream, dtype="<f4", count=nv * 3).reshape(nv, 3)
        faces = np.fromfile(stream, dtype=np.dtype([("n", "u1"), ("v", "<u4", (3,))]), count=nf)
        assert len(faces) == nf and stream.read(1) == b""
        assert np.isfinite(vertices).all() and np.all(faces["n"] == 3)
        assert int(faces["v"].max()) < nv
        actual = [*vertices.min(axis=0).tolist(), *vertices.max(axis=0).tolist()]
        tolerance = max(1e-5, max(abs(v) for v in bounds) * 1e-5)
        assert np.allclose(actual, bounds, rtol=0, atol=tolerance), (actual, bounds)
    return {"vertices": nv, "triangles": nf, "bounds": actual, "bytes": path.stat().st_size, "sha256": digest(path)}


def jobs():
    config = json.loads((PROJECT / "Config/ApprovedCharacterModels.json").read_text(encoding="utf-8-sig"))
    result, unavailable = [], []
    for name, entry in config["characters"].items():
        source = PROJECT / entry.get("source_file", "__no_source__")
        if source.is_file():
            result.append(("characters", name, source, entry.get("excluded_mesh_names", [])))
        else:
            unavailable.append(name)
    for directory, group in [(ROOT / "assets/creatures", "creatures"), (ROOT / "assets/characters", "people")]:
        for source in sorted(directory.rglob("*.glb")):
            name = source.parent.name if source.stem == "model" else source.stem
            result.append((group, name, source, []))
    return result, unavailable


def convert(group, name, source, excluded):
    before = digest(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if source.suffix.lower() == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(source), use_anim=False)
    elif source.suffix.lower() in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=str(source), import_pack_images=False)
    else:
        raise ValueError(f"Unsupported source format: {source}")
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            obj.data.pose_position = "REST"
    bpy.context.view_layer.update()
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.visible_get() and o.name not in excluded and o.data.name not in excluded and len(o.data.polygons)]
    assert meshes, name
    # Explicit triangles give the verifier and exporter the same topology,
    # including FBX polygons whose collinear corners triangulate differently.
    triangulated = set()
    for obj in meshes:
        if obj.data.as_pointer() in triangulated:
            continue
        triangulated.add(obj.data.as_pointer())
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=list(bm.faces), quad_method="FIXED", ngon_method="EAR_CLIP")
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    triangles = 0
    low, high = Vector((math.inf,) * 3), Vector((-math.inf,) * 3)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in meshes:
        obj.hide_set(False)
        obj.select_set(True)
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            mesh.calc_loop_triangles()
            triangles += len(mesh.loop_triangles)
            positions = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
            mesh.vertices.foreach_get("co", positions)
            positions = positions.reshape(-1, 3)
            matrix = np.array(evaluated.matrix_world, dtype=np.float64)
            positions = positions @ matrix[:3, :3].T + matrix[:3, 3]
            assert np.isfinite(positions).all()
            minimum, maximum = positions.min(axis=0), positions.max(axis=0)
            low = Vector(tuple(min(low[i], minimum[i]) for i in range(3)))
            high = Vector(tuple(max(high[i], maximum[i]) for i in range(3)))
        finally:
            evaluated.to_mesh_clear()
    bpy.context.view_layer.objects.active = meshes[0]
    bounds = [*low, *high]
    folder = DEST / group
    folder.mkdir(parents=True, exist_ok=True)
    output = folder / (name + ".ply")
    # Never overwrite an export the artist may have edited.
    counter = 2
    while output.exists():
        output = folder / f"{name}_v{counter}.ply"
        counter += 1
    kwargs = dict(filepath=str(output), check_existing=False, export_selected_objects=True,
                  apply_modifiers=True, ascii_format=False, forward_axis="Y", up_axis="Z",
                  export_uv=False, export_normals=False, export_colors="NONE",
                  export_attributes=False, export_triangulated_mesh=False)
    supported = bpy.ops.wm.ply_export.get_rna_type().properties.keys()
    bpy.ops.wm.ply_export(**{k: v for k, v in kwargs.items() if k in supported})
    proof = verify(output, triangles, bounds)
    assert digest(source) == before, "Original source changed"
    proof.update(group=group, model=name, source=source.relative_to(ROOT).as_posix(),
                 output=output.relative_to(ROOT).as_posix(), source_sha256=before,
                 mesh_parts=len(meshes), excluded_mesh_names=excluded, verified=True)
    print("[InstantMeshes] " + json.dumps(proof), flush=True)
    return proof


if __name__ == "__main__":
    DEST.mkdir(parents=True, exist_ok=True)
    items, unavailable = jobs()
    old_manifest = DEST / "manifest.json"
    old = json.loads(old_manifest.read_text(encoding="utf-8")) if old_manifest.exists() else {}
    previous = {m["model"]: m for m in old.get("models", [])}
    report = {"format": "binary_little_endian PLY", "blender": bpy.app.version_string,
              "originals_modified": False, "models": [], "unavailable_source_models": unavailable, "errors": []}
    for group, name, source, excluded in items:
        try:
            prior = previous.get(name)
            if prior and digest(source) == prior["source_sha256"] and (ROOT / prior["output"]).is_file() and digest(ROOT / prior["output"]) == prior["sha256"]:
                verify(ROOT / prior["output"], prior["triangles"], prior["bounds"])
                report["models"].append(prior)
            else:
                report["models"].append(convert(group, name, source, excluded))
        except Exception:
            report["errors"].append({"model": name, "error": traceback.format_exc()})
            traceback.print_exc()
        (DEST / "manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    assert not report["errors"], report["errors"]
    print("[InstantMeshes] COMPLETE " + json.dumps({"models": len(report["models"]), "bytes": sum(m["bytes"] for m in report["models"])}), flush=True)
