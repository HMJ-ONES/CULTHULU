"""Read-only anatomy/rig audit and native Blender reference renders.

Run in a separate background Blender process with --disable-autoexec. The
artist's open sessions and all source meshes, actions and files stay untouched.
"""
from pathlib import Path
import hashlib
import json
import math
import bpy
import numpy as np
from mathutils import Vector

assert bpy.app.background
PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
DEST = PROJECT / 'Saved/CharacterAnimationAudit'
DEST.mkdir(parents=True, exist_ok=True)
CONFIG = json.loads((PROJECT / 'Config/ApprovedCharacterModels.json').read_text(encoding='utf-8-sig'))
STATIC = ['dagon', 'cthulhu_avatar', 'nyarlathotep', 'yog_sothoth']
JOBS = [(key, ROOT / f'assets/retopology/{key}_retopology.obj') for key in STATIC]
JOBS += [(key, PROJECT / CONFIG['characters'][key]['source_file']) for key in ['shoggoths', 'mi_go', 'yig']]
report = {'characters': [], 'excluded': ['nodens'], 'ability_animation_work': False,
          'additional_original_model': {'id': 'sghllor', 'kind': 'native light-and-mist orb; preserve its nonhuman form'}}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


for key, source in JOBS:
    before = digest(source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if source.suffix == '.obj':
        bpy.ops.wm.obj_import(filepath=str(source), forward_axis='Y', up_axis='Z')
    elif source.suffix == '.fbx':
        bpy.ops.import_scene.fbx(filepath=str(source), use_anim=True)
    else:
        bpy.ops.import_scene.gltf(filepath=str(source), import_pack_images=False)
    scene = bpy.context.scene
    scene.frame_set(1)
    bpy.context.view_layer.update()
    meshes = [o for o in scene.objects if o.type == 'MESH' and not o.hide_render]
    points = []
    for obj in meshes:
        evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = evaluated.to_mesh()
        a = np.array([tuple(evaluated.matrix_world @ v.co) for v in mesh.vertices])
        points.append(a)
        evaluated.to_mesh_clear()
        obj.color = (.30, .48, .43, 1)
    points = np.concatenate(points)
    minimum, maximum = points.min(axis=0), points.max(axis=0)
    height = float(maximum[2] - minimum[2])
    assert height > .001 and np.isfinite(points).all()
    origin = np.array([(minimum[0] + maximum[0]) / 2, (minimum[1] + maximum[1]) / 2, minimum[2]])
    norm = lambda p: ((np.array(p) - origin) * 2 / height).tolist()
    rigs = []
    for arm in [o for o in scene.objects if o.type == 'ARMATURE']:
        bones = []
        for bone in arm.data.bones:
            pose = arm.pose.bones[bone.name]
            bones.append({'name': bone.name, 'parent': bone.parent.name if bone.parent else None,
                          'head': norm(arm.matrix_world @ pose.head), 'tail': norm(arm.matrix_world @ pose.tail),
                          'deform': bone.use_deform})
        rigs.append({'name': arm.name, 'bones': bones})
    slices = []
    normalized = (points - origin) * 2 / height
    for z in np.arange(.1, 2.01, .1):
        a = normalized[np.abs(normalized[:, 2] - z) < .025]
        if len(a):
            slices.append({'z': round(float(z), 2), 'count': len(a),
                           'min': a.min(axis=0).tolist(), 'max': a.max(axis=0).tolist()})
    row = {'id': key, 'source': source.relative_to(ROOT).as_posix(), 'sha256': before,
           'world_bounds': [*minimum.tolist(), *maximum.tolist()], 'normalized_origin': origin.tolist(),
           'normalization_scale': 2 / height, 'vertices': len(points),
           'mesh_parts': [{'name': o.name, 'vertices': len(o.data.vertices), 'uv_layers': len(o.data.uv_layers),
                           'armature_modifiers': [m.object.name for m in o.modifiers if m.type == 'ARMATURE' and m.object]} for o in meshes],
           'rigs': rigs, 'normalized_slices': slices, 'actions': [a.name for a in bpy.data.actions], 'views': []}
    center = Vector((minimum + maximum) * .5)
    camera_data = bpy.data.cameras.new('Anatomy Reference')
    camera = bpy.data.objects.new('Anatomy Reference', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = height * 1.25
    camera_data.clip_end = max(10000, height * 20)
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'OBJECT'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.background_type = 'WORLD'
    if not scene.world:
        scene.world = bpy.data.worlds.new('Anatomy Reference World')
    scene.world.color = (.035, .045, .055)
    for label, direction in [('front', (0, 3, 0)), ('side', (3, 0, 0)), ('back', (0, -3, 0))]:
        camera.location = center + Vector(direction) * height
        camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(DEST / f'{key}_{label}.png')
        bpy.ops.render.render(write_still=True)
        row['views'].append(str(Path(scene.render.filepath).relative_to(PROJECT)))
    assert digest(source) == before
    report['characters'].append(row)
    (DEST / 'audit.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('ANATOMY_AUDIT ' + json.dumps({'id': key, 'vertices': len(points), 'rigs': len(rigs), 'actions': row['actions']}), flush=True)
print('All requested source audits finished without modifying source files.', flush=True)
