"""Persist native first-pose geometry measurements for runtime builds without CPU skinning."""
from pathlib import Path
import json
project=Path(__file__).resolve().parents[1]
config_path=project/'Config/ApprovedCharacterModels.json'
registry_path=project/'Content/CharacterModels/registry.json'
config=json.loads(config_path.read_text(encoding='utf-8-sig')); registry=json.loads(registry_path.read_text(encoding='utf-8-sig'))
for path in (project/'Saved/Reports/CharacterCalibration').glob('*.json'):
 record=json.loads(path.read_text(encoding='utf-8-sig')); entity=record['id']
 if entity not in registry['characters']: continue
 binding=registry['characters'][entity]
 assert record['source_sha256']==binding['source_sha256'],'Stale source calibration'
 assert record.get('import_profile','default')==binding.get('import_profile','default'),'Stale importer calibration'
 config['characters'][entity]['reference_bounds']=record['reference_bounds']
 config['characters'][entity]['reference_bounds_sha256']=record['source_sha256']
 config['characters'][entity]['reference_bounds_profile']=record.get('import_profile','default')
 config['characters'][entity]['reference_bounds_rotation']=binding.get('rotation',[0,0,0])
 binding['reference_bounds']=record['reference_bounds']
for path,data in [(config_path,config),(registry_path,registry)]: path.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
