"""Bake the approved original rig's supplied motion; downloaded scripts never run."""
from pathlib import Path
import bpy,json,hashlib
from mathutils import Vector
project=Path(__file__).resolve().parents[1]
source=project/'Saved/CharacterSources/mi_go/original_blend/source/Bugoid2.blend'
output=source.parents[1]/'prepared_v2'; output.mkdir(exist_ok=True); target=output/'MiGo.fbx'
assert not target.exists(),'Existing revision preserved'
bpy.ops.wm.open_mainfile(filepath=str(source),load_ui=False,use_scripts=False)
arm=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
for track in arm.animation_data.nla_tracks: track.mute=True
action=bpy.data.actions['PoseLib']; arm.animation_data.action=action
if action.slots: arm.animation_data.action_slot=action.slots[0]
action.name='Prowl'
for image in bpy.data.images:
 candidate=source.parents[1]/'textures'/Path(image.filepath.replace('\\','/')).name
 if candidate.exists(): image.filepath=str(candidate); image.reload()
scene=bpy.context.scene; scene.frame_start=1; scene.frame_end=35; scene.frame_set(1)
feet=[arm.pose.bones[n] for n in ['Foot.l','Foot.r','toebase.l','toebase.r']]
initial_location=arm.location.copy(); samples=[]
for frame in range(1,36):
 scene.frame_set(frame); arm.location=initial_location; bpy.context.view_layer.update()
 points=[arm.matrix_world@bone.head for bone in feet]
 anchor=sum(points,Vector())/len(points); anchor.z=min(p.z for p in points)
 samples.append(anchor)
for frame,anchor in enumerate(samples,1):
 scene.frame_set(frame); arm.location=initial_location+arm.matrix_parent_inverse.to_3x3()@(samples[0]-anchor)
 arm.keyframe_insert(data_path='location',frame=frame)
scene.frame_set(1); bpy.context.view_layer.update()
meshes=[o for o in scene.objects if o.type=='MESH' and not o.hide_render]
bpy.ops.object.select_all(action='DESELECT')
for obj in meshes+[arm]: obj.select_set(True)
bpy.context.view_layer.objects.active=arm
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_simplify_factor=0.0,path_mode='COPY',embed_textures=True,axis_forward='-Y',axis_up='Z')
record=dict(original_blend_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),fbx_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),source_action='PoseLib',export_name='Prowl',frames=[1,35],changes=['Baked supplied rig constraints','Removed global clip translation using the original foot anchors; original local poses preserved'])
(output/'conversion.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8'); print('[PreparedMiGo]',json.dumps(record))
