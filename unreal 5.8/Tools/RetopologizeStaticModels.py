"""Closed-surface Blender retopology for dense static approved source meshes.

Background-only. Original imports and open artist sessions remain unchanged.
Voxel surface repair precedes QuadriFlow; results must pass topology and shape QA.
"""
from pathlib import Path
import sys
import json
import time
import runpy
import math
import bpy
import bmesh
assert bpy.app.background, "Run in a separate background Blender process; preserve the artist session"
import numpy as np

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
FOLDER = ROOT / "assets/retopology"
FOLDER.mkdir(parents=True, exist_ok=True)
helper = runpy.run_path(str(PROJECT / "Tools/VerifyStaticRemesh.py"))
helper["prepare_scene"].__globals__["FOLDER"] = FOLDER
GOALS = {"dagon": 30000, "cthulhu_avatar": 40000, "nyarlathotep": 26000, "yog_sothoth": 40000}
manifest = json.loads((ROOT / "assets/instant_meshes/manifest.json").read_text())
report = {"method": "Voxel surface repair + QuadriFlow", "models": [], "errors": [],
          "untouched": ["mi_go", "yig"], "already_lightweight": {"shoggoths": 20302, "nodens": 6260}}
previous_report = FOLDER / "report.json"
if previous_report.exists():
    report["models"] = json.loads(previous_report.read_text()).get("models", [])

def build_low(source, original, name):
    if name in ("nyarlathotep", "yog_sothoth"):
        # These intersecting sculptures retain their surfaces better in the
        # verified Instant Meshes solution than in the voxel/QuadriFlow pass.
        native_report = json.loads((ROOT / "assets/instant_meshes/remeshed/remesh_report.json").read_text())
        native = next(m for m in native_report["models"] if m["model"] == name)
        path = ROOT / native["output"]
        assert helper["sha"](path) == native["sha256"] and native["input_sha256"] == original["sha256"]
        bpy.ops.wm.obj_import(filepath=str(path), forward_axis="Y", up_axis="Z")
        return helper["imported_mesh"]()
    cache = PROJECT / "Saved/RetopologyCache"
    cache.mkdir(exist_ok=True)
    checkpoint = cache / f"{name}_quadflow.ply"
    metadata = cache / f"{name}_quadflow.json"
    expected = {"source": original["sha256"], "faces": GOALS[name], "voxel_divisor": 850}
    if checkpoint.exists():
        assert metadata.exists() and json.loads(metadata.read_text()) == expected
        bpy.ops.wm.ply_import(filepath=str(checkpoint), forward_axis="Y", up_axis="Z")
        print(f"RETOPO {name}: using existing solver checkpoint", flush=True)
        return helper["imported_mesh"]()
    low = source.copy()
    low.data = source.data.copy()
    bpy.context.scene.collection.objects.link(low)
    bpy.ops.object.select_all(action="DESELECT")
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bounds = np.array(original["bounds"])
    diagonal = float(np.linalg.norm(bounds[3:] - bounds[:3]))
    bm = bmesh.new()
    bm.from_mesh(low.data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=diagonal * 1e-9)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(low.data)
    bm.free()
    low.data.remesh_voxel_size = diagonal / 850
    low.data.remesh_voxel_adaptivity = 0
    if hasattr(low.data, "use_remesh_preserve_volume"):
        low.data.use_remesh_preserve_volume = True
    print(f"RETOPO {name}: voxel surface repair", flush=True)
    bpy.ops.object.voxel_remesh()
    low.data.calc_loop_triangles()
    if len(low.data.loop_triangles) > 150000:
        decimate = low.modifiers.new("Temporary solver surface", "DECIMATE")
        decimate.ratio = 150000 / len(low.data.loop_triangles)
        bpy.ops.object.modifier_apply(modifier=decimate.name)
    print(f"RETOPO {name}: QuadriFlow target {GOALS[name]}, input faces {len(low.data.polygons)}", flush=True)
    kwargs = dict(use_mesh_symmetry=False, use_preserve_sharp=False, use_preserve_boundary=False,
                  preserve_attributes=False, smooth_normals=True, mode="FACES", target_faces=GOALS[name], seed=0)
    supported = bpy.ops.object.quadriflow_remesh.get_rna_type().properties.keys()
    result = bpy.ops.object.quadriflow_remesh(**{k: v for k, v in kwargs.items() if k in supported})
    assert "FINISHED" in result, result
    bpy.ops.object.select_all(action="DESELECT")
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    bpy.ops.wm.ply_export(filepath=str(checkpoint),check_existing=False,export_selected_objects=True,
                          export_uv=False,export_normals=False,export_colors="NONE",export_attributes=False,
                          export_triangulated_mesh=False,ascii_format=False,forward_axis="Y",up_axis="Z")
    metadata.write_text(json.dumps(expected))
    return low

only = None
if "--" in sys.argv:
    only = sys.argv[sys.argv.index("--") + 1]
    assert only in GOALS
for name in ([only] if only else GOALS):
    if any(m["model"] == name and m.get("quality", {}).get("shape_check_passed") for m in report["models"]):
        print(f"RETOPO {name}: retaining completed working files", flush=True)
        continue
    began = time.monotonic()
    try:
        output = FOLDER / f"{name}_retopology.obj"
        assert not output.exists(), "Never overwrite artist output"
        bpy.ops.wm.read_factory_settings(use_empty=True)
        original = next(m for m in manifest["models"] if m["model"] == name)
        source_file = ROOT / original["output"]
        assert helper["sha"](source_file) == original["sha256"]
        bpy.ops.wm.ply_import(filepath=str(source_file), forward_axis="Y", up_axis="Z")
        source = helper["imported_mesh"]()
        low = build_low(source, original, name)
        bounds = np.array(original["bounds"])
        diagonal = float(np.linalg.norm(bounds[3:] - bounds[:3]))
        # QuadriFlow can leave small singularity gaps even on a repaired closed
        # source. Close those solver gaps and triangulate only non-quad patches.
        bm = bmesh.new()
        bm.from_mesh(low.data)
        invalid = [v for v in bm.verts if not all(math.isfinite(c) for c in v.co)]
        if invalid:
            bmesh.ops.delete(bm, geom=invalid, context='VERTS')
        for repair_pass in range(3):
            bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=diagonal * 1e-6)
            bmesh.ops.dissolve_degenerate(bm, edges=list(bm.edges), dist=diagonal * 1e-7)
            zero = [f for f in bm.faces if f.calc_area() <= diagonal * diagonal * 1e-18]
            if zero:
                bmesh.ops.delete(bm, geom=zero, context='FACES_ONLY')
            boundary = [e for e in bm.edges if e.is_boundary]
            patches = bmesh.ops.holes_fill(bm, edges=boundary, sides=0).get("faces", []) if boundary else []
            large = [f for f in patches if len(f.verts) > 4]
            if large:
                bmesh.ops.triangulate(bm, faces=large)
        # Remove tiny open solver fragments (e.g. an isolated triangle), never
        # substantial disconnected body pieces or closed sculpt features.
        remaining = set(bm.faces)
        dust = []
        while remaining:
            seed = remaining.pop()
            component, todo = [seed], [seed]
            while todo:
                face = todo.pop()
                for edge in face.edges:
                    for other in edge.link_faces:
                        if other in remaining:
                            remaining.remove(other)
                            component.append(other)
                            todo.append(other)
            if len(component) <= 8 and any(e.is_boundary for f in component for e in f.edges) and sum(f.calc_area() for f in component) <= diagonal * diagonal * 1e-5:
                dust.extend(component)
        if dust:
            bmesh.ops.delete(bm, geom=dust, context='FACES_ONLY')
        # The generic hole filler occasionally skips a small simple loop next
        # to a concave quad. Explicitly cap those remaining solver gaps.
        edges_left = {e for e in bm.edges if e.is_boundary}
        while edges_left:
            edge = edges_left.pop()
            component, todo = {edge}, [edge]
            while todo:
                current = todo.pop()
                for vertex in current.verts:
                    for other in vertex.link_edges:
                        if other in edges_left:
                            edges_left.remove(other)
                            component.add(other)
                            todo.append(other)
            vertices = {v for e in component for v in e.verts}
            if len(component) > 64 or any(sum(e in component for e in v.link_edges) != 2 for v in vertices):
                continue
            first = edge.verts[0]
            loop, current, previous = [first], edge.verts[1], edge
            while current != first:
                loop.append(current)
                following = next(e for e in current.link_edges if e in component and e != previous)
                current, previous = following.other_vert(current), following
            try:
                patch = bm.faces.new(loop)
                if len(loop) > 4:
                    bmesh.ops.triangulate(bm, faces=[patch])
            except ValueError:
                pass  # Existing face; never duplicate an existing surface.
        # Remove any orphan solver geometry left after degenerate-face cleanup.
        loose = [v for v in bm.verts if not v.link_faces]
        if loose:
            bmesh.ops.delete(bm, geom=loose, context='VERTS')
        open_edges = [e for e in bm.edges if e.is_boundary]
        if open_edges and len(open_edges) <= 12:
            bm.faces.index_update()
            print("RETOPO_BOUNDARY " + json.dumps({"model": name, "edges": [{"points": [list(v.co) for v in e.verts], "faces": [f.index for f in e.link_faces], "areas": [f.calc_area() for f in e.link_faces]} for e in open_edges]}), flush=True)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(low.data)
        bm.free()
        low.data.update()
        quality = helper["check"](source, low)
        record = {"model": name, "source_triangles": original["triangles"], "source_bounds": original["bounds"],
                  "input_sha256": original["sha256"], "vertices": len(low.data.vertices),
                  "faces": len(low.data.polygons), "quads": sum(len(f.vertices) == 4 for f in low.data.polygons),
                  "triangles_after_triangulation": sum(len(f.vertices) - 2 for f in low.data.polygons),
                  "quality": quality, "voxel_size": diagonal / 850}
        record["method"] = "Instant Meshes with seam welding and validated topology repair" if name in ("nyarlathotep", "yog_sothoth") else "Voxel repair, QuadriFlow and validated topology repair"
        print("RETOPO_QA " + json.dumps(record), flush=True)
        assert quality["shape_check_passed"], "Surface shape check failed"
        assert quality["topology"]["boundary_edges"] == 0 and quality["topology"]["nonmanifold_interior_edges"] == 0, "Expected closed manifold result"
        # Small closure patches may use triangles; preserve closed topology
        # while retaining at least 95% quads over the actual working surface.
        assert record["quads"] / record["faces"] >= .95 and record["triangles_after_triangulation"] < original["triangles"]
        bpy.ops.object.select_all(action="DESELECT")
        low.select_set(True)
        bpy.context.view_layer.objects.active = low
        bpy.ops.wm.obj_export(filepath=str(output), check_existing=False, export_selected_objects=True,
                              apply_modifiers=True, export_materials=False, export_uv=False, export_normals=False,
                              forward_axis="Y", up_axis="Z")
        record["output"] = output.relative_to(ROOT).as_posix()
        record["sha256"] = helper["sha"](output)
        record.update(helper["prepare_scene"](source, low, record))
        record["seconds"] = round(time.monotonic() - began, 2)
        assert helper["sha"](source_file) == original["sha256"]
        report["models"].append(record)
    except Exception as error:
        import traceback
        traceback.print_exc()
        report["errors"].append({"model": name, "error": repr(error)})
    (FOLDER / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
assert not report["errors"], report["errors"]
