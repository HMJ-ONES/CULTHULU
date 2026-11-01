"""Restore original glTF PBR textures on original-source FBX rigs, without new art."""
from pathlib import Path
import json,struct,re
import unreal
project=Path(__file__).resolve().parents[1]
registry=json.loads((project/'Content/CharacterModels/registry.json').read_text())
assets=unreal.EditorAssetLibrary; mel=unreal.MaterialEditingLibrary
owner='CultUlhu.ImportSelectedCharacterAssets'
normalize=lambda s:re.sub('[^a-z0-9]','',s.lower())
for entity in ['mi_go','yig']:
 binding=registry['characters'][entity]
 assert binding['import_profile']=='original_fbx_v1','Original source rig required'
 destination=binding['meshes'][0].split('.',1)[0].rsplit('/',1)[0]+'/RestoredMaterials_v1'
 raw=(project/f'Saved/CharacterSources/{entity}/source.glb').read_bytes()
 length=struct.unpack_from('<I',raw,12)[0]; gltf=json.loads(raw[20:20+length]); binary=raw[28+length:]
 staging=project/f'Saved/CharacterSources/{entity}/original_pbr'; staging.mkdir(exist_ok=True)
 textures={}; materials={}
 def owned(obj):
  assets.set_metadata_tag(obj,'CultCharacterOwner',owner)
  assets.set_metadata_tag(obj,'CultCharacterSourceSHA256',binding['source_sha256'])
  assert assets.save_loaded_asset(obj,False),obj.get_path_name()
 def texture(index,kind):
  key=(index,kind)
  if key in textures: return textures[key]
  image=gltf['images'][gltf['textures'][index]['source']]; view=gltf['bufferViews'][image['bufferView']]
  data=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
  path=staging/f"Image_{index}_{kind}{'.jpg' if image['mimeType']=='image/jpeg' else '.png'}"
  if path.exists(): assert path.read_bytes()==data
  else: path.write_bytes(data)
  name=f'T_{index}_{kind}'; obj=unreal.load_asset(destination+'/'+name)
  if not obj:
   task=unreal.AssetImportTask(); task.filename=str(path); task.destination_path=destination; task.destination_name=name; task.automated=True; task.replace_existing=False
   unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]); obj=unreal.load_asset(destination+'/'+name)
   assert obj,'Missing source texture'
   obj.set_editor_property('srgb',kind=='Color')
   if kind=='Normal':
    obj.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP); obj.set_editor_property('flip_green_channel',True)
   owned(obj)
  else: assert assets.get_metadata_tag(obj,'CultCharacterOwner')==owner,'User texture preserved'
  textures[key]=obj; return obj
 for definition in gltf['materials']:
  name='M_'+re.sub('[^A-Za-z0-9_]','_',definition['name']); obj=unreal.load_asset(destination+'/'+name)
  if not obj:
   obj=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,destination,unreal.Material,unreal.MaterialFactoryNew())
   obj.set_editor_property('two_sided',definition.get('doubleSided',False))
   mel.set_base_material_usage(obj,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,True)
   def sample(info,kind):
    expression=mel.create_material_expression(obj,unreal.MaterialExpressionTextureSample)
    expression.set_editor_property('texture',texture(info['index'],kind))
    expression.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if kind=='Color' else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    return expression
   pbr=definition.get('pbrMetallicRoughness',{})
   if pbr.get('baseColorTexture'):
    color=sample(pbr['baseColorTexture'],'Color'); mel.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    if definition.get('alphaMode')=='BLEND':
     obj.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT); mel.connect_material_property(color,'A',unreal.MaterialProperty.MP_OPACITY)
   if pbr.get('metallicRoughnessTexture'):
    packed=sample(pbr['metallicRoughnessTexture'],'Linear')
    mel.connect_material_property(packed,'G',unreal.MaterialProperty.MP_ROUGHNESS)
    if pbr.get('metallicFactor',1)>0: mel.connect_material_property(packed,'B',unreal.MaterialProperty.MP_METALLIC)
   if definition.get('normalTexture'): mel.connect_material_property(sample(definition['normalTexture'],'Normal'),'RGB',unreal.MaterialProperty.MP_NORMAL)
   mel.recompile_material(obj); owned(obj)
  else: assert assets.get_metadata_tag(obj,'CultCharacterOwner')==owner,'User material preserved'
  materials[normalize(definition['name'])]=obj
 for path in binding['meshes']:
  mesh=unreal.load_asset(path); assert assets.get_metadata_tag(mesh,'CultCharacterOwner')==owner
  slots=list(mesh.get_editor_property('materials'))
  for slot in slots:
   key=normalize(str(slot.material_slot_name)); current=slot.material_interface
   if key not in materials and current: key=normalize(current.get_name().removeprefix('M_'))
   assert key in materials,f'Unmapped original material: {entity}/{key}'
   slot.material_interface=materials[key]
  mesh.set_editor_property('materials',slots); owned(mesh)
 unreal.log('[CultOriginalPBR] '+entity+' restored '+str(len(materials))+' original materials')
unreal.SystemLibrary.quit_editor()
