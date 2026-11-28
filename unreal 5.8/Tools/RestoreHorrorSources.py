"""Restore compact licensed Blender sources from GitHub's binary-safe archives."""
from pathlib import Path
import base64,hashlib,io,json,zipfile
PROJECT=Path(__file__).resolve().parents[1]
assert PROJECT.name=='unreal 5.8' and (PROJECT/'CultUlhu.uproject').is_file()
ROOT=PROJECT.parent
records=json.loads((ROOT/'assets/characters/horror_refresh_20261127/sources.json').read_text())
for record in records:
    target=(PROJECT/record['source_file']).resolve()
    assert target.is_relative_to(PROJECT/'Saved/CharacterSources/HorrorRefresh20261127')
    with zipfile.ZipFile(io.BytesIO(base64.b64decode((ROOT/record['archive']).read_bytes()))) as archive:
        assert set(archive.namelist())=={'source.glb','LICENSE_AND_SOURCE.json'}
        data=archive.read('source.glb')
    assert hashlib.sha256(data).hexdigest()==record['source_sha256']
    if target.exists(): assert target.read_bytes()==data,'Preserve edited local source: '+str(target)
    else: target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    print(record['key'],target)
