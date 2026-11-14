"""Read-only inspection of exact approved FBX rigs and supplied motion keys."""
from pathlib import Path
import hashlib
import json
import sys
import bpy
import numpy as np
from mathutils import Vector

assert bpy.app.background
PROJECT=Path(__file__).resolve().parents[1]
ROOT=PROJECT.parent
key=sys.argv[sys.argv.index('--')+1];assert key in ['mi_go','yig']
config=json.loads((PROJECT/'Config/ApprovedCharacterModels.json').read_text())['characters'][key]
source=PROJECT/config['source_file'];before=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source),use_anim=True)
scene=bpy.context.scene
rigs=[o for o in scene.objects if o.type=='ARMATURE']
meshes=[o for o in scene.objects if o.type=='MESH']
for rig in rigs:rig.data.pose_position='REST'
scene.frame_set(1);bpy.context.view_layer.update()
surfaces=[]
for obj in meshes:
    evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    surfaces.append(np.array([tuple(obj.matrix_world@v.co) for v in evaluated.data.vertices]))
p=np.concatenate(surfaces);lo,hi=p.min(axis=0),p.max(axis=0)
origin=np.array([(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,lo[2]])
scale=2/(hi[2]-lo[2])
def normalized(v):return ((np.array(v)-origin)*scale).tolist()
rows=[]
for rig in rigs:
    bones=[{'name':b.name,'parent':b.parent.name if b.parent else None,
        'head':normalized(rig.matrix_world@b.head_local),'tail':normalized(rig.matrix_world@b.tail_local),
        'deform':b.use_deform} for b in rig.data.bones]
    rows.append({'name':rig.name,'worldMatrix':[list(row) for row in rig.matrix_world],
        'objectLocation':list(rig.location),'objectRotationMode':rig.rotation_mode,
        'constraints':[(b.name,[c.type for c in b.constraints]) for b in rig.pose.bones if b.constraints],
        'bones':bones})
actions=[]
for action in bpy.data.actions:
    curves=[]
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    curves.append({'path':curve.data_path,'index':curve.array_index,'keys':len(curve.keyframe_points),
                        'first':list(curve.keyframe_points[0].co) if curve.keyframe_points else None,
                        'last':list(curve.keyframe_points[-1].co) if curve.keyframe_points else None})
    actions.append({'name':action.name,'frames':list(action.frame_range),'curves':curves})
weights=[]
for obj in meshes:
    sums=[sum(g.weight for g in v.groups) for v in obj.data.vertices]
    weights.append({'name':obj.name,'vertices':len(obj.data.vertices),'maxInfluences':max(len(v.groups) for v in obj.data.vertices),
        'normalizationMaxError':max(abs(s-1) for s in sums),'uvLayers':len(obj.data.uv_layers),'materials':len(obj.data.materials)})
dest=PROJECT/'Saved/CharacterAnimationAudit'
record={'character':key,'sourceSha256':before,'fps':scene.render.fps,'fpsBase':scene.render.fps_base,
    'restBounds':[lo.tolist(),hi.tolist()],'origin':origin.tolist(),'scale':float(scale),
    'rigs':rows,'actions':actions,'meshes':weights}
(dest/(key+'_supplied_rig.json')).write_text(json.dumps(record,indent=2)+'\n')
scene.world=bpy.data.worlds.new('Supplied Rig Review');scene.world.color=(.035,.045,.055)
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='OBJECT'
scene.display.shading.background_type='WORLD';scene.display.shading.show_cavity=True
scene.render.resolution_x=scene.render.resolution_y=720;scene.render.resolution_percentage=100
camera_data=bpy.data.cameras.new('Rig Review');camera_data.type='ORTHO';camera_data.ortho_scale=float(hi[2]-lo[2])*1.25
camera_data.clip_end=10000
camera=bpy.data.objects.new('Rig Review',camera_data);scene.collection.objects.link(camera);scene.camera=camera
center=Vector((lo+hi)*.5)
for label,direction in [('front',(0,-3,0)),('side',(3,0,0))]:
    camera.location=center+Vector(direction)*float(hi[2]-lo[2]);camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(dest/(key+'_rest_'+label+'.png'));bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
print('SUPPLIED_RIG',json.dumps({'character':key,'fps':record['fps'],'meshes':weights,'rigs':[(r['name'],len(r['bones']),len(r['constraints'])) for r in rows],
    'actions':[(r['name'],r['frames'],len(r['curves'])) for r in actions]}),flush=True)
