"""Build only the original, texture-free vessel material; preserve authored assets."""
from pathlib import Path
import hashlib,json,shutil
import unreal

project=Path(__file__).resolve().parents[1]
assert project.name=='unreal 5.8' and (project/'CultUlhu.uproject').is_file()
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==project
path='/Game/CultUlhu/Materials/M_VesselSurface'
owner='CultUlhu.BuildVesselSurface'
lib=unreal.MaterialEditingLibrary
assets=unreal.EditorAssetLibrary
material=unreal.load_asset(path) if assets.does_asset_exist(path) else None
if material:
    assert assets.get_metadata_tag(material,'CultVesselOwner')==owner,'Preserve user-authored material'
    disk=project/'Content/CultUlhu/Materials/M_VesselSurface.uasset'
    backup=project/'Saved/Backups'/('VesselSurface_'+hashlib.sha256(disk.read_bytes()).hexdigest()[:16]+'.uasset')
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists(): shutil.copy2(disk,backup)
    lib.delete_all_material_expressions(material)
else:
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_VesselSurface','/Game/CultUlhu/Materials',unreal.Material,unreal.MaterialFactoryNew())
material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
uv=lib.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate,-700,-150)
def color(name,value,y):
    node=lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter,-700,y)
    node.set_editor_property('parameter_name',name)
    node.set_editor_property('default_value',unreal.LinearColor(*value,1))
    return node
body=color('BodyColor',(.045,.08,.09),0)
veins=color('VeinColor',(.12,.36,.32),150)
shader=lib.create_material_expression(material,unreal.MaterialExpressionCustom,-350,-100)
shader.set_editor_property('description','Original continuous living-obsidian colour')
shader.set_editor_property('code',(project/'Tools/VesselSurface.hlsl').read_text(encoding='utf-8-sig'))
shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for name in ['UV','BodyColor','VeinColor']:
    item=unreal.CustomInput(); item.set_editor_property('input_name',name); inputs.append(item)
shader.set_editor_property('inputs',inputs)
for node,name in [(uv,'UV'),(body,'BodyColor'),(veins,'VeinColor')]: assert lib.connect_material_expressions(node,'',shader,name)
assert lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_BASE_COLOR)
rough=lib.create_material_expression(material,unreal.MaterialExpressionConstant,-100,-250)
rough.set_editor_property('r',.6)
assert lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
fresnel=lib.create_material_expression(material,unreal.MaterialExpressionFresnel,-350,250)
fresnel.set_editor_property('exponent',3.)
fresnel.set_editor_property('base_reflect_fraction',.04)
def multiply(node,factor,x,y):
    result=lib.create_material_expression(material,unreal.MaterialExpressionMultiply,x,y)
    result.set_editor_property('const_b',factor)
    assert lib.connect_material_expressions(node,'',result,'A')
    return result
rim=multiply(fresnel,.04,-100,250)
rim_color=lib.create_material_expression(material,unreal.MaterialExpressionMultiply,100,250)
assert lib.connect_material_expressions(rim,'',rim_color,'A')
assert lib.connect_material_expressions(veins,'',rim_color,'B')
fill=multiply(shader,.045,-100,100)
emission=lib.create_material_expression(material,unreal.MaterialExpressionAdd,300,150)
assert lib.connect_material_expressions(fill,'',emission,'A')
assert lib.connect_material_expressions(rim_color,'',emission,'B')
assert lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assets.set_metadata_tag(material,'CultVesselOwner',owner)
assets.set_metadata_tag(material,'CultVesselRevision','20261022.2')
lib.recompile_material(material)
assert assets.save_loaded_asset(material)
names=[str(n) for n in lib.get_vector_parameter_names(material)]
assert set(names)=={'BodyColor','VeinColor'},names
unreal.log('[CultVesselSurface] Original opaque material saved; inspect native shader diagnostics and paired renders')
