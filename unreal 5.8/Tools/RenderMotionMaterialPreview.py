"""Native textured pose preview; read-only Blender study, original materials.

Usage: -- <repo-relative blend> <motion role> <phase>. No source resave.
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
source=(ROOT/args[0]).resolve();assert source.is_relative_to(ROOT)
before=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(source),use_scripts=False)
scene=bpy.context.scene
rig=next(o for o in scene.objects if o.type=='ARMATURE')
mesh=next(o for o in scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers))
assert mesh.data.uv_layers and mesh.data.materials,'Preview requires retained UVs and source materials'
role,phase=args[1],float(args[2]);assert 0<=phase<=1
action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')==role)
rig.animation_data.action=action
if action.slots:rig.animation_data.action_slot=action.slots[0]
frame=float(action.frame_range[0]+phase*(action.frame_range[1]-action.frame_range[0]))
scene.frame_set(int(frame),subframe=frame%1)
dest=source.parent/'material_review';dest.mkdir(exist_ok=True)
output=dest/(role+'_'+str(round(phase*1000))+'.png');assert not output.exists()
images=[]
for image in bpy.data.images:
    if image.packed_file:
        data=bytes(image.packed_file.data)
        images.append({'name':image.name,'packedBytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
assert images,'Source texture images should be packed in the working copy'
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16
scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=2
scene.render.resolution_x,scene.render.resolution_y,scene.render.resolution_percentage=640,800,100
scene.render.image_settings.file_format='PNG'
scene.world.use_nodes=True
background=scene.world.node_tree.nodes.get('Background')
background.inputs['Color'].default_value=(.045,.055,.075,1)
background.inputs['Strength'].default_value=.35
for name,position,power,color,size in [
    ('Soft key',(-3,4,5),550,(1.,.90,.78),4.),
    ('Cool fill',(3,2,3),280,(.65,.80,1.),3.),
    ('Rim',(0,-3,4),700,(.55,.75,1.),3.)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.color=color;data.shape='DISK';data.size=size
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj)
    obj.location=position;obj.rotation_euler=(Vector((0,0,1))-obj.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,0))
floor=bpy.context.object
material=bpy.data.materials.new('Preview floor');material.use_nodes=True
shader=material.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=(.025,.035,.045,1)
shader.inputs['Roughness'].default_value=.8;floor.data.materials.append(material)
rig.hide_render=True
vertices=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.vertices
p=np.empty(len(vertices)*3,dtype=np.float32);vertices.foreach_get('co',p);p=p.reshape(-1,3)
camera=scene.camera;inverse=camera.matrix_world.inverted()
projected=np.einsum('ij,nj->ni',np.array(inverse.to_3x3()),p,optimize=False)+np.array(inverse.translation)
extent=np.abs(projected[:,:2]).max(axis=0)
camera.data.ortho_scale=float(max(2*extent[1],2*extent[0]/.8)*1.15)
scene.render.filepath=str(output);bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
(dest/(role+'_'+str(round(phase*1000))+'.json')).write_text(json.dumps({
    'blendSha256':before,'role':role,'phase':phase,'renderer':'Cycles CPU 16 samples; original source materials',
    'packedImages':images,'preview':output.relative_to(ROOT).as_posix()},indent=2)+'\n')
print('ORIGINAL_MATERIAL_PREVIEW_COMPLETE',str(output),flush=True)
