"""Create an unlit, vertex-colored activity label from the engine font material."""
import unreal
path = "/Game/CultUlhu/Materials/M_CultActivityText"
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.EditorAssetLibrary.duplicate_asset("/Engine/EngineMaterials/DefaultTextMaterialOpaque", path)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -250, 0)
assert unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log("Cult activity text material ready")

# These tiny original capes/hats/staves use regular meshes and preserve their imported textures.
for asset_path in unreal.EditorAssetLibrary.list_assets("/Game/CultUlhu/People", recursive=True):
    if "/StaticMeshes/" not in asset_path:
        continue
    asset = unreal.load_asset(asset_path)
    if isinstance(asset, unreal.StaticMesh):
        settings = asset.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", False)
        asset.set_editor_property("nanite_settings", settings)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
