"""Normalize object names and source clip range; preserve creator's original FBX rig."""
from pathlib import Path
import bpy,json,re,hashlib
from mathutils import Matrix
project=Path(__file__).resolve().parents[1]
source=project/'Saved/CharacterSources/yig/original_fbx/source/Draconic Cinematic3 Test.fbx'
output=source.parents[1]/'prepared_v3'; output.mkdir(exist_ok=True)
target=output/'Yig.fbx'; assert not target.exists(),'Existing revision preserved'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source))
for objects in [bpy.data.objects,bpy.data.meshes,bpy.data.armatures,bpy.data.materials]:
 for obj in objects: obj.name=re.sub(r'[^A-Za-z0-9_]','_',obj.name)
scene=bpy.context.scene; scene.frame_start=401; scene.frame_end=641; scene.frame_set(401)
# The four swords use object animation, which a skeletal-mesh import otherwise
# drops. Preserve those exact transforms as rigid weights on four new bones.
weapons=[o for o in scene.objects if o.type=='MESH' and o.name.startswith('Sword_scimitar')]
tracks={o.name:[] for o in weapons}
for frame in range(401,642):
 scene.frame_set(frame); bpy.context.view_layer.update()
 for obj in weapons: tracks[obj.name].append(obj.matrix_world.copy())
scene.frame_set(401); bpy.context.view_layer.update()
data=bpy.data.armatures.new('OriginalWeaponRig'); rig=bpy.data.objects.new('OriginalWeaponRig',data); scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig; rig.select_set(True); bpy.ops.object.mode_set(mode='EDIT')
root=data.edit_bones.new('WeaponsRoot'); root.head=(0,0,0); root.tail=(0,0,1)
for obj in weapons:
 bone=data.edit_bones.new(obj.name); bone.head=(0,0,0); bone.tail=(0,0,1); bone.parent=root
bpy.ops.object.mode_set(mode='OBJECT')
for obj in weapons:
 graph=bpy.context.evaluated_depsgraph_get(); evaluated=obj.evaluated_get(graph)
 mesh=bpy.data.meshes.new_from_object(evaluated,depsgraph=graph); mesh.transform(obj.matrix_world)
 copy=bpy.data.objects.new(obj.name+'_Skinned',mesh); scene.collection.objects.link(copy)
 group=copy.vertex_groups.new(name=obj.name); group.add(list(range(len(mesh.vertices))),1.0,'REPLACE')
 modifier=copy.modifiers.new('OriginalWeaponRig','ARMATURE'); modifier.object=rig
 obj.hide_render=True
for index,frame in enumerate(range(401,642)):
 scene.frame_set(frame)
 for obj in weapons:
  bone=rig.pose.bones[obj.name]; bone.rotation_mode='QUATERNION'
  bone.matrix_basis=tracks[obj.name][index]@tracks[obj.name][0].inverted()
  bone.keyframe_insert(data_path='location',frame=frame); bone.keyframe_insert(data_path='rotation_quaternion',frame=frame); bone.keyframe_insert(data_path='scale',frame=frame)
scene.frame_set(401); bpy.context.view_layer.update()
for image in bpy.data.images:
 filename=Path(image.filepath.replace('\\','/')).name
 normalize=lambda s:re.sub(r'[^a-z0-9]','',s.lower())
 candidates=[p for p in (source.parents[1]/'textures').iterdir() if normalize(p.name)==normalize(filename)]
 if 'scimitar_Normal' in filename: candidates=[source.parents[1]/'textures/scimitar_normal_base.png']
 if 'Body_Normal' in filename: candidates=[source.parents[1]/'textures/Body_normal_base.png']
 candidate=candidates[0] if len(candidates)==1 else source.parents[1]/'textures'/filename
 if candidate.exists(): image.filepath=str(candidate); image.reload()
bpy.ops.object.select_all(action='DESELECT')
for obj in scene.objects:
 if obj.type in ['ARMATURE','MESH'] and not obj.hide_render: obj.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=True,bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_simplify_factor=0.0,path_mode='COPY',embed_textures=True,axis_forward='-Y',axis_up='Z')
record=dict(original_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),prepared_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),frames=[401,641],changes=['Filesystem-safe object names','Original supplied animation range','Embedded original textures'])
(output/'conversion.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
