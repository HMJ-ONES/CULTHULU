"""Persist Nanite shader usage on the existing world art and its living copies.

Only usage flags change. Textures, graph connections, colors and geometry remain
authored as before. GitHub checkpoint aa050c7 preserves the pre-change assets.
"""
from pathlib import Path
import unreal

project = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == project
mel = unreal.MaterialEditingLibrary
usage = next(getattr(unreal.MaterialUsage, name) for name in dir(unreal.MaterialUsage) if 'NANITE' in name)
names = ['cave-rock-tall-a', 'rocks-tall', 'tree-dead-large', 'pine-dead-crooked', 'dark-crystal-cluster', 'cave-wall-rock']
targets = {}
for name in names:
    mesh = unreal.load_asset('/Game/CultUlhu/World/' + name + '/StaticMeshes/' + name)
    assert mesh, 'Missing existing world mesh: ' + name
    for slot, entry in enumerate(mesh.get_editor_property('static_materials')):
        source = entry.material_interface
        assert source, f'Missing material {name}/{slot}'
        targets[source.get_path_name()] = source
        path = '/Game/CultUlhu/Materials/M_Living_' + name.replace('-', '_') + '_' + str(slot)
        copy = unreal.load_asset(path)
        assert copy, 'Missing living world material: ' + path
        targets[copy.get_path_name()] = copy

saved = set()
for path, material in sorted(targets.items()):
    base = material if isinstance(material, unreal.Material) else material.get_base_material()
    assert base and path.startswith('/Game/'), 'Refusing to edit engine/plugin material'
    assert base.get_editor_property('blend_mode') in (unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED)
    if isinstance(material, unreal.MaterialInstanceConstant):
        # UE 5.8 supports per-instance usage; preserve engine/importer parents.
        mel.set_material_usage_override(material, usage, True, True)
        mel.update_material_instance(material)
    else:
        mel.set_base_material_usage(base, usage, True)
        mel.recompile_material(base)
    assert mel.has_material_usage(material, usage), 'Usage did not persist on ' + path
    for asset in (material,):
        asset_path = asset.get_path_name()
        if asset_path not in saved:
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False), 'Could not save ' + asset_path
            saved.add(asset_path)
    unreal.log('[CultNanite] Ready: ' + path)
unreal.log(f'[CultNanite] Verified {len(targets)} runtime material interfaces; saved {len(saved)} assets')
