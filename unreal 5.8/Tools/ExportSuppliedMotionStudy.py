"""Read-only Blender export of reviewed clips on the supplied Mi-Go skeleton.

One source-space animation-only FBX per newly authored role. The existing UE
Prowl asset is reused for dash; it is never rebaked or replaced by this importer.
"""
from pathlib import Path
import bpy,sys,json,hashlib,math
from mathutils import Matrix

assert bpy.app.background
PROJECT=Path(__file__).resolve().parents[1]; ROOT=PROJECT.parent
selected=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters']['mi_go']
revision=selected['revision']
source=ROOT/f'assets/character_animation/mi_go/{revision}/MiGo_BasicMotion.blend'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before=sha(source)
proof=json.loads((source.parent/'source_preservation.json').read_text())
assert proof['blendSha256']==before and proof['allSuppliedKeysAndHandlesUnchanged']
dest=PROJECT/f'Saved/MotionIntegration/mi_go_{revision}';dest.mkdir(parents=True,exist_ok=True)
assert not (dest/'exports.json').exists(),'Preserve already exported revision'
bpy.ops.wm.open_mainfile(filepath=str(source),load_ui=False,use_scripts=False)
scene=bpy.context.scene
rig=next(o for o in scene.objects if o.type=='ARMATURE')
actions={a.get('CultUlhu.MotionRole'):a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')}
roles=['idle','walk','run','melee_1','melee_2','melee_3','jump','land','taunt']
assert all(r in actions for r in roles)
frame=bpy.data.objects['MiGo_AuthorFrame']
for obj in list(frame.children):
    # Parent removal intentionally restores source space, without keep-world.
    basis=obj.matrix_basis.copy();obj.parent=None;obj.matrix_parent_inverse=Matrix.Identity(4);obj.matrix_basis=basis
frame.hide_render=True
source_rate=scene.render.fps/scene.render.fps_base
scene.render.fps=60;scene.render.fps_base=1
for track in rig.animation_data.nla_tracks:track.mute=True
rig.animation_data.action=None
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
report={'character':'mi_go','study':source.relative_to(ROOT).as_posix(),'studySha256':before,
        'sourceRigSha256':proof['rigSignatures'],'fps':60,'dash':'reuse supplied Prowl UE asset; never replace',
        'space':'original supplied rig, author-frame wrapper removed; rest geometry/weights unchanged','clips':{}}
for role in roles:
    action=actions[role]
    track=rig.animation_data.nla_tracks.new();strip=track.strips.new(role,1,action)
    if action.slots:strip.action_slot=action.slots[0]
    strip.extrapolation='NOTHING';strip.scale=60/source_rate;strip.repeat=1
    scene.frame_start=1;scene.frame_end=round(float(strip.frame_end))
    scene.frame_set(1);bpy.context.view_layer.update()
    target=dest/(role+'.fbx');assert not target.exists()
    bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,object_types={'ARMATURE'},
        add_leaf_bones=False,use_armature_deform_only=False,bake_anim=True,
        bake_anim_use_nla_strips=False,bake_anim_use_all_actions=False,
        bake_anim_step=1,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z')
    report['clips'][role]={'file':target.relative_to(PROJECT).as_posix(),'sha256':sha(target),
                          'seconds':(scene.frame_end-scene.frame_start)/60,'frames':scene.frame_end-scene.frame_start+1}
    rig.animation_data.nla_tracks.remove(track)
assert sha(source)==before,'Never overwrite an artist study'
(dest/'exports.json').write_text(json.dumps(report,indent=2)+'\n')
print('SUPPLIED_MOTION_EXPORTED',str(dest),flush=True)
