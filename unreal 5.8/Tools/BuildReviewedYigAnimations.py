"""Create new clips on Yig's original UE skeletons; never modify source assets."""
from pathlib import Path
import unreal,json,runpy,hashlib,shutil
PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
lib=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
registry=json.loads((PROJECT/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
binding=registry['characters']['yig']
revision=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters']['yig']['revision']
folder=PROJECT/f'Saved/MotionIntegration/yig_{revision}';folder.mkdir(parents=True,exist_ok=True)
OWNER='CultUlhu.ReviewedMotion.20261106'
recipe=runpy.run_path(str(PROJECT/'Tools/CultYigReviewedMotion.py'))
recipe_sha=hashlib.sha256((PROJECT/'Tools/CultYigReviewedMotion.py').read_bytes()).hexdigest()
rows=[r for r in runpy.run_path(str(PROJECT/'Tools/InspectApprovedRigs.py'))['collect']() if r['id']=='yig']
contexts={r['clip']:recipe['context'](r) for r in rows}
armor=next(contexts[r['clip']] for r in rows if 'Armor_' in r['clip'])
mapping=json.loads((PROJECT/'Config/ApprovedRigMotion.json').read_text())['weapon_hands']
ROLES={'idle':3.8,'walk':1.35,'run':.75,'melee_1':.50,'melee_2':.56,'melee_3':.68,'jump':1.,'dash':.28,'taunt':2.6,'land':.3}
def disk(path):return PROJECT/'Content'/(path.split('.',1)[0].removeprefix('/Game/')+'.uasset')
paths=set(binding['meshes']+[r['clip'] for r in rows])
preserved={p:hashlib.sha256(disk(p).read_bytes()).hexdigest() for p in paths}
sets=[];checks=[]
for row in rows:
    original=unreal.load_asset(row['clip']);skeleton=original.get_editor_property('skeleton')
    roles={'idle':row['clip'],'preview':row['clip']}
    previous=next((s['roles'] for s in binding.get('animation_sets',[]) if s['skeleton']==skeleton.get_path_name()),{})
    if 'cast' in previous:roles['cast']=previous['cast']
    label=original.get_name().replace('_Anim','')
    ctx=contexts[row['clip']]
    for role,seconds in ROLES.items():
        destination='/Game/CultUlhu/Characters/yig/ReviewedMotion_'+revision;name='A_'+label+'_'+role
        anim=unreal.load_asset(destination+'/'+name)
        if anim:assert lib.get_metadata_tag(anim,'CultOwner')==OWNER and lib.get_metadata_tag(anim,'RecipeSHA256')==recipe_sha
        else:
            factory=unreal.AnimSequenceFactory();factory.set_editor_property('target_skeleton',skeleton)
            anim=tools.create_asset(name,destination,unreal.AnimSequence,factory);assert anim
            # UE 5.8's compression timebase is independent of the data model.
            # Set both before writing odd-length 60 Hz tracks (e.g. 81 frames).
            target_rate=anim.get_editor_property('platform_target_frame_rate')
            target_rate.set_editor_property('default',unreal.FrameRate(60,1))
            assert anim.get_editor_property('platform_target_frame_rate').get_editor_property('default').numerator==60,'Compression must match the data model'
            controller=anim.get_editor_property('controller');controller.open_bracket('Reviewed Yig basic motion',False)
            controller.remove_all_bone_tracks(False);controller.set_frame_rate(unreal.FrameRate(60,1),False)
            frames=round(seconds*60);controller.set_number_of_frames(unreal.FrameNumber(frames),False)
            samples=[recipe['drive_weapons'](ctx,armor,role,i/frames,mapping) if 'Sword_' in row['clip'] else recipe['sample'](ctx,role,i/frames) for i in range(frames+1)]
            for bone in row['poses']:
                controller.add_bone_curve(bone,False)
                positions=[unreal.Vector(*p[bone.lower()]['location']) for p in samples]
                rotations=[unreal.Quat(*p[bone.lower()]['rotation']) for p in samples]
                scales=[unreal.Vector(*p[bone.lower()]['scale']) for p in samples]
                assert controller.set_bone_track_keys(bone,positions,rotations,scales,False)
            controller.close_bracket(False)
            lib.set_metadata_tag(anim,'CultOwner',OWNER);lib.set_metadata_tag(anim,'RecipeSHA256',recipe_sha)
            lib.set_metadata_tag(anim,'MotionRole',role);assert lib.save_loaded_asset(anim)
        assert anim.get_editor_property('skeleton')==skeleton
        roles[role]=anim.get_path_name();checks.append({'role':role,'asset':anim.get_path_name(),'seconds':anim.get_play_length()})
    roles['preview']=roles['idle'];roles['attack']=roles['melee_1'];sets.append({'skeleton':skeleton.get_path_name(),'roles':roles})
assert all(hashlib.sha256(disk(p).read_bytes()).hexdigest()==s for p,s in preserved.items())
report={'character':'yig','owner':OWNER,'recipeSha256':recipe_sha,'animation_sets':sets,'checks':checks,'originalAssetsUnchanged':preserved,'abilitiesAuthored':False}
(folder/'pending_binding.json').write_text(json.dumps(report,indent=2)+'\n')
unreal.log('REVIEWED_YIG_ANIMATIONS_CREATED '+str(len(checks)))
unreal.SystemLibrary.quit_editor()
