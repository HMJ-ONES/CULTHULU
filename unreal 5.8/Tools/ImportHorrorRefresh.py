"""Import the three acquired designs into new owned folders; retain previous art."""
from pathlib import Path
import json
import unreal

PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
records=json.loads((PROJECT/'Saved/HorrorRefresh20261127/sources.json').read_text())
assets=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools()
runtime_path=PROJECT/'Content/CharacterModels/registry.json'
config_path=PROJECT/'Config/ApprovedCharacterModels.json'
runtime=json.loads(runtime_path.read_text(encoding='utf-8-sig'))
config=json.loads(config_path.read_text(encoding='utf-8-sig'))
groups=[]; owner='CultUlhu.HorrorRefresh.20261127'
for record in records:
    key=record['key'];dest=f'/Game/CultUlhu/Characters/HorrorRefresh20261127/{key}'
    if not assets.does_directory_exist(dest):
        task=unreal.AssetImportTask();task.filename=str(PROJECT/record['source_file']);task.destination_path=dest
        task.automated=True;task.replace_existing=False;task.save=True
        tools.import_asset_tasks([task])
        for path in assets.list_assets(dest,recursive=True,include_folder=False):
            obj=unreal.load_asset(path);assets.set_metadata_tag(obj,'CultOwner',owner);assets.save_loaded_asset(obj)
    paths=assets.list_assets(dest,recursive=True,include_folder=False)
    assert paths and all(assets.get_metadata_tag(unreal.load_asset(p),'CultOwner')==owner for p in paths),'Preserve unowned assets'
    loaded=[unreal.load_asset(p) for p in paths]
    meshes=[o for o in loaded if isinstance(o,(unreal.SkeletalMesh,unreal.StaticMesh))]
    if key=='nug': meshes=[o for o in meshes if isinstance(o,unreal.SkeletalMesh)] # Source's wall/ground are not part of the creature.
    clips=[o for o in loaded if isinstance(o,unreal.AnimSequence)]
    assert meshes
    sets=[]
    for rig in {m.get_editor_property('skeleton') for m in meshes if isinstance(m,unreal.SkeletalMesh)}:
        matching=[c for c in clips if c.get_editor_property('skeleton')==rig]
        roles={role:matching[0].get_path_name() for role in ['idle','walk','run','preview']} if matching else {}
        sets.append({'skeleton':rig.get_path_name(),'roles':roles})
    if key=='hastur':
        subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        for mesh in meshes:
            assert isinstance(mesh,unreal.StaticMesh),'Reaper environment rig must be removed'
            if assets.get_metadata_tag(mesh,'CultReduced')!='v1':
                nanite=mesh.get_editor_property('nanite_settings');nanite.enabled=False;subsystem.set_nanite_settings(mesh,nanite,False)
                a=unreal.StaticMeshReductionSettings();a.percent_triangles=1;a.screen_size=1
                b=unreal.StaticMeshReductionSettings();b.percent_triangles=.12;b.screen_size=.5
                opts=unreal.StaticMeshReductionOptions();opts.reduction_settings=[a,b]
                assert subsystem.set_lods(mesh,opts)==2
                assets.set_metadata_tag(mesh,'CultReduced','v1');assets.save_loaded_asset(mesh)
        # Original two-tone design becomes an aged ochre shroud with a dark hood.
        mel=unreal.MaterialEditingLibrary
        overrides={}
        for index,color in enumerate([(.19,.115,.017),(.025,.019,.01)]):
            name='M_KingShroud_'+str(index);path=dest+'/'+name
            mat=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
            if not assets.get_metadata_tag(mat,'CultOwner'):
                base=mel.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);base.constant=unreal.LinearColor(*color,1)
                mel.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
                rough=mel.create_material_expression(mat,unreal.MaterialExpressionConstant);rough.r=.72
                mel.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
                mat.set_editor_property('two_sided',True);mel.recompile_material(mat);assets.set_metadata_tag(mat,'CultOwner',owner);assets.save_loaded_asset(mat)
        for mesh in meshes:
            # The source's GRIM_1 section is the face; keep it shadow-dark.
            overrides[mesh.get_path_name()]=dest+('/M_KingShroud_1' if mesh.get_name().endswith('_1') else '/M_KingShroud_0')
        binding=dict(meshes=[m.get_path_name() for m in meshes],skeletal=False,clips=[],animation_sets=[],animation_roles={},rotation=[0,-90,0],bodies=1,floating=True,render_lod=1,material_overrides=overrides,temporary=True,status='IMPORTED_STATIC',**{k:record[k] for k in ['source_url','author','license','source_sha256']})
        runtime['characters']['hastur']=binding
        config['characters']['hastur'].update(record,approved=True,license_verified=True,rights_cleared=True,temporary=True,reviewed_gameplay_binding=binding)
    else:
        groups.append(dict(meshes=[m.get_path_name() for m in meshes],animation_sets=sets,source=record,clips=[dict(name=c.get_name(),asset=c.get_path_name(),seconds=c.get_editor_property('sequence_length')) for c in clips]))
assert len(groups)==2
binding=dict(meshes=[m for g in groups for m in g['meshes']],skeletal=True,clips=[c for g in groups for c in g['clips']],animation_sets=[s for g in groups for s in g['animation_sets']],animation_roles={},rotation=[0,0,0],bodies=1,floating=True,twin_split=len(groups[0]['meshes']),distinct_twins=True,temporary=True,status='IMPORTED_ANIMATED',source_url=records[0]['source_url'],author='ChopperManiac; Yury Misiyuk',license='CC-BY-4.0',source_sha256=records[0]['source_sha256'],sources=[g['source'] for g in groups])
runtime['characters']['nug_and_yeb']=binding
config['characters']['nug_and_yeb'].update(approved=True,license_verified=True,rights_cleared=True,temporary=True,reviewed_gameplay_binding=binding,sources=binding['sources'])
for path,doc in [(runtime_path,runtime),(config_path,config)]: path.write_text(json.dumps(doc,indent=2)+'\n',encoding='utf-8')
(PROJECT/'Saved/HorrorRefresh20261127/Imported.json').write_text(json.dumps({'hastur':runtime['characters']['hastur'],'nug_and_yeb':binding},indent=2))
unreal.log('CULT_HORROR_REFRESH_IMPORTED')
unreal.SystemLibrary.quit_editor()
