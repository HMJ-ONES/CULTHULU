"""Build depth-tested signal edges with contrast on bright and dark ground."""
import unreal
lib = unreal.MaterialEditingLibrary
path = '/Game/CultUlhu/Materials/M_AttackSignal'
m = unreal.load_asset(path)
if not m:
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AttackSignal', '/Game/CultUlhu/Materials', unreal.Material, unreal.MaterialFactoryNew())
# This dedicated generated asset is owned by this script, never imported art.
lib.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
color = lib.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
color.set_editor_property('parameter_name', 'Color')
color.set_editor_property('default_value', unreal.LinearColor(.1, .7, .8, 1))
power = lib.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 150)
power.set_editor_property('parameter_name', 'Intensity')
power.set_editor_property('default_value', 1)
mult = lib.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 0)
lib.connect_material_expressions(color, '', mult, 'A')
lib.connect_material_expressions(power, '', mult, 'B')
lib.connect_material_property(mult, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)
if 'Color' not in [str(n) for n in lib.get_vector_parameter_names(m)]:
    raise RuntimeError('Missing attack signal Color parameter')
unreal.log('[CultUlhu] Attack signal material verified: opaque unlit edges')
