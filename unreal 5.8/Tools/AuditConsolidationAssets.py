"""Read-only audit of active character render LODs and the optional default map."""
from pathlib import Path
import json
import unreal
project=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==project
registry=json.loads((project/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
rows=[]
for entity,binding in registry['characters'].items():
 row=dict(id=entity,kind='skeletal' if binding.get('skeletal') else 'procedural' if binding.get('procedural') else 'static',render_lod=binding.get('render_lod',0),meshes=[])
 for path in binding.get('meshes',[]):
  mesh=unreal.load_asset(path); assert mesh,path
  if isinstance(mesh,unreal.StaticMesh):
   counts=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())]
   row['meshes'].append(dict(asset=path,triangles_by_lod=counts,used_triangles=counts[row['render_lod']]))
 row['used_static_triangles']=sum(m['used_triangles'] for m in row['meshes'])
 rows.append(row)
map_path=project/'Content/NewMap.umap'
default_map=dict(local_file_exists=map_path.exists(),removed=False,reason='Preserved: no verified empty/unused-map deletion.')
if map_path.exists():
 registry_api=unreal.AssetRegistryHelpers.get_asset_registry(); registry_api.scan_paths_synchronous(['/Game'],force_rescan=False)
 default_map['referencers']=[str(p) for p in registry_api.get_referencers('/Game/NewMap',unreal.AssetRegistryDependencyOptions(True,True,True,True,True))]
report=dict(characters=rows,default_map=default_map)
path=project/'Saved/Consolidation20261027/AssetAudit.json'; path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
unreal.log('[CultConsolidationAssets] '+json.dumps(report)); unreal.SystemLibrary.quit_editor()
