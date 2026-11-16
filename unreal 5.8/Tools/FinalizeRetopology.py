"""Make reviewed smooth-shaded Blender working copies and comparison captures."""
from pathlib import Path
import json
import runpy
import bpy
assert bpy.app.background, "Use a separate background Blender process"
PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
FOLDER = ROOT / "assets/retopology"
report_file = FOLDER / "report.json"
report = json.loads(report_file.read_text())
helper = runpy.run_path(str(PROJECT / "Tools/VerifyStaticRemesh.py"))
context = helper["prepare_scene"].__globals__
context.update(FOLDER=FOLDER, BLEND_SUFFIX="_retopology.blend", COMPARISON_SUFFIX="_review.png")
for model in report["models"]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source_path = ROOT / "assets/instant_meshes/characters" / (model["model"] + ".ply")
    assert helper["sha"](source_path) == model["input_sha256"]
    assert helper["sha"](ROOT / model["output"]) == model["sha256"]
    bpy.ops.wm.ply_import(filepath=str(source_path), forward_axis="Y", up_axis="Z")
    source = helper["imported_mesh"]()
    bpy.ops.wm.obj_import(filepath=str(ROOT / model["output"]), forward_axis="Y", up_axis="Z")
    low = helper["imported_mesh"]()
    quality = helper["check"](source, low)
    assert quality["shape_check_passed"] and not any(quality["topology"].values()), quality
    model["quality"] = quality
    model.update(helper["prepare_scene"](source, low, model))
    report_file.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("FINAL_ARTIST_COPY " + model["model"], flush=True)
