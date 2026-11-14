"""Read-only native multi-angle pose captures for anatomical motion review.

Usage after Blender --: <repo-relative blend> <comma-separated roles> <phase>
Writes new images beside the study, never resaves the source or overwrites art.
"""
from pathlib import Path
import hashlib
import json
import sys
import bpy
import numpy as np
from mathutils import Vector

assert bpy.app.background
ROOT=Path(__file__).resolve().parents[2]
args=sys.argv[sys.argv.index('--')+1:]
source=(ROOT/args[0]).resolve()
assert source.is_relative_to(ROOT)
before=hashlib.sha256(source.read_bytes()).hexdigest()
phase=float(args[2]);assert 0<=phase<=1
bpy.ops.wm.open_mainfile(filepath=str(source),use_scripts=False)
scene=bpy.context.scene
rig=next(o for o in scene.objects if o.type=='ARMATURE')
meshes=[o for o in scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers)]
dest=source.parent/'angles';dest.mkdir(exist_ok=True)
scene.render.resolution_x,scene.render.resolution_y,scene.render.resolution_percentage=480,600,100
scene.render.image_settings.file_format='PNG'
rig.hide_render=True
record={}
for role in args[1].split(','):
    action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')==role)
    rig.animation_data.action=action
    if action.slots:rig.animation_data.action_slot=action.slots[0]
    frame=float(action.frame_range[0]+phase*(action.frame_range[1]-action.frame_range[0]))
    scene.frame_set(int(frame),subframe=frame%1)
    graph=bpy.context.evaluated_depsgraph_get()
    surfaces=[]
    for obj in meshes:
        vertices=obj.evaluated_get(graph).data.vertices
        p=np.empty(len(vertices)*3,dtype=np.float32);vertices.foreach_get('co',p)
        surfaces.append(np.einsum('ij,nj->ni',np.array(obj.matrix_world.to_3x3()),p.reshape(-1,3),optimize=False)+np.array(obj.matrix_world.translation))
    points=np.concatenate(surfaces)
    target=Vector(((points.min(axis=0)+points.max(axis=0))*.5))
    row=[]
    for label,offset in [('front',(0,6,1.0)),('side',(6,0,1.0)),('back',(0,-6,1.0))]:
        camera=scene.camera
        camera.location=target+Vector(offset)
        camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
        bpy.context.view_layer.update()
        inverse=camera.matrix_world.inverted()
        projected=np.einsum('ij,nj->ni',np.array(inverse.to_3x3()),points,optimize=False)+np.array(inverse.translation)
        extent=np.abs(projected[:,:2]).max(axis=0)
        camera.data.ortho_scale=float(max(2*extent[1],2*extent[0]/.8)*1.12)
        output=dest/f'{role}_{round(phase*1000):04d}_{label}.png'
        assert not output.exists(),'Preserve prior review captures'
        scene.render.filepath=str(output);bpy.ops.render.render(write_still=True)
        row.append(output.relative_to(ROOT).as_posix())
    record[role]={'frame':frame,'phase':phase,'captures':row}
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
metadata=dest/f'captures_{round(phase*1000):04d}.json'
assert not metadata.exists()
metadata.write_text(json.dumps({'blendSha256':before,'poses':record},indent=2)+'\n')
print('MULTI_ANGLE_REVIEW_COMPLETE',str(dest),flush=True)
