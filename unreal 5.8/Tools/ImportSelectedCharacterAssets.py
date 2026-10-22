"""Idempotently import exact approved local packages into the canonical project.

Run in the full Editor with -ExecutePythonScript=<this file>. No web scraping,
license inference, source-script execution, source replacement or map saving.
"""
from pathlib import Path
import hashlib,json,os,struct
import unreal

PROJECT=Path(__file__).resolve().parents[1]
assert PROJECT.name=='unreal 5.8' and (PROJECT/'CultUlhu.uproject').is_file()
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
CONFIG=PROJECT/'Config/ApprovedCharacterModels.json'
REPORT=PROJECT/'Saved/Reports/CharacterModelImport.json'
RUNTIME=PROJECT/'Content/CharacterModels/registry.json'
OWNER='CultUlhu.ImportSelectedCharacterAssets'
assets=unreal.EditorAssetLibrary
entries=json.loads(CONFIG.read_text(encoding='utf-8-sig'))['characters']
runtime=json.loads(RUNTIME.read_text(encoding='utf-8-sig')) if RUNTIME.exists() else {'schema_version':1,'characters':{}}
results=[]
for entity,entry in entries.items():
    row={'id':entity,'source_url':entry.get('source_url',''),'status':'BLOCKED_ACQUISITION'}
    try:
        assert entity.replace('_','').isalnum() and entity==entity.lower(),'Invalid character ID'
        if not entry.get('approved'):
            row['status']='PENDING_SELECTION'; results.append(row); continue
        if entry.get('procedural')=='sghllor_light_and_mist':
            row['status']='ORIGINAL_PROCEDURAL' if runtime['characters'].get(entity,{}).get('core_material') else 'PENDING_PROCEDURAL_BUILD'
            results.append(row); continue
        if not entry.get('license_verified') or not entry.get('rights_cleared'):
            row['status']=entry.get('status','BLOCKED_RIGHTS'); runtime['characters'].pop(entity,None); results.append(row); continue
        source=(PROJECT/entry.get('source_file','')).resolve()
        assert source.is_relative_to(PROJECT/'Saved/CharacterSources'),'Sources must be in local staging'
        if not source.is_file(): results.append(row); continue
        data=source.read_bytes(); digest=hashlib.sha256(data).hexdigest()
        row.update(source_sha256=digest,source_bytes=len(data))
        suffix=source.suffix.lower()
        assert suffix in ['.glb','.gltf','.fbx','.obj'],'Supply a mesh package; do not execute downloaded .blend scripts'
        if suffix=='.fbx': row['source_clips']=entry.get('source_animation_evidence',[])
        if suffix=='.glb':
            assert data[:4]==b'glTF' and struct.unpack_from('<I',data,4)[0]==2,'Invalid GLB'
            size,kind=struct.unpack_from('<II',data,12); gltf=json.loads(data[20:20+size])
            row['source_skins']=len(gltf.get('skins',[])); row['source_clips']=[a.get('name','') for a in gltf.get('animations',[])]
        profile=entry.get('import_profile','default')
        assert profile in ['default','unbaked_rig_v1','original_fbx_v1','blender_fbx_v2'],'Unknown import profile'
        revision='' if profile=='default' else '_'+profile
        destination=f'/Game/CultUlhu/Characters/{entity}/Imports/h{digest[:12]}{revision}'
        marker=PROJECT/f'Saved/CharacterSources/{entity}/import_{digest[:12]}{revision}.json'
        if assets.does_directory_exist(destination):
            if marker.is_file():
                receipt=json.loads(marker.read_text(encoding='utf-8')); paths=receipt['assets']
            else:
                paths=list(assets.list_assets(destination,recursive=True,include_folder=False))
                assert paths and all(assets.get_metadata_tag(unreal.load_asset(p),'CultCharacterOwner')==OWNER and assets.get_metadata_tag(unreal.load_asset(p),'CultCharacterSourceSHA256')==digest for p in paths),'Existing unowned destination preserved'
                marker.parent.mkdir(parents=True,exist_ok=True)
                marker.write_text(json.dumps({'source_sha256':digest,'assets':paths},indent=2)+'\n',encoding='utf-8')
            assert all(assets.does_asset_exist(p) and assets.get_metadata_tag(unreal.load_asset(p),'CultCharacterOwner')==OWNER for p in paths),'Existing user assets preserved'
        else:
            task=unreal.AssetImportTask(); task.filename=str(source); task.destination_path=destination
            task.automated=True; task.replace_existing=False; task.save=False
            if profile=='unbaked_rig_v1':
                pipeline=unreal.InterchangeGenericAssetsPipeline()
                common=pipeline.get_editor_property('common_meshes_properties')
                common.set_editor_property('bake_meshes',False)
                pipeline.get_editor_property('mesh_pipeline').set_editor_property('create_physics_asset',False)
                override=unreal.InterchangePipelineStackOverride()
                override.set_editor_property('override_pipelines',[unreal.SoftObjectPath(pipeline.get_path_name())])
                task.options=override
            if suffix=='.fbx':
                # Original FBX retains the creator's bind transforms; use the legacy
                # factory explicitly rather than converting the rig through glTF.
                unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'Interchange.FeatureFlags.Import.FBX 0')
                task.factory=unreal.FbxFactory()
                options=unreal.FbxImportUI(); options.automated_import_should_detect_type=False
                options.import_as_skeletal=True; options.create_physics_asset=False
                options.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
                options.import_mesh=True; options.import_materials=True; options.import_textures=True; options.import_animations=True
                task.options=options
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            paths=list(assets.list_assets(destination,recursive=True,include_folder=False))
            assert paths,'Importer produced no assets'
            for path in paths:
                obj=unreal.load_asset(path); assets.set_metadata_tag(obj,'CultCharacterOwner',OWNER)
                assets.set_metadata_tag(obj,'CultCharacterSourceSHA256',digest)
                assert assets.save_loaded_asset(obj),'Could not save '+path
            marker.parent.mkdir(parents=True,exist_ok=True)
            marker.write_text(json.dumps({'source_sha256':digest,'assets':paths},indent=2)+'\n',encoding='utf-8')
        loaded=[unreal.load_asset(p) for p in paths]
        skeletal=[o for o in loaded if isinstance(o,unreal.SkeletalMesh)]
        static=[o for o in loaded if isinstance(o,unreal.StaticMesh)]
        clips=[o for o in loaded if isinstance(o,unreal.AnimSequence)]
        meshes=[o for o in (skeletal or static) if o.get_name() not in entry.get('excluded_mesh_names',[])]; assert meshes,'No playable mesh imported'
        if row.get('source_skins',0): assert skeletal,'Rigged GLB was flattened; preserve its skeleton'
        binding=dict(meshes=[o.get_path_name() for o in meshes],skeletal=bool(skeletal),clips=[dict(name=o.get_name(),asset=o.get_path_name(),seconds=o.get_editor_property('sequence_length')) for o in clips],rotation=entry.get('rotation',[0,0,0]),bodies=entry.get('bodies',1),floating=entry.get('floating',False),animation_roles={},source_sha256=digest)
        binding['import_profile']=profile
        if skeletal:
            skeleton=skeletal[0].get_editor_property('skeleton')
            assert skeleton,'Missing source skeleton'
            skeletons=sorted({o.get_editor_property('skeleton') for o in skeletal},key=lambda o:o.get_path_name())
            assert all(o.get_editor_property('skeleton') in skeletons for o in clips),'Animation/skeleton mismatch'
            binding['skeleton']=skeleton.get_path_name()
            binding['mesh_skeletons']={o.get_path_name():o.get_editor_property('skeleton').get_path_name() for o in skeletal}
            binding['animation_sets']=[]
            for rig in skeletons:
                roles={}
                for role,wanted in entry.get('animation_roles',{}).items():
                    candidates=[o for o in clips if o.get_editor_property('skeleton')==rig]
                    found=[o for o in candidates if wanted.lower() in o.get_name().lower()]
                    if not found and len(candidates)==1 and row.get('source_clips')==[wanted]: found=candidates
                    if len(found)==1: roles[role]=found[0].get_path_name()
                binding['animation_sets'].append({'skeleton':rig.get_path_name(),'roles':roles})
        for role,wanted in entry.get('animation_roles',{}).items():
            found=[o for o in clips if wanted.lower() in o.get_name().lower()]
            # Interchange renames a single source animation to source_Anim.
            # One-to-one source/import evidence makes that mapping unambiguous.
            if not found and len(clips)==1 and row.get('source_clips')==[wanted]: found=clips
            if len(found)==1: binding['animation_roles'][role]=found[0].get_path_name()
        if row.get('source_clips'): assert clips,'Source clips were lost during import'
        if entry.get('reference_bounds_sha256')==digest and entry.get('reference_bounds_profile','default')==profile and entry.get('reference_bounds_rotation',[0,0,0])==entry.get('rotation',[0,0,0]) and entry.get('reference_bounds'): binding['reference_bounds']=entry['reference_bounds']
        if 'render_lod' in entry: binding['render_lod']=entry['render_lod']
        row['status']='IMPORTED_ANIMATED' if clips else 'IMPORTED_RIGGED' if skeletal else 'IMPORTED_STATIC'
        binding['status']=row['status']; binding['source_url']=entry['source_url']; binding['author']=entry.get('author',''); binding['license']=entry.get('license','')
        previous=runtime['characters'].get(entity,{})
        if previous.get('source_sha256')==digest and previous.get('portrait'): binding['portrait']=previous['portrait']
        runtime['characters'][entity]=binding; row['binding']=binding
    except Exception as error:
        row['status']='BLOCKED_TECHNICAL'; row['error']=str(error)
        unreal.log_error('[CultCharacterImport] '+entity+': '+str(error))
    results.append(row)
RUNTIME.parent.mkdir(parents=True,exist_ok=True); REPORT.parent.mkdir(parents=True,exist_ok=True)
temporary=RUNTIME.with_suffix('.json.tmp'); temporary.write_text(json.dumps(runtime,indent=2)+'\n',encoding='utf-8'); os.replace(temporary,RUNTIME)
REPORT.write_text(json.dumps({'characters':results},indent=2)+'\n',encoding='utf-8')
unreal.log('[CultCharacterImport] '+json.dumps([{'id':r['id'],'status':r['status']} for r in results]))
unreal.SystemLibrary.quit_editor()
