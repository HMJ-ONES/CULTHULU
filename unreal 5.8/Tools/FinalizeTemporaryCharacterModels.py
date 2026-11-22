"""Bind the imported stand-ins and preserve their original files for Blender."""
import copy
import json
from pathlib import Path
import re

import unreal

PROJECT = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
config_path = PROJECT / 'Config/ApprovedCharacterModels.json'
runtime_path = PROJECT / 'Content/CharacterModels/registry.json'
config = json.loads(config_path.read_text(encoding='utf-8-sig'))
runtime = json.loads(runtime_path.read_text(encoding='utf-8-sig'))
assets = unreal.EditorAssetLibrary
for entity in config['temporary_character_ids']:
    entry = config['characters'][entity]
    binding = runtime['characters'][entity]
    assert binding['source_url'] == entry['source_url'] and entry['rights_cleared']
    binding['temporary'] = True
    if entity == 'ghatanothoa':
        def key(value):
            return re.sub('[^a-z0-9]', '', value.lower())
        wanted = dict(idle='Idle', preview='Idle', walk='Walking', run='Run', attack='Attack_Left',
                      cast='Attack.Powerfull', melee_1='Attack_Left', melee_2='Attack.Right',
                      melee_3='Attack.Powerfull', hit='HitReact.Front', death='Death')
        roles = {}
        for role, name in wanted.items():
            matches = [clip['asset'] for clip in binding['clips'] if key(clip['name']) == 'source' + key(name)]
            assert len(matches) == 1, (entity, role, matches)
            roles[role] = matches[0]
        binding['animation_roles'] = roles
        binding['animation_sets'] = [{'skeleton': binding['skeleton'], 'roles': roles}]
        for mesh_path in binding['meshes']:
            mesh = unreal.load_asset(mesh_path)
            for slot in mesh.get_editor_property('materials'):
                material = slot.get_editor_property('material_interface')
                assert isinstance(material, unreal.MaterialInstanceConstant)
                assert assets.get_metadata_tag(material, 'CultCharacterOwner') == 'CultUlhu.ImportSelectedCharacterAssets'
                unreal.MaterialEditingLibrary.set_material_usage_override(material, unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS, True, True)
                assert assets.save_loaded_asset(material)
    if entity in ('nug_and_yeb', 'rhan_tegoth'):
        roles = {}
        for role, wanted in entry['animation_roles'].items():
            matches = [clip['asset'] for clip in binding['clips'] if re.sub('[^a-z0-9]', '', wanted.lower()) in re.sub('[^a-z0-9]', '', clip['name'].lower())]
            if entity == 'nug_and_yeb' and len(binding['clips']) == 1:
                matches = [binding['clips'][0]['asset']]
            assert len(matches) == 1, (entity, role, matches)
            roles[role] = matches[0]
        binding['animation_roles'] = roles
        binding['animation_sets'] = [{'skeleton': binding['skeleton'], 'roles': roles}]
    if entity == 'nug_and_yeb':
        meshes = list(dict.fromkeys(binding['meshes']))
        assert len(meshes) >= 1
        binding['meshes'] = meshes + meshes
        binding['bodies'] = 2
        binding['floating'] = True
    for asset_path in binding['meshes']:
        assert assets.does_asset_exist(asset_path), asset_path
    for group in binding.get('animation_sets', []):
        for path in group['roles'].values():
            clip = unreal.load_asset(path)
            assert clip and clip.get_editor_property('skeleton').get_path_name() == group['skeleton']
    entry['reviewed_gameplay_binding'] = copy.deepcopy(binding)
    unreal.log('TEMPORARY_CHARACTER_READY ' + entity + ' parts=' + str(len(binding['meshes'])))

assert len(runtime['characters']) == 16
for path, document in ((config_path, config), (runtime_path, runtime)):
    path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
unreal.SystemLibrary.quit_editor()
