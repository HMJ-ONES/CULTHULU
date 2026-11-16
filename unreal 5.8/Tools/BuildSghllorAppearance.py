"""Build the user's approved original light-ball/mist concept; no downloaded art or textures."""
from pathlib import Path
import json
import unreal
project=Path(__file__).resolve().parents[1]
assets=unreal.EditorAssetLibrary; lib=unreal.MaterialEditingLibrary
folder='/Game/CultUlhu/Characters/sghllor/Original'
owner='CultUlhu.OriginalSghllor.v1'
def material(name,mist):
 path=folder+'/'+name
 if assets.does_asset_exist(path):
  result=unreal.load_asset(path)
  assert assets.get_metadata_tag(result,'CultCharacterOwner')==owner,'Existing artist material preserved'
  return result
 result=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
 result.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 if mist:
  result.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
  result.set_editor_property('two_sided',True)
 time=lib.create_material_expression(result,unreal.MaterialExpressionTime,-700,0)
 uv=lib.create_material_expression(result,unreal.MaterialExpressionTextureCoordinate,-700,200)
 shader=lib.create_material_expression(result,unreal.MaterialExpressionCustom,-400,0)
 shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 shader.set_editor_property('code','return float3(.6+.18*sin(T*.8),.45+.12*sin(T*1.1+2),.82+.15*sin(T*.6+4)) * (1.7+.25*sin(T*1.4)) * '+('1;' if mist else '(.3+1.7*pow(1-F,3));'))
 fresnel=lib.create_material_expression(result,unreal.MaterialExpressionFresnel,-700,-150)
 inputs=[]
 for name in ['T','F']:
  item=unreal.CustomInput(); item.set_editor_property('input_name',name); inputs.append(item)
 shader.set_editor_property('inputs',inputs)
 assert lib.connect_material_expressions(time,'',shader,'T')
 assert lib.connect_material_expressions(fresnel,'',shader,'F')
 assert lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 if mist:
  opacity=lib.create_material_expression(result,unreal.MaterialExpressionCustom,-400,200)
  opacity.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
  opacity.set_editor_property('code','float2 p=UV*2-1; float r=length(p); float wave=sin(p.x*8+T*.7)*sin(p.y*6-T*.4); return saturate((1-smoothstep(.25,1,r))*(.13+.09*wave));')
  inputs=[]
  for name in ['UV','T']:
   item=unreal.CustomInput(); item.set_editor_property('input_name',name); inputs.append(item)
  opacity.set_editor_property('inputs',inputs); lib.connect_material_expressions(uv,'',opacity,'UV'); lib.connect_material_expressions(time,'',opacity,'T')
  lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
 assets.set_metadata_tag(result,'CultCharacterOwner',owner); lib.recompile_material(result); assert assets.save_loaded_asset(result)
 return result
core=material('M_SghllorCore',False); mist=material('M_SghllorMist',True)
config_path=project/'Config/ApprovedCharacterModels.json'; config=json.loads(config_path.read_text(encoding='utf-8-sig'))
config['characters']['sghllor']=dict(approved=True,procedural='sghllor_light_and_mist',source_url='Original user-approved light-ball and mist concept',title='Sghllor original light and mist',author='CULT-ULHU',license='ORIGINAL',license_url='',license_verified=True,rights_cleared=True,status='ORIGINAL_PROCEDURAL',remaining_art=['Refine the original light and mist art after reviewing in play'])
config_path.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
registry_path=project/'Content/CharacterModels/registry.json'; registry=json.loads(registry_path.read_text(encoding='utf-8-sig'))
binding=dict(procedural='sghllor_light_and_mist',meshes=[],skeletal=False,clips=[],animation_roles={},floating=True,source_sha256='original-sghllor-v1',status='ORIGINAL_PROCEDURAL',source_url=config['characters']['sghllor']['source_url'],author='CULT-ULHU',license='ORIGINAL',core_material=core.get_path_name(),mist_material=mist.get_path_name())
previous=registry['characters'].get('sghllor',{})
if previous.get('portrait'): binding['portrait']=previous['portrait']
registry['characters']['sghllor']=binding
registry_path.write_text(json.dumps(registry,indent=2)+'\n',encoding='utf-8')
unreal.SystemLibrary.quit_editor()
