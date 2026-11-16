"""Create missing prototype materials only; preserve all existing art assets."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
root = "/Game/CultUlhu/Materials"
colors = {
    "M_PawnVessel": (0.055, 0.025, 0.11),
    "M_Glow_Eye": (1.6, 2.2, 0.5),
    "M_Glow_Rune": (1.2, 0.35, 2.2),
    "M_CultistBody": (0.3, 0.06, 0.6),
    "M_CivilianBody": (0.5, 0.29, 0.12),
    "M_FX_Dust": (0.25, 0.21, 0.18),
    "M_FX_Flash": (3.0, 1.5, 0.35),
    "M_Decal_Scorch": (0.015, 0.01, 0.02),
}
for name, color in colors.items():
    path = root + "/" + name
    if library.does_asset_exist(path):
        continue
    material = tools.create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -300, 0)
    node.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    library.save_loaded_asset(material)
unreal.log("[CultUlhu] Missing play materials created; existing assets preserved.")
