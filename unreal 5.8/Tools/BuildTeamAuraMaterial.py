"""Build the additive, depth-tested character rim used in competitive modes."""
from pathlib import Path
import unreal

project = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == project
folder = "/Game/CultUlhu/Materials"
name = "M_CompetitiveTeamRim"
owner = "CultUlhu.CompetitiveTeamRim.v1"
assets = unreal.EditorAssetLibrary
lib = unreal.MaterialEditingLibrary
path = f"{folder}/{name}"
if assets.does_asset_exist(path):
    material = unreal.load_asset(path)
    assert assets.get_metadata_tag(material, "CultOwner") == owner
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    assets.set_metadata_tag(material, "CultOwner", owner)

lib.delete_all_material_expressions(material)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("used_with_skeletal_mesh", True)

def node(cls, x, y, **properties):
    expression = lib.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression

rim = node(unreal.MaterialExpressionFresnel, -300, 80, exponent=4.1, base_reflect_fraction=0.)
color = node(unreal.MaterialExpressionVectorParameter, -300, -70, parameter_name="TeamColor",
             default_value=unreal.LinearColor(.05, .29, .95, 1.))
strength = node(unreal.MaterialExpressionMultiply, -75, 80, const_b=.23)
assert lib.connect_material_expressions(rim, "", strength, "A")
assert lib.connect_material_property(strength, "", unreal.MaterialProperty.MP_OPACITY)
assert lib.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(material)
assert assets.save_loaded_asset(material)
unreal.log("COMPETITIVE_TEAM_RIM_SAVED")
unreal.SystemLibrary.quit_editor()
