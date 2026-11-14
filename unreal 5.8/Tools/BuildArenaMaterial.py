"""Author the arena's actual tint parameter; never alter imported art materials."""
import unreal

path = '/Game/CultUlhu/Materials/M_ArenaSurface'
lib = unreal.MaterialEditingLibrary
material = unreal.load_asset(path)
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_ArenaSurface', '/Game/CultUlhu/Materials', unreal.Material,
        unreal.MaterialFactoryNew())
    color = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(.12, .16, .18, 1))
    lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 220)
    rough.set_editor_property('r', .86)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    # A faint fill preserves geometry in deep vessel shadows; it is not glowing stone.
    fill = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 120)
    fill.set_editor_property('const_b', .035)
    lib.connect_material_expressions(color, '', fill, 'A')
    lib.connect_material_property(fill, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
if 'Color' not in [str(x) for x in lib.get_vector_parameter_names(material)]:
    raise RuntimeError('Arena material has no Color parameter')
unreal.log('[CultUlhu] Arena material verified: Color parameter ready')
