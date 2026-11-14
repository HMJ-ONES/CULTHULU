"""Blender background shape checks, comparison renders and editable low-poly .blend copies."""
from pathlib import Path
import json
import hashlib
import math
import bpy
import bmesh
assert bpy.app.background, "Run in a separate background Blender process; preserve the artist session"
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
FOLDER = ROOT / "assets/instant_meshes/remeshed"
REPORT = FOLDER / "remesh_report.json"
BLEND_SUFFIX = "_remeshed.blend"
COMPARISON_SUFFIX = "_comparison.png"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def imported_mesh():
    meshes = [o for o in bpy.context.selected_objects if o.type == "MESH"]
    assert len(meshes) == 1
    return meshes[0]


def mesh_vertices(obj):
    a = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


def stats(values, diagonal):
    a = np.array(values)
    return {"mean": float(a.mean()), "p95": float(np.percentile(a, 95)), "max": float(a.max()),
            "mean_percent_diagonal": float(a.mean() / diagonal * 100),
            "p95_percent_diagonal": float(np.percentile(a, 95) / diagonal * 100),
            "max_percent_diagonal": float(a.max() / diagonal * 100)}


def check(source, low):
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    high_bvh = BVHTree.FromObject(source, graph)
    low_bvh = BVHTree.FromObject(low, graph)
    original = mesh_vertices(source)
    reduced = mesh_vertices(low)
    assert np.isfinite(original).all() and np.isfinite(reduced).all(), "Non-finite mesh coordinates"
    diagonal = float(np.linalg.norm(original.max(axis=0) - original.min(axis=0)))
    nearest = [high_bvh.find_nearest(Vector(v))[3] for v in reduced]
    assert all(d is not None for d in nearest), "New mesh cannot be compared with the source surface"
    # Area-weighted samples detect missing silhouette/appendage surfaces, not
    # just whether the new vertices happen to lie somewhere on the sculpture.
    source.data.calc_loop_triangles()
    tris = np.empty(len(source.data.loop_triangles) * 3, dtype=np.int32)
    source.data.loop_triangles.foreach_get("vertices", tris)
    tris = tris.reshape(-1, 3)
    areas = np.linalg.norm(np.cross(original[tris[:, 1]] - original[tris[:, 0]], original[tris[:, 2]] - original[tris[:, 0]]), axis=1)
    rng = np.random.default_rng(20261031)
    selected = rng.choice(len(tris), size=15000, p=areas.astype(np.float64) / areas.sum(dtype=np.float64))
    bary = rng.random((15000, 2))
    bary[bary.sum(axis=1) > 1] = 1 - bary[bary.sum(axis=1) > 1]
    points = original[tris[selected, 0]] + bary[:, :1] * (original[tris[selected, 1]] - original[tris[selected, 0]]) + bary[:, 1:] * (original[tris[selected, 2]] - original[tris[selected, 0]])
    reverse = [low_bvh.find_nearest(Vector(v))[3] for v in points]
    assert all(d is not None for d in reverse), "Source surface cannot be compared with the new mesh"
    bm = bmesh.new()
    bm.from_mesh(low.data)
    topology = {"boundary_edges": sum(e.is_boundary for e in bm.edges),
                "nonmanifold_interior_edges": sum(not e.is_manifold and not e.is_boundary for e in bm.edges),
                "loose_vertices": sum(not v.link_faces for v in bm.verts),
                "zero_area_faces": sum(f.calc_area() <= diagonal * diagonal * 1e-18 for f in bm.faces)}
    bm.free()
    result = {"new_vertices_to_original": stats(nearest, diagonal),
              "original_surface_to_new": stats(reverse, diagonal), "surface_samples": 15000,
              "topology": topology}
    result["shape_check_passed"] = result["original_surface_to_new"]["p95_percent_diagonal"] < 1 and result["original_surface_to_new"]["max_percent_diagonal"] < 5 and topology["loose_vertices"] == 0 and topology["zero_area_faces"] == 0
    return result


def camera_at(camera, center, size):
    camera.location = Vector(center) + Vector((0, size * 2.8, size * .15))
    camera.rotation_euler = (Vector(center) - camera.location).to_track_quat("-Z", "Y").to_euler()


def prepare_scene(source, low, record):
    for polygon in low.data.polygons:
        polygon.use_smooth = True
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1400, 900, 100
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "OBJECT"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "BOTH"
    scene.display.shading.background_type = "WORLD"
    if scene.world is None:
        scene.world = bpy.data.worlds.new("Review World")
    scene.world.color = (.035, .045, .055)
    low.color = (.20, .56, .48, 1)
    source.color = (.65, .68, .72, 1)
    bounds = np.array(record["source_bounds"])
    center = (bounds[:3] + bounds[3:]) / 2
    extent = bounds[3:] - bounds[:3]
    gap = max(extent[0] * 1.6, extent[2] * .6)
    source.location = -Vector(center) + Vector((-gap / 2, 0, 0))
    low.location = -Vector(center) + Vector((gap / 2, 0, 0))
    data = bpy.data.cameras.new("Review Camera")
    camera = bpy.data.objects.new("Review Camera", data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    data.type = "ORTHO"
    data.ortho_scale = max(extent[2] * 1.4 * (1400 / 900), (gap + extent[0]) * 1.15)
    camera_at(camera, (0, 0, 0), max(extent))
    labels = []
    for x, text, color in [(-gap / 2, f"ORIGINAL / {record['source_triangles']:,} triangles", source.color),
                           (gap / 2, f"REMESH / {record['quads']:,} quads", low.color)]:
        curve = bpy.data.curves.new("Comparison Label", "FONT")
        curve.body, curve.align_x, curve.size = text, "CENTER", extent[2] * .034
        label = bpy.data.objects.new("Comparison Label", curve)
        scene.collection.objects.link(label)
        label.location = (x, extent[1] * .7, -extent[2] * .60)
        label.rotation_euler = camera.rotation_euler
        label.color = color
        labels.append(label)
    image = FOLDER / (record['model'] + COMPARISON_SUFFIX)
    assert not image.exists()
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(image)
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(source, do_unlink=True)
    for label in labels:
        bpy.data.objects.remove(label, do_unlink=True)
    low.location = (0, 0, 0)
    low.name = record["model"] + "_Retopology"
    camera_at(camera, center, float(np.linalg.norm(extent)))
    data.ortho_scale = max(extent[2] * 1.3 * (1400 / 900), extent[0] * 1.3)
    bpy.ops.object.select_all(action="DESELECT")
    low.select_set(True)
    bpy.context.view_layer.objects.active = low
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == "VIEW_3D":
                area.spaces.active.region_3d.view_location = center
                area.spaces.active.region_3d.view_rotation = camera.rotation_euler.to_quaternion()
                area.spaces.active.region_3d.view_distance = float(np.linalg.norm(extent)) * 1.4
                area.spaces.active.clip_end = max(1000, float(np.linalg.norm(extent)) * 100)
    # Remove the high-poly datablock from this new working file to keep it small.
    bpy.data.orphans_purge(do_recursive=True)
    blend = FOLDER / (record['model'] + BLEND_SUFFIX)
    assert not blend.exists()
    bpy.ops.wm.save_as_mainfile(filepath=str(blend), check_existing=False)
    return {"blend": blend.relative_to(ROOT).as_posix(), "blend_bytes": blend.stat().st_size,
            "comparison": image.relative_to(ROOT).as_posix(), "blend_sha256": sha(blend)}


if __name__ == "__main__":
    report = json.loads(REPORT.read_text())
    for record in report["models"]:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        source_path = ROOT / "assets/instant_meshes/characters" / f"{record['model']}.ply"
        assert sha(source_path) == record["input_sha256"]
        bpy.ops.wm.ply_import(filepath=str(source_path), forward_axis="Y", up_axis="Z")
        source = imported_mesh()
        bpy.ops.wm.obj_import(filepath=str(ROOT / record["output"]), forward_axis="Y", up_axis="Z")
        low = imported_mesh()
        record["quality"] = check(source, low)
        record.update(prepare_scene(source, low, record))
        assert sha(source_path) == record["input_sha256"]
        REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print("REMESH_QA " + json.dumps({"model": record["model"], "quality": record["quality"]}), flush=True)
    assert all(m["quality"]["shape_check_passed"] for m in report["models"]), "Review shape deviation before delivery"
