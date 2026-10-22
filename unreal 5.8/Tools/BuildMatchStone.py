"""Build the original match-only procedural stone; preserve imported materials."""
from pathlib import Path
import shutil
import unreal

project = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == project
path = '/Game/CultUlhu/Materials/M_MatchStone'
lib = unreal.MaterialEditingLibrary
mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
force = '-CultRebuildMatchStone' in unreal.SystemLibrary.get_command_line()
if mat and not force:
    raise RuntimeError('Existing generated material preserved; use -CultRebuildMatchStone explicitly')
if mat:
    backup = project / 'Saved/Backups/MatchStoneBeforeRebuild.uasset'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(project / 'Content/CultUlhu/Materials/M_MatchStone.uasset', backup)
    lib.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MatchStone', '/Game/CultUlhu/Materials', unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('tangent_space_normal', False)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -700, -100)
normal = lib.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -700, 50)
color = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 200)
color.set_editor_property('parameter_name', 'Color')
color.set_editor_property('default_value', unreal.LinearColor(.12, .16, .18, 1))
masonry = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 350)
masonry.set_editor_property('parameter_name', 'Masonry')
masonry.set_editor_property('default_value', 1.)
shader = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 0)
shader.set_editor_property('description', 'World stone / mortar / weather / surface-gradient normal')
shader.set_editor_property('code', (project / 'Tools/MatchStone.hlsl').read_text(encoding='utf-8-sig'))
shader.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for name in ['WorldP', 'SurfaceN', 'Tint', 'Masonry']:
    item = unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
shader.set_editor_property('inputs', inputs)
outputs = []
for name, kind in [('StoneRoughness', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('StoneNormal', unreal.CustomMaterialOutputType.CMOT_FLOAT3)]:
    item = unreal.CustomOutput()
    item.set_editor_property('output_name', name)
    item.set_editor_property('output_type', kind)
    outputs.append(item)
shader.set_editor_property('additional_outputs', outputs)
for node, name in [(pos, 'WorldP'), (normal, 'SurfaceN'), (color, 'Tint'), (masonry, 'Masonry')]:
    assert lib.connect_material_expressions(node, '', shader, name)
assert lib.connect_material_property(shader, '', unreal.MaterialProperty.MP_BASE_COLOR)
assert lib.connect_material_property(shader, 'StoneRoughness', unreal.MaterialProperty.MP_ROUGHNESS)
assert lib.connect_material_property(shader, 'StoneNormal', unreal.MaterialProperty.MP_NORMAL)
fill = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 50, 300)
fill.set_editor_property('const_b', .02)
assert lib.connect_material_expressions(color, '', fill, 'A')
assert lib.connect_material_property(fill, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
assert 'Color' in {str(n) for n in lib.get_vector_parameter_names(mat)}
assert 'Masonry' in {str(n) for n in lib.get_scalar_parameter_names(mat)}
unreal.log('[CultMatchStone] Original match material authored; inspect compile diagnostics and native renders')
