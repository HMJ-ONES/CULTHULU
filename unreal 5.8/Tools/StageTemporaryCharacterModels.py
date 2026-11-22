"""Stage the user's freely licensed stand-ins without replacing source art."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct

PROJECT = Path(__file__).resolve().parents[1]
assert PROJECT.name == 'unreal 5.8' and (PROJECT / 'CultUlhu.uproject').is_file()
CONFIG = PROJECT / 'Config/ApprovedCharacterModels.json'
RUNTIME = PROJECT / 'Content/CharacterModels/registry.json'
config = json.loads(CONFIG.read_text(encoding='utf-8-sig'))
runtime = json.loads(RUNTIME.read_text(encoding='utf-8-sig'))
backup = PROJECT / 'Saved/TemporaryCharacters20261122/Before'
backup.mkdir(parents=True, exist_ok=True)
for path in (CONFIG, RUNTIME, PROJECT / 'Config/DefaultGame.ini', PROJECT / 'Content/CharacterModels/CREDITS.md'):
    target = backup / path.name
    if not target.exists():
        shutil.copy2(path, target)

choices = {
    'ghatanothoa': ('troll.glb', 'troll-59e679e0da5c4906935b8841aefe6dc0', [0, -90, 0]),
    'hastur': ('low_poly_wizard_rigged.glb', 'low-poly-wizard-rigged-f8c91af65aec4779b0e3d857b1878cc2', [0, -90, 0]),
    'mother_hydra': ('yamata_no_orochi.glb', 'yamata-no-orochi-8f4c0e2c632e401eb5c5381acc3ce789', [0, -90, 0]),
    'rhan_tegoth': ('crabulb_-_rigged.glb', 'crabulb-rigged-eccf00581ba04a68a98a7fd52424c1a8', [0, -90, 0]),
    'shub_niggurath': ('tree_monster.glb', 'tree-monster-97f38826e6f14050b8aed9e8250c1d43', [0, -90, 0]),
    'nug_and_yeb': ('simple_moon_jellyfish_baked_animation.glb', 'simple-moon-jellyfish-baked-animation-3d911c511f2744ec9bfb99d46d2d6e14', [0, 0, 0]),
}
roles = {
    'ghatanothoa': {'preview': '_Idle', 'idle': '_Idle', 'walk': '_Walking', 'run': '_Run',
                   'attack': '_Attack_Left', 'cast': '_Attack.Powerfull', 'melee_1': '_Attack_Left',
                   'melee_2': '_Attack.Right', 'melee_3': '_Attack.Powerfull', 'hit': '_HitReact.Front', 'death': '_Death'},
    'rhan_tegoth': {'preview': '_Idle', 'idle': '_Idle', 'walk': '_Walking', 'run': '_Walking'},
    'nug_and_yeb': {'preview': 'Loop-400', 'idle': 'Loop-400', 'walk': 'Loop-400', 'run': 'Loop-400'},
}
preserved = config.setdefault('preferred_character_selections', {})
for entity, (filename, slug, rotation) in choices.items():
    original = Path.home() / 'Downloads' / filename
    data = original.read_bytes()
    magic, version, size, length, kind = struct.unpack_from('<4sIIII', data)
    assert (magic, version, size, kind) == (b'glTF', 2, len(data), 0x4e4f534a)
    model = json.loads(data[20:20 + length])
    extras = model['asset']['extras']
    expected = 'https://sketchfab.com/3d-models/' + slug
    assert extras['source'] == expected and extras['license'].startswith('CC-BY-4.0')
    previous = config['characters'].get(entity)
    if previous and not previous.get('temporary'):
        preserved.setdefault(entity, copy.deepcopy(previous))
    source = PROJECT / 'Saved/CharacterSources' / entity / 'Temporary20261122/source.glb'
    source.parent.mkdir(parents=True, exist_ok=True)
    if source.exists():
        assert source.read_bytes() == data, 'Preserve existing staged source'
    else:
        shutil.copy2(original, source)
    staged = source
    changes = []
    if entity == 'hastur':
        # This export animates rigid node transforms and contains no skin.
        # Import it as assembled geometry; retain the animated original for Blender.
        model.pop('animations', None)
        changes.append('Rigid node animation retained in original download; temporary Unreal visual uses static geometry.')
    if entity == 'ghatanothoa':
        # The original FBX conversion encoded morph positions in centimetres
        # while the base mesh is in metres. Repair deltas, as with Bokrug.
        binary = bytearray(data[28 + length:])
        def positions(index):
            accessor = model['accessors'][index]
            view = model['bufferViews'][accessor['bufferView']]
            assert accessor['type'] == 'VEC3' and accessor['componentType'] == 5126
            start = view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
            stride = view.get('byteStride', 12)
            return accessor, range(start, start + stride * accessor['count'], stride)
        count = 0
        for mesh in model['meshes']:
            for primitive in mesh['primitives']:
                _, offsets = positions(primitive['attributes']['POSITION'])
                base = [struct.unpack_from('<3f', binary, offset) for offset in offsets]
                for target in primitive.get('targets', []):
                    accessor, offsets = positions(target['POSITION'])
                    deltas = [struct.unpack_from('<3f', binary, offset) for offset in offsets]
                    corrected = [tuple((delta - 99 * v) / 100 for v, delta in zip(vertex, change))
                                 for vertex, change in zip(base, deltas)]
                    assert len(corrected) == len(base)
                    assert max(abs(value) for row in corrected for value in row) < 1
                    for offset, values in zip(offsets, corrected):
                        struct.pack_into('<3f', binary, offset, *values)
                    accessor['min'] = [min(axis) for axis in zip(*corrected)]
                    accessor['max'] = [max(axis) for axis in zip(*corrected)]
                    count += 1
        assert count == 12
        changes.append('Corrected the units of twelve source morph targets.')
    if changes:
        header = json.dumps(model, separators=(',', ':')).encode()
        header += b' ' * (-len(header) % 4)
        binary = binary if entity == 'ghatanothoa' else data[28 + length:]
        prepared = struct.pack('<4sIIII', magic, version, 28 + len(header) + len(binary), len(header), 0x4e4f534a)
        prepared += header + struct.pack('<II', len(binary), 0x004e4942) + binary
        staged = source.parent / 'prepared/source.glb'
        staged.parent.mkdir(exist_ok=True)
        if staged.exists():
            assert staged.read_bytes() == prepared
        else:
            staged.write_bytes(prepared)
    author, author_url = extras['author'].rsplit(' (', 1)
    entry = dict(approved=True, temporary=True, source_url=expected, title=extras['title'],
                 author=author, author_url=author_url.rstrip(')'), license='CC-BY-4.0',
                 license_url='https://creativecommons.org/licenses/by/4.0/', license_verified=True,
                 rights_cleared=True, verified_date='2026-11-22', status='ACQUIRED',
                 source_file=staged.relative_to(PROJECT).as_posix(), original_source_file=source.relative_to(PROJECT).as_posix(),
                 original_source_sha256=hashlib.sha256(data).hexdigest(), rotation=rotation,
                 floating=entity=='nug_and_yeb', bodies=2 if entity=='nug_and_yeb' else 1,
                 animation_roles=roles.get(entity, {}), changes=changes)
    if entity == 'ghatanothoa':
        entry['excluded_mesh_names'] = ['Eyes_002_Teeth_Eye_0']
    config['characters'][entity] = entry

# The user's temporary substitutions also permit the previously licensed local art.
for entity in ('nyarlathotep', 'nodens'):
    previous = config['characters'][entity]
    if not previous.get('temporary'):
        preserved.setdefault(entity, copy.deepcopy(previous))
    entry = copy.deepcopy(config['superseded_selections'][entity])
    assert entry['license_verified'] and entry['rights_cleared']
    entry['temporary'] = True
    config['characters'][entity] = entry
    if entity == 'nyarlathotep':
        runtime['characters'][entity] = copy.deepcopy(entry['reviewed_gameplay_binding'])
    else:
        before = json.loads((PROJECT / 'Saved/MotionIntegration/BeforeOctoberSelectionRefresh/registry.json').read_text(encoding='utf-8-sig'))
        binding = before['characters'][entity]
        assert binding['source_url'] == entry['source_url']
        runtime['characters'][entity] = binding

config['temporary_character_ids'] = list(choices) + ['nyarlathotep', 'nodens']
for path, document in ((CONFIG, config), (RUNTIME, runtime)):
    path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print('Temporary selections staged:', ', '.join(config['temporary_character_ids']))
