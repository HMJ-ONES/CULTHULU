"""Create new presentation materials without modifying imported art or the old map."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
root = '/Game/CultUlhu/Materials'

def material(name, color=None):
    path = root + '/' + name
    existing = unreal.load_asset(path)
    if existing:
        return existing
    mat = tools.create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
    if color is None:
        vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -400, 0)
        tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 180)
        tex.texture = unreal.load_asset('/Game/CultUlhu/Terrain/ground-detail-dark')
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 0)
        mel.connect_material_expressions(vc, '', mul, 'A')
        mel.connect_material_expressions(tex, 'RGB', mul, 'B')
        mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        base = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -200, 0)
        base.constant = unreal.LinearColor(*color)
        mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 300)
    rough.r = .92
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('two_sided', True)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    return mat

material('M_LivingGround')
material('M_LivingCavern', (.075, .085, .11, 1))
print('[CultUlhu] Living landscape materials ready')

# Material usage changes live on copies; imported source materials remain intact.
usage = next(getattr(unreal.MaterialUsage, n) for n in dir(unreal.MaterialUsage) if 'INSTANCED_STATIC' in n)
nanite_usage = next(getattr(unreal.MaterialUsage, n) for n in dir(unreal.MaterialUsage) if 'NANITE' in n)
names = ['cave-rock-tall-a', 'rocks-tall', 'tree-dead-large', 'pine-dead-crooked', 'dark-crystal-cluster', 'cave-wall-rock']
for name in names:
    mesh = unreal.load_asset('/Game/CultUlhu/World/' + name + '/StaticMeshes/' + name)
    if not mesh:
        raise RuntimeError('Missing instanced mesh: ' + name)
    for slot, entry in enumerate(mesh.get_editor_property('static_materials')):
        source = entry.material_interface
        if not source:
            continue
        target = root + '/M_Living_' + name.replace('-', '_') + '_' + str(slot)
        copy = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(), target)
        if isinstance(copy, unreal.Material):
            mel.set_base_material_usage(copy, usage, True)
            mel.set_base_material_usage(copy, nanite_usage, True)
            mel.recompile_material(copy)
        else:
            base_path = target + '_Base'
            base = unreal.load_asset(base_path) if unreal.EditorAssetLibrary.does_asset_exist(base_path) else unreal.EditorAssetLibrary.duplicate_asset(source.get_base_material().get_path_name(), base_path)
            mel.set_base_material_usage(base, usage, True)
            mel.set_base_material_usage(base, nanite_usage, True)
            mel.set_material_instance_parent(copy, base)
            mel.set_material_usage_override(copy, nanite_usage, True, True)
            mel.recompile_material(base)
            unreal.EditorAssetLibrary.save_asset(base_path)
        unreal.EditorAssetLibrary.save_asset(target)
        print('[CultUlhu] Instance material ready: ' + target)
