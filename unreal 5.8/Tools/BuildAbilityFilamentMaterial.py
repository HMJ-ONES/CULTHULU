"""Bake the original soft additive material used by ability filaments."""
from pathlib import Path
import unreal

PROJECT = Path(__file__).resolve().parents[1]
assert PROJECT.name == "unreal 5.8" and (PROJECT / "CultUlhu.uproject").is_file()
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT

LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
FOLDER = "/Game/CultUlhu/Materials"
NAME = "M_AbilityFilament"
OWNER = "CultUlhu.AbilityFilaments.v1"
material = unreal.load_asset(f"{FOLDER}/{NAME}")
if material:
    assert ASSETS.get_metadata_tag(material, "CultOwner") == OWNER, "Preserve artist-created material"
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    assert material
    ASSETS.set_metadata_tag(material, "CultOwner", OWNER)

LIB.delete_all_material_expressions(material)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

def node(cls, x, y, **settings):
    result = LIB.create_material_expression(material, cls, x, y)
    for name, value in settings.items():
        result.set_editor_property(name, value)
    return result

def wire(a, out, b, pin):
    assert LIB.connect_material_expressions(a, out, b, pin), (a, out, b, pin)

color = node(unreal.MaterialExpressionVertexColor, -700, -140)
gain = node(unreal.MaterialExpressionScalarParameter, -700, 0, parameter_name="Gain", default_value=4.5)
emissive = node(unreal.MaterialExpressionMultiply, -260, -140)
wire(color, "", emissive, "A")
wire(gain, "", emissive, "B")

uv = node(unreal.MaterialExpressionTextureCoordinate, -930, 240)
u = node(unreal.MaterialExpressionComponentMask, -740, 240, r=True, g=False, b=False, a=False)
middle = node(unreal.MaterialExpressionSubtract, -550, 240, const_b=.5)
absolute = node(unreal.MaterialExpressionAbs, -370, 240)
distance = node(unreal.MaterialExpressionMultiply, -190, 240, const_b=2.)
feather = node(unreal.MaterialExpressionOneMinus, -10, 240)
falloff = node(unreal.MaterialExpressionPower, 160, 240, const_exponent=1.3)
opacity = node(unreal.MaterialExpressionMultiply, 360, 90)
wire(uv, "", u, "")
wire(u, "", middle, "A")
wire(middle, "", absolute, "")
wire(absolute, "", distance, "A")
wire(distance, "", feather, "")
wire(feather, "", falloff, "")
wire(falloff, "", opacity, "A")
wire(color, "A", opacity, "B")
assert LIB.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert LIB.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
LIB.recompile_material(material)
assert ASSETS.save_loaded_asset(material)
unreal.log("ABILITY_FILAMENT_MATERIAL_SAVED")
unreal.SystemLibrary.quit_editor()
