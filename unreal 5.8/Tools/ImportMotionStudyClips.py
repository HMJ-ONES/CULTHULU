"""Import new Mi-Go motion clips without replacing models, rigs or supplied Prowl.

Run in the canonical UE editor. Writes a pending binding report only; activation
is a separate reviewed step after native playback checks.
"""
from pathlib import Path
import unreal,json,hashlib,shutil
PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
OWNER='CultUlhu.ReviewedMotion.20261106'
revision=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters']['mi_go']['revision']
folder=PROJECT/f'Saved/MotionIntegration/mi_go_{revision}'
record=json.loads((folder/'exports.json').read_text())
registry=json.loads((PROJECT/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
binding=registry['characters']['mi_go'];lib=unreal.EditorAssetLibrary
source=unreal.load_asset(binding['clips'][0]['asset']);skeleton=source.get_editor_property('skeleton')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def disk(asset):return PROJECT/'Content'/(asset.split('.',1)[0].removeprefix('/Game/')+'.uasset')
preserved={path:sha(disk(path)) for path in binding['meshes']+[binding['clips'][0]['asset'],skeleton.get_path_name()]}
backup=folder/'BeforeImport';backup.mkdir(exist_ok=True)
for path,digest in preserved.items():
    file=disk(path);copy=backup/(digest+'_'+file.name)
    if not copy.exists():shutil.copy2(file,copy)
tools=unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'Interchange.FeatureFlags.Import.FBX 0')
report={'owner':OWNER,'character':'mi_go','skeleton':skeleton.get_path_name(),'studySha256':record['studySha256'],
        'roles':{'dash':source.get_path_name(),'preview':source.get_path_name()},'checks':{}}
for role,clip in record['clips'].items():
    filename=PROJECT/clip['file'];assert sha(filename)==clip['sha256']
    destination='/Game/CultUlhu/Characters/mi_go/ReviewedMotion_'+revision
    name='A_MiGo_'+role;path=destination+'/'+name
    anim=unreal.load_asset(path)
    if anim:
        assert lib.get_metadata_tag(anim,'CultOwner')==OWNER and lib.get_metadata_tag(anim,'SourceSHA256')==clip['sha256'],'Preserve unowned or different animation revision'
    else:
        task=unreal.AssetImportTask();task.filename=str(filename);task.destination_path=destination;task.destination_name=name
        task.automated=True;task.replace_existing=False;task.save=True;task.factory=unreal.FbxFactory()
        options=unreal.FbxImportUI();options.automated_import_should_detect_type=False
        options.import_as_skeletal=True;options.import_mesh=False;options.import_animations=True
        options.import_materials=False;options.import_textures=False;options.create_physics_asset=False
        options.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION;options.skeleton=skeleton
        data=options.anim_sequence_import_data;data.set_editor_property('use_default_sample_rate',False)
        data.set_editor_property('custom_sample_rate',60)
        data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        task.options=options;tools.import_asset_tasks([task])
        candidates=[unreal.load_asset(p) for p in task.imported_object_paths]
        candidates=[a for a in candidates if isinstance(a,unreal.AnimSequence)]
        assert len(candidates)==1,(role,task.imported_object_paths)
        anim=candidates[0]
        lib.set_metadata_tag(anim,'CultOwner',OWNER);lib.set_metadata_tag(anim,'SourceSHA256',clip['sha256'])
        lib.set_metadata_tag(anim,'MotionRole',role);assert lib.save_loaded_asset(anim)
    assert anim.get_editor_property('skeleton')==skeleton
    assert abs(anim.get_play_length()-clip['seconds'])<1/60+.001,(role,anim.get_play_length(),clip['seconds'])
    names=unreal.AnimationLibrary.get_animation_track_names(anim)
    assert len(names)>=80,(role,len(names))
    report['roles'][role]=anim.get_path_name();report['checks'][role]={'seconds':anim.get_play_length(),'tracks':len(names)}
assert all(sha(disk(path))==digest for path,digest in preserved.items()),'Supplied art/rig/Prowl asset changed'
report['originalAssetsUnchanged']=preserved
(folder/'pending_binding.json').write_text(json.dumps(report,indent=2)+'\n')
unreal.log('REVIEWED_MIGO_CLIPS_IMPORTED '+str(len(record['clips'])))
unreal.SystemLibrary.quit_editor()
