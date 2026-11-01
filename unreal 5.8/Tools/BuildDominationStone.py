"""Create a new original Domination material; never overwrite existing art."""
from pathlib import Path
import hashlib
import json
import unreal

PROJECT = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
NAME = 'M_DominationStone_v1'
PATH = '/Game/CultUlhu/Materials/' + NAME
assert not unreal.EditorAssetLibrary.does_asset_exist(PATH), 'Existing material preserved; author a new version instead'
original = PROJECT / 'Content/CultUlhu/Materials/M_MatchStone.uasset'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = sha(original)
lib = unreal.MaterialEditingLibrary
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, '/Game/CultUlhu/Materials', unreal.Material, unreal.MaterialFactoryNew())
assert mat
mat.set_editor_property('tangent_space_normal', False)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
nodes = {}
nodes['WorldP'] = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -700, -150)
nodes['SurfaceN'] = lib.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -700, 0)
nodes['Tint'] = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 150)
nodes['Tint'].set_editor_property('parameter_name', 'Color')
nodes['Tint'].set_editor_property('default_value', unreal.LinearColor(.12, .16, .18, 1))
for i, (name, value) in enumerate([('Masonry', 1.), ('SurfaceStyle', 0.)]):
    node = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 300 + i * 150)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    nodes[name] = node
shader = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 0)
shader.set_editor_property('description', 'Original broad slate / ashlar / cut trim / bronze')
source = PROJECT / 'Tools/DominationStone.hlsl'
shader.set_editor_property('code', source.read_text(encoding='utf-8-sig'))
shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for name in nodes:
    item = unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
shader.set_editor_property('inputs', inputs)
outputs = []
for name, kind in [('StoneRoughness', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('StoneNormal', unreal.CustomMaterialOutputType.CMOT_FLOAT3), ('StoneMetallic', unreal.CustomMaterialOutputType.CMOT_FLOAT1)]:
    item = unreal.CustomOutput()
    item.set_editor_property('output_name', name)
    item.set_editor_property('output_type', kind)
    outputs.append(item)
shader.set_editor_property('additional_outputs', outputs)
for name, node in nodes.items():
    assert lib.connect_material_expressions(node, '', shader, name)
for output, prop in [('', unreal.MaterialProperty.MP_BASE_COLOR), ('StoneRoughness', unreal.MaterialProperty.MP_ROUGHNESS), ('StoneNormal', unreal.MaterialProperty.MP_NORMAL), ('StoneMetallic', unreal.MaterialProperty.MP_METALLIC)]:
    assert lib.connect_material_property(shader, output, prop)
unreal.EditorAssetLibrary.set_metadata_tag(mat, 'CultUlhu.Generated', 'DominationStone_v1')
unreal.EditorAssetLibrary.set_metadata_tag(mat, 'CultUlhu.ShaderSHA256', sha(source))
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
assert {'Masonry', 'SurfaceStyle'} <= {str(n) for n in lib.get_scalar_parameter_names(mat)}
assert sha(original) == before, 'Original material changed'
receipt = PROJECT / 'Saved/Reports/DominationStoneBuild.json'
receipt.parent.mkdir(parents=True, exist_ok=True)
receipt.write_text(json.dumps({'asset': PATH, 'shaderSha256': sha(source), 'originalMaterialUnchangedSha256': before, 'outputSha256': sha(PROJECT / ('Content/CultUlhu/Materials/' + NAME + '.uasset'))}, indent=2) + '\n')
unreal.log('[CultDominationStone] New version saved; original material preserved. Native shader-map and render checks required.')
