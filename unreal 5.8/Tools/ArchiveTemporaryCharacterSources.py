"""Keep the six licensed original GLBs recoverable using the repo's b64 convention."""
import base64
import hashlib
import json
from pathlib import Path

project = Path(__file__).resolve().parents[1]
root = project.parent
config = json.loads((project / 'Config/ApprovedCharacterModels.json').read_text(encoding='utf-8-sig'))
directory = root / 'assets/characters/temporary_20261122'
directory.mkdir(exist_ok=True)
records = {}
for entity in config['temporary_character_ids']:
    entry = config['characters'][entity]
    if 'original_source_file' not in entry:
        continue  # Previously archived Nyarlathotep and Nodens stay unchanged.
    source = project / entry['original_source_file']
    data = source.read_bytes()
    assert hashlib.sha256(data).hexdigest() == entry['original_source_sha256']
    archive = directory / (entity + '.glb.b64')
    encoded = base64.encodebytes(data)
    if archive.exists():
        assert archive.read_bytes() == encoded, 'Preserve previously archived source'
    else:
        archive.write_bytes(encoded)
    records[entity] = {key: entry[key] for key in ('title', 'author', 'author_url', 'source_url', 'license', 'license_url', 'original_source_sha256')}
    records[entity]['archive'] = archive.relative_to(root).as_posix()
    records[entity]['local_original'] = source.relative_to(root).as_posix()
    print(entity, len(data), 'bytes archived')
assert len(records) == 6
(directory / 'sources.json').write_text(json.dumps(records, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
