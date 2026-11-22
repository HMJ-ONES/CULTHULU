"""Persist bounds measured from the rendered idle poses of the two imported rigs.

Run after the ApprovedCharacterModels check, before recapturing portraits.
The source meshes and skeletons remain untouched.
"""
import json
from pathlib import Path
import re

project = Path(__file__).resolve().parents[1]
log = project / 'Saved/Logs/TemporaryModelsCalibration.log'
text = log.read_text(encoding='utf-8-sig')
assert 'Result={Success}' in text
runtime_path = project / 'Content/CharacterModels/registry.json'
config_path = project / 'Config/ApprovedCharacterModels.json'
runtime = json.loads(runtime_path.read_text(encoding='utf-8-sig'))
config = json.loads(config_path.read_text(encoding='utf-8-sig'))
vector = r'X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+)'
for entity in ('ghatanothoa', 'rhan_tegoth'):
    matches = re.findall(r'MODEL_ACTUAL ' + entity + r' IsValid=true, Min=\(' + vector + r'\), Max=\(' + vector + r'\) part=' + vector, text)
    assert len(matches) == len(runtime['characters'][entity]['meshes'])
    scale = re.search(r'MODEL_GEOMETRY ' + entity + r' .*? scale=' + vector, text)
    assert scale
    scale = float(scale[1])
    rows = [tuple(map(float, row)) for row in matches]
    offset = rows[0][6:9]
    assert all(row[6:9] == offset for row in rows)
    low = [min(row[axis] for row in rows) for axis in range(3)]
    high = [max(row[axis + 3] for row in rows) for axis in range(3)]
    bounds = [(value - offset[axis % 3]) / scale for axis, value in enumerate(low + high)]
    binding = runtime['characters'][entity]
    binding['reference_bounds'] = bounds
    binding['bounds_calibration'] = 'Rendered idle-pose CPU vertices; 2026-11-22; source meshes unchanged.'
    config['characters'][entity]['reviewed_gameplay_binding'] = binding.copy()
    print(entity, bounds)
for path, document in ((runtime_path, runtime), (config_path, config)):
    path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
