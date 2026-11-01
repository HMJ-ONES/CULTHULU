"""Reopen a motion study, sample real skin deformation and render review movies.

Run in separate background Blender. Never saves or overwrites the artist blend.
Usage: blender --background --python ReviewCharacterMotion.py -- <repo-relative blend>
Reports measure problems; they do not certify artistic quality or game integration.
"""
from pathlib import Path
import hashlib
import json
import sys
import bpy
import numpy as np

assert bpy.app.background, 'Preserve open artist sessions'
ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / sys.argv[sys.argv.index('--') + 1]).resolve()
assert source.is_relative_to(ROOT) and source.suffix == '.blend'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = sha(source)
dest = source.parent / 'review'
dest.mkdir(exist_ok=True)
assert not (dest / 'verification.json').exists(), 'Keep previous review evidence'
bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
scene = bpy.context.scene
rigs = [o for o in scene.objects if o.type == 'ARMATURE']
meshes = [o for o in scene.objects if o.type == 'MESH' and any(m.type == 'ARMATURE' for m in o.modifiers)]
assert rigs and meshes
actions = {a.get('CultUlhu.MotionRole'): a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')}
required = ['idle', 'walk', 'run', 'melee_1', 'melee_2', 'melee_3', 'jump', 'dash', 'taunt']
assert all(k in actions for k in required), list(actions)
rig = rigs[0]
assert scene.camera and scene.camera.data.type == 'ORTHO', 'Review expects an orthographic source camera'
camera_inverse = scene.camera.matrix_world.inverted()
camera_rotation = np.array(camera_inverse.to_3x3())
camera_offset = np.array(camera_inverse.translation)
camera_extents = np.zeros(2)
report = {'blend': source.relative_to(ROOT).as_posix(), 'sourceSha256': before,
          'requiredRoles': required, 'actions': {}, 'abilitiesAuthored': False,
          'status': 'independent sampling and native rendered review; artistic acceptance pending'}
for mesh in meshes:
    assert all(1 <= len(v.groups) <= 4 for v in mesh.data.vertices), 'Unweighted vertex or excess influences'
    assert max(abs(sum(g.weight for g in v.groups)-1) for v in mesh.data.vertices) < 1e-5
for role in required + (['land'] if 'land' in actions else []):
    action = actions[role]
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    start, end = action.frame_range
    lows = []
    largest_step = 0.
    first = previous = last = None
    for frame in np.arange(start, end + .25, .5):
        scene.frame_set(int(frame), subframe=float(frame % 1))
        graph = bpy.context.evaluated_depsgraph_get()
        surfaces = []
        for obj in meshes:
            vertices = obj.evaluated_get(graph).data.vertices
            local = np.empty(len(vertices)*3, dtype=np.float32)
            vertices.foreach_get('co', local)
            matrix = obj.matrix_world
            surfaces.append(np.einsum('ij,nj->ni', np.array(matrix.to_3x3()), local.reshape(-1,3),
                optimize=False) + np.array(matrix.translation))
        points = np.concatenate(surfaces)
        assert np.isfinite(points).all(), (role, frame)
        projected = np.einsum('ij,nj->ni', camera_rotation, points, optimize=False) + camera_offset
        camera_extents = np.maximum(camera_extents, np.abs(projected[:, :2]).max(axis=0))
        if first is None:
            first = points.copy()
        if previous is not None:
            largest_step = max(largest_step, float(np.linalg.norm(points-previous, axis=1).max()))
        previous = last = points
        lows.append(float(points[:, 2].min()))
    closure = float(np.linalg.norm(last-first, axis=1).max())
    loop = role in ['idle', 'walk', 'run']
    report['actions'][role] = {'sampleStepFrames': .5, 'samples': len(lows),
        'minimumSurfaceZ': min(lows), 'maximumVertexHalfFrameTravel': largest_step,
        'loop': loop, 'endpointMaxVertexDistance': closure,
        'loopClosurePass': closure < .001 if loop else None,
        'durationSeconds': float((end-start)/scene.render.fps)}
    if loop:
        assert closure < .001, (role, 'loop closure', closure)
    print('REOPENED_MOTION_CHECK', role, report['actions'][role], flush=True)

# Actual Blender animation rendering, with the source actions and skinning.
# In-place review exposes stance travel; the floor makes penetration visible.
rig.hide_render = True
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.012))
floor = bpy.context.object
floor.name = 'Review only ground; never saved to artist file'
floor.color = (.075, .085, .10, 1)
scene.render.resolution_x, scene.render.resolution_y = 480, 600
scene.render.resolution_percentage = 100
# Fit the whole sampled motion envelope, including outstretched claws. Preserve
# orientation and enlarge only the review camera; the source blend stays intact.
aspect = scene.render.resolution_x / scene.render.resolution_y
scene.camera.data.ortho_scale = max(scene.camera.data.ortho_scale,
    2*camera_extents[1]*1.08, 2*camera_extents[0]/aspect*1.08)
report['reviewCamera'] = {'orthographicScale': scene.camera.data.ortho_scale,
    'fit': 'all sampled skinned geometry across every required role'}
if hasattr(scene.render.image_settings, 'media_type'):
    scene.render.image_settings.media_type = 'VIDEO'
scene.render.image_settings.file_format = 'FFMPEG'
scene.render.ffmpeg.format = 'MPEG4'
scene.render.ffmpeg.codec = 'H264'
scene.render.ffmpeg.constant_rate_factor = 'MEDIUM'
scene.render.ffmpeg.ffmpeg_preset = 'GOOD'
for role in required + (['land'] if 'land' in actions else []):
    output = dest / (role + '.mp4')
    assert not output.exists(), 'Preserve existing review movies'
    action = actions[role]
    rig.animation_data.action = None
    track = rig.animation_data.nla_tracks.new()
    strip = track.strips.new(role, 1, action)
    if action.slots:
        strip.action_slot = action.slots[0]
    strip.extrapolation = 'NOTHING'
    strip.repeat = 2 if role in ['walk', 'run'] else 1
    scene.frame_start, scene.frame_end = 1, int(strip.frame_end)-1
    scene.render.filepath = str(output)
    bpy.ops.render.render(animation=True)
    rig.animation_data.nla_tracks.remove(track)
    assert output.exists() and output.stat().st_size > 1000
    report['actions'][role]['reviewMovie'] = output.relative_to(ROOT).as_posix()
assert sha(source) == before, 'Artist source was changed'
(dest / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
print('MOTION_REVIEW_COMPLETE', str(dest), flush=True)
