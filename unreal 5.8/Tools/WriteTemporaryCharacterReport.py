"""Write an honest roster report after the actual model and network checks pass."""
import json
from pathlib import Path

project = Path(__file__).resolve().parents[1]
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
config = read(project / 'Config/ApprovedCharacterModels.json')
runtime = read(project / 'Content/CharacterModels/registry.json')
assert len(runtime['characters']) == 16
log = (project / 'Saved/Logs/TemporaryModelsFinal.log').read_text(encoding='utf-8-sig')
assert 'Result={Success}' in log and 'LogAutomationController: Error:' not in log
for mode in range(3):
    for role in ('host', 'guest'):
        assert read(project / f'Saved/Automation/Room{mode}_{role}.json')['ok']
    log = (project / f'Saved/Room{mode}Guest.log').read_text(encoding='utf-8-sig')
    assert 'Guest verified' in log and 'nug_and_yeb model' in log
notes = {
    'hastur': 'Procedural body motion only. Original GLB has rigid node animation; Unreal geometry has no skin.',
    'mother_hydra': 'Procedural body motion only. Static sculpture needs an armature and skin weights.',
    'shub_niggurath': 'Procedural body motion only. Static tree needs an armature and skin weights.',
    'nodens': 'Procedural chariot motion only; no creature skeleton. Horse omitted.',
    'nug_and_yeb': 'One licensed jellyfish duplicated into two bodies. Supplied pulse loop plus independent diagonal drifting; no separate attack clip.',
    'rhan_tegoth': 'Supplied idle/walk plus procedural claw combat. Run reuses walk; no dedicated death clip.',
    'ghatanothoa': 'Supplied idle/walk/run/attack/cast/hit/death. Corrected morph units and calibrated posed bounds.',
    'sghllor': 'Original animated light and mist; no creature skeleton.'}
static = {'hastur', 'mother_hydra', 'shub_niggurath', 'nodens'}
lines = ['# Temporary character models — 2026-11-22', '',
    'All 16 released characters have in-game visuals. Eight missing selections now use user-authorized free substitutes. Preferred final selections remain in `Config/ApprovedCharacterModels.json` → `preferred_character_selections`; Ghatanothoa remains Biblical Beast — Behemoth there. The rejected Overlord is not used.', '',
    '## Per-character status', '',
    'IMPORTED and PLAYABLE describe the tested local build. ANIMATED describes the implementation, not finished animation polish. No missing-model blockers remain. Full limb animation remains unfinished for the four static substitutes.', '',
    '| Character ID | Model | Status | Animation / remaining work |', '|---|---|---|---|']
for entity, binding in runtime['characters'].items():
    entry = config['characters'][entity]
    status = 'IMPORTED / PLAYABLE / ANIMATED' + (' (procedural body only)' if entity in static else '')
    model = entry['title'] + (' (temporary)' if binding.get('temporary') else '')
    detail = notes.get(entity, 'Existing skeletal gameplay bindings preserved; poses and camera visibility checked.')
    lines.append(f'| {entity} | {model} | {status} | {detail} |')
lines += ['', '## Open in Blender', '',
    '1. Edit a copy. Keep current imported Unreal assets intact.',
    '2. Blender: File → Import → glTF 2.0, then choose a GLB below. For the troll use the prepared file, which fixes the supplied morph units.',
    '3. Save your working `.blend` in a new folder under the repository’s `assets/characters/` directory.',
    '4. Rig and weight Hastur, Mother Hydra, Shub-Niggurath and Nodens before creating limb animations.',
    '5. Export improved geometry and animations to a new versioned file, preserving textures, UVs and skeleton compatibility.',
    '6. Provide the exported path for reimport and validation. Do not overwrite `.uasset` files.', '',
    'Paths below are relative to `unreal 5.8/`:', '', '| Character ID | Blender source |', '|---|---|']
for entity in config['temporary_character_ids']:
    entry = config['characters'][entity]
    source = entry.get('original_source_file', entry.get('source_file', ''))
    if entity == 'ghatanothoa':
        source = entry['source_file']
    assert (project / source).is_file(), source
    lines.append(f'| {entity} | `{source}` |')
lines += ['',
    'The six new untouched GLBs are also archived as `.glb.b64` under `assets/characters/temporary_20261122/`. Source URLs, licensing records and SHA-256 hashes are in `sources.json`. `assets/decode_assets.py` restores them after cloning. Preserve credits when distributing modified art. These source files are outside the deleted Blender folder.', '',
    '## Validation performed', '',
    '- Canonical validator and actual CultUlhuEditor Win64 Development build passed.',
    '- `CultUlhu.Art.ApprovedCharacterModels` passed: all 16 previews; pawn installation in all three modes; canonical height, blocking capsules, first/third-person body visibility and corpse geometry. Troll and crab posed vertices fit gameplay height.',
    '- Visually inspected all six new imports, including troll attack/run and crab walk. Verified separate floating twin bodies and independent drifting.',
    '- Local two-process host/client probes passed in Free Roam, Domination and DOTGOO, with Nug & Yeb hosting and Mi-Go joining. Remote imported visual, movement, combat, local leaving and bot replacement passed. DOTGOO creep movement and ward-warning replication passed. This does not claim an internet/Radmin or Cloudflare test.',
    '- Refreshed eight temporary selection portraits. Cook paths include the new art, materials and portraits.',
    '- Inspected the desktop Cult-Ulhu shortcut: it targets this exact canonical project with `-game` and uses these updates.', '',
    'Evidence: `Saved/Logs/TemporaryModelsFinal.log`, `Saved/Screenshots/ApprovedCharacters/`, and `Saved/Automation/Room{0,1,2}_*.json`. Build output and test saves are excluded from GitHub.', '',
    'Licenses and artist credits: [CREDITS.md](../Content/CharacterModels/CREDITS.md).']
path = project / 'Docs/TemporaryCharacterModels20261122.md'
path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
print(path)
