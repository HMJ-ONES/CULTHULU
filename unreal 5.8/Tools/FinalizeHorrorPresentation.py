"""Keep the new tiny dynamic presentation meshes on the lightweight render path."""
from pathlib import Path
import json,os,runpy
import unreal
PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
assets=unreal.EditorAssetLibrary;sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for directory in ['/Game/CultUlhu/Projectiles/Visceral20261127','/Game/CultUlhu/Characters/HorrorRefresh20261127/hastur']:
    for path in assets.list_assets(directory,recursive=True,include_folder=False):
        obj=unreal.load_asset(path)
        if isinstance(obj,unreal.StaticMesh):
            assert assets.get_metadata_tag(obj,'CultOwner') in ['CultUlhu.OriginalVisceral20261127','CultUlhu.OriginalHasturMantle20261127','CultUlhu.HorrorRefresh.20261127']
            settings=obj.get_editor_property('nanite_settings')
            if settings.enabled:
                settings.enabled=False;sub.set_nanite_settings(obj,settings,False)
                assert assets.save_loaded_asset(obj)
materials=[unreal.load_asset('/Game/CultUlhu/Materials/'+name) for name in ['M_VisceralGlob','M_VisceralOrgan']]
for material in materials:
    assert material and assets.get_metadata_tag(material,'CultOwner')=='CultUlhu.OriginalVisceral20261127'
    unreal.MaterialEditingLibrary.recompile_material(material)
unreal.AutomationLibrary.finish_loading_before_screenshot()
for material in materials: assert assets.save_loaded_asset(material,False)
if os.environ.get('CULT_REFRESH_HORROR_PORTRAITS')=='1':
    os.environ['CULT_CHARACTER_IMPORT_ID']='hastur,nug_and_yeb'
    runpy.run_path(str(PROJECT/'Tools/ImportCharacterPortraits.py'))
    config_path=PROJECT/'Config/ApprovedCharacterModels.json'
    config=json.loads(config_path.read_text(encoding='utf-8-sig'))
    runtime=json.loads((PROJECT/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
    for entity in ['hastur','nug_and_yeb']: config['characters'][entity]['reviewed_gameplay_binding']=runtime['characters'][entity]
    config_path.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
unreal.log('CULT_HORROR_PRESENTATION_FINALIZED')
unreal.SystemLibrary.quit_editor()
