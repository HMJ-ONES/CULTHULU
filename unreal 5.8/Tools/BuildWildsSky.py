"""Create the small original gradient-sky material used by Free Roam."""
from pathlib import Path
import shutil
import unreal

assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == Path(__file__).resolve().parents[1]
path = '/Game/CultUlhu/Materials/M_WildsSky'
lib = unreal.MaterialEditingLibrary
material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
force = '-CultRebuildWildsSky' in unreal.SystemLibrary.get_command_line()
if material and force:
    project = Path(__file__).resolve().parents[1]
    backup = project / 'Saved/Backups/WildsSkyBeforeGraphRepair.uasset'
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(project / 'Content/CultUlhu/Materials/M_WildsSky.uasset', backup)
if not material or force:
    if material:
        lib.delete_all_material_expressions(material)
    material = material or unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_WildsSky', '/Game/CultUlhu/Materials', unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property('two_sided', True)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -600, 250)
    vertical = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -450, 250)
    for channel in ('r', 'g', 'b', 'a'):
        vertical.set_editor_property(channel, channel == 'g')
    assert lib.connect_material_expressions(uv, '', vertical, '')
    scale = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 250)
    scale.set_editor_property('const_b', 2.)
    assert lib.connect_material_expressions(vertical, '', scale, 'A')
    clamp = lib.create_material_expression(material, unreal.MaterialExpressionSaturate, -150, 250)
    assert lib.connect_material_expressions(scale, '', clamp, '')
    zenith = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -450, -100)
    zenith.set_editor_property('parameter_name', 'Zenith')
    zenith.set_editor_property('default_value', unreal.LinearColor(.008, .014, .028, 1))
    horizon = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -450, 50)
    horizon.set_editor_property('parameter_name', 'Horizon')
    horizon.set_editor_property('default_value', unreal.LinearColor(.045, .065, .075, 1))
    gradient = lib.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 50, 0)
    assert lib.connect_material_expressions(zenith, '', gradient, 'A')
    assert lib.connect_material_expressions(horizon, '', gradient, 'B')
    assert lib.connect_material_expressions(clamp, '', gradient, 'Alpha')
    assert lib.connect_material_property(gradient, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
assert {'Zenith', 'Horizon'} <= {str(name) for name in lib.get_vector_parameter_names(material)}
# An ordinary unlit background works without SkyAtmosphere's coverage pass.
material.set_editor_property('is_sky', False)
lib.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)

root = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
visited = set()
def visit(node):
    if node and node.get_path_name() not in visited:
        visited.add(node.get_path_name())
        unreal.log('[CultWildsSky] Connected node: ' + node.get_class().get_name())
        for source in lib.get_inputs_for_material_expression(material, node):
            visit(source)
visit(root)
assert len(visited) == 7, 'Gradient graph has a disconnected input; refusing to report ready'

unreal.log('[CultWildsSky] Original gradient material ready; all seven nodes connected')
