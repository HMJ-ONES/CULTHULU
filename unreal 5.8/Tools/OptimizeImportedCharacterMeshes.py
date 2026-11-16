"""Generate conservative render LODs on importer-owned sculptures; preserve LOD0 and raw originals."""
from pathlib import Path
import json
import unreal
project=Path(__file__).resolve().parents[1]
config=json.loads((project/'Config/ApprovedCharacterModels.json').read_text(encoding='utf-8-sig'))
path=project/'Content/CharacterModels/registry.json'; registry=json.loads(path.read_text(encoding='utf-8-sig'))
subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for entity,binding in registry['characters'].items():
 if binding['skeletal'] or config['characters'][entity].get('source_triangles',0)<100000: continue
 for mesh_path in binding['meshes']:
  mesh=unreal.load_asset(mesh_path)
  assert unreal.EditorAssetLibrary.get_metadata_tag(mesh,'CultCharacterOwner')=='CultUlhu.ImportSelectedCharacterAssets','Unowned asset preserved'
  if unreal.EditorAssetLibrary.get_metadata_tag(mesh,'CultRenderLOD')!='sculpture_10pct_v1':
   nanite=mesh.get_editor_property('nanite_settings'); nanite.enabled=False; subsystem.set_nanite_settings(mesh,nanite,False)
   full=unreal.StaticMeshReductionSettings(); full.percent_triangles=1.0; full.screen_size=1.0
   reduced=unreal.StaticMeshReductionSettings(); reduced.percent_triangles=.1; reduced.screen_size=.5
   options=unreal.StaticMeshReductionOptions(); options.reduction_settings=[full,reduced]
   assert subsystem.set_lods(mesh,options)==2,'Could not generate sculpture render LOD'
   unreal.EditorAssetLibrary.set_metadata_tag(mesh,'CultRenderLOD','sculpture_10pct_v1')
   assert unreal.EditorAssetLibrary.save_loaded_asset(mesh),'Could not save owned render LOD'
 binding['render_lod']=1
 config['characters'][entity]['render_lod']=1
path.write_text(json.dumps(registry,indent=2)+'\n',encoding='utf-8')
(project/'Config/ApprovedCharacterModels.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
unreal.SystemLibrary.quit_editor()
