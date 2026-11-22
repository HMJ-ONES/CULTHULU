"""Give the temporary twins readable bodies and Hastur a gold cloak."""
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
registry_path = PROJECT / 'Content/CharacterModels/registry.json'
config_path = PROJECT / 'Config/ApprovedCharacterModels.json'
registry = json.loads(registry_path.read_text(encoding='utf-8-sig'))
config = json.loads(config_path.read_text(encoding='utf-8-sig'))
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
owner = 'CultUlhu.TemporaryCharacterMaterials.20261122'
for entity, color, glow in [('nug_and_yeb', (.34, .25, .48), (.08, .055, .15)),
                            ('hastur', (.42, .23, .025), (.01, .005, 0))]:
    destination = f'/Game/CultUlhu/Characters/{entity}/TemporaryMaterials'
    name = 'M_TemporaryBody'
    path = destination + '/' + name
    material = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if material:
        assert assets.get_metadata_tag(material, 'CultOwner') == owner
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
        assert material
        material.set_editor_property('two_sided', True)
        editing.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
        for value, prop in [(color, unreal.MaterialProperty.MP_BASE_COLOR),
                            (glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)]:
            expression = editing.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
            expression.set_editor_property('constant', unreal.LinearColor(*value, 1))
            editing.connect_material_property(expression, '', prop)
        roughness = editing.create_material_expression(material, unreal.MaterialExpressionConstant)
        roughness.set_editor_property('r', .65)
        editing.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
        editing.recompile_material(material)
        assets.set_metadata_tag(material, 'CultOwner', owner)
        assert assets.save_loaded_asset(material)
    binding = registry['characters'][entity]
    overrides = {}
    for mesh in binding['meshes']:
        if entity == 'nug_and_yeb' or any(part in mesh.lower() for part in ('hat_', 'cape_', 'skirt_', 'armor_', 'lower_body_')):
            overrides[mesh] = material.get_path_name()
    binding['material_overrides'] = overrides
    config['characters'][entity]['reviewed_gameplay_binding'] = binding
    unreal.log('TEMPORARY_MATERIAL_READY ' + entity + ' parts=' + str(len(overrides)))
for path, document in ((registry_path, registry), (config_path, config)):
    path.write_text(json.dumps(document, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
unreal.SystemLibrary.quit_editor()
