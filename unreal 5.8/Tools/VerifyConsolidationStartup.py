"""Run the map-ready entry point, prove maps preserved, without replacing authored maps."""
from pathlib import Path
import hashlib,json,runpy
import unreal
project=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==project
paths=list((project/'Content').rglob('*.umap'))
before={str(p.relative_to(project)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
runpy.run_path(str(project/'Tools/BuildAll.py'),run_name='__main__')
after={str(p.relative_to(project)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
assert before==after,'Authored maps must stay byte-identical'
(project/'Saved/Consolidation20261027/Startup.json').write_text(json.dumps(dict(build_all='PASSED_MAPS_PRESERVED',maps=before),indent=2)+'\n',encoding='utf-8')
unreal.SystemLibrary.quit_editor()
