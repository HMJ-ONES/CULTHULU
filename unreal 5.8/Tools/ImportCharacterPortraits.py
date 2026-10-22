"""Import native Unreal model captures as selection-card thumbnails; keep ownership receipts."""
from pathlib import Path
import hashlib,json
import unreal
project=Path(__file__).resolve().parents[1]
path=project/'Content/CharacterModels/registry.json'
registry=json.loads(path.read_text(encoding='utf-8-sig'))
for entity,binding in registry['characters'].items():
 image=project/f'Saved/Screenshots/ApprovedCharacters/{entity}.png'
 if not image.is_file(): continue
 digest=hashlib.sha256(image.read_bytes()).hexdigest()[:12]
 destination=f'/Game/CultUlhu/Characters/{entity}/Portraits'
 name='T_Portrait_'+digest
 asset_path=destination+'/'+name
 if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
  task=unreal.AssetImportTask(); task.filename=str(image); task.destination_path=destination; task.destination_name=name
  task.automated=True; task.replace_existing=False; task.save=True
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
  asset=unreal.load_asset(asset_path); assert asset,'Portrait import failed'
  unreal.EditorAssetLibrary.set_metadata_tag(asset,'CultCharacterOwner','CultUlhu.NativeModelPortrait')
  asset.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
  unreal.EditorAssetLibrary.save_loaded_asset(asset)
 asset=unreal.load_asset(asset_path)
 assert unreal.EditorAssetLibrary.get_metadata_tag(asset,'CultCharacterOwner')=='CultUlhu.NativeModelPortrait','Existing user portrait preserved'
 binding['portrait']=asset.get_path_name()
path.write_text(json.dumps(registry,indent=2)+'\n',encoding='utf-8')
unreal.SystemLibrary.quit_editor()
