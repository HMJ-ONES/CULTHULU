"""Stage a new anatomical skeletal model and clips, reusing original UE materials.
No active binding changes, no source art replacement. Set CULT_MOTION_CHARACTER.
"""
from pathlib import Path
import unreal,json,hashlib,os,re,itertools
PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
key=os.environ['CULT_MOTION_CHARACTER']
revision=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters'][key]['revision']
folder=PROJECT/f'Saved/MotionIntegration/{key}_{revision}'
record=json.loads((folder/'exports.json').read_text())
registry=json.loads((PROJECT/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
binding=registry['characters'][key];lib=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
OWNER='CultUlhu.ReviewedMotion.20261106'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
norm=lambda s:re.sub('[^a-z0-9]','',str(s).lower())
def disk(path):return PROJECT/'Content'/(path.split('.',1)[0].removeprefix('/Game/')+'.uasset')
preserved={p:sha(disk(p)) for p in binding['meshes']}
materials={}
def slot_names(slot):
    result=[]
    for name in ['imported_material_slot_name','material_slot_name']:
        try:result.append(slot.get_editor_property(name))
        except Exception:pass
    return result
for path in binding['meshes']:
    original=unreal.load_asset(path)
    slots=original.get_editor_property('materials' if isinstance(original,unreal.SkeletalMesh) else 'static_materials')
    for slot in slots:
        material=slot.material_interface
        if not material:continue
        for name in slot_names(slot)+[material.get_name(),material.get_name().removeprefix('M_')]:
            materials.setdefault(norm(name),material)
stage=revision+'_r2' if key=='shoggoths' else revision
destination=f'/Game/CultUlhu/Characters/{key}/ReviewedMotion_{stage}'
material_copies={}
def skeletal_material(source):
    path=source.get_path_name()
    if path in material_copies:return material_copies[path]
    name=source.get_name()+'_'+hashlib.sha256(path.encode()).hexdigest()[:8]
    target=destination+'/SourceMaterials/'+name
    copy=unreal.load_asset(target)
    if copy:assert lib.get_metadata_tag(copy,'CultOwner')==OWNER
    else:
        copy=lib.duplicate_asset(path,target);assert copy
        lib.set_metadata_tag(copy,'CultOwner',OWNER);lib.set_metadata_tag(copy,'SourceMaterial',path)
        if isinstance(copy,unreal.Material):
            unreal.MaterialEditingLibrary.set_base_material_usage(copy,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,True)
        else:
            parent=source.get_editor_property('parent');assert parent
            copy.set_editor_property('parent',skeletal_material(parent))
        assert lib.save_loaded_asset(copy)
    material_copies[path]=copy;return copy
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'Interchange.FeatureFlags.Import.FBX 0')
def do_import(name,source,animation=False,skeleton=None):
    filename=PROJECT/source['file'];assert sha(filename)==source['sha256']
    existing=unreal.load_asset(destination+'/'+name)
    if existing:
        assert lib.get_metadata_tag(existing,'CultOwner')==OWNER and lib.get_metadata_tag(existing,'SourceSHA256')==source['sha256']
        return existing
    task=unreal.AssetImportTask();task.filename=str(filename);task.destination_path=destination;task.destination_name=name
    task.automated=True;task.replace_existing=False;task.save=True;task.factory=unreal.FbxFactory()
    options=unreal.FbxImportUI();options.automated_import_should_detect_type=False
    options.import_as_skeletal=True;options.import_mesh=not animation;options.import_animations=animation
    options.import_materials=False;options.import_textures=False;options.create_physics_asset=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH
    if animation:
        options.skeleton=skeleton;data=options.anim_sequence_import_data
        data.set_editor_property('use_default_sample_rate',False);data.set_editor_property('custom_sample_rate',60)
        data.set_editor_property('animation_length',unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task.options=options;tools.import_asset_tasks([task])
    candidates=[unreal.load_asset(p) for p in task.imported_object_paths]
    wanted=unreal.AnimSequence if animation else unreal.SkeletalMesh
    candidates=[a for a in candidates if isinstance(a,wanted)]
    assert len(candidates)==1,(name,task.imported_object_paths)
    obj=candidates[0];lib.set_metadata_tag(obj,'CultOwner',OWNER);lib.set_metadata_tag(obj,'SourceSHA256',source['sha256'])
    if isinstance(obj,unreal.SkeletalMesh):
        generated_skeleton=obj.get_editor_property('skeleton');assert generated_skeleton,'No imported skeleton'
        lib.set_metadata_tag(generated_skeleton,'CultOwner',OWNER)
        assert lib.save_loaded_asset(generated_skeleton),'The mesh and clips require a persisted skeleton'
    assert lib.save_loaded_asset(obj);return obj
mesh=do_import('SK_'+key+'_'+revision,record['mesh'])
skeleton=mesh.get_editor_property('skeleton');slots=mesh.get_editor_property('materials')
assert skeleton,'Staged mesh has an unresolved skeleton; preserve the stage and import into a fresh revision'
assigned=[]
for slot in slots:
    names=slot_names(slot);material=next((materials[norm(n)] for n in names if norm(n) in materials),None)
    assert material,('Missing original material',[str(n) for n in names],list(materials))
    assigned.append(material.get_path_name());slot.material_interface=skeletal_material(material)
mesh.set_editor_property('materials',slots);assert lib.save_loaded_asset(mesh)
roles={};checks={}
for role,source in record['clips'].items():
    anim=do_import('A_'+key+'_'+role,source,True,skeleton)
    assert anim.get_editor_property('skeleton')==skeleton
    assert abs(anim.get_play_length()-source['seconds'])<1/60+.001
    roles[role]=anim.get_path_name();checks[role]={'seconds':anim.get_play_length(),'tracks':len(unreal.AnimationLibrary.get_animation_track_names(anim))}
roles['preview']=roles['idle'];roles['attack']=roles['melee_1']
bounds=mesh.get_imported_bounds();origin=bounds.origin;extent=bounds.box_extent
points=[(-y,x,z) for x,y,z in itertools.product([origin.x-extent.x,origin.x+extent.x],[origin.y-extent.y,origin.y+extent.y],[origin.z-extent.z,origin.z+extent.z])]
reference=[min(p[i] for p in points) for i in range(3)]+[max(p[i] for p in points) for i in range(3)]
assert 100<reference[5]-reference[2]<400,reference
assert all(sha(disk(path))==digest for path,digest in preserved.items())
report={'owner':OWNER,'character':key,'revision':revision,'studySha256':record['studySha256'],
        'meshes':[mesh.get_path_name()],'skeleton':skeleton.get_path_name(),'roles':roles,'checks':checks,
        'rotation':[0,90,0],'reference_bounds':reference,'originalMaterials':assigned,
        'originalAssetsUnchanged':preserved,'rootTravelNeedsIntegration':key=='cthulhu_avatar'}
(folder/'pending_binding.json').write_text(json.dumps(report,indent=2)+'\n')
unreal.log('ANATOMICAL_MOTION_IMPORTED '+key)
unreal.SystemLibrary.quit_editor()
