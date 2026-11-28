"""Measure a canonical anatomical frame for static sculptures before rigging."""
from pathlib import Path
import json
import math
import hashlib
import bpy
import numpy as np
from mathutils import Vector, Matrix
from mathutils.kdtree import KDTree
assert bpy.app.background
PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
DEST = PROJECT / 'Saved/CharacterAnimationAudit'
report = {}
for name in ['dagon', 'cthulhu_avatar', 'nyarlathotep']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = ROOT / f'assets/retopology/{name}_retopology.obj'
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis='Y', up_axis='Z')
    obj = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
    p = np.array([tuple(obj.matrix_world @ v.co) for v in obj.data.vertices])
    center = p.mean(axis=0)
    points = p - center
    tree = KDTree(len(points))
    for i, v in enumerate(points):
        tree.insert(v, i)
    tree.balance()
    sample = points[np.random.default_rng(19).choice(len(points), 900, replace=False)]
    def score(theta):
        n = np.array([math.cos(theta), -math.sin(theta), 0])
        reflected = sample - 2 * (sample @ n)[:, None] * n
        distances = np.array([tree.find(v)[2] for v in reflected])
        return float(np.mean(np.minimum(distances, np.quantile(distances, .9)) ** 2))
    coarse = min(np.arange(-90, 90, 3), key=lambda a: score(math.radians(a)))
    yaw = float(min(np.arange(coarse - 3, coarse + 3.01, .25), key=lambda a: score(math.radians(a))))
    rotation = Matrix.Rotation(math.radians(yaw), 4, 'Z')
    q = np.array([tuple(rotation @ Vector(v)) for v in p])
    low, high = q.min(axis=0), q.max(axis=0)
    origin = np.array([(low[0] + high[0]) / 2, (low[1] + high[1]) / 2, low[2]])
    scale = 2 / (high[2] - low[2])
    normalized = (q - origin) * scale
    for vertex, xyz in zip(obj.data.vertices, normalized):
        vertex.co = xyz
    obj.matrix_world = Matrix.Identity(4)
    for face in obj.data.polygons:
        face.use_smooth = True
    obj.color = (.35, .5, .46, 1)
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new('Anatomy World')
    scene.world.color = (.035, .045, .055)
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.display.shading.color_type = 'OBJECT'
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.background_type = 'WORLD'
    camera_data = bpy.data.cameras.new('Anatomy Camera')
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = 2.4
    camera = bpy.data.objects.new('Anatomy Camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    for label, location in [('front', (0, 5, 1)), ('side', (5, 0, 1))]:
        camera.location = location
        camera.rotation_euler = (Vector((0, 0, 1)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(DEST / f'{name}_canonical_{label}.png')
        bpy.ops.render.render(write_still=True)
    slices = []
    for z in np.arange(.1, 2, .1):
        a = normalized[np.abs(normalized[:, 2] - z) < .02]
        if len(a):
            slices.append({'z': round(float(z), 2), 'x': [float(a[:, 0].min()), float(a[:, 0].max())], 'y': [float(a[:, 1].min()), float(a[:, 1].max())]})
    report[name] = {'sourceSha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'yawDegrees': yaw,
                    'originAfterRotation': origin.tolist(), 'scale': float(scale), 'slices': slices,
                    'frame': '+Y front, +X anatomical left, +Z up; height 2m; soles at Z=0'}
    print('ANATOMICAL_FRAME', name, yaw, flush=True)
(DEST / 'anatomical_frames.json').write_text(json.dumps(report, indent=2) + '\n')
