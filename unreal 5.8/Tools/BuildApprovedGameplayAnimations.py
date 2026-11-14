"""Create original gameplay motions on the existing approved rigs; preserve all source imports.
Run with canonical Unreal Editor -ExecutePythonScript. Re-running reuses owned assets.
"""
from pathlib import Path
import unreal,json,math,shutil,itertools,runpy
PROJECT=Path(__file__).resolve().parents[1]
assert PROJECT.name=='unreal 5.8' and Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
OWNER='CultUlhu.GameplayMotion.20261030.v1'
registry_path=PROJECT/'Content/CharacterModels/registry.json'
config_path=PROJECT/'Config/ApprovedCharacterModels.json'
backup=PROJECT/'Saved/CombatTactics20261030/PresentationBefore'; backup.mkdir(parents=True,exist_ok=True)
for path in [registry_path,config_path,PROJECT/'Config/DefaultGame.ini']:
 dest=backup/path.name
 if not dest.exists(): shutil.copy2(path,dest)
registry=json.loads(registry_path.read_text(encoding='utf-8-sig')); config=json.loads(config_path.read_text(encoding='utf-8-sig'))
audit=runpy.run_path(str(PROJECT/'Tools/InspectApprovedRigs.py'))['collect']()
lib=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools(); generated=[]
ROLES={'idle':1.2,'walk':1.,'run':.65,'jump':.8,'attack':.42,'cast':.65,'dash':.24,'land':.24}
recipes=runpy.run_path(str(PROJECT/'Tools/CultRigMotion.py'))
contexts={row['clip']:recipes['context'](row) for row in audit}
armor=next(contexts[row['clip']] for row in audit if row['id']=='yig' and 'Armor_' in row['clip'])
profile=json.loads((PROJECT/'Config/ApprovedRigMotion.json').read_text(encoding='utf-8'))
recipe_sha=__import__('hashlib').sha256((PROJECT/'Tools/CultRigMotion.py').read_bytes()).hexdigest()
for entity in ['mi_go','yig']:
 entry=registry['characters'][entity]; sets=[]
 for row in [r for r in audit if r['id']==entity]:
  source=unreal.load_asset(row['clip']); skeleton=source.get_editor_property('skeleton'); roles={}
  label=source.get_name().replace('_Anim','')
  for role,seconds in ROLES.items():
   # Mi-Go's supplied Prowl is retained for walking/running; Yig's supplied clips are true idles.
   if (entity=='mi_go' and role in ['walk','run']) or (entity=='yig' and role=='idle'):
    roles[role]=source.get_path_name(); continue
   folder='/Game/CultUlhu/Characters/'+entity+'/Gameplay20261030_v1'; name='A_'+label+'_'+role
   path=folder+'/'+name
   clip=lib.load_asset(path) if lib.does_asset_exist(path) else None
   if clip:
    assert lib.get_metadata_tag(clip,'CultOwner')==OWNER,'Refusing to overwrite an unowned animation'
    original=PROJECT/'Content'/Path(path.removeprefix('/Game/')+'.uasset'); preserved=backup/'GeneratedClips'/entity/original.name
    preserved.parent.mkdir(parents=True,exist_ok=True)
    if original.exists() and not preserved.exists(): shutil.copy2(original,preserved)
   else:
    factory=unreal.AnimSequenceFactory(); factory.set_editor_property('target_skeleton',skeleton)
    clip=tools.create_asset(name,folder,unreal.AnimSequence,factory)
    assert clip,'Animation creation failed'
   controller=clip.get_editor_property('controller')
   controller.open_bracket('Original CULT-ULHU gameplay motion',False)
   controller.remove_all_bone_tracks(False)
   controller.set_frame_rate(unreal.FrameRate(30,1),False)
   frames=max(2,round(seconds*30)); controller.set_number_of_frames(unreal.FrameNumber(frames),False)
   ctx=contexts[row['clip']]
   samples=[recipes['drive_weapons'](ctx,armor,role,i/frames,profile['weapon_hands']) if entity=='yig' and 'Sword_' in row['clip'] else recipes['sample'](ctx,role,i/frames) for i in range(frames+1)]
   for bone,pose in row['poses'].items():
    controller.add_bone_curve(bone,False)
    positions=[unreal.Vector(*sample[bone.lower()]['location']) for sample in samples]
    rotations=[unreal.Quat(*sample[bone.lower()]['rotation']) for sample in samples]
    scales=[unreal.Vector(*sample[bone.lower()]['scale']) for sample in samples]
    assert controller.set_bone_track_keys(bone,positions,rotations,scales,False),bone
   controller.close_bracket(False) # AnimSequenceFactory already marks its model populated.
   lib.set_metadata_tag(clip,'CultOwner',OWNER); lib.set_metadata_tag(clip,'SourceClip',row['clip']); lib.set_metadata_tag(clip,'MotionRole',role); lib.set_metadata_tag(clip,'MotionRecipeSHA256',recipe_sha)
   assert lib.save_loaded_asset(clip),'Animation save failed'
   roles[role]=clip.get_path_name(); generated.append(clip.get_path_name())
  roles['preview']=source.get_path_name(); sets.append({'skeleton':skeleton.get_path_name(),'roles':roles})
 entry['animation_sets']=sets
 if entity=='mi_go': entry['animation_roles']=sets[0]['roles']
 # Bounds were measured in the previous imported orientation; rotate the existing calibrated box exactly once.
 old=entry.get('rotation',[0,0,0]); desired=[0,-90,0]
 if old!=desired:
  assert old==[0,0,0],'Unexpected calibration orientation'
  b=entry['reference_bounds']; points=[(y,-x,z) for x,y,z in itertools.product([b[0],b[3]],[b[1],b[4]],[b[2],b[5]])]
  entry['reference_bounds']=[min(v[i] for v in points) for i in range(3)]+[max(v[i] for v in points) for i in range(3)]
 entry['rotation']=desired; entry['gameplay_motion_owner']=OWNER
 config['characters'][entity].update(rotation=desired,reference_bounds=entry['reference_bounds'],reference_bounds_rotation=desired,gameplay_animation_sets=sets)
 config['characters'][entity]['remaining_art']=['Polish the original generated gameplay motions and transitions on the supplied rig; source clips and geometry are preserved.']
for path,data in [(registry_path,registry),(config_path,config)]: path.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
report={'owner':OWNER,'assets':generated,'source_imports_modified':False,'generated_animation_count':len(generated),'motion_recipe_sha256':recipe_sha,'rig_motion_revision':2}
(PROJECT/'Saved/CombatTactics20261030/GameplayAnimations.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('Original gameplay animations created: '+str(len(generated)))
unreal.SystemLibrary.quit_editor()
