"""Export a reviewed anatomical study in a new staging folder, source read-only.

Retopology receives projected source UVs and original material assignments.
Original UV/material art is reused; clay review shading is never a game material.
Run separately for each character to bound peak memory.
"""
from pathlib import Path
import bpy,sys,json,hashlib,math
import numpy as np
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform

assert bpy.app.background
PROJECT=Path(__file__).resolve().parents[1];ROOT=PROJECT.parent
key=sys.argv[sys.argv.index('--')+1]
selected=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters'][key]
assert selected['kind']=='anatomical';title,revision=selected['title'],selected['revision']
source=ROOT/f'assets/character_animation/{key}/{revision}/{title}_BasicMotion.blend'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before=sha(source)
verification=json.loads((source.parent/'review/verification.json').read_text());assert verification['sourceSha256']==before
destination=PROJECT/f'Saved/MotionIntegration/{key}_{revision}';destination.mkdir(parents=True,exist_ok=True)
assert not (destination/'exports.json').exists(),'Preserve previous staged export'
bpy.ops.wm.open_mainfile(filepath=str(source),use_scripts=False,load_ui=False)
scene=bpy.context.scene
rig=next(o for o in scene.objects if o.type=='ARMATURE')
mesh=next(o for o in scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' for m in o.modifiers))
geometry_before=np.array([tuple(v.co) for v in mesh.data.vertices])
rig.data.pose_position='REST'
material_report={'method':'original UVs and material slots already retained'}
if not mesh.data.uv_layers:
    frame=json.loads((PROJECT/'Config/CharacterAnatomicalFrames.json').read_text())[key]
    config=json.loads((PROJECT/'Config/ApprovedCharacterModels.json').read_text())['characters'][key]
    original=PROJECT/config['source_file'];original_sha=sha(original)
    existing=set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(original),import_pack_images=True)
    imported=[o for o in bpy.data.objects if o not in existing]
    parts=[o for o in imported if o.type=='MESH']
    transform=Matrix.Scale(frame['scale'],4)@Matrix.Translation(-Vector(frame['originAfterRotation']))@Matrix.Rotation(math.radians(frame['yawDegrees']),4,'Z')
    for part in parts:
        part.data.transform(transform@part.matrix_world);part.parent=None;part.matrix_world=Matrix.Identity(4)
    bpy.ops.object.select_all(action='DESELECT')
    for part in parts:part.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    if len(parts)>1:bpy.ops.object.join()
    original_mesh=bpy.context.view_layer.objects.active
    data=original_mesh.data;data.calc_loop_triangles();triangles=list(data.loop_triangles)
    positions=[v.co.copy() for v in data.vertices]
    indices=[tuple(t.vertices) for t in triangles]
    tree=BVHTree.FromPolygons(positions,indices,all_triangles=True)
    mesh.data.materials.clear()
    for material in data.materials:mesh.data.materials.append(material)
    layer=mesh.data.uv_layers.new(name='SourceSurfaceProjection')
    original_uv=data.uv_layers.active
    distances=[]
    for face in mesh.data.polygons:
        center=sum((mesh.data.vertices[i].co for i in face.vertices),Vector())/len(face.vertices)
        hit,normal,index,distance=tree.find_nearest(center)
        assert index is not None
        face.material_index=triangles[index].material_index
        for loop_index in face.loop_indices:
            point=mesh.data.vertices[mesh.data.loops[loop_index].vertex_index].co
            hit,normal,index,distance=tree.find_nearest(point);assert index is not None
            distances.append(distance)
            if original_uv:
                triangle=triangles[index]
                values=[Vector((*original_uv.data[i].uv,0)) for i in triangle.loops]
                uv=barycentric_transform(hit,*[positions[i] for i in triangle.vertices],*values)
                layer.data[loop_index].uv=uv.xy
    material_report={'method':'nearest original surface, per-corner barycentric UV projection',
                     'sourceSha256':original_sha,'originalUV':bool(original_uv),
                     'projectionDistanceP99M':float(np.quantile(distances,.99)),
                     'projectionDistanceMaxM':max(distances),
                     'note':'UV seam faces require visual review; geometry and source textures are unchanged'}
    assert sha(original)==original_sha
    # Remove only transient imported objects, never the artist study on disk.
    for obj in imported:
        try:
            if obj.name in bpy.data.objects:bpy.data.objects.remove(obj,do_unlink=True)
        except ReferenceError:pass
assert np.array_equal(geometry_before,np.array([tuple(v.co) for v in mesh.data.vertices]))
for modifier in mesh.modifiers:
    if modifier.type=='ARMATURE':modifier.use_deform_preserve_volume=False
actions={a.get('CultUlhu.MotionRole'):a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')}
roles=['idle','walk','run','melee_1','melee_2','melee_3','jump','dash','taunt','land']
roles+=sorted(r for r in actions if r.startswith('melee_back_'))
assert all(r in actions for r in roles)
for track in rig.animation_data.nla_tracks:track.mute=True
rig.animation_data.action=None
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=rig
target=destination/'body.fbx';assert not target.exists()
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'ARMATURE','MESH'},
    add_leaf_bones=False,use_armature_deform_only=False,bake_anim=False,axis_forward='-Y',axis_up='Z')
report={'character':key,'revision':revision,'study':source.relative_to(ROOT).as_posix(),'studySha256':before,
        'mesh':{'file':target.relative_to(PROJECT).as_posix(),'sha256':sha(target)},'materials':material_report,
        'materialNames':[m.name for m in mesh.data.materials],
        'authorHeightM':2,'authorForward':'+Y','runtimeSkinning':'linear blend',
        'clips':{}}
rig.data.pose_position='POSE';mesh.select_set(False)
source_rate=scene.render.fps/scene.render.fps_base;scene.render.fps=60;scene.render.fps_base=1
for role in roles:
    action=actions[role];track=rig.animation_data.nla_tracks.new();strip=track.strips.new(role,1,action)
    if action.slots:strip.action_slot=action.slots[0]
    strip.scale=60/source_rate;strip.extrapolation='NOTHING';strip.repeat=1
    scene.frame_start=1;scene.frame_end=round(float(strip.frame_end));scene.frame_set(1)
    target=destination/(role+'.fbx');assert not target.exists()
    bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'ARMATURE'},
        add_leaf_bones=False,use_armature_deform_only=False,bake_anim=True,
        bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,bake_anim_step=1,
        bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
    report['clips'][role]={'file':target.relative_to(PROJECT).as_posix(),'sha256':sha(target),
                          'seconds':(scene.frame_end-scene.frame_start)/60,
                          'rootTravelNeedsIntegration':key=='cthulhu_avatar' and role.startswith('melee_')}
    rig.animation_data.nla_tracks.remove(track)
assert sha(source)==before
(destination/'exports.json').write_text(json.dumps(report,indent=2)+'\n')
print('ANATOMICAL_MOTION_EXPORTED',key,str(destination),flush=True)
